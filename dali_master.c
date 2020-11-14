/******************************************************************************/
/*  Copyright (c) 2012 NXP B.V.  All rights are reserved.                     */
/*  Reproduction in whole or in part is prohibited without the prior          */
/*  written consent of the copyright owner.                                   */
/*                                                                            */
/*  This software and any compilation or derivative thereof is, and           */
/*  shall remain the proprietary information of NXP and is                    */
/*  highly confidential in nature. Any and all use hereof is restricted       */
/*  and is subject to the terms and conditions set forth in the               */
/*  software license agreement concluded with NXP B.V.                        */
/*                                                                            */
/*  Under no circumstances is this software or any derivative thereof         */
/*  to be combined with any Open Source Software, exposed to, or in any       */
/*  way licensed under any Open License Terms without the express prior       */
/*  written permission of the copyright owner.                                */
/*                                                                            */
/*  For the purpose of the above, the term Open Source Software means         */
/*  any software that is licensed under Open License Terms. Open              */
/*  License Terms means terms in any license that require as a                */
/*  condition of use, modification and/or distribution of a work              */
/*                                                                            */
/*  1. the making available of source code or other materials                 */
/*     preferred for modification, or                                         */
/*                                                                            */
/*  2. the granting of permission for creating derivative                     */
/*     works, or                                                              */
/*                                                                            */
/*  3. the reproduction of certain notices or license terms                   */
/*     in derivative works or accompanying documentation, or                  */
/*                                                                            */
/*  4. the granting of a royalty-free license to any party                    */
/*     under Intellectual Property Rights                                     */
/*                                                                            */
/*  regarding the work and/or any work that contains, is combined with,       */
/*  requires or otherwise is based on the work.                               */
/*                                                                            */
/*  This software is provided for ease of recompilation only.                 */
/*  Modification and reverse engineering of this software are strictly        */
/*  prohibited.                                                               */
/*                                                                            */
/******************************************************************************/

/*******************************************************************************
 *
 * dali_master.c
 *
 * DALI forward frame format:
 *
 *  | S |        8 address bits         |        8 command bits         | stop  |
 *  | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | 1 | 1 | 1 | 1 | 0 | 0 | 0 |   |   |
 *
 * -+ +-+ +---+ +-+ +-+ +-+ +-+   +-+ +---+   +-+ +-+ +-+ +---+ +-+ +-+ +--------
 *  | | | |   | | | | | | | | |   | | |   |   | | | | | | |   | | | | | |
 *  +-+ +-+   +-+ +-+ +-+ +-+ +---+ +-+   +---+ +-+ +-+ +-+   +-+ +-+ +-+
 *
 *  |2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|  4TE  |
 *
 *
 * DALI slave backward frame format:
 *
 *                   | S |         8 data bits           | stop  |
 *                   | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 1 |   |   |
 *
 *   +---------------+ +-+ +---+ +-+ +-+ +-+ +-+   +-+ +-------------
 *   |               | | | |   | | | | | | | | |   | | |
 *  -+               +-+ +-+   +-+ +-+ +-+ +-+ +---+ +-+
 *
 *   |4 + 7 to 22 TE |2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|2TE|  4TE  |
 *
 * 2TE = 834 usec (1200 bps)
 *
 ********************************************************************************
 *  commands supported
 *  ------------------
 *  Type				Range			Repeat		Answer from slave
 *  Power control	0 - 31 			N			N
 *
 *  Configuration	32-129			Y			N
 *  Reserved			130-143			N			N
 *
 *  Query			144-157			N			Y
 *  Reserved			158-159			N			N
 *  Query			160-165			N			Y
 *  Reserved			166-175			N			N
 *  Query			176-197			N			Y
 *  Reserved			198-223			N			N
 *  Query,2xx Std.	224-254			?			?
 *  Query			255				N			Y
 *
 *  Special			256-257			N			N
 *  Special			258-259			Y			N
 *  Special			260-261			N			N
 *  Special			262-263			N			N
 *  Special			264-267			N			N
 *  Special			268-269			N			Y
 *  Special			270				N			N
 *  Reserved			271				N			N
 *  Special			272				N			N
 *******************************************************************************/

#include <stdbool.h>
#include "dali_master_conf.h"
#include "dali_master_hal.h"
#include "dali_master.h"



// Configuration commands
#define CMD32                (0x0120)
#define CMD129               (0x0181)

// Query commands
#define CMD144               (0x0190)
#define CMD157               (0x019D)
#define CMD160               (0x01A0)
#define CMD165               (0x01A5)
#define CMD176               (0x01B0)
#define CMD197               (0x01C5)
#define CMD255               (0x01FF)

// Special commands
#define INITIALISE           (0xA500) // command for starting initialization mode
#define RANDOMISE            (0xA700) // command for generating a random address
#define COMPARE              (0xA900)
#define VERIFY_SHORT_ADDRESS (0xB901)
#define QUERY_SHORT_ADDRESS  (0xBB00)


typedef enum daliMsgTypeTag
{
	DALI_MSG_UNDETERMINED    = 0,
	DALI_MSG_SHORT_ADDRESS   = 1,
	DALI_MSG_GROUP_ADDRESS   = 2,
	DALI_MSG_BROADCAST       = 4,
	DALI_MSG_SPECIAL_COMMAND = 8
} daliMsgType_t;



/* state machine related definitions */
typedef enum stateTag
{
	MS_IDLE = 0,                        // bus idle
	MS_TX_SECOND_HALF_START_BIT,        //
	MS_TX_DALI_FORWARD_FRAME_1_2,           // sending the dali forward frame
	MS_TX_DALI_FORWARD_FRAME_2_2,           // sending the dali forward frame
	MS_TX_STOP_BITS,                    //
	MS_SETTLING_BEFORE_BACKWARD,        // settling between forward and backward - stop bits
	MS_SETTLING_BEFORE_IDLE,            // settling before going to idle, after forward frame
	MS_WAITING_FOR_SLAVE_START_WINDOW,  // waiting for 7Te, start of slave Tx window
	MS_WAITING_FOR_SLAVE_START,         // start of slave Tx window
	MS_RECEIVING_ANSWER                 // receiving slave message
} MASTER_STATE;



static volatile MASTER_STATE masterState;
static volatile bool         waitForAnswer;
static volatile bool         earlyAnswer;
static volatile uint16_t     daliForwardFrame; // converted DALI master command
static volatile uint16_t     capturedFrame;    // data structure for the capture

/***********************************************************/
/* Local functions                                         */
/***********************************************************/

static inline bool DALI_CheckLogicalError(void)
{
	if (!(capturedFrame & 0x100)) return true;
	return false;
}


static inline daliMsgType_t DALI_CheckMsgType(uint16_t forwardFrame)
{
	daliMsgType_t type = DALI_MSG_UNDETERMINED;

	if ((forwardFrame & 0x8000) == 0)
	{
		type = DALI_MSG_SHORT_ADDRESS;
	}
	else if ((forwardFrame & 0xE000) == 0x8000)
	{
		type = DALI_MSG_GROUP_ADDRESS;
	}
	else if ((forwardFrame & 0xFE00) == 0xFE00)
	{
		type = DALI_MSG_BROADCAST;
	}
	else if (((forwardFrame & 0xFF00) >= 0xA000) &&
			((forwardFrame & 0xFF00) <= 0xFD00))
	{
		type = DALI_MSG_SPECIAL_COMMAND;
	}
	return type;
}

static inline bool DALI_CheckWaitForAnswer(uint16_t forwardFrame, daliMsgType_t type)
{
	bool waitFlag = false;

	if (type == DALI_MSG_SPECIAL_COMMAND)
	{
		// Special commands
		if ((forwardFrame == COMPARE) ||
				((forwardFrame & 0xFF81) == VERIFY_SHORT_ADDRESS) ||
				(forwardFrame == QUERY_SHORT_ADDRESS))
		{
			waitFlag = true;
		}
	}
	else
	{
		// Query commands
		if ((((forwardFrame & 0x01FF) >= CMD144) && ((forwardFrame & 0x01FF) <= CMD157)) ||
				(((forwardFrame & 0x01FF) >= CMD160) && ((forwardFrame & 0x01FF) <= CMD165)) ||
				(((forwardFrame & 0x01FF) >= CMD176) && ((forwardFrame & 0x01FF) <= CMD197)) ||
				((forwardFrame & 0x01FF) == CMD255))
		{
			waitFlag = true;
		}
	}
	return waitFlag;
}

static inline bool DALI_CheckRepeatCmd(uint16_t forwardFrame, daliMsgType_t type)
{
	bool repeatCmd = false;

	if (type == DALI_MSG_SPECIAL_COMMAND)
	{
		// Special commands 'initialize' and 'randomize' shall be repeated within 100 ms
		if (((forwardFrame & 0xFF00) == INITIALISE) ||
				(forwardFrame == RANDOMISE))
		{
			repeatCmd = true;
		}
	}
	else
	{
		// Configuration commands (32 - 129) shall all be repeated within 100 ms
		if (((forwardFrame & 0x01FF) >= CMD32) &&
				((forwardFrame & 0x01FF) <= CMD129))
		{
			repeatCmd = true;
		}
	}
	return repeatCmd;
}

static inline void DALI_DoTransmission(uint32_t convertedForwardFrame, bool waitFlag)
{
	bsp_set_led(LED_RTX_DALI_BUS, 0); // LED OFF MEANS TX TO DALI BUS
	// Claim the bus and setup global variables
	masterState      = MS_TX_SECOND_HALF_START_BIT;
	waitForAnswer    = waitFlag;
	daliForwardFrame = convertedForwardFrame;
	DALI_SetOutputLow();
	DALI_PrepareTimerForSending();

	while (masterState != MS_IDLE)
	{
		// wait till transmission is completed
		// __WFI();
		__NOP();
	}
	/*
	if (waitForAnswer)
	{
		if (capturedFrame.capturedItems == 0)
		{
			usbBackwardFrameAnswer = ANSWER_NOTHING_RECEIVED;
		}
		else if (earlyAnswer)
		{
			usbBackwardFrameAnswer = ANSWER_TOO_EARLY;
		}
		else
		{
			if (DALI_Decode())
			{
				usbBackwardFrameAnswer = ANSWER_GOT_DATA;
			}
			else
			{
				usbBackwardFrameAnswer = ANSWER_INVALID_DATA;
			}
		}
		while (usbBackwardFrameAnswer != ANSWER_NOT_AVAILABLE)
		{
			// wait till answer is send to USB host (PC)
			// __WFI();
		}
	}
	*/
}




static inline void DALI_Init(void)
{
	//Precondition GPIO and Timer and Interrupts are configured
	// First init ALL the global variables
	masterState             = MS_IDLE;
	waitForAnswer           = false;
	earlyAnswer             = false;
	daliForwardFrame        = 0;

	bsp_set_led(LED_RTX_DALI_BUS, 1); // LED ON MEANS RX FROM DALI BUS

	// Initialize the phisical layer of the dali master
	DALI_SetOutputHigh();

}

/***********************************************************/
/* Exported Counter/Timer IRQ handler                      */
/***********************************************************/

/* the handling of the protocol is done in the IRQ */
void DALI_StateMachineWrite(void)
{
	static uint16_t bitcount;
	static uint16_t mask;

	// match 0 interrupt
	switch(masterState){
	case MS_TX_SECOND_HALF_START_BIT:
		DALI_SetOutputHigh();
		bitcount = 0;
		masterState = MS_TX_DALI_FORWARD_FRAME_1_2;
		break;
	case MS_TX_DALI_FORWARD_FRAME_1_2:
		mask = (daliForwardFrame & 0x8000)>>15;
		DALI_SetOutputFirstHalf(mask);
		masterState = MS_TX_DALI_FORWARD_FRAME_2_2;
		bitcount++;
		break;
	case MS_TX_DALI_FORWARD_FRAME_2_2:
		DALI_SetOutputSecondHalf(mask);
		daliForwardFrame <<=1;
		bitcount++;
		if (bitcount == 32)
		{
			masterState = MS_TX_STOP_BITS;
		}
		else
		{
			masterState = MS_TX_DALI_FORWARD_FRAME_1_2;
		}
		break;
	case MS_TX_STOP_BITS:
		DALI_SetOutputHigh();
		// the first half of the first stop bit has just been output.
		// do we have to wait for an answer?
		if (waitForAnswer)
		{   // elapse until the end of the last half of the second stop bit
			SET_TIMER_REG_PERIOD(4*TE);
			earlyAnswer = false;
			masterState = MS_SETTLING_BEFORE_BACKWARD;
		}
		else
		{
			// no answer from slave expected, need to wait for the remaining
			// bus idle time before next forward frame
			// add additional 3 TE to minimum specification to be not at the edge of the timing specification
			SET_TIMER_REG_PERIOD((4*TE) + (22*TE) + (3*TE));
			masterState = MS_SETTLING_BEFORE_IDLE;
		}
		break;
	case MS_SETTLING_BEFORE_BACKWARD:
		bsp_set_led(LED_RTX_DALI_BUS, 1); // LED ON MEANS RX FROM DALI BUS
		// setup the first window limit for the slave answer
		// slave should not respond before 7TE
		SET_TIMER_REG_PERIOD(7*MIN_TE);
		DALI_PrepareTimerForReceiving();   // enable receive, capture on both edges
		masterState = MS_WAITING_FOR_SLAVE_START_WINDOW;
		break;

	case MS_WAITING_FOR_SLAVE_START_WINDOW:
		// setup the second window limit for the slave answer,
		// slave must start transmit within the next 23TE window
		SET_TIMER_REG_PERIOD(23*MAX_TE);
		masterState = MS_WAITING_FOR_SLAVE_START;
		break;
	case MS_WAITING_FOR_SLAVE_START:
		// if we still get here, got 'no' or too early answer from slave
		// idle time of 23TE was already elapsed while waiting, so
		// immediately release the bus
		DALI_StopAll();
		masterState = MS_IDLE;
		break;
	case MS_RECEIVING_ANSWER:
		// stop receiving
		// now idle the bus between backward and next forward frame
		// since we don't track the last edge of received frame,
		// conservatively we wait for 23 TE (>= 22 TE as for specification)
		// Receive interval considered anyway the max tolerance for
		// backward frame duration so >22TE should already be asserted
		SET_TIMER_REG_PERIOD(23*TE);
		DALI_StopCapture();
		masterState = MS_SETTLING_BEFORE_IDLE;
		break;
	case MS_SETTLING_BEFORE_IDLE:

		bsp_set_led(LED_RTX_DALI_BUS, 1); // LED ON MEANS RX FROM DALI BUS
		DALI_StopAll();// reset and stop the timer TODO is this == StopAll???
		masterState = MS_IDLE;
		break;
	}
}
void DALI_StateMachineRead(void)
{
	static int32_t lastCaptureTime; //wann wurde diese Routine das letzte Mal aufgerufen
	static int32_t lastBitRelevantTime; //wann wurde ein Wechsel zuletzt als Bit-relevant erkannt
	static bool lastBit;
	static bool lastLevel; //Level muss sich mit jedem Aufruf dieser Funktion ändern
	// capture interrupt
	if (masterState == MS_WAITING_FOR_SLAVE_START_WINDOW)
	{   // slave should not answer yet, it came too early!!!!
		DALI_StopCapture();   // disable capture
		earlyAnswer = true;
	}
	else if (masterState == MS_WAITING_FOR_SLAVE_START)
	{   // we got an edge, so the slave is transmitting now
		// allowed remaining answering time is 22TE
		SET_TIMER_REG_PERIOD(22*MAX_TE);
		DALI_ClearTimerCounter();
		// first pulse is begin of the start bit (1-->0)
		DALI_GetInput(lastLevel);
		capturedFrame = 0;
		lastCaptureTime = -TE;
		lastBit=true;
		masterState = MS_RECEIVING_ANSWER;
	}
	else if (masterState == MS_RECEIVING_ANSWER)
	{   // this part just captures the frame data, evaluation is done
		// at the end of max backward frame duration
		bool nowLevel;
		DALI_GetInput(nowLevel)
		if(nowLevel == lastLevel)
		{
			//Error; CPU too slow
		}

		uint32_t nowTime = DALI_GetCaptureValue();
		uint32_t interval = nowTime - lastCaptureTime;
		if (!((interval >= MIN_TE) && (interval <= MAX_TE)) || ((interval >= MIN_2TE) && (interval <= MAX_2TE)))
		{
			//ERROR
		}
		lastCaptureTime = nowTime;
		lastLevel = nowLevel;
		interval = nowTime - lastBitRelevantTime;
		if ((interval >= MIN_2TE) && (interval <= MAX_2TE))
		{
			//dies ist Bitrelevant - das negierte Level ist der Bitwert
			capturedFrame |= !nowLevel;
			capturedFrame <<=1;
			lastBitRelevantTime = nowTime;
		}

	}
}

/***********************************************************/
/* Public (exported) functions                             */
/***********************************************************/

DALI_Answer_t DALI_GetAnswer(uint8_t *answer)
{
	if (!(capturedFrame & 0xFF00)) return true;
	return false;
}

 void DALI_Send(uint16_t forwardFrame)
 {
 	daliMsgType_t  daliMsgType = DALI_CheckMsgType(forwardFrame);
 	bool           waitFlag = DALI_CheckWaitForAnswer(forwardFrame,daliMsgType);

 	DALI_DoTransmission(forwardFrame, waitFlag);
 	if (DALI_CheckRepeatCmd(forwardFrame,daliMsgType))
 	{
 		DALI_DoTransmission(forwardFrame, waitFlag);
 	}
 }

void DALI_Thread(void)
{
	bool dali_cmd;
	uint16_t forwardFrame;
	uint8_t  busy_led_state = 0;

	DALI_Init();
	//USB_Setup(DALI_GetInReport, DALI_SetOutReport);
	while (1)
	{
		forwardFrame = 0xCAFE;
		bsp_set_led(LED_DALI_BUS_BUSY, busy_led_state);
		DALI_Send(forwardFrame);
		HAL_Delay(1000);
	}
}

#pragma once

#include <stm32f4xx_hal.h>
#include <stm32f4_discovery.h>
#include "dali_master_conf.h"

#define DALI_INPUT_PORT		GPIOB
#define DALI_INPUT_PIN		GPIO_PIN_7
#define DALI_INPUT_TIMER	TIM4
#define DALI_INPUT_CCR		2
#define DALI_OUTPUT_PORT	GPIOB
#define DALI_OUTPUT_PIN		GPIO_PIN_8

#define LED_DALI_BUS_BUSY	LED_ORANGE
#define LED_RTX_DALI_BUS	LED_GREEN


/* PIO pin P2.6 is used as DALI send (tx) pin */
#define DALI_SetOutputHigh() { DALI_OUTPUT_PORT->BSRRL =  DALI_OUTPUT_PIN; }
#define DALI_SetOutputLow()  { DALI_OUTPUT_PORT->BSRRH  = DALI_OUTPUT_PIN; }

#define DALI_SetOutputFirstHalf(mask)	*(&(DALI_OUTPUT_PORT->BSRRL)+mask)=DALI_OUTPUT_PIN
#define DALI_SetOutputSecondHalf(mask)	*(&(DALI_OUTPUT_PORT->BSRRL)+(mask ^ 0x0001))=DALI_OUTPUT_PIN


#define SET_TIMER_REG_PERIOD(x) { DALI_INPUT_TIMER->ARR = x; }

/* PIO pin P1.1 is used as DALI receive (rx) pin */
#ifdef INVERTED_RX
#define DALI_GetInput(x)     { x = (DALI_INPUT_PORT->IDR & DALI_INPUT_PIN) ? false : true; }
#else
#define DALI_GetInput(x)     { x = (DALI_INPUT_PORT->IDR & DALI_INPUT_PIN) ? true : false; }
#endif

void DALI_PrepareTimerForSending(void);
void DALI_PrepareTimerForReceiving(void);
void DALI_StopAll(void);
void DALI_StopCapture(void);
void DALI_ClearTimerCounter(void);
uint32_t DALI_GetCaptureValue(void);
void bsp_set_led(Led_TypeDef led, uint8_t state);

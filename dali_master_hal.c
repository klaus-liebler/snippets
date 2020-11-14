#include "dali_master_hal.h"
#include "dali_master_conf.h"



void bsp_set_led(Led_TypeDef led, uint8_t state){
	if(state){
		BSP_LED_On(led);
	}
	else
	{
		BSP_LED_Off(led);
	}
}

void DALI_PrepareTimerForSending(void)
{
	DALI_INPUT_TIMER->ARR=TE;
	DALI_INPUT_TIMER->SR = ~ (TIM_SR_UIF); // clear possible MR0 interrupt flag
	DALI_INPUT_TIMER->DIER |= TIM_IT_UPDATE;
	DALI_INPUT_TIMER->CR1  |= TIM_CR1_CEN;
}

// enable receive, capture on both edges
void DALI_PrepareTimerForReceiving(void)
{
	//Enable Interrupt
	DALI_INPUT_TIMER->DIER |= (1 << DALI_INPUT_CCR);
	//Clear Interrupt flag?
	DALI_INPUT_TIMER->SR = ~ (1 << DALI_INPUT_CCR);
	//Enable Capture
	/* Set  the CCxE Bit */
	DALI_INPUT_TIMER->CCER |= (1 << DALI_INPUT_CCR);
}

void DALI_StopAll(void)
{
	// reset and stop the timer
	DALI_INPUT_TIMER->CR1  &= ~TIM_CR1_CEN;
	//TODO: Howto "reset" --> write 0 in Counter register?
	DALI_INPUT_TIMER->CNT =0;
	// disable capture
	DALI_INPUT_TIMER->CCER &= ~(1 << DALI_INPUT_CCR);
	// clear possible capture interrupt flag
	DALI_INPUT_TIMER->SR = ~ (1 << DALI_INPUT_CCR);
}

void DALI_StopCapture(void){
	// disable capture
	DALI_INPUT_TIMER->CCER &= ~(1 << DALI_INPUT_CCR);
	// clear possible capture interrupt flag
	DALI_INPUT_TIMER->SR = ~ (1 << DALI_INPUT_CCR);
}

void DALI_ClearTimerCounter(void)
{
	DALI_INPUT_TIMER->CNT =0;	// clear timer
	DALI_INPUT_TIMER->SR = ~ (TIM_SR_UIF); // clear possible MR0 interrupt flag
}

uint32_t DALI_GetCaptureValue(void)
{
	return DALI_INPUT_TIMER->CCR2;
}


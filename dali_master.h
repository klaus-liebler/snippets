#pragma once

typedef enum answerTypeTag
{
	ANSWER_NOT_AVAILABLE = 0,
	ANSWER_NOTHING_RECEIVED,
	ANSWER_GOT_DATA,
	ANSWER_INVALID_DATA,
	ANSWER_TOO_EARLY
} DALI_Answer_t;

void DALI_Thread(void);

DALI_Answer_t DALI_GetAnswer(uint8_t *answer);

void DALI_Send(uint16_t forwardFrame);

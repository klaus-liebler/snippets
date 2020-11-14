#pragma once

#include <inttypes.h>
void
TERMINAL_Init(void *font, uint16_t fg, uint16_t bg, uint8_t orientation);

void
TERMINAL_Putc (char c);
void
TERMINAL_Puts (char *str);
void
TERMINAL_CursorSet (uint16_t row, uint16_t col);

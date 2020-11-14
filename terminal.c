#include "terminal.h"
#include "lcdBaseGFX.h"
#include "gfx.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "DefaultFonts.h"


typedef struct
{
  uint16_t row;
  uint16_t col;
} Cursor;

static struct
{
  Cursor c;
  char *scr;
  uint16_t fg;
  uint16_t bg;
  uint8_t nrow;
  uint8_t ncol;

} _screen;

uint8_t *_font;

/***********************************************************************
 ***********************************************************************
 ***
 ***  ASCII green-screen terminal emulator
 ***
 ***  Written by Oleg Yakovlev
 ***  MIT license, all text below must be included in any redistribution
 ***
 ***********************************************************************
 ***********************************************************************/

static void
_putch (uint8_t c);

#define _scr(r,c) ((char *)(_screen.scr + ((r) * _screen.ncol) + (c)))

/********************************************************************
 *********************************************************************
 *********************** Private functions ***************************
 *********************************************************************
 *********************************************************************/
static void
_scrollup (void)
{
  int r, c;
  _screen.c.row = 0;
  _screen.c.col = 0;
  for (r = 1; r < _screen.nrow; r++)
    for (c = 0; c < _screen.ncol; c++)
      {
	_putch (*_scr(r, c));
	_screen.c.col++;
	if (_screen.c.col == _screen.ncol)
	  {
	    _screen.c.col = 0;
	    _screen.c.row++;
	  }
      }
  for (c = 0; c < _screen.ncol; c++)
    {
      _putch (' ');
      _screen.c.col++;
    }
  _screen.c.row = _screen.nrow - 1;
  _screen.c.col = 0;
}

static void
cursor_expose (int flg)
{
  uint8_t i, fz;
  uint16_t j;
  int x, y;

  fz = FONT_XSIZE / 8;
  x = _screen.c.col * FONT_XSIZE;
  y = _screen.c.row * FONT_YSIZE;
  LCD_SetAddrWindow (x, y, x + FONT_XSIZE - 1,
		     y + FONT_YSIZE - 1);
  for (j = 0; j < ((fz) * FONT_YSIZE); j++)
    {
      for (i = 0; i < 8; i++)
	{
	  if (flg)
	    LCD_PushColor (_screen.fg);
	  else
	    LCD_PushColor (_screen.bg);
	}
    }
}

#define cursor_draw	cursor_expose(1)
#define cursor_erase	cursor_expose(0)

static void
cursor_nl (void)
{
  _screen.c.col = 0;
  _screen.c.row++;
  if (_screen.c.row == _screen.nrow)
    {
      _scrollup ();
    }
}

static void
cursor_fwd (void)
{
  _screen.c.col++;
  if (_screen.c.col == _screen.ncol)
    {
      cursor_nl ();
    }
}

static void
cursor_init (void)
{
  _screen.c.row = 0;
  _screen.c.col = 0;
}

static void
_putch (uint8_t c)
{
  uint8_t i, ch, fz;
  uint16_t j;
  uint16_t temp;
  int x, y;

  fz = FONT_XSIZE / 8;
  x = _screen.c.col * FONT_XSIZE;
  y = _screen.c.row * FONT_YSIZE;
  LCD_SetAddrWindow (x, y, x + FONT_XSIZE - 1,
			 y + FONT_YSIZE - 1);
  temp = ((c - FONT_OFFSET) * ((fz) * FONT_YSIZE)) + 4;
  for (j = 0; j < ((fz) * FONT_YSIZE); j++)
    {
      ch = _font[temp];
      for (i = 0; i < 8; i++)
	{
	  if ((ch & (1 << (7 - i))) != 0)
	    {
	      LCD_PushColor (_screen.fg);
	    }
	  else
	    {
	      LCD_PushColor (_screen.bg);
	    }
	}
      temp++;
    }
  *_scr(_screen.c.row, _screen.c.col) = c;
}

/********************************************************************
 *********************************************************************
 *********************** Public functions ***************************
 *********************************************************************
 *********************************************************************/
void
TERMINAL_Init(void *font, uint16_t fg, uint16_t bg, uint8_t orientation)
{
  LCD_SetOrientation(orientation);
  GFX_FillScreen(bg);
  _screen.fg = fg;
  _screen.bg = bg;
  _font = (uint8_t *) font;

  _screen.nrow = LCD_GetHeight () / FONT_YSIZE;
  _screen.ncol = LCD_GetWidth () / FONT_XSIZE;
  _screen.scr = malloc (_screen.nrow * _screen.ncol);
  memset ((void*) _screen.scr, ' ', _screen.nrow * _screen.ncol);
  cursor_init ();
  cursor_draw;
}

void
TERMINAL_Putc (char c)
{
  if (c != '\n' && c != '\r')
    {
      _putch (c);
      cursor_fwd ();
    }
  else
    {
      cursor_erase;
      cursor_nl ();
    }
  cursor_draw;
}

void
TERMINAL_Puts (char *str)
{
  int i=0;
  while(str[i]!=0)
    {
      if (str[i] != '\n' && str[i] != '\r')
	{
	  _putch (str[i]);
	  cursor_fwd ();
	}
      else
	{
	  cursor_erase;
	  cursor_nl ();
	}
      i++;
    }
  cursor_draw;
}

void
TERMINAL_CursorSet (uint16_t row, uint16_t col)
{
  if (row < _screen.nrow && col < _screen.ncol)
    {
      _screen.c.row = row;
      _screen.c.col = col;
    }
  cursor_draw;
}

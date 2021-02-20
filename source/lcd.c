/*
 * lcd.c
 *
 *  Created on: Dec 19, 2014
 *      Author: CSullivan
 *
 *  This file manages one or more LCD devices. There is one restriction in that all data pins must
 *  be on the same microprocessor I/O port. The signaling pins can be anywhere.
 */

/*
    GPSDO - Discipline an adjustable oscillator (typically OCXO) with GPS timing signals
    Copyright (C) 2021  Chris Sullivan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    You may contact the author via his Github page: SullivanChrisJ
d
*/


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>

#include "lcd.h"
#include "gpsdo.h"
#include "time.h"
#include "serial.h"
#include "ringbuf.h"

/*
 * Ring buffer for commands & data. Commands have Bit7=1 plus a command byte that follows. 0x80 indicates a position to follow.
 * Ox81 indicates another type of command to follow. 0xFF means no data in buffer (underrun).
 */

ring * lcd_buf;

// Internal function prototypes & macros

#define lcd_e_delay()   __asm__ __volatile__( "rjmp 1f\n 1:" );				// Short delay for strobing e pin(s)
void lcd_data_dir(int);
int8_t lcd_put2b(uint8_t, uint8_t);
void lcd_write_byte(uint8_t, uint8_t);

// Pin mapping optimizations (check for pins in order)
#ifdef LCD_DX_PORT
  #if LCD_08
    #if (LCD_D7_PIN == 7) && (LCD_D6_PIN == 6) && (LCD_D5_PIN == 5) && (LCD_D4_PIN == 4) &&
	    (LCD_D3_PIN == 3) && (LCD_D2_PIN == 2) && (LCD_D1_PIN == 1) && (LCD_D0_PIN == 0)
      #define LCD_NATURAL 0
    #endif
  #else
    #if (LCD_D3_PIN == (LCD_D2_PIN + 1)) && (LCD_D2_PIN == (LCD_D1_PIN + 1)) && (LCD_D1_PIN == (LCD_D0_PIN + 1))
      #define LCD_NATURAL LCD_D0_PIN
    #endif
  #endif
#endif

int8_t lcd_cmd(uint8_t);				// Write a command into the ring buffer
int8_t lcd_putc(uint8_t);				// Write a character to the ring buffer
void lcd_write_nibble(uint8_t);				// Write the bottom 4 bits to the panel (independent of top/bottom)
int8_t lcd_read_busy(int8_t);				// Read busy flag directly from the panel
void lcd_e_toggle(int8_t);				// Toggle E0 or E1
uint8_t lcd_run(void);

/*
 * Initialization function which requires lots of short timers. Using the fast delay functions (time_delay) of time.c, the initialization
 * is broken up into a series of 'co-routines' that run at (timer) interrupt level (so keep 'em short).
 */

uint8_t lcd_init1(void);
unsigned char  lcd_reset(struct tlist *);					// Last step runs in background mode

/*
 * lcd_init - start LCD panel. This uses the fast timer mechanism, that hitch-hikes on the regular timer to time intervals in
 * multiples of 1024 clock cycles (minimum 3 for now) or about .1 milliseconds. Data direction and write operation are set to
 * output. Any function that needs to read from the panel shall restore these settings before returning.
 */

void lcd_init(void)
{

// Set all control pins to outputs (which will never need to change)
#ifdef LCD_CTRL_PORT
	LCD_CTRL_DDR |= ((1<<LCD_E0_PIN) | (1<<LCD_E1_PIN) | (1<<LCD_RS_PIN) | (1<<LCD_RW_PIN));
#else
	sbi(LCD_E0_DDR, LCD_E0_PIN);
	sbi(LCD_E1_DDR, LCD_E1_PIN);
	sbi(LCD_RS_DDR, LCD_RS_PIN);
	sbi(LCD_RW_DDR, LCD_RW_PIN);
#endif

	lcd_buf = ring_create(LCD_BUFSIZE);		// Create an output ring buffer
	if (lcd_buf)
	{
		lcd_data_dir(1);			// Set all data pins to output, which is the assumed state
		cbi(LCD_RS_PORT, LCD_RS_PIN);		// Command, not data
		cbi(LCD_RW_PORT, LCD_RW_PIN);		// Set operation to write, which is assumed state
		lcd_write_nibble(3);			// Initial 8 bit write (0x30 - Wake up!)
		lcd_e_toggle(0);			// Toggle 0 #1

		time_delay(50, lcd_init1);		// Startup sequence; Delay 5 ms.
	};
};

uint8_t lcd_init1(void)
{
	static int8_t state;
	switch (state)
	{
		case 0:
		case 1:
		{
			lcd_e_toggle(0);		// Toggle (both halves) of the panel (0 value)
			state++;			//
			return 2;			//
		}
		case 2:
		{
			lcd_write_nibble(2);		// Set 4 bit mode
			lcd_e_toggle(0);		// on both halves of the display
			time_set(lcd_reset, 0, 0, 0, 0); // Defer reset until background loop begins (with 0 length timer)
		}
	}
	return 0;
};

unsigned char lcd_reset(struct tlist *tl)		// Called
{
	lcd_cmd(LCD_FUNCTION_DEFAULT);			// Queue command to set 2 line mode
	lcd_cmd(LCD_DISPLAY_OFF);
	lcd_cmd(LCD_DISPLAY_CLEAR);
	lcd_cmd(LCD_CURSOR_MODE_DEFAULT);		// Forward direction, no display shift
	lcd_cmd(LCD_DISPLAY_ON);			// Display on, no cursor
	time_delay(3,lcd_run);				// Start output after 3 ticks
	return 0;
};

/*
 * int8_t lcd_printf(int8_t row, int8_t col, const char *fmt, ...) Formatted print to LCD panel with row and column
 * before doing anything else to see how big it is. This will be done before the timer interrupt is disabled.
 */

int8_t lcd_printf(uint8_t row, uint8_t col, const char *fmt, ...)
{
	int8_t status;					// Return code

	va_list vars;
    va_start(vars, fmt);

    uint8_t outlen = vsnprintf(0, 0, fmt, vars);	// Measure length of string to output
    unsigned char * outstr = malloc(outlen);		// Allocate sufficient memory to hold string

    status = 0;
    if (outstr)
    {
    	vsnprintf((char *)outstr, outlen + 1, fmt, vars); // Write formatted string to buffer
        va_end(vars);
		if (!lcd_put2b(0x80, col | row<<6)) 	// Ready new cursor position
		{
			cbi(TIMSK0,OCIE0B);		// Stop the fast timer from interrupting us
			if (ring_putbs(lcd_buf, outlen, outstr))
			{
				serial_printf(0, "lcd_printf buffer overrun\n\r");
			};
		} else {
			serial_printf(0, "lcd_put2b (lcd_printf) buffer overrun\n\r");
		}
		free(outstr);
		time_delay(2,lcd_run);			// Start display after 2 ticks if not already running
		return 0;
    } else {
    	return 1;					// malloc allocation failure
    }
    return 1;
};

// Packs row and column into appropriate address. If output bit 7 = 1, it indicates the second panel
/*
 * lcd_pos - set position for subsequent character output. Arg 1 (row) is a number from 0 to 3 and must respect the actual number of
 * rows on the panel, while arg2 is the column number. The row # is shifted left by 6 bits to put it in the upper two bits of the byte.
 * The column number is in lower 6 bits
 */

int8_t lcd_pos(int8_t row, int8_t col)
{
	return lcd_put2b(0x80, col & (row<<6));
};

int8_t lcd_cmd(uint8_t cmd)
{
	return lcd_put2b(0x81, cmd);
};

int8_t lcd_putc(uint8_t c)
{
	cbi(TIMSK0,OCIE0B);											// Clear interrupt to share ring buffer with ISR
	uint8_t r = ring_putb(lcd_buf, c);
	time_delay(3,lcd_run);										// Start display after 2 ticks
	return r;
};

/*
 * int8_t lcd_put2b(uint8_t c1, uint8_t c2) Puts 2 bytes on the ring buffer, making sure there is room for both. This is to provide
 * for an escape sequence for commands. All commands will have the parity bit set, whilst data will not. As a consequence if the upper
 * half of the character generation table needs to be accessed, it will have to be escaped. A non-zero return indicates that the buffer
 * does not have room for 2 bytes.
 */

int8_t lcd_put2b(uint8_t b1, uint8_t b2)
{
	uint8_t b[2];
	b[0] = b1;
	b[1] = b2;
	cbi(TIMSK0,OCIE0B);											// Clear interrupt to share ring buffer with ISR
	int r = ring_putbs(lcd_buf, 2, b);
	time_delay(3,lcd_run);										// Start display after 2 ticks
	return r; 													// Won't fit
};


/*
 * lcd_run is called from the timer ISR with timer interrupts disabled. The ISR will re-enable them.
 */

uint8_t lcd_run(void)
{
	static int8_t waitstate;									//
	static int8_t unit;
	static uint8_t c;
	if (!waitstate)	c = ring_getb(lcd_buf);						// Get next byte if we need it
	if (lcd_read_busy(0))
	{
		waitstate = 1;
	} else {
		waitstate = 0;
		if (!(c & 0x80))										// Is it data?
			{
				sbi(LCD_RS_PORT, LCD_RS_PIN);					// Set data flag
				lcd_write_byte(c, unit);						// And write the byte
			} else if (c == 0x81) {								// Command
				cbi(LCD_RS_PORT, LCD_RS_PIN);					// Ready for command
				c = ring_getb(lcd_buf);							// get the value of the command
				lcd_write_byte(c,0);							// Write both panels
			} else if (c == 0x80) {								// cursor position
				c = ring_getb(lcd_buf);							// get the value of the command
				unit = (c & 0x80) ? 2 : 1;						// Determine upper or lower half
				cbi(LCD_RS_PORT, LCD_RS_PIN);					// Ready for positioning command
				lcd_write_byte(c | 0x80, unit);
		} else {
			return 0;											// Invalid sequence found in buffer (including FF empty flag)
		};
	};
	return 2;
};

/*
 * lcd_e_toggle(int8_t unit) toggles the enable line(s) of the LCD panel. If unit = 0, then both upper & lower halves are
 * toggle if the panel has more than 2 rows. If unit = 1 or 2, then the upper or lower halves are toggled respectively.
 * unit is ignored if the panel is only 1 or two rows.
 */

void lcd_e_toggle(int8_t unit)
{
#if LCD_ROWS > 2
	if (unit)
	{
		if (unit == 1)
		{
			sbi(LCD_E0_PORT, LCD_E0_PIN);
			lcd_e_delay();
			cbi(LCD_E0_PORT, LCD_E0_PIN);
		} else {
			sbi(LCD_E1_PORT, LCD_E1_PIN);
			lcd_e_delay();
			cbi(LCD_E1_PORT, LCD_E1_PIN);
		}
	} else {
		sbi(LCD_E0_PORT, LCD_E0_PIN);
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();										// DEBUG
		cbi(LCD_E0_PORT, LCD_E0_PIN);
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	};
#else
	sbi(LCD_E0_PORT, LCD_E0_PIN);
	lcd_e_delay();
	cbi(LCD_E0_PORT, LCD_E0_PIN);
#endif
};

/*
 * void lcd_data_dir(int out); Set read or write data direction if out = 0 or 1 respectively. The complexity of this is good
 * illustration of the value of putting all the data lines on the same port.
 */

void lcd_data_dir(int out)
{
	if (out)
	{
#ifdef LCD_DX_PORT
  #if LCD_D8
		LCD_DX_DDR = 0xFF;
  #else
		LCD_DX_DDR |= ((1<<LCD_D0_PIN) | (1<<LCD_D1_PIN) | (1<<LCD_D2_PIN) | (1<<LCD_D3_PIN));
  #endif
#else
		sbi(LCD_D0_DDR, LCD_D0_PIN);
		sbi(LCD_D1_DDR, LCD_D1_PIN);
		sbi(LCD_D2_DDR, LCD_D2_PIN);
		sbi(LCD_D3_DDR, LCD_D3_PIN);
  #if LCD_D8
		sbi(LCD_D4_DDR, LCD_D4_PIN);
		sbi(LCD_D5_DDR, LCD_D5_PIN);
		sbi(LCD_D6_DDR, LCD_D6_PIN);
		sbi(LCD_D7_DDR, LCD_D7_PIN);
  #endif
#endif
	} else {
#ifdef LCD_DX_DDR
  #if LCD_D8
		LCD_DX_DDR = 0;
  #else
		LCD_DX_DDR &= ~((1<<LCD_D0_PIN) | (1<<LCD_D1_PIN) | (1<<LCD_D2_PIN) | (1<<LCD_D3_PIN));
  #endif
#else
		cbi(LCD_D0_DDR, LCD_D0_PIN);
		cbi(LCD_D1_DDR, LCD_D1_PIN);
		cbi(LCD_D2_DDR, LCD_D2_PIN);
		cbi(LCD_D3_DDR, LCD_D3_PIN);
  #if LCD_D8
		cbi(LCD_D4_DDR, LCD_D4_PIN);
		cbi(LCD_D5_DDR, LCD_D5_PIN);
		cbi(LCD_D6_DDR, LCD_D6_PIN);
		cbi(LCD_D7_DDR, LCD_D7_PIN);
  #endif
#endif
	};
};

/*
 * Read busy status from the panel. 0 = both units, 1 = upper panel, 2 = lower panel. Returns non-zero if busy.
 * Assumes data direction is out, and returns the same way.
 */

int8_t lcd_read_busy(int8_t unit)
{
	uint8_t result;
	result = 0;
// Set RS low to indicate a command & configure data lines as inputs
	cbi(LCD_RS_PORT, LCD_RS_PIN);								// Indicate busy status read (vs. RAM read)
	sbi(LCD_RW_PORT, LCD_RW_PIN);								// Indicate read
	lcd_data_dir(0);											// Set data pins to read direction
#if LCD_ROWS > 2												// No need to worry about a second panel if none exists
	if (unit == 0 || unit == 1)
	{
		sbi(LCD_E0_PORT, LCD_E0_PIN); 							//Read high nibble
		lcd_e_delay();
  #if LCD_D8 == 1
		result |= (LCD_D7_READ & (1<<LCD_D7_PIN));
  #else
		result |= (LCD_D3_READ & (1<<LCD_D3_PIN));				// We're just reading the high order pin for status
		cbi(LCD_E0_PORT, LCD_E0_PIN);
		lcd_e_delay();
		sbi(LCD_E0_PORT, LCD_E0_PIN);							// Get ready for next operation
  #endif
		lcd_e_delay();
		cbi(LCD_E0_PORT, LCD_E0_PIN);							// Read second nibble anyway to complete the operation
	};
	if (unit == 0 || unit == 2)
	{
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();
  #if LCD_D8 == 1
		result = (LCD_D7_READ & (1<<LCD_D7_PIN));
  #else
		result |= (LCD_D3_READ & (1<<LCD_D3_PIN));				// 4 bit mode all we want is the high nibble
		cbi(LCD_E1_PORT, LCD_E1_PIN);							// Ready next nibble
		lcd_e_delay();
		sbi(LCD_E1_PORT, LCD_E1_PIN);							//
  #endif
		lcd_e_delay();
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	};
#else
	sbi(LCD_E0_PORT, LCD_E0_PIN);
	lcd_e_delay();
  #if LCD_D8 == 1
	result = (LCD_D7_READ & (1<<LCD_D7_PIN));
  #else
	result = (LCD_D3_READ & (1<<LCD_D3_PIN));						// 4 bit mode all we want is the high nibble
	cbi(LCD_E0_PORT, LCD_E0_PIN);								// Read second nibble anyway
	lcd_e_delay();
	sbi(LCD_E0_PORT, LCD_E0_PIN);								// Do this early to save a delay call
  #endif
	lcd_e_delay();
	cbi(LCD_E0_PORT, LCD_E0_PIN);								// Read second nibble anyway
#endif
	lcd_data_dir(1);											// Re-enable write operations
	cbi(LCD_RW_PORT, LCD_RW_PIN);
	return result;												// 0 means not busy
};

/*
 * This routine is only used on startup. It writes a 4 bit value to the panel.
 * Caller must toggle the E0/E1 flags and have the RW and RS flags set appropriately.
 */

void lcd_write_nibble(uint8_t b)
{
#ifdef LCD_DX_PORT												// Only applies to 4 bit mode
	#ifdef LCD_NATURAL
		LCD_DX_PORT &= ~(0xF<<LCD_NATURAL);
		LCD_DX_PORT |= (b<<LCD_NATURAL);
	#else
		LCD_DX_PORT &= ~((1<<LCD_D0_PIN)|(1<<LCD_D1_PIN)|(1<<LCD_D2_PIN)|(1<<LCD_D3_PIN));
		if (b & 1) sbi(LCD_D0_PORT, LCD_D0_PIN);
		if (b & 2) sbi(LCD_D1_PORT, LCD_D1_PIN);
		if (b & 4) sbi(LCD_D2_PORT, LCD_D2_PIN);
		if (b & 8) sbi(LCD_D3_PORT, LCD_D3_PIN);
	#endif
#else
	if (b & 1) sbi(LCD_D0_PORT, LCD_D0_PIN); cbi(LCD_D0_PORT, LCD_D0_PIN);
	if (b & 2) sbi(LCD_D1_PORT, LCD_D1_PIN); cbi(LCD_D1_PORT, LCD_D1_PIN);
	if (b & 4) sbi(LCD_D2_PORT, LCD_D2_PIN); cbi(LCD_D2_PORT, LCD_D2_PIN);
	if (b & 8) sbi(LCD_D3_PORT, LCD_D3_PIN); cbi(LCD_D3_PORT, LCD_D3_PIN);
#endif
};

/*
 * lcd_write_byte(uint8_t c, uint8_t unit). Output C. RS should be already set to indicate command or data. unit = 0 means write to both
 * units. Unit = 1 or 2 means upper or lower panel respectively. DDR is assumed to be output for all data pins
 */

void lcd_write_byte(uint8_t c, uint8_t unit)
{
#ifndef LCD_NATURAL
	uint8_t t;
#endif
#if LCD_D8 == 1
  #ifdef LCD_NATURAL
	LCD_DATA_PORT = c;						// Write the whole byte
  #else
	t  = (c & 0x01) ? (1<<LCD_D0_PIN): 0;		// Reorder them
	t |= (c & 0x02) ? (1<<LCD_D1_PIN): 0;
	t |= (c & 0x04) ? (1<<LCD_D2_PIN): 0;
	t |= (c & 0x08) ? (1<<LCD_D3_PIN): 0;
	t |= (c & 0x10) ? (1<<LCD_D4_PIN): 0;
	t |= (c & 0x20) ? (1<<LCD_D5_PIN): 0;
	t |= (c & 0x40) ? (1<<LCD_D6_PIN): 0;
	t |= (c & 0x80) ? (1<<LCD_D7_PIN): 0;
	LCD_DX_PORT = t;
  #endif
#else
  #ifdef LCD_NATURAL
	LCD_DX_PORT = (c && 0xF0) >> LCD_NATURAL;	// Align for output
  #else
	LCD_DX_PORT &= ~((1<<LCD_D0_PIN) | (1<<LCD_D1_PIN) | (1<<LCD_D2_PIN) | (1<<LCD_D3_PIN));
	t  = (c & 0x10) ? (1<<LCD_D0_PIN): 0;
	t |= (c & 0x20) ? (1<<LCD_D1_PIN): 0;
	t |= (c & 0x40) ? (1<<LCD_D2_PIN): 0;
	t |= (c & 0x80) ? (1<<LCD_D3_PIN): 0;
	LCD_DX_PORT |= t;
  #endif
	if (unit == 0)
	{
		sbi(LCD_E0_PORT, LCD_E0_PIN);
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();													// DEBUG
		cbi(LCD_E0_PORT, LCD_E0_PIN);
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	} else if (unit == 1) {
		sbi(LCD_E0_PORT, LCD_E0_PIN);
		lcd_e_delay();
		cbi(LCD_E0_PORT, LCD_E0_PIN);
	} else {
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	};
  #ifdef LCD_NATURAL											// Write second nibble to panel
	LCD_DX_PORT = c && 0x0F;
  #else
	LCD_DX_PORT &= ~((1<<LCD_D0_PIN) | (1<<LCD_D1_PIN) | (1<<LCD_D2_PIN) | (1<<LCD_D3_PIN));
	t  = (c & 0x01) ? (1<<LCD_D0_PIN): 0;
	t |= (c & 0x02) ? (1<<LCD_D1_PIN): 0;
	t |= (c & 0x04) ? (1<<LCD_D2_PIN): 0;
	t |= (c & 0x08) ? (1<<LCD_D3_PIN): 0;
	LCD_DX_PORT |= t;
  #endif
#endif
	if (unit == 0)
	{
		sbi(LCD_E0_PORT, LCD_E0_PIN);
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();													// DEBUG
		cbi(LCD_E0_PORT, LCD_E0_PIN);
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	} else if (unit == 1)
	{
		sbi(LCD_E0_PORT, LCD_E0_PIN);
		lcd_e_delay();
		cbi(LCD_E0_PORT, LCD_E0_PIN);
	} else {
		sbi(LCD_E1_PORT, LCD_E1_PIN);
		lcd_e_delay();
		cbi(LCD_E1_PORT, LCD_E1_PIN);
	}
};

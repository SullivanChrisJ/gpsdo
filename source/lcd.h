/*
 * lcd.h
 *
 *  Created on: Dec 19, 2014
 *      Author: CSullivan
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
*/

#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>
#include "config.h"

// Ring buffer definiteion
#define LCD_BUFSIZE_LOG2 6
#define LCD_BUFSIZE (1<<LCD_BUFSIZE_LOG2)


#ifdef YRARC_CONFIG
  #define LCD_ROWS 4										// Number of lines in display
  #define LCD_COLUMNS 40
  #define LCD_ALL_PORT PORTB								// All connections to same port (4 bit mode only)
  #define LCD_ALL_READ PINB
  #define LCD_ALL_DDR DDRB
  #define LCD_D8 0											// 0 = 4 bit mode, 1 = 8 bit mode
  #define LCD_BUFLEN										//
// Define Data Ports
  #define LCD_DX_PORT PORTB									// All data pins are on the same port
  #define LCD_D0_PORT PORTB
  #define LCD_D1_PORT PORTB
  #define LCD_D2_PORT PORTB
  #define LCD_D3_PORT PORTB
  #define LCD_CTRL_PORT PORTB								// Further optimization if control lines are on same port
  #define LCD_E0_PORT PORTB									// Top 2 lines
  #define LCD_E1_PORT PORTB									// Bottom 2 lines
  #define LCD_RS_PORT PORTB
  #define LCD_RW_PORT PORTB
  #define LCD_D0_READ PINB
  #define LCD_D1_READ PINB
  #define LCD_D2_READ PINB
  #define LCD_D3_READ PINB
// Define Data Direction Ports
  #define LCD_DX_DDR DDRB									// All data pins are on the same port
  #define LCD_D0_DDR DDRB
  #define LCD_D1_DDR DDRB
  #define LCD_D2_DDR DDRB
  #define LCD_D3_DDR DDRB
  #define LCD_CTRL_DDR DDRB
  #define LCD_E0_DDR DDRB									// Top 2 lines
  #define LCD_E1_DDR DDRB
  #define LCD_RS_DDR DDRB
  #define LCD_RW_DDR DDRB
// Define Pins (We'll use for both PORT and DDR)
  #define LCD_D0_PIN PINB5
  #define LCD_D1_PIN PINB4
  #define LCD_D2_PIN PINB7
  #define LCD_D3_PIN PINB6
  #define LCD_E1_PIN PINB0
  #define LCD_E0_PIN PINB2
  #define LCD_RS_PIN PINB1
  #define LCD_RW_PIN PINB3
#endif
// Additional hardware configurations can go here

// Commands

#if LCD_ROWS > 1
	#define LCD_FUNCTION_DEFAULT 0x28							// 2-lines (per 1/2 panel if 4 line display)
#else
	#define LCD_FUNCTION_DEFAULT 0x20							// single line display
#endif
#define LCD_DISPLAY_OFF 0x08									// Display off, cursor off, cursor position off
#define LCD_DISPLAY_CLEAR 0x01									// Special command which requires 1.52ms delay (vs. 37 us)
#define LCD_CURSOR_MODE_DEFAULT 0x06							// Forward shift etc.
#define LCD_DISPLAY_ON 0x0F										// DEBUG - would normally be 0x0B

// external function prototypes
void lcd_init(void);
int8_t lcd_pos(int8_t, int8_t);									// Position cursor for write
int8_t lcd_printf(uint8_t, uint8_t, const char *, ...);


#endif										//

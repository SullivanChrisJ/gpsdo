/*
 * serial.h
 *
 *  Created on: Dec 26, 2014
 *  Revised: July 2020
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

#ifndef SERIAL_H_
#define SERIAL_H_

// Number and *data* length of serial buffers
#define SERBUF_NUM    8
#define SERBUF_CLEN  83			// 80 + CR + LF + null

#if defined (__AVR_ATmega32A__)
  #if NUM_USARTS >= 1
    #define USART0_UDR   UDR
    #define USART0_UCSRA UCSRA		// 3 status registers
    #define USART0_UCSRB UCSRB
    #define USART0_UCSRC UCSRC
    #define USART0_UBRRL UBRRL		// 2 baud rate registers
    #define USART0_UBBRH UBBRH
  #endif
#elif defined (__AVR_ATmega1284P__)
  #if NUM_USARTS >= 1
  #elif NUM_USARTS == 2
  #endif
#else
  #pragma GCC error "Unknown processor type"
#endif

#define USART0_RX_BUFSIZE 83
#define USART0_RX_BUFCNT 2
#define USART1_RX_BUFSIZE 83
#define USART1_RX_BUFCNT 2

// All the baud rates we care about

#define BPS_1200  0
#define BPS_2400  1
#define BPS_4800  2
#define BPS_9600  3
#define BPS_19200 4
#define BPS_38400 5
#define BPS_57600 6
#define BPS_115200 7
#define BPS_LEN BPS_115200 + 1

// All the UBRR values for each baud rate (rounded)

#define UBRRN_1200   ((F_CPU / (8L * 1200)   - 1) / 2)
#define UBRRN_2400   ((F_CPU / (8L * 2400)   - 1) / 2)
#define UBRRN_4800   ((F_CPU / (8L * 4800)   - 1) / 2)
#define UBRRN_9600   ((F_CPU / (8L * 9600)   - 1) / 2)
#define UBRRN_19200  ((F_CPU / (8L * 19200)  - 1) / 2)
#define UBRRN_38400  ((F_CPU / (8L * 38400)  - 1) / 2)
#define UBRRN_57600  ((F_CPU / (8L * 57600)  - 1) / 2)
#define UBRRN_115200 ((F_CPU / (8L * 115200) - 1) / 2)

// All the UBRR values for each baud rate in fast mode

#define UBRRF_1200   ((F_CPU / (4L * 1200)   - 1) / 2)
#define UBRRF_2400   ((F_CPU / (4L * 2400)   - 1) / 2)
#define UBRRF_4800   ((F_CPU / (4L * 4800)   - 1) / 2)
#define UBRRF_9600   ((F_CPU / (4L * 9600)   - 1) / 2)
#define UBRRF_19200  ((F_CPU / (4L * 19200)  - 1) / 2)
#define UBRRF_38400  ((F_CPU / (4L * 38400)  - 1) / 2)
#define UBRRF_57600  ((F_CPU / (4L * 57600)  - 1) / 2)
#define UBRRF_115200 ((F_CPU / (4L * 115200) - 1) / 2)

// Error values for each (0 = within 0.5%, 1 = within 2%, -1 = out of spec), normal rate
// Flags for baud rate table
#define UBRR_FAST 0x8000			// In spec only in fast mode <= 2% error
#define UBRR_ERR  0x4000			// Out of spec > 2% error
#define UBRR_MGNL 0x2000			// Marginal .5% < error <= 2% 

// 1200 bps constants

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_1200 + 1))) / 1200) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_1200 + 1))) / 1200) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_1200 UBRRN_1200
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_1200 (UBRRF_1200 | UBRR_FAST)
  #else
    #define UBRR_1200 (UBRRN_1200 | UBRR_MGNL)
  #endif
#else						// Error is >2%
  #if ERRF <= 1					// But fast mode error is < .5%
    #define UBRR_1200 (UBRRF_1200 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_1200 (UBRRF_1200 | UBRR_FAST ! UBRR_MGNL)
  #else
    #define UBRR_1200 (UBRRF_1200 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 2400 bps constants

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_2400 + 1))) / 2400) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_2400 + 1))) / 2400) % 100


#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_2400 UBRRN_2400
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_2400 (UBRRF_2400 | UBRR_FAST)
  #else
    #define UBRR_2400 (UBRRN_2400 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_2400 (UBRRF_2400 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_2400 (UBRRF_2400 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_2400 (UBRRF_2400 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 4800 bps

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_4800 + 1))) / 4800) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_4800 + 1))) / 4800) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_4800 UBRRN_4800
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_4800 (UBRRF_4800 | UBRR_FAST)
  #else
    #define UBRR_4800 (UBRRN_4800 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_4800 (UBRRF_4800 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_4800 (UBRRF_4800 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_4800 (UBRRF_4800 | UBRR_FAST | UBRR_ERR)
  #endif
#endif


#undef ERRN
#undef ERRF

// 9600 bps

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_9600 + 1))) / 9600) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_9600 + 1))) / 9600) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_9600 UBRRN_9600
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_9600 (UBRRF_9600 | UBRR_FAST)
  #else
    #define UBRR_9600 (UBRRN_9600 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_9600 (UBRRF_9600 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_9600 (UBRRF_9600 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_9600 (UBRRF_9600 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 19200 bps

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_19200 + 1))) / 19200) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_19200 + 1))) / 19200) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_19200 UBRRN_19200
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_19200 (UBRRF_19200 | UBRR_FAST)
  #else
    #define UBRR_19200 (UBRRN_19200 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_19200 (UBRRF_19200 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_19200 (UBRRF_19200 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_19200 (UBRRF_19200 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 38400 bps

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_38400 + 1))) / 38400) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_38400 + 1))) / 38400) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_38400 UBRRN_38400
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_38400 (UBRRF_38400 | UBRR_FAST)
  #else
    #define UBRR_38400 (UBRRN_38400 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_38400 (UBRRF_38400 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_38400 (UBRRF_38400 | UBRR_FAST |UBRR_MGNL)
  #else
    #define UBRR_38400 (UBRRF_38400 | UBRR_FAST |UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 57600

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_57600 + 1))) / 57600) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_57600 + 1))) / 57600) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_57600 UBRRN_57600
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_57600 (UBRRF_57600 | UBRR_FAST)
  #else
    #define UBRR_57600 (UBRRN_57600 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_57600 (UBRRF_57600 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_57600 (UBRRF_57600 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_57600 (UBRRF_57600 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

// 115200 bps

#define ERRN (1 + (100 * F_CPU / (16 * (UBRRN_115200 + 1))) / 115200) % 100
#define ERRF (1 + (100 * F_CPU / ( 8 * (UBRRF_115200 + 1))) / 11520) % 100

#if ERRN <= 1					// Error is less than 0.5%
  #define UBRR_115200 UBRRN_115200
#elif ERRN <= 4					// Error is less than 2%
  #if ERRF <= 1
    #define UBRR_115200 (UBRRF_115200 | UBRR_FAST)
  #else
    #define UBRR_115200 (UBRRN_115200 | UBRR_MGNL)
  #endif
#else
  #if ERRF <= 1
    #define UBRR_115200 (UBRRF_115200 | UBRR_FAST)
  #elif ERRF <= 4
    #define UBRR_115200 (UBRRF_115200 | UBRR_FAST | UBRR_MGNL)
  #else
    #define UBRR_115200 (UBRRF_115200 | UBRR_FAST | UBRR_ERR)
  #endif
#endif

#undef ERRN
#undef ERRF

struct serial_buf {
	struct serial_buf *next;
	char * ptr;
	char buf[SERBUF_CLEN];
};

#define SERBUF_LEN len(serial_buf)
#define SERBUF_SPACE  SERBUF_NUM * SERBUF_LEN

// External function prototypes
  int8_t serial_init(uint8_t);
  int8_t serial_printf(const char *, ...);

#endif /* SERIAL_H_ */

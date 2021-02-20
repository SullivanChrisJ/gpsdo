/*
 * serial.c
 *
 *  Created on: Dec 26, 2014
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"
#include "gpsdo.h"
#include "serial.h"

static uint16_t bps_div[BPS_LEN] =
{
        UBRR_1200,
        UBRR_2400,
        UBRR_4800,
        UBRR_9600,
        UBRR_19200,
        UBRR_38400,
        UBRR_57600,
        UBRR_115200
};

  volatile struct serial_buf * outbuf_head;		// Queue of things to be printed
  volatile struct serial_buf * outbuf_tail;		// Locates the end of queue
  
  volatile struct serial_buf * serial_free_head;	// Free list for dedicated print buffers

           struct serial_buf serial_bufs[SERBUF_NUM];	// Static print buffer allocation

int8_t serial_init(uint8_t rate)
{
        // Return an error if a bad rate is provided
        if (rate >= BPS_LEN) return 1;

	// Create a free list of output buffers.
	serial_free_head = serial_bufs;
	{
	    struct serial_buf * next = 0;
	    for (int8_t i=SERBUF_NUM; i-- > 0;)
	    {	
		serial_bufs[i].next = next;
		next = &serial_bufs[i];
	    };
	};

	// Initialize pointers to buffers queued for transmit
	outbuf_head = 0;
	outbuf_tail = 0;

        // Set 12 bit baud rate divisor & double speed if req'd
	UBRRH = (uint8_t)((bps_div[rate]>>8) & 0x0F);
	UBRRL = (uint8_t)(bps_div[rate] & 0xFF);
	if (bps_div[rate] & 0x8000) UCSRA |= 1<<U2X;

	// Set 12 bit baud rate divisor & double speed if req'd
	//UBRRH = 0; 
	//UBRRL = 155; // 12, Change to 51 for 4 MHz clock, 8MHz 103, 12MHz 155
        //UCSRA = 0;

	// 8 bit async no parity
	UCSRC = 1<<URSEL | 1<<UCSZ1 | 1<<UCSZ0;

	// Enable transmit & transmit data ready interrupts
        UCSRB = 1 << TXEN | 1 << UDRIE;

	return 0;
}

int8_t serial_printf(const char *fmt, ...)
{
	struct serial_buf * buf;

	va_list vars;
	va_start(vars, fmt);

	// Get address of a free buffer if there is one
	cbi(UCSRB, UDRIE);
	if (buf = (struct serial_buf *)serial_free_head)
	{
	    // Unlink the buffer from the free list
	    serial_free_head = buf->next;
            sbi(UCSRB, UDRIE);

	    // Format the string to be output into the buffer
	    buf->ptr = buf->buf;				// Start at first character to print
	    buf->next = 0;					// There is no next buffer yet
	    vsnprintf(buf->buf, SERBUF_CLEN - 1, fmt, vars);	// Format string into buffer, truncate if too long

	    // Link buffer into the output queue
	    cbi(UCSRB, UDRIE);
	    if (outbuf_tail)
	    {
	    	outbuf_tail->next = buf;
	    } else {
	    	outbuf_head = buf;
	    }
	    outbuf_tail = buf;
	    sbi(UCSRB, UDRIE);
	    return 0;
	}
	sbi(UCSRB, UDRIE);
	return 1;
};


/*
	Transmit ready ISR
*/
ISR(USART_UDRE_vect)
{
	char txchar;
	struct serial_buf * head;

	while (outbuf_head && !(txchar = *(outbuf_head->ptr++)))
	{
	    // No data left, unlink used buffer from output queue
	    head = (struct serial_buf *)outbuf_head;
	    outbuf_head = head->next;

            // Return used buffer to free list
	    head->next = (struct serial_buf *)serial_free_head;
	    serial_free_head = head;
	};

	// If we found a character, send it, otherwise done.
	if (txchar)
	{
	    UDR = txchar;
	} else {
	    outbuf_tail = 0;
	    cbi(UCSRB, UDRIE);
	};
};

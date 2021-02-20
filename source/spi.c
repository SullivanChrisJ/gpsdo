/*
	spi.c	Send and receive messages from the SPI Bus

		In the current configuration, a Raspberry Pi is continuously connected
		to the MCU. The same pins (both GPIO & MCU) that are used for programming
		the MCU can also be used for communication, without having to tie up
		a USB serial port on the Pi (given that the GPIO USART is tied up talking
		to the GPS chip)

		Binary format: Based on SLIP characters:.

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

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"
#include <stdint.h>
#include "gpsdo.h"
#include "spi.h"
#include "led.h"

// Output only for now
volatile struct spi_buf * spi_rx_head;            		// Queue of things to be printed
volatile struct spi_buf * spi_rx_tail;            		// Locates the end of queue
volatile struct spi_buf * spi_tx_head;            		// Queue of things to be printed
volatile struct spi_buf * spi_tx_tail;            		// Locates the end of queue

	 struct spi_buf spi_bufs[SPIBUF_NUM];     	// Static spi buffer allocation
volatile struct spi_buf * spi_free_head;       		// Free list for dedicated spi buffers

volatile struct spi_buf * spi_rx;			// Active receive buffer
volatile struct spi_buf * spi_tx;			// Active transmit buffer

uint8_t spi_rx_shift;					// Receiver is in shift state
uint8_t spi_tx_shift;					// Transmitter is in shift state

void spi_init()
{
	// Enable output on MISO
	DDRB = (1 << MISO_PIN);

	// Enable SPI slave mode (configures all other required pins)
	SPCR = (1 << SPE);

	// Initialize output buffers
        spi_free_head = spi_bufs;
        {
            struct spi_buf * next = 0;
            for (int8_t i = SPIBUF_NUM; i-- > 0;)
            {
                spi_bufs[i].next = next;
                next = &spi_bufs[i];
            };
        };

        // Initialize buffer pointers
        spi_rx_head = 0;
        spi_rx_tail = 0;
        spi_tx_head = 0;
        spi_tx_tail = 0;

	spi_tx = 0;
	spi_rx = 0;

	// NULs are used to indicate nothing to send (yet)
	SPDR = NUL;	

	// Let the interrupts run
	sbi(SPCR, SPIE);
};
struct spi_buf * spi_getbuf()
{
	struct spi_buf * buf;

	cbi(SPCR, SPIE);
	if (buf = (struct spi_buf *)spi_free_head)
	{
	    spi_free_head = buf->next;
	    buf->ptr = buf->buf;
	}
	sbi(SPCR, SPIE);
	return buf;
};

void spi_tx_queue(struct spi_buf * buf)
{
	// Get byte count from pointer then reset to start of message
	buf->cnt = buf->ptr - buf->buf;
	buf->ptr = buf->buf;

	// Add to output queue, or to active tx buffer if tx not active
	cbi(SPCR, SPIE);
	if (spi_tx)
	{
	    if (spi_tx_tail)
	    {
		spi_tx_tail->next = buf;
	    } else {
		spi_tx_head = buf;
		buf->next = 0;
	    };
	    spi_tx_tail = buf;
	} else {
	    spi_tx = buf;
	    // This is the reason transmissions can't start with ESC or END.
	    // SPDR = *(spi_tx->ptr++);
	    // spi_tx->cnt--;
	}; 
	sbi(SPCR, SPIE);
};

// Execute any commands that may have come from the SPI master
void spi_cmd()
{
	struct spi_buf * buf;

	cbi(SPCR, SPIE);
	if (buf = (struct spi_buf *)spi_rx_head)
	{
	    if(!(spi_rx_head = buf->next))
            {
		spi_rx_tail = 0;
	    };
            sbi(SPCR, SPIE);
	    switch (*(buf->ptr++))
	    {
		case 1:
		    msg1(buf);
		    break;
		default:
		    // TBA - send "Unknown Message" repsonse
		    break;
	    };
	    cbi(SPCR, SPIE);
            buf->next = spi_free_head;
	    spi_free_head = buf;
	};
	sbi(SPCR, SPIE);
	return;
}

ISR(SPI_STC_vect)
{
	char txchar;
	char rxchar;

	// We're here because the character has been received
	rxchar = SPDR;

	// If we're receiving something, then continue to do so
	if (spi_rx)
	{
	    if (rxchar == END)
	    // END means move the buffer onto the receive queue, if there is one
	    {
		if (spi_rx_tail)
		{
		    spi_rx_tail->next = spi_rx;
		} else {
		    spi_rx_head = spi_rx;
		}
		spi_rx->next = 0;
		spi_rx_tail = spi_rx;
		spi_rx = 0;
	    } else if (spi_rx_shift) {
		if (rxchar == ESC_ESC)
		{
		    *(spi_rx->ptr++) = ESC;
		} else if (rxchar == ESC_END){
		    *(spi_rx->ptr++) = END;
		// Only the above 2 options valid, toss transmission otherwise
		} else {
		    spi_rx->next = spi_free_head;
		    spi_free_head = spi_rx;
		};
		spi_rx_shift = 0;
	    } else if (rxchar == ESC) {
		spi_rx_shift = -1;
	    } else {
		*(spi_rx->ptr++) = rxchar;
	    };
	} else if (rxchar != NUL) {
            // Non-null characters indicate a message, capture it.
	    spi_rx = spi_free_head;
	    if (spi_rx)
	    {
	    	spi_free_head = spi_rx->next;
                spi_rx->ptr = spi_rx->buf;
		*(spi_rx->ptr++) = rxchar;
	    };
	};
	// Transmit if there's something to send;
	if (spi_tx)
	{
	    if (spi_tx_shift)
	    {
		SPDR = spi_tx_shift;
		spi_tx_shift = 0;
	    } else {
		txchar = *(spi_tx->ptr++);
		if (spi_tx->cnt--)
		{
		    if (txchar == END)
		    {
		        SPDR = ESC;
		        spi_tx_shift = ESC_END;
		    } else if (txchar == ESC) { 
		        SPDR = ESC;
		        spi_tx_shift = ESC_ESC;
		    } else {
			SPDR = txchar;
			spi_tx_shift = 0;
		    };
		} else {
		    // DEBUG - turn red led off
		    led_state(0, LEDR_unit);
		    SPDR = END;

		    // Return this buffer to the free pool
		    spi_tx->next = spi_free_head;
		    spi_free_head = spi_tx;

		    // Get another buffer from the xmit queue, if any
		    if (spi_tx = spi_tx_head)
		    {
		    	spi_tx_head = spi_tx->next;
		    };
		};
	    };
	} else {
	    SPDR = NUL;
	};
};

/*
 * spi.h
 *
 * Created on: October 15, 2020
 *
 *	Author: Chris Sullivan VE3NRT
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

#ifndef SPI_H_
#define SPI_H_

#define SPIBUF_NUM 4
#define SPIBUF_CLEN 24

#define MISO_PIN 6

// Control characters
#define NUL  0
#define END 0xC0
#define ESC 0xDB
#define ESC_END 0xDC
#define ESC_ESC 0xDD

// Commands
#define SPICMD_PPS 0x01

struct spi_buf {
        volatile struct spi_buf *next;
        volatile char * ptr;
	volatile int8_t cnt;
        char buf[SPIBUF_CLEN];
};


void spi_init();
uint8_t spi_printf(const char *, ...);

struct spi_buf * spi_getbuf();
void spi_tx_queue(struct spi_buf *);
void spi_cmd();
void msg1(struct spi_buf *);

#endif

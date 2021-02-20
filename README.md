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

Under development
=================

Using the 1PPS signal from a GPS receiver, this software, designed for an
Microchip (f.k.a. AVR) ATMEGA 32A MCU, measures the frequency of its own
clock. This allows measurement at the maximum speed of the CPU (16 MHz at
5V, and about 10.6 MHz at 3.3V), rather than 40% of the CPU speed if a
timer input were used.

Things that work so far:
1. A non-precision timer function that allows the setting of one-shot
and periodic timers in increments of 20ms. As it uses the 8-bit timer
and clock divider, it is not entirely accurate. Timers cannot be cancelled
although that may be added in future should there be a reason to do so. The
main purpose of the timer is to update status messages.

2. SPI communications. Communications (to a Raspberry Pi 3B in my case) is
asynchronous. Only the delivery of messages from the MCU to the Pi has
been tested, but the other direction is coded and awaiting test. The MCU
is configured as the slave. 25KHz clock works well at 4MHz CPU speed. 100KHz
does not. Note that if messages are not being read, they will stop being
added to the output queue. So when the Pi resume reading, it will get the
oldest messages, not the latest.

3. Frequency measurement. The clock is measured at 16 second intervals
and delivered to the SPI master. The program gpsdo.py will print these
messages. Even with the GPS antenna placed near a window the measurements
taken so far are good enough to see a diurnal pattern in a piezoelectric
crystal frequency due to temperature effects.

4. Serial output. Messages can be sent to a serial port. This has been used
for debugging and it is unlikely to be used in the final version. The code
will be retained as in some configurations it may be preferable to have the
MCU run standalone without the Pi. In my case I am also using the Pi/GPS
combination as an NTP server for standalone use.

5. Other stuff: There is some simple code for turning LEDs on and off. This
has been useful for debugging at times as it is simple enough to do inside
an ISR. It was also useful diagnosing startup problems prior to the serial
port initialization. The is also a module called ringbuf.c that was intended
for serial input but is not referenced by the main program. It may be useful
someday (or for some other purpose) so it remains for the time being.

The calculation of serial port speeds in the serial.h file using the
preprocessor is way overkill. It probably should be converted to a runtime
calculation. While the RAM space is fairly limited on the 32A, 32K of flash
ROM is plenty and will support a lot more code.

FAQ
===

1. Why not use Arduino like everyone else? I'm not a fan of Arduino. More
   relevant is that Arduino configurations don't allow external CPU clocks.

2. Why is this needed? I like the idea of a high precision frequency reference.
   My Elecraft K3 has a 10MHz reference input and I plan to use it. In future
   I'll be working my way up from the 6 metre band to much higher frequencies
   and a frequency reference is a great asset in the microwave band.

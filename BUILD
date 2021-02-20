# FUSE
# sudo avrdude -c linuxgpio -U lfuse:w:0xcf:m -U hfuse:w:0x99:m
# hfuse: Disable OCD, Enable JTAG, Enable SPI, CKOPT unprogrammed, 
#	 EEPROM not preserved thru erase, Boot size 2048 words, No boot reset vector
# lfuse: Brown out detector disabled, startup time: 65ms, External Crystal/Ceramic Clock
# END FUSE
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/gpsdo.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/time.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/led.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/ringbuf.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/serial.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/pps.c
avr-gcc -Os -mmcu=atmega32a -I/usr/lib/avr/include -c source/spi.c
avr-gcc -mmcu=atmega32a -o gpsdo.elf gpsdo.o time.o led.o ringbuf.o serial.o pps.o spi.o
rm -f *.o
avr-objcopy -j .text -j .data -O ihex gpsdo.elf gpsdo.hex

# uncomment next line to get a dump file
#avr-objdump -h -S gpsdo.elf > gpsdo.dump
rm gpsdo.elf
#For programming the AVR in my environment over the SPI interface
#sudo avrdude -p atmega32 -c linuxgpio -U flash:w:gpsdo.hex

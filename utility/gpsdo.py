"""
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
"""


import spidev
import time
import struct
from collections import deque


class spiman():
    def __init__(self, speed=10000):
        self.spi = spidev.SpiDev(0,0)
        self.spi.open(0,0)
        self.spi.max_speed_hz = speed
        self.spi.mode = 0
        self.fragment = bytes()
        self.resync = False

    def __del__(self):
        self.spi.close()

    def strip(self, resp):
        if not resp:
            return bytes()
        i = 0
        while resp[i] == 0:
            i += 1
            if i >= len(resp): break
        return resp[i:]

    def transfer(self, msg=None):
        # Send message, or if none, send idle characters to receive messages
        if  msg:
            msg.replace(bytes([ESC]), bytes([ESC, ESC_ESC]))
            msg.replace(bytes([END]), bytes([ESC, ESC_END]))
        else:
            msg = bytes(32 * (0x00,))
        resp = bytes(self.spi.xfer(msg))
        resplen = len(resp)
        fullresp = resp

        if self.resync:
            resp = self.strip(resp)
            if not resp:
                return True
            i = 0
            while resp[i] != END:
                i += 1
                if i >= len(resp): return True
            resp = resp[i+1:]
            self.resync = False
            if not resp: return True


        # If we're mid-message, add the response to what we have so fare
        if self.fragment:
            resp = self.fragment + resp
            self.fragment = bytes()
        # Otherwise the next non-null byte shoudl be the beginning of a message
        else:
            resp = self.strip(resp)

        resp.replace(bytes([ESC, ESC_ESC]), bytes([ESC]))
        resp.replace(bytes([ESC, ESC_END]), bytes([END]))
        
        if resp:
            # Look up message type byte
            cmd = cmds.get(int(resp[0]), None)
            if cmd:
                if len(resp) < cmd['len']:
                    self.fragment = resp
                    return True

                l = cmd['len']
                if resp[l-1] == END:
                    r = resp[:l]
                    resp = self.strip(resp[l:])
                    cmd['fn'](*struct.unpack("<BIBhB", r))
                    self.fragment = resp
                    return len(resp) > 0

            print(f"Resynchronizing - resp: {resp}, frag: {self.fragment}, len={resplen}")
            print(fullresp)
            self.fragment = bytes()
            self.resync = True
        

if __name__ == '__main__':
    # cmds structure defines the available message types
    cmds = {1: {'name': 'Oscillator Interval',
                'decoder': '<BIBhB',
                'len': 9,
                'fn': lambda cmd, fcpu, interval, variance, end: \
                          print(f"F_CPU: {fcpu}, Interval {interval}, Variance: {variance}"),
               },
            }


    # Control codes
    NUL = 0			# This is the idle character in both directions
    END = 0xC0
    ESC = 0xDB
    ESC_END = 0xDC
    ESC_ESC = 0xDD


    spi = spiman(10000)

    try:
        while True:
            while spi.transfer():
                pass
            time.sleep(1)

    except KeyboardInterrupt:
        # Ctrl+C pressed, so...
        print('Goodbye')

    except RuntimeError:
        print('Run time error')

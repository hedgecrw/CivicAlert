from serial import Serial
from serial.tools.list_ports import comports
import struct

if __name__ == '__main__':

    for port, _, hwid in comports():
        if '303A:4001' in hwid:
            print('Found device on port:', port)
            with Serial(port) as s:
                s.write('Test Text: Hello!\n'.encode())
                while True:
                    print(struct.unpack('<H', s.read(2))[0])

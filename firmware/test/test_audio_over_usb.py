from serial import Serial
from serial.tools.list_ports import comports
from datetime import datetime
import struct

USB_PACKET_DELIMITER = [ 0x7E, 0x6F, 0x50, 0x11 ]

AUDIO_SAMPLE_RATE_HZ = 48000
AUDIO_BYTES_PER_SAMPLE = 2
AUDIO_DATA_LEN = AUDIO_SAMPLE_RATE_HZ * AUDIO_BYTES_PER_SAMPLE

if __name__ == '__main__':

   for port, _, hwid in comports():
      if '303A:4001' in hwid:
         print('Found device on port:', port)
         with Serial(port) as s:
            with open(datetime.now().strftime('%Y-%m-%d_%H-%M-%S') + '.wav', 'wb') as f:
               while True:
                  if s.read(1)[0] == USB_PACKET_DELIMITER[0] and s.read(1)[0] == USB_PACKET_DELIMITER[1] and s.read(1)[0] == USB_PACKET_DELIMITER[2] and s.read(1)[0] == USB_PACKET_DELIMITER[3]:
                     timestamp = struct.unpack('<d', s.read(8))[0]
                     lat = struct.unpack('<f', s.read(4))[0]
                     lon = struct.unpack('<f', s.read(4))[0]
                     height = struct.unpack('<f', s.read(4))[0]
                     data = s.read(AUDIO_DATA_LEN)
                     #samples = struct.unpack(f'<{AUDIO_SAMPLE_RATE_HZ}h', data)
                     print(f'Storing audio for timestamp {timestamp} @ <{lat}, {lon}, {height}>...')
                     f.write(data)

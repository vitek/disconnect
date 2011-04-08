import serial
from crc16 import crc16
import time

FLASH_PAGE_SIZE = 1056
FLASH_PAGES = 8192


class LoaderError(Exception):
    pass

class LoaderSignatureError(LoaderError):
    pass

class LoaderCRCError(LoaderError):
    pass

class Loader(object):
    def __init__(self, device):
        self.fp = serial.Serial(device,
                                57600,
                                bytesize=8,
                                parity=serial.PARITY_NONE,
                                stopbits=2,
                                timeout=2)

    def version(self):
        self.fp.write('hi\r\n')
        self.fp.flush()
        reply = self.fp.readline()
        reply = reply.rstrip().split(None, 1)
        if not reply or reply[0] != 'disconnect':
            raise LoaderSignatureError, "No DISCONNECT device found"
        return reply[1]

    def read_page(self, page):
        self.fp.write('read %x\r\n' % page)
        reply = self.fp.readline()
        if reply != 'ok\r\n':
            raise LoaderError, "got %r instead of OK" % reply
        reply = self.fp.read(FLASH_PAGE_SIZE + 4)
        if len(reply) != FLASH_PAGE_SIZE + 4:
            raise LoaderError, \
                  "Reply to short for read command, length = %d" % len(reply)
        data = reply[:FLASH_PAGE_SIZE]
        crc = int(reply[FLASH_PAGE_SIZE:], 16)

        if crc16(data) != crc:
            raise LoaderCRCError, "CRC16 error"
        return data

    def write_page(self, page, data):
        if len(data) > FLASH_PAGE_SIZE:
            raise LoaderError, "data is larger than page size"

        crc = crc16(data)
        self.fp.write('write %x %x %x\r\n' % (page, len(data), crc))
        self.fp.write(data)
        self.fp.flush()

        reply = self.fp.readline()
        if reply != 'ok\r\n':
            raise LoaderError, "got %r instead of OK" % reply

loader = Loader('/dev/ttyUSB0')
version = loader.version()

print 'DISCONNECT device version %r found' % version

PAGE = 'hello, world!!'
PAGE = PAGE + (FLASH_PAGE_SIZE - len(PAGE)) * 'X'

loader.write_page(0, PAGE)
print loader.read_page(0)

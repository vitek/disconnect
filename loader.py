import serial
import time
import wave
import os

from crc16 import crc16


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
        self.wait()

        reply = self.fp.read(FLASH_PAGE_SIZE + 6)
        if len(reply) != FLASH_PAGE_SIZE + 6:
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
        self.wait()

    def custom(self, cmd):
        self.fp.write("%s\r\n" % cmd)
        self.fp.flush()

    def wait(self, timeout=2):
        """Wait for okay reply"""
        self.fp.setTimeout(timeout)
        reply = self.fp.readline()
        if reply != 'ok\r\n':
            raise LoaderError, "got %r instead of OK" % reply


class SampleError(Exception):
    pass


class WavFile(object):
    def __init__(self, fname):
        fp = wave.open(fname, 'r')
        if fp.getnchannels() != 1:
            raise SampleError, "only mono samples are supported"
        if fp.getsampwidth() != 1:
            raise SampleError, "only 8-bit samples are supported"

        self.frames = ''

        while True:
            data = fp.readframes(1024)
            if not data:
                break
            self.frames += data

#sample = WavFile('standby.wav')

#raise SystemExit

loader = Loader('/dev/ttyUSB0')
version = loader.version()

print 'DISCONNECT device version %r found' % version


def test_hardware(loader):
    print 'Writing to flash'
    data = os.urandom(FLASH_PAGE_SIZE)
    loader.write_page(666, data)

    print 'Reading from flash'
    rdata = loader.read_page(666)

    if data != rdata:
        raise LoaderError, "data mismatch"

    print 'Testing beeper'
    loader.custom('zoom')
    loader.wait(5)

    print 'Testing busy beeper'
    loader.custom('busy')
    loader.wait(5)

    print 'Testing ring'
    loader.custom('ring')
    loader.wait(8)

test_hardware(loader)


#PAGE = 'hello, world!!'
#PAGE = PAGE + (FLASH_PAGE_SIZE - len(PAGE)) * 'X'
#
#loader.write_page(0, PAGE)
#print loader.read_page(0)

## data = sample.frames
## pageno = 0
## while data:
##     page = data[:FLASH_PAGE_SIZE]
##     data = data[FLASH_PAGE_SIZE:]
##     print 'writting page', pageno
##     loader.write_page(pageno, page)
##     pageno += 1



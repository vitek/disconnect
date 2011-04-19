import os
import sys
import time

import serial
import wave

from crc16 import crc16
from firmware import FLASH_PAGE_SIZE, FLASH_PAGES


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


def test_hardware(loader):
    print 'Writing to flash'
    data = os.urandom(FLASH_PAGE_SIZE)
    loader.write_page(FLASH_PAGES - 1, data)

    print 'Reading from flash'
    rdata = loader.read_page(FLASH_PAGES - 1)

    if data != rdata:
        raise LoaderError, "data mismatch"

    print 'Testing speaker with "saw"'
    loader.custom('saw')
    loader.wait(5)

    print 'Testing beeper'
    loader.custom('zoom')
    loader.wait(5)

    print 'Testing busy beeper'
    loader.custom('busy')
    loader.wait(5)

    print 'Testing ring'
    loader.custom('ring')
    loader.wait(8)


def flash_data(loader, data, page_no=0):
    while data:
        page = data[:FLASH_PAGE_SIZE]
        data = data[FLASH_PAGE_SIZE:]
        loader.write_page(page_no, page)
        page_no += 1
        sys.stdout.write('.')
        sys.stdout.flush()
    sys.stdout.write('\n')


if __name__ == "__main__":
    from optparse import OptionParser

    parser = OptionParser()
    parser.add_option("-d", "--device",
                      dest="device", default="/dev/ttyUSB0",
                      help="Specify serial device (default %default)")
    parser.add_option("--hwtest", dest="hwtest", default=False,
                      action="store_true", help="Run hardware test")
    parser.add_option("--go", dest="go", default=False,
                      action="store_true", help="Enter normal operation mode")
    parser.add_option("-l", "--load", dest="firmware",
                      help="Flash firmware file")

    (options, args) = parser.parse_args()

    loader = Loader(options.device)
    version = loader.version()

    print 'DISCONNECT device version %r found' % version

    if options.hwtest:
        test_hardware(loader)
    elif options.go:
        loader.custom('go')
    elif options.firmware:
        with open(options.firmware, 'rb') as fp:
            data = fp.read()
        flash_data(loader, data)

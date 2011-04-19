import struct
import sys
import wave

from crc16 import crc16


FLASH_PAGE_SIZE = 1056
FLASH_PAGES = 8192

SIGNATURE = 'v1\r\n'


def pad_page(data):
    padn = FLASH_PAGE_SIZE - len(data) % FLASH_PAGE_SIZE
    return data + '\xff' * padn



class SampleError(Exception):
    pass


class WavFile(object):
    def __init__(self, fname):
        fp = wave.open(fname, 'r')
        if fp.getnchannels() != 1:
            raise SampleError, "only mono samples are supported"
        if fp.getsampwidth() != 1:
            raise SampleError, "only 8-bit samples are supported"
        self.frames = fp.readframes(fp.getnframes())

    def __len__(self):
        return len(self.frames)


class Sample:
    def __init__(self, fname, role, weight):
        self.wave = WavFile(fname)
        self.role = role
        self.weight = weight
        self.page = -1


    def pages(self):
        """Length in pages"""
        return (len(self.wave) + FLASH_PAGE_SIZE) // FLASH_PAGE_SIZE

    def tobin(self):
        page = 0
        pages, odd = divmod(len(self.wave), FLASH_PAGE_SIZE)
        return struct.pack('<BBHHH',
                           self.role, self.weight,
                           self.page,
                           pages, odd)

ROLES_MAP = {
    'free': 0,
    'music': 1,
}


if __name__ == "__main__":
    output = sys.stdout

    samples = [Sample('output8.wav', 0, 10)]

    descr = ''
    pageno = 1
    for sample in samples:
        sample.page = pageno
        pageno += sample.pages()
        descr += sample.tobin()

    data = SIGNATURE
    data += struct.pack('<BH', len(samples), crc16(descr))
    data += descr
    data = pad_page(data)
    output.write(data)

    for sample in samples:
        data = pad_page(sample.wave.frames)
        output.write(data)

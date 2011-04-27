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
    def __init__(self, fname, role, weight=1, repeat=1):
        self.wave = WavFile(fname)
        self.role = role
        self.weight = weight
        self.repeat = repeat
        self.page = -1


    def pages(self):
        """Length in pages"""
        return (len(self.wave) + FLASH_PAGE_SIZE) // FLASH_PAGE_SIZE

    def tobin(self):
        page = 0
        pages, odd = divmod(len(self.wave), FLASH_PAGE_SIZE)
        return struct.pack('<BBBHHH',
                           self.role,
                           self.weight,
                           self.repeat,
                           self.page,
                           pages, odd)

ROLES_MAP = {
    'incoming': 0,    # Incoming call
    'busy':     1,    # Operators busy message
    'music':    2,    # Music loop
}

class FirmwareError(Exception):
    pass

# <wav-file> <role> <weight> [repeat]
def parse_fwin(fname):
    firmware = []
    with open(fname, 'rt') as fp:
        for lineno, line in enumerate(fp.readlines(), 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) not in range(3, 5):
                raise FirmwareError, "%d: wrong number of arguments" % lineno
            fname = parts[0]
            role = ROLES_MAP[parts[1]]
            weight = int(parts[2])
            if len(parts) >= 4:
                repeat = int(parts[3])
            else:
                repeat = 1
            firmware.append(Sample(fname, role, weight, repeat))
        return firmware

if __name__ == "__main__":
    output = sys.stdout

    if sys.argv < 2:
        print >> sys.stderr, 'Syntax: %s <fw.in>' % sys.argv[0]
        sys.exit(1)

    samples = parse_fwin(sys.argv[1])

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

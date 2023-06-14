#
# GMV(Gob MoVie file)
# gmv.gy jpg_dir wav_path fps output_path
#
import os
import sys
import argparse
import glob
from ctypes import *
import struct

class WaveHeader(LittleEndianStructure):
    _pack = 1
    _fields_ = [
        ('RIFF', c_uint32),
        ('chunk_size', c_uint32),
        ('WAVEfmt', c_uint8 * 8),
        ('fmt_chunk_size;', c_uint32),
        ('audiofmt;', c_uint16),
        ('channel', c_uint16),
        ('sample_rate', c_uint32),
        ('byte_per_sec', c_uint32),
        ('block_size', c_uint16),
        ('bit_per_sample', c_uint16)
    ]
        
    def __init__(self):
        self.RIFF = 0x46464952; # RIFF"
        self.chunk_size = 0
        self.WAVEFmt = [0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20 ]; # "WAVEfmt "
        self.fmt_chunk_size = 0
        self.audiofmt = 0
        self.channel = 0
        self.sample_rate = 0;
        self.byye_per_sec = 0
        self.bit_per_sample = 0

    def __repr__(self):
        return 'RIFF:{:x} chunk_size:{} fmt_chunk_size:{} audiofmt:{} channel:{} sample_rate:{} byte_per_sec:{}'.format(self.RIFF, self.chunk_size, self.fmt_chunk_size, self.audiofmt, self.channel, self.sample_rate, self.byte_per_sec)

class SubChunk(LittleEndianStructure):
    _pack = 1
    _fields_ = [
        ('identifier', c_uint32),
        ('chunk_size', c_uint32),
    ]

    def __init__(self, identifier = 0, sz = 0):
        self.identifier = identifier
        self.chunk_size = sz

    def __repr__(self):
        return 'identifier:{:x}, chnk_size:{}'.format(self.identifier, self.chunk_size)

class GMV0(LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ('signature', c_uint32),
        ('blocks', c_uint32),
        ('gcfOffset', c_uint32),
        ('fps', c_float),
        ]
    def __init__(self, blks = 0, fps = 1.0):
        self.signature = 0x30564d47 # "GMV0"
        self.blocks = blks
        self.gcfOffset = sizeof(GMV0) + sizeof(WaveHeader)
        self.fps = fps

class GCF0(LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ('signature', c_uint32),
        ('blocks', c_uint32),
        ('reserved0', c_uint32),
        ('reserved1', c_uint32),
    ]

    def __init__(self, blks = 0):
        self.signature = 0x30464347 # "GCF0"
        self.blocks = blks
        self.reserved0 = self.reserved1 = 0

def loadWav(fname):
    wh = WaveHeader()
    sub = SubChunk()
    data = []
    
    with open(fname, 'rb') as f:
        if f.readinto(wh) != sizeof(WaveHeader):
            raise OSError
        if f.readinto(sub) != sizeof(SubChunk):
            raise OSError
        data = f.read(sub.chunk_size)

        while sub.identifier != 0x61746164: # "data"
            if f.readinto(sub) != sizeof(SubChunk):
                raise OSError
            data = f.read(sub.chunk_size)

    return wh, sub, data
        
def main():
    parser = argparse.ArgumentParser(description='Create GMV file from JPEG files and wav file')
    parser.add_argument('dirname', help='Target image directory')
    parser.add_argument('wavfile', help='Target wav file')
    parser.add_argument('fps', type=int, help='Base frame per second')
    parser.add_argument('outfile', help='Output filename')
    parser.add_argument('--ext', '-e', type=str, default='jpg', help='Target image file extension')
    parser.add_argument('--verbose', '-v', action='store_true')
    args = parser.parse_args()

    wild = '*.' + args.ext
    fileFilter = os.path.join(os.getcwd(), args.dirname, wild)
    fileList = sorted(glob.glob(fileFilter))
    if(len(fileList) == 0):
        print('Not exists files', fileFilter)
        return 0

    if args.verbose:
        print('wave file size:{}'.format(os.path.getsize(args.wavfile)))
        print('Files:{}'.format(len(fileList)))

    wh,sub,wdata = loadWav(args.wavfile)
    wrest = len(wdata)
    wblk, mod = divmod(wh.byte_per_sec, args.fps)
    # Align 4 bytes
    wadd = 4
    wblk &= ~3
    wpos = 0
    wmod = 0

    if args.verbose:
        print(wh)
        print(sub)
        print('data len:{} blocksize:{} mod:{} add:{}'.format(len(wdata), wblk, mod, wadd))
    
    # Output GMV
    with open(args.outfile, 'wb') as outf:
        # GMV header
        header = GMV0(len(fileList), args.fps)
        outf.write(header)
        outf.write(wh)

        # Image and wav block
        for name in fileList:
            # Read imgae block
            with open(name, 'rb') as inf:
                d = inf.read()
                sz = len(d)
                # image size
                outf.write(struct.pack('<L', sz))
                # wav block size
                wsz = wblk
                wmod += mod
                if wmod >= args.fps:
                    wmod -= args.fps
                    wsz += wadd
                if(wsz > wrest): wsz = wrest
                outf.write(struct.pack('<L', wsz))
                # image data and wav data
                if sz > 0:
                    outf.write(d)
                if wsz > 0:
                    outf.write(wdata[wpos: wpos + wsz])
                    wpos += wsz
                    wrest -= wsz

                if args.verbose:
                    print('Image {} size:{} wsize:{}'.format(name, sz, wsz))
                
        outf.write(struct.pack('<L', 0xFFFFFFFF)) 
        outf.write(struct.pack('<L', 0xFFFFFFFF)) 
        outf.flush();
        outf.close()

if __name__ == '__main__':
    sys.exit(main())

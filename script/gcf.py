#
# Combines files in the specified directory
#
# //GCF HEADER
# uint32_t signature; // "GCF0" 0x30464347 (little endian)
# uint32_t blocks; // Number of blocks
# uint32_t reserved[2]; // Reserved
# //GCF FILES
# uint32_t size0; // File size 0
# uint8_t data0[size0]; // File data 0
# uint32_t size1; // File size 1
# uint8_t data1[size1]; // File data 1
# .
# .
# .
# uint32_t sizen{0xFFFFFFFF}; // Terminator
#

import os
import sys
import argparse
import glob
from ctypes import *
import struct

class GCF0(LittleEndianStructure):
    _pack_ = 2
    _fields_ = [
        ('signature', c_uint32),
        ('blocks', c_uint32),
        ('reserved0', c_uint32),
        ('reserved1', c_uint32),
    ]

    def __init__(self, blocks = 0):
        self.signature = 0x30464347 # GCF0
        self.blocks = blocks
        self.reserved0 = self.reserved1 = 0


def main():
    parser = argparse.ArgumentParser(description='Combines files in the specified directory')
    parser.add_argument('dirname', help='Target directory')
    parser.add_argument('outfile', help='Output filename')
    parser.add_argument('--ext', '-e', type=str, default='jpg', help='Target file extension')
    parser.add_argument('--verbose', '-v', action='store_true')
    args = parser.parse_args()

    wild = '*.' + args.ext
    fileFilter = os.path.join(os.getcwd(), args.dirname, wild)
    fileList = sorted(glob.glob(fileFilter))
    if(len(fileList) == 0):
        print('Not exists files', fileFilter)
        return 0

    if args.verbose:
        print('Files:{}'.format(len(fileList)))
    
    with open(args.outfile, 'wb') as outf:
        header = GCF0(len(fileList))
        outf.write(header)

        for name in fileList:
            with open(name, 'rb') as inf:
                d = inf.read()
                sz = len(d)
                if args.verbose:
                    print('Combine {} size:{}'.format(name, sz))

                outf.write(struct.pack('<L', sz))
                if sz > 0:
                    outf.write(d)
                    outf.flush()

        outf.write(struct.pack('<L', 0xFFFFFFFF)) # Terminator
        outf.flush();
        outf.close()
    

if __name__ == '__main__':
    sys.exit(main())

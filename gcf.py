#
# Combines files in the specified directory
#
#
#  header
#  uint32_t signeture "GCF0" gob combine file Version 0
#  uint32_t file count
#  uint32_t reserve[2]
#
#  files
#  uint32_t size  0xFFFFFF is end of GCF file
#  uint8_t [size] 
#  ....

import os
import sys
import argparse
import glob
from ctypes import *
import struct
import pprint as pp

class GCF0(LittleEndianStructure):
    _pack_ = 2
    _fields_ = [
        ('signeture', c_uint32),
        ('files', c_uint32),
        ('reserved0', c_uint32),
        ('reserved1', c_uint32),
    ]

    def __init__(self, files = 0):
        self.signeture = 0x30464347 # GCF0
        self.files = files
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

        outf.write(struct.pack('<L', 0xFFFFFFFF)) # End mark
        outf.close()
    

if __name__ == '__main__':
    sys.exit(main())

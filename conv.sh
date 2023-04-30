#!/bin/bash
#
# Make data for M5Stack_JpgLoopAnimaSD
#
# Author: GOB https://twitter.com/gob_52_gob
#
# Require
#  ffmpeg, convert (ImageMagick)
#  ffmpeg-normalize
#  gcf.py (Written by GOB)
#
# Usage: conv.sh moviefile framerate(number)
# ex) conv.sh foo.mp4 24
#     output foo.24.gcf and foo.24.wav
#

if [ $# -ne 2 ];then
   echo "Argument Error (Need 2 arguments)"
   echo "Usage: $0 moviefilepath frameRate(integer)"
   exit 1
fi

# Movie to each jpeg files
rm -rf jpg
mkdir jpg
ffmpeg -i $1 -r $2 -vf scale=320:-1 jpg/%05d.jpg

# Check file size and re-convert if oversized 10K
for fname in jpg/*.jpg
do
    size=$(wc -c < $fname)
    if [ $size -gt 10240 ]; then
	convert  $fname -define jpeg:extent=10kb $fname
    fi
done

# Combine jpg files
#python gcf.py -v jpg ${1%.*}.$2.gcf
python gcf.py jpg ${1%.*}.$2.gcf
# Make 8bit 8K mono wav and normalize
ffmpeg -i $1 -ac 1 -ar 8000 -acodec pcm_u8 -y ${1%.*}.$2.wav
ffmpeg-normalize ${1%.*}.$2.wav --audio-codec pcm_u8 --sample-rate 8000 -f -o ${1%.*}.$2.wav
exit 0


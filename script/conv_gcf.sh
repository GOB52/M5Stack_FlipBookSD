#!/bin/bash
#
# [Deprecated] Make GCF and wav file for M5Stack_FlipBookSD
# 
# Author: GOB https://twitter.com/gob_52_gob
#
# Require
#  FFmpeg,
#  convert (ImageMagick)
#  ffmpeg-normalize
#  gcf.py (Written by GOB)
#

# Check arguments
if [ $# -lt 2 ] || [ $# -gt 3 ];then
   echo "Usage: $0 movie_file_path frame_rate [jpeg_maximum_size]"
   echo "  frame_rate 1 ~ 30"
   echo "  jpeg_maximum_size 1024 - 10240 (7168 as default)"
   exit 1
fi

if [ ! -e $1 ];then
   echo "$1 is not exists."
   exit 1
fi

if [[ ! $2 =~ ^[0-9]+$ ]] || [ $2 -lt 1 ] || [ $2 -gt 30 ];then
   echo "Invalid frame_rate range (1 - 30)"
   exit 1
fi

if [ -n "$3" ] && [[ $3 =~ ^[0-9]+$ ]];then
   JPEGSIZE=$3
else
   JPEGSIZE=7168
fi
if [ $JPEGSIZE -lt 1024 ] || [ $JPEGSIZE -gt 10240 ];then
   echo "Invalid jpeg_maxumum_size range (1024 - 10240)"
   exit 1
fi
#echo "jpeg_maximum_size is $JPEGSIZE"

# Output JPEG images from movie.
rm -rf jpg$$
mkdir jpg$$
ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg

# Check file size and re-convert if oversized
for fname in jpg$$/*.jpg
do
    size=$(wc -c < $fname)
    if [ $size -gt $JPEGSIZE ]; then
	convert  $fname -define jpeg:extent=$JPEGSIZE $fname
    fi
done

# Combine JPEG files
#python gcf.py -v jpg ${1%.*}.$2.gcf
python gcf.py jpg$$ ${1%.*}.$2.gcf
# Make 8bit 8K mono wav and normalize
ffmpeg -i $1 -ac 1 -ar 8000 -acodec pcm_u8 -y ${1%.*}.$2.wav
ffmpeg-normalize ${1%.*}.$2.wav --audio-codec pcm_u8 --sample-rate 8000 -f -o ${1%.*}.$2.wav

#Cleanup
rm -rf jpg$$
exit 0



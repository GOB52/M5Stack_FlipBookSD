#!/bin/bash
#
# Make GMV file for M5Stack_FlipBookSD
#
# Author: GOB https://twitter.com/gob_52_gob
#
# Require
#  FFmpeg,
#  convert (ImageMagick)
#  ffmpeg-normalize
#  gmv.py (Written by GOB)
#

# Check arguments
if [ $# -lt 2 ] || [ $# -gt 3 ];then
   echo "Usage: $0 movie_file_path frame_rate [jpeg_maximum_size]"
   echo "  movie_file_path (File that FFmpeg can handle)"
   echo "  frame_rate 1.0 ~ 30.0 (Floating-point number)"
   echo "  jpeg_maximum_size 1024 - 10240 (7168 as default)"
   exit 1
fi

# Exists file $1?   
if [ ! -e $1 ];then
   echo "$1 is not exists."
   exit 1
fi

# Is valid $2?  
echo "$2" | grep -q "^-\?[0-9]\+\.\?[0-9]*$"
if [ $? -ne 0 ] || [ `echo "$2 < 1.0" | bc` -eq 1 ] || [ `echo "$2 > 30.0" | bc` -eq 1 ];then
   echo "Invalid frame_rate range (1.0 - 30.0)"
   exit 1
fi

# Exists and valid $3?
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

# ------------------------
# Choose output option
# ------------------------
# (1) Specify the frame rate per second without changing the total number of frames
#ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg

# (2) If the frame rate is less than the original, it is thinned out; if it is more, it is increased by averaging the frames before and after.
ffmpeg -i $1 -vf scale=320:-1,dejudder,framerate=$2 -qmin 1 -q 1 jpg$$/%06d.jpg

# (3) If the frame rate is less than the original, it is thinned out; if it is more, duplicate frames are created.
#ffmpeg -i $1 -vf scale=320:-1,dejudder,fps=$2 -qmin 1 -q 1 jpg$$/%06d.jpg

# Check file size and re-convert if oversized
for fname in jpg$$/*.jpg
do
    size=$(wc -c < $fname)
    if [ $size -gt $JPEGSIZE ]; then
	convert  $fname -define jpeg:extent=$JPEGSIZE $fname
    fi
done

# Make 8bit 8K mono wav and normalize
ffmpeg -i $1 -ac 1 -ar 8000 -acodec pcm_u8 -y ${1%.*}.wav
ffmpeg-normalize ${1%.*}.wav --audio-codec pcm_u8 --sample-rate 8000 -f -o ${1%.*}.wav
# Make 16bit 22.05K streo wav and normalize
#ffmpeg -i $1 -ac 2 -ar 22050 -acodec pcm_s16le -y ${1%.*}.wav
#ffmpeg-normalize ${1%.*}.wav --audio-codec pcm_s16le --sample-rate 22050 -f -o ${1%.*}.wav

# Combine JPEG files and wave file
python gmv.py jpg$$ ${1%.*}.wav $2 ${1%.*}.gmv

#Cleanup
rm -rf jpg$$
rm ${1%.*}.wav
exit 0

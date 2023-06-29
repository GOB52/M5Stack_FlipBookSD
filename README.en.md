# M5Stack_FlipBookSD

[日本語](README.md)

## Demo

https://github.com/GOB52/M5Stack_FlipBookSD/assets/26270227/3c894cd5-74e4-4016-9707-a489553fe9e6

SINTEL (Trailer)
[Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)  
© copyright Blender Foundation | [www.sintel.org](https://www.sintel.org)  
The data format is modified for playback.


## Overview
This application streams video files converted to the dedicated format gmv from SD.  
It uses multi-cores to perform rendering with DMA and audio playback.  
***The old format gcf + wav is no longer playable since 0.1.1. Please regenerate it in gmv format or convert it using the gcf + wav => gmv conversion script.***


## Target devices
It must be able to run the libraries it depends on and have an SD card.
* M5Stack Basic 2.6 or later
* M5Stack Gray
* M5Stack Core2 
* M5Stack CoreS3

## Required libraries
* [M5Unified](https://github.com/m5stack/M5Unified)
* [M5GFX](https://github.com/m5stack/M5GFX) 
* [SdFat](https://github.com/greiman/SdFat)

## Supported libraries (not required)
* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater)

## Included
* [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)  Version modified by Lovyan03 - san

## Build type (PlatformIO)
### For Basic,Gray,Core2
|Env|Description|
|---|---|
|release|Basic Settings|
|release\_DisplayModule| Support [DisplayModule](https://shop.m5stack.com/products/display-module-13-2)|
|release\_SdUpdater| Support SD-Updater |
|release\_SdUpdater\_DisplayModule| Support DisplayModule and SD-Updater |

### For CoreS3
|Env|Description|
|---|---|
|S3\_release|Basic Settings|
|S3\_release_DisplayModule| Support DisplayModule |

### Sample data for playback
Download [sample_0_1_1.zip](https://github.com/GOB52/M5Stack_FlipBookSD/files/11871296/sample_0_1_1.zip), unzip it and copy to **/gcf** on your SD card.

## How to make data
### Required tools 
* [Python3](https://www.python.org/)
* [FFmpeg](https://ffmpeg.org/)
* [Convert(ImageMagick)](https://imagemagick.org/)
* [ffmpeg-normalize](https://github.com/slhck/ffmpeg-normalize)

(See also each webpage for installation) 

### Procedure
Making data on terminal.  
Video data can be in any format that can be processed by FFmpeg.

1. Copy video data to an arbitrarily created directory.
1. Copy [conv.sh](script/conv.sh) and [gmv.py](script/gmv.py) to the same directory.
1. Execute the shell script as follows  
**bash conv.sh movie_file_name frame_rate [ jpeg_maxumum_size (Default if not specified is 7168) ]**

| Argument | Required?| Description |
|---|---|---|
|movie\_file_path|YES|Source movie|
|frame\_rate|YES|Output frame rate (1.0 - 30.0)<br>**Integer or decimal numbers can be specified**|
|jpeg\_maximum\_size|NO|Maximum file size of one image to output (1024 - 10240)<BR>Larger sizes preserve quality but are more likely to cause processing delays (see "Known Issues").|

4. The files that named "videofilename.gmv" output to same directory.
5. Copy the above files to **/gcf** on the SD card.

e.g.)
```sh
mkdir foo
cp bar.mp4 foo
cp script/conv.sh foo
cp script/gcf.py foo
cd foo
bash conv.sh bar.mp4 29.97
cp bar.gmv your_sd_card_path/gcf
```

### Processes performed by shell scripts
* Output JPEG images from video at the specified frame rate.  
Create an output directory of . /jpg+PID as the output directory. This allows multiple terminals to convert in parallel.
* If the size of the output JPEG file exceeds the specified size, reconvert it to fit.
* Output audio data from the video and normalize it out.
* gmv.py creates a dedicated file containing images and audio.

#### Parameters of FFmpeg
```sh
ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg
```
You can change the output quality, filters, etc. to your liking. The best parameters depend on the source video, so please refer to FFmpeg's information.

### Data restrictions
* wav data quality (8KHz unsigned 8bit mono)  
The quality of the audio data is lowered to reduce the processing load.  
Scripts can be edited to improve quality, but processing delays may occur due to processing load. (See Known Issues)

* Image size and frame rate  
When converting a video to JPEG, the width is 320px and the height is a value that maintains the aspect ratio.  
<ins>Currently, 320 x 240 can be played back at about 24 FPS, and 320 x 180 at about 30 FPS.</ins>  
To change the image size, edit the parameter for FFmpeg in conv.sh. **(scale=)**

* Image size and output device size  
If the image size is narrower or wider than the output device size, it will be centered.

## Known issues
### Audio is choppy or playback speed is slow.
This may be due to the processing not being completed in time within a frame.  
In rare cases, it may take a long time to read from the SD card, which may cause some frames to not be processed in time.  
There may be a problem with the compatibility or formatting of the SD card.

https://github.com/greiman/SdFat/issues/96#issuecomment-377332392

>OS utilities should not be used for formatting SD cards. FAT16/FAT32 has lots of options for file system layout. In addition to the cluster size there are options for aligning file structures.  
>The SD Association has a standard layout for each size SD card. Cards are designed to optimize performance for the standard layout. For example, flash chip boundaries are aligned with file system structures.  
>My SdFormatter example produces the standard layout. On a PC use the [SD Association Formatter](https://www.sdcard.org/downloads/).  
>You should not be getting errors due to the format. The correct format will only enhance performance, not reduce errors.  
>I rarely see the type errors you are having. Most users either have solid errors or no errors.  
>I have seen this type error when another SPI device interferes with the SD or when there are noisy or poor SPI signals.  

#### Workaround by the data
* Reduce playback frame rate
```sh
bash conv.sh video.mp4 30 # 30 FPS
bash conv.sh video.mp4 24 # Reduce to 24
```
* Reduce JPEG file size 
```sh
bash conv.sh video.mp4 30      # 7168 as default
bash conv.sh video.mp4 30 5120 # Reduce to 5120
```
* Reduce image size
```sh
conv.sh
# ...
#ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg  # 320 x n pixel
 ffmpeg -i $1 -r $2 -vf scale=240:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg  # 240 x n pixel
# ...
```

## How to operate
### Menu
| Button | Description |
|---|---|
|Click A| To previous file |
|Click B| Playback current file|
|Click C| To next file |
|Hold A| Change playback method |
|Hold C| Change playback method |

### During playback
| Button(Basic,Gray,Core2) | Touch(CoreS3)| Description |
|---|---|---|
|Press A| Press left 1/3 of the screen | Decrease sound volume|
|Click B| Click center 1/3 of the screen | Stop payback and back to menu|
|Press C| Press right 1/3 of the screen | Increase sound volume|


## Conversion from old format (gcf + wav)
Pyhton script for conversion [gcf\_to\_gmv.py](script/gcf_to_gmv.py) and shell script for conversion of files in the current directory [convert\_gcf\_to\_gmv.sh](script/convert_gcf_to_gmv.sh) for converting files in the current directory.

```sh
# gcf_dir has gcf + wav files
cp script/gcf_to_gmv.py gcf_dir
cp script/convert_gcf_to_gmv.sh gcf_dir
cd gcf_dir
bash convert_gcf_to_gmv.sh
cp *.gmv your_sd_card_path/gcf
```

## Digression
### Why combine all the JPEG files together?
Opening and seeking files on an SD card takes a fair amount of time.  
When JPEG files are opened and displayed one at a time, the speed becomes a bottleneck.  
Therefore, we have adopted the method of sequentially loading a large single file, which was effective for physical media (floppy disks, etc.) and optical media (CDs, etc.), to shorten the card access time.  

Initially, we used unzipLIB to read from uncompressed ZIP files, but since the file is internally seek  once, we developed an original file format that assumes sequential reading without seeking, and by using this format we were able to eliminate the seeking.  

The read processing time per image has gone from tens of ms for each file opened to a few ms.

### Digression of the digression
I was experimenting with unzipLIB because I wanted to handle ZIP files. I wanted to learn how to use it, and I wanted to play back a collection of image files like a flip book.  
(In the end, I ended up not using unzipLIB (´･ω･`) )

### Is this different from [MotionJPEG](https://en.wikipedia.org/wiki/Motion_JPEG)?
According to the definition, this can be called MJPEG. However, MJPEG is not a unified file format, so this application is only a gmv format player.

## Appendix
* src/gob\_jpg\_sprite.hpp
* src/gob\_jpg\_sprite.cpp  
This class is for JPEG output using multi-cores to LGFX\_Sprite instead of screen.  
I made it when I was doing a lot of trial and error, but ended up not using it.  
Too good to waste, so I include it in the package.

## Acknowledgments
Those who have been the forerunners in the flip-book program. Your activities have inspired us to create this program.
* [6jiro](https://twitter.com/6jiro1) -san  
https://smile-dental-clinic.info/wordpress/?p=10904  
* [PocketGriffon](https://twitter.com/GriffonPocket) -san  
https://pocketgriffon.hatenablog.com/entry/2023/02/27/103329  


The author of TJpgDec
* [ChaN](http://elm-chan.org/) -san  
[TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)

I borrowed and modified the logic for DMA drawing to the screen using TJpegDec.  
He also gave me lots of technical advice.  
And he is the author of M5Unified and M5GFX.

* [Lovyan03](https://twitter.com/lovyan03) -san  
[M5Stack_JpgLoopAnime](https://github.com/lovyan03/M5Stack_JpgLoopAnime)  
[ESP32_ScreenShotReceiver](https://github.com/lovyan03/ESP32_ScreenShotReceiver)



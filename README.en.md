# M5Stack_FlipBookSD

[日本語](README.md)

## Demo

https://github.com/GOB52/M5Stack_FlipBookSD/assets/26270227/3c894cd5-74e4-4016-9707-a489553fe9e6

SINTEL (Trailer)
[Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)  
© copyright Blender Foundation | [www.sintel.org](https://www.sintel.org)  
The data format is modified for playback.


## Overview
This application plays [gcf file](#gcf-file-format)(original format) that are combined [JPEG](https://en.wikipedia.org/wiki/JPEG) files splited from video file, along with Wave file.  
This application works like a video playback by playing back the combined images in SD with sound.  
It uses multi-cores to perform rendering with DMA and audio playback.


## Target devices
It must be able to run the libraries it depends on and have an SD card.
* M5Stack Basic 2.6
* M5Stack Gray
* M5Stack Core2 
* M5Stack CoreS3
 
However, Basic and Gray, which do not have PSRAM, have significant limitations on the amount of time that audio can be played.

## Required libraries
* [M5Unified](https://github.com/m5stack/M5Unified)
* [M5GFX](https://github.com/m5stack/M5GFX) 
* [SdFat](https://github.com/greiman/SdFat)

## Supported libraries (not required)
* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater)

## Included
* [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)  Version modified by Lovyan03 - san

## Build type (PlatfromIO)
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
|S3\_release_DisplayModule| Support DislayModule |

### Sample data for playback
Download [sample_data_002.zip](https://github.com/GOB52/M5Stack_FlipBookSD/files/11531987/sample_data_002.zip) and copy it to **/gcf** on your SD card.


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
1. Copy [conv.sh](conv.sh) and [gcf.py](gcf.py) to the same directory.
1. Execute the shell script as follows  
**bash conv.sh move_file_name frame_rate [ jpeg_maxumu,_size (Default if not specified is 7168) ]**

| Argument | Required?| Description |
|---|---|---|
|move_file_path|YES|Source movie|
|frame_rate|YES|Output frame rate (1 - 30)|
|jpeg_maximum_size|NO|Maximum file size of one image to output (1024 - 10240)<BR>Larger size helps maintain quality but increases the likelihood of crashes (see Known Issues)|

4. The files that named "videofilename.framerate.gcf" and "videofilename.wav" output to same directory.
5. Copy the above two files to **/gcf** on the SD card.

e.g.)
```
mkdir foo
cp bar.mp4 foo
cp conv.sh foo
cp gcf.py foo
cd foo
bash conv.sh bar.mp4 24
cp bar.24.gcf your_sd_card_path/gcf
cp bar.wav your_sd_card_path/gcf
```

### Processes performed by shell scripts
* Output JPEG images from video at the specified frame rate.  
Create an output directory of . /jpg+PID as the output directory. This allows multiple terminals to convert in parallel.
* If the size of the output JPEG file exceeds the specified size, reconvert it to fit.
* Combine JPEG files to create a gcf file.
* Output audio data from the video and normalize it out.

#### Parameters of FFmpeg
```sh
ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg
```
You can change the output quality, filters, etc. to your liking. The best parameters depend on the source video, so please refer to FFmpeg's information.

### Data restrictions
* Wav data is output in 8KHz, unsigned 8bit, mono format.
If there is not enough memory, no sound will be played. For device without PSRAM, it would have to be a short one to make it sound.  
On Core2, PSRAM normally has a usable size of 4 MiB, so approximately about 8 minutes and 30 seconds seems to be the maximum playback length.  
On Core3 that can use 8MB,approximately 16 minutes is considered the maximum playback length.

* Image size and frame rate
When converting a video to JPEG, the width is 320px and the height is a value that maintains the aspect ratio.  
<ins>Currently, 320 x 240 can be played back at about 24 FPS, and 320 x 180 at about 30 FPS.</ins>  
To change the image size, edit the parameter for FFmpeg in conv.sh. **(scale=)**

## Known issues
### Reset during playback
If a reset occurs during execution, the Serial monitor should display a message that the assertion was caught.  
The cause is that drawing did not finish within the specified time, and the SD and Lcd buses collided.

Also, in rare cases, it may take a long time to read from SD, which may cause a reset as described above.  
The cause of this is not known.

#### Workaround by the program
* Switch multi-core playback to single-core playback.  
Switch the corresponding section of main.cpp to the one using a single core.  
Playback speed will be reduced, but bus contention will be reliably avoided.

```cpp
src/main.cpp
static void loopRender()
{
    // ...
	{
        ScopedProfile(drawCycle);
        //mainClass.drawJpg(buffer, JPG_BUFFER_SIZE); // Process on multiple cores
        mainClass.drawJpg(buffer, JPG_BUFFER_SIZE, false); // Process on single core.
	}
    // ...
}
```

#### Workaround by the data
* Reduce playback frame rate
```
bash conv.sh video.mp4 30 # 30 FPS
bash conv.sh video.mp4 24 # Reduce to 24
```
* Reduce JPEG file size 
```
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

## gcf file format
The extension stands for **G**ob **C**ombined **F**iles.  
As shown in the schematic below, this is a simple file with a header followed by the size and actual data.  
No explicit seek is required.

```cpp
//GCF HEADER
uint32_t signature; // "GCF0" 0x30464347 (little endian)
uint32_t files; // Number of files stored
uint32_t reserved[2]; // Reserved
//GCF FILES
uint32_t size0; // File size 0
uint8_t data0[size0]; // File data 0
uint32_t size1; // File size 1
uint8_t data1[size1]; // File data 1
.
.
.
uint32_t sizen{0xFFFFFFFF}; // Terminator
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
According to the definition, this can be called MJPEG. However, MJPEG is not a unified file format, so this application is only a gcf format player.

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



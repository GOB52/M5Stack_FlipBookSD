/*
  M5Stack_FlipBookSD
  Author: GOB https://twitter.com/gob_52_gob
*/
#include <SdFat.h>
#if defined(FBSD_ENABLE_DISPLAY_MODULE)
# pragma message "[FBSD] Enable display module"
# include <M5ModuleDisplay.h>
#endif
#include <M5Unified.h>

#if defined(FBSD_ENABLE_SD_UPDATER)
# pragma message "[FBSD] Enable SD-Updater"
# define SDU_NO_AUTODETECT
# define USE_SDFATFS
# define HAS_M5_API
# define SDU_USE_DISPLAY
# define HAS_LGFX
# define SDU_Sprite LGFX_Sprite
# define SDU_DISPLAY_TYPE M5GFX*
# define SDU_DISPLAY_OBJ_PTR &M5.Display
# include <M5StackUpdater.h>
#endif

#include <esp_system.h>
#include <esp_idf_version.h>

#include "scoped_profiler.hpp"
#include "MainClass.h"
#include "movie_file.hpp"
#include "file_list.hpp"
#include <gob_unifiedButton.hpp>

#ifndef TFCARD_CS_PIN
# define TFCARD_CS_PIN (4)
#endif
#ifndef BASE_FPS
# define BASE_FPS (30)
#endif
#ifndef JPG_BUFFER_SIZE
# define JPG_BUFFER_SIZE (1024*10)
#endif
#ifndef WAV_BLOCK_BUFFER_SIZE
#define WAV_BLOCK_BUFFER_SIZE (1024 * 8)  // 44100 2ch 16bit => bytes per sec 176400 / fps
#endif
#ifndef NUMBER_OF_BUFFERS
#define NUMBER_OF_BUFFERS (3)  // Circular buffer
#endif

namespace
{
using UpdateDuration = std::chrono::duration<float, std::ratio<1, BASE_FPS> >;
UpdateDuration durationTime{1}; // 1 == (1 / BASE_FPS)
ESP32Clock::time_point lastTime{};
float fps{};

// std::this_thread::sleep_until returns earlier than the specified time, so implemeted own function.
void sleepUntil(const std::chrono::time_point<ESP32Clock, UpdateDuration>& absTime)
{
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(absTime - ESP32Clock::now()).count();
    //delay(us >= 0 ? us/1000 : 0); // ms
    if(us > 1000)
    {
        auto tc = xTaskGetTickCount();
        vTaskDelayUntil(&tc, us/1000/portTICK_PERIOD_MS);
    }
    //auto at = std::chrono::time_point_cast<ESP32Clock::duration>(absTime);
    //while(ESP32Clock::now() < at) { taskYIELD(); }
    while(std::chrono::time_point_cast<UpdateDuration>(ESP32Clock::now()) < absTime) { taskYIELD(); }
}

auto& display = M5.Display;
SdFs sd;
uint8_t volume{72}; // 0~255
uint32_t currentFrame{}, maxFrames{};
uint32_t loadCycle{}, drawCycle{}, wavCycle{};
bool primaryDisplay{};

MainClass mainClass;

FileList list;
MovieFile* mfile{};
MovieFileGCF gcf{};
MovieFileGMV gmv{};

uint8_t* buffers[NUMBER_OF_BUFFERS]; // For 1 of JPEG file(GCF), JPEG and wav block(GMV)
uint32_t bufferIndex{};

uint8_t* wavBuffer{}; // wav buffer (PSRAM) for GCF 
uint32_t wavBufferSize{};

gob::UnifiedButton unfiedButton;

enum class PlayType : int8_t { Single, RepeatSingle, RepeatAll, Shuffle };
PlayType& operator++(PlayType& pt)
{
    pt = static_cast<PlayType>( static_cast<int8_t>(pt) + 1 );
    if(static_cast<int8_t>(pt) > static_cast<int8_t>(PlayType::Shuffle)) { pt = static_cast<PlayType>(0); }
    return pt;
}
PlayType& operator--(PlayType& pt)
{
    pt = static_cast<PlayType>( static_cast<int8_t>(pt) - 1 );
    if(static_cast<int8_t>(pt) < 0) { pt = static_cast<PlayType> ( static_cast<int8_t>(PlayType::Shuffle) ); }
    return pt;
}
PlayType playType{PlayType::RepeatAll};

PROGMEM const char ptSingle[] = "Play single";
PROGMEM const char ptRepeatSingle[] = "Repeat single";
PROGMEM const char ptRepeatAll[] = "Repeat all";
PROGMEM const char ptRepeatSfuffle[] = "Repeat shuffle";
PROGMEM const char* ptTable[] = { ptSingle, ptRepeatSingle, ptRepeatAll, ptRepeatSfuffle };
//
}

// Load one of the JPEG image and playback Wav
// WARNING: BUS must be released
static bool load1Frame()
{
    if(!mfile) { return false; }

    auto buf = buffers[bufferIndex];
    uint32_t isize{};
    
#if !defined(FIXED_FRAME)
    if(!mfile->eof())
    {
        // Load image
        {
            ScopedProfile(loadCycle);
            isize = mfile->readImage(buf, JPG_BUFFER_SIZE + WAV_BLOCK_BUFFER_SIZE);
            bufferIndex += (isize > 0);
            bufferIndex %= 3;
            currentFrame = mfile->readCount();
        }
        // In case of GCF, load wav file and played back in memory.
        if(mfile == &gcf && currentFrame == 0 && wavBufferSize)
        {
            auto sz = mfile->readWav(wavBuffer, wavBufferSize);
            M5.Speaker.playWav(wavBuffer, sz);
        }
        // In the case of GMV, it reads and plays back partially.
        else if(mfile == &gmv)
        {
            ScopedProfile(wavCycle);
            auto wh = mfile->wavHeader();
            auto wsz = mfile->readWav(nullptr/*dummy*/, 0/*dummy*/);
            M5_LOGD("isz:%u wsz:%u", isize, wsz);
            if(wh.bit_per_sample >> 4)
            {
                M5.Speaker.playRaw((const int16_t*)(buf + isize), wsz >> 1, wh.sample_rate, wh.channel >= 2, 1, 0);
            }
            else
            {
                M5.Speaker.playRaw(buf + isize, wsz, wh.sample_rate, wh.channel >= 2, 1, 0);
            }
        }
        return isize > 0;
    }
    return false;
#else
    if(FIXED_FRAME < mfile->files())
    {
        while(!mfile->eof() && currentFrame < FIXED_FRAME)
        {
            mfile->readImage(buf, JPG_BUFFER_SIZE + WAV_BLOCK_BUFFER_SIZE);
            currentFrame = mfile->readCount();
        }
    }
    return true;
#endif
}

// WARNING: BUS must be released
static bool playMovie(const String& path, const bool bus = true)
{
    M5.Speaker.stop();
    currentFrame = maxFrames = 0;

    if(mfile) { mfile->close(); }

    auto ext = FileList::getExt(path);
    M5_LOGI("ext[%s]", ext.c_str());
    
    if(ext == "gcf")
    {
        mfile = &gcf;
    }
    else if(ext == "gmv")
    {
        mfile = &gmv;
    }
    else { M5_LOGE("Invalid ext %s", path.c_str()); return false; }

    if(!mfile->open(path) || mfile->baseFps() == 0)
    {
        M5_LOGE("Failed to open %s : %u", path.c_str(), mfile->baseFps()); return false;
    }

    maxFrames = mfile->maxFrames();
    M5_LOGI("[%s] MaxFrames:%u FrameRate:%u", path.c_str(), maxFrames, mfile->baseFps()); 
    
    // durationTime is calculated from frame rate.
    durationTime = UpdateDuration((float)BASE_FPS / mfile->baseFps());

    const auto& wh = mfile->wavHeader();
    M5_LOGI("Wav rate:%u bit_per_sample:%u ch:%u blocksize:%u byte_per_sec:%u",
            wh.sample_rate, wh.bit_per_sample, wh.channel, wh.block_size, wh.byte_per_sec);

    auto b = load1Frame();
    if(b && bus) { display.startWrite(); }
    return b;
}

void setup()
{
    // M5Unified
    auto cfg = M5.config();
#if defined(__M5GFX_M5MODULEDISPLAY__)
    cfg.module_display.logical_width = 320;
    cfg.module_display.logical_height = 240;
#endif
    M5.begin(cfg);
    M5.Log.setEnableColor(m5::log_target_t::log_target_serial, false);
    display.clear(TFT_DARKGRAY);

    // Speaker settings
    auto spk_cfg = M5.Speaker.config();
#if defined(__M5GFX_M5MODULEDISPLAY__)
    // Choose Display if Display module and cable connected
    int32_t idx = M5.getDisplayIndex(m5gfx::board_M5ModuleDisplay);
    M5_LOGI("ModuleDisplay?:%d",idx);
    if (idx >= 0)
    {
        uint8_t buf[256];
        //auto dsp = &M5.Displays(idx);
        if (0 < ((lgfx::Panel_M5HDMI*)(M5.Displays(idx).panel()))->readEDID(buf, sizeof(buf)))
        {
            M5_LOGI("Detected the display, Set Display primary");            
            primaryDisplay = true;
            M5.setPrimaryDisplay(idx);
            // Speaker settings for ModuleDisplay
#if defined ( CONFIG_IDF_TARGET_ESP32S3 )
            static constexpr const uint8_t pins[][2] =
                    {// DOUT       , BCK
                        { GPIO_NUM_13, GPIO_NUM_6 }, // CoreS3 + ModuleDisplay
                    };
            int pins_index = 0;
#else
            static constexpr const uint8_t pins[][2] =
                    {// DOUT       , BCK
                        { GPIO_NUM_2 , GPIO_NUM_27 }, // Core2 and Tough + ModuleDisplay
                        { GPIO_NUM_15, GPIO_NUM_12 }, // Core + ModuleDisplay
                    };
            // !core is (Core2 + Tough)
            int pins_index = (M5.getBoard() == m5::board_t::board_M5Stack) ? 1 : 0;
#endif
            spk_cfg.pin_data_out = pins[pins_index][0];
            spk_cfg.pin_bck      = pins[pins_index][1];
            spk_cfg.pin_ws       = GPIO_NUM_0;     // LRCK
            spk_cfg.i2s_port = I2S_NUM_1;
            spk_cfg.sample_rate = 48000; // Module Display audio output is fixed at 48 kHz
            spk_cfg.magnification = 16;
            spk_cfg.stereo = true;
            spk_cfg.buzzer = false;
            spk_cfg.use_dac = false;
        }
    }
#endif
    M5_LOGI("Speaker sample_rate:%u dma_buf_len:%u dma_buf_count:%u", spk_cfg.sample_rate, spk_cfg.dma_buf_len, spk_cfg.dma_buf_count);
    M5.Speaker.config(spk_cfg);

    M5_LOGI("Output to %s", primaryDisplay ? "Display" : "Lcd");
    if(M5.getBoard() == m5::board_t::board_M5Stack) { volume = 144; }
    if(primaryDisplay) { volume = 128; }
    M5.Speaker.setVolume(volume);
    
#if defined(FBSD_ENABLE_SD_UPDATER)
    // SD-Updater
    SDUCfg.setAppName("M5Stack_FlipBookSD");
    SDUCfg.setBinFileName("/M5Stack_FlipBookSD.bin");
    auto SdFatSPIConfig = SdSpiConfig( TFCARD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(25) );
    checkSDUpdater(sd, String(MENU_BIN), 2000, &SdFatSPIConfig);
#endif
    
    // SdFat
    int retry = 10;
    bool mounted{};
    while(retry-- && !(mounted = sd.begin((unsigned)TFCARD_CS_PIN, SD_SCK_MHZ(25))) ) { delay(100); }
    if(!mounted) { M5_LOGE("Failed to mount %xH", sd.sdErrorCode()); display.clear(TFT_RED); while(1) { delay(10000); } }
    
    // 
    if(display.width() < display.height()) { display.setRotation(display.getRotation() ^ 1); }
    display.setBrightness(128);
    display.setFont(&Font2);
    display.setTextDatum(textdatum_t::top_center);
    display.clear(0);

    M5.BtnA.setHoldThresh(500);
    M5.BtnB.setHoldThresh(500);
    M5.BtnC.setHoldThresh(500);
    unfiedButton.begin(&display);

    // file list
#if defined(FBSD_SUPPORT_GCF)
    // Support old and new format
    list.make("/gcf", "gcf");
    list.append("gmv");
#else
    // Only new format
    list.make("/gcf", "gmv");
#endif
    
    // Allocate buffer
    for(auto& buf : buffers)
    {
        buf = (uint8_t*)heap_caps_malloc(JPG_BUFFER_SIZE + WAV_BLOCK_BUFFER_SIZE,  MALLOC_CAP_DMA); // For DMA transfer
        assert(buf);
    }
    M5_LOGI("ImageBuffer:%p/%p/%p", buffers[0], buffers[1], buffers[2]);

    wavBufferSize = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if(wavBufferSize)
    {
        wavBuffer = (uint8_t*)heap_caps_malloc(wavBufferSize, MALLOC_CAP_SPIRAM);
        if(!wavBuffer) { wavBufferSize = 0; }
    }
    M5_LOGI("PSRAM Wav buffer:%p (%u)", wavBuffer, wavBufferSize);
    
    mainClass.setup(&display);
    
    // Information
    M5_LOGI("ESP-IDF Version %d.%d.%d",
           (ESP_IDF_VERSION>>16) & 0xFF, (ESP_IDF_VERSION>>8)&0xFF, ESP_IDF_VERSION & 0xFF);
    M5_LOGI("End of setup free:%u internal:%u large internal:%u",
            esp_get_free_heap_size(), esp_get_free_internal_heap_size(),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    M5_LOGI("PSRAM:size:%u free:%u largest:%u",
            ESP.getPsramSize(), ESP.getFreePsram(),heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    
    lastTime = ESP32Clock::now();
}

using loop_function = void(*)();
static void loopMenu();
static void loopRender();
loop_function loop_f = loopMenu;

// Menu
static void loopMenu()
{
    auto prevType = playType;
    auto prevCur = list.current();

    // Decide and play
    if(M5.BtnB.wasClicked())
    {
        display.clear(0);
        if(playMovie(list.getCurrentFullpath()))
        {
            loop_f = loopRender;
            unfiedButton.changeAppearance(gob::UnifiedButton::appearance_t::transparent_all);
            lastTime = ESP32Clock::now();
            return;
        }
    }

    // Change play type or prev GCF file.
    if(M5.BtnA.wasHold())
    {
        --playType;
        if(playType == PlayType::Shuffle) { list.shuffle(); }
        if(prevType == PlayType::Shuffle) { list.sort(); }
    }
    else if(M5.BtnA.wasClicked())
    {
        list.prev();
    }

    // Change play type or prev GCF file.
    if(M5.BtnC.wasHold())
    {
        ++playType;
        if(playType == PlayType::Shuffle) { list.shuffle(); }
        if(prevType == PlayType::Shuffle) { list.sort(); }
    }
    else if(M5.BtnC.wasClicked())
    {
        list.next();
    }

    bool dirty{};
    if(prevType != playType || prevCur != list.current()) { display.clear(0); dirty = true;}
    
    display.drawString(list.getCurrent().c_str(), display.width()/2, display.height()/2);
    char str[128];
    snprintf(str, sizeof(str), "%d/%d", list.current(), list.files()); str[sizeof(str)-1] = '\0';
    display.drawString(str, display.width()/2, display.height()/2 + 16);
    display.drawString(ptTable[(int8_t)playType], display.width()/2, display.height()/2 + 48);

    unfiedButton.draw(dirty);
}

static void changeToMenu()
{
    M5.Speaker.stop();
    loop_f = loopMenu;
    unfiedButton.changeAppearance(gob::UnifiedButton::appearance_t::bottom);
    display.clear(0);

}

// Render to lcd directly with DMA
static void loopRender()
{
    {
        ScopedProfile(drawCycle);
        mainClass.drawJpg(buffers[(bufferIndex - 1 + NUMBER_OF_BUFFERS) % NUMBER_OF_BUFFERS], JPG_BUFFER_SIZE); // Process on multiple cores
        //mainClass.drawJpg(buffers[(bufferIndex - 1 + NUMBER_OF_BUFFERS) % NUMBER_OF_BUFFERS], JPG_BUFFER_SIZE, false); // Process on single core. Try it, if If assert occurs on xQueueSend call. (However, FPS will be reduced)
    }

    // Keep FPS
    auto absTime = std::chrono::time_point_cast<UpdateDuration>(lastTime) + durationTime;
    sleepUntil(absTime);
    auto now = ESP32Clock::now();
    auto delta = now - lastTime;
    lastTime = now;
    fps = BASE_FPS / std::chrono::duration_cast<UpdateDuration>(delta).count();
    M5_LOGI("%2.2f %5d/%5d %u/%u/%u", fps, currentFrame, maxFrames, loadCycle, wavCycle, drawCycle);
    
    // Change volume
    if(M5.BtnA.isPressed()) { if(volume >   0) { --volume; M5.Speaker.setVolume(volume); } }
    if(M5.BtnC.isPressed()) { if(volume < 255) { ++volume; M5.Speaker.setVolume(volume); } }
    // Stop
    if(M5.BtnB.wasClicked())
    {
        changeToMenu();
        display.endWrite();
        return;
    }

    display.endWrite(); // Must be call when access to the SD.
    if(currentFrame < maxFrames - 1)
    {
        load1Frame();
    }
    // If the end of frames
    else
    {
        switch(playType)
        {
        case PlayType::RepeatAll:
        case PlayType::Shuffle:
            M5_LOGI("To next file");
            list.next();
            // fallthrough
        case PlayType::RepeatSingle:  // Repeat single
            M5_LOGI("Playback from top");
            display.clear(0);
            if(!playMovie(list.getCurrentFullpath(), false)) { changeToMenu(); return; }
            lastTime = ESP32Clock::now();
            break;
        default:
            break; // Nop
        }
    }
    display.startWrite();
}

//
void loop()
{
    unfiedButton.update();
    M5.update();
    loop_f();
}


/*
  M5Stack_FlipBookSD

  This application is similar to video playback by playing back images from SD like a flip book.
  SD からパラパラ漫画のように画像を音声と再生することで、動画再生に近い事をするアプリケーションです。  

  Author: GOB https://twitter.com/gob_52_gob
*/
#if defined(FBSD_ENABLE_DISPLAY_MODULE)
# pragma message "[FBSD] Enable display module"
# include <M5ModuleDisplay.h>
#endif 

#include <SdFat.h>
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
//# define SDU_TouchButton LGFX_Button
# include <M5StackUpdater.h>
#endif

#include <esp_system.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_idf_version.h>

#include "scoped_profiler.hpp"
#include "MainClass.h"
#include "gob_combined_files.hpp"
#include "file_list.hpp"

#ifndef TFCARD_CS_PIN
# define TFCARD_CS_PIN (4)
#endif
#ifndef BASE_FPS
# define BASE_FPS (30)
#endif
#ifndef JPG_BUFFER_SIZE
# define JPG_BUFFER_SIZE (1024*10)
#endif

// Display only the specified frame (For debug)
//#define FIXED_FRAME (1023)

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
    auto ms = us > 0 ? us / 1000 : 0;
    delay(ms);
    while(ESP32Clock::now() < absTime){ taskYIELD(); }
}

auto& display = M5.Display;
SdFs sd;
uint8_t volume{72}; // 0~255
uint8_t* buffer; // For 1 of JPG file
uint32_t currentFrame{}, maxFrames{};
uint32_t loadCycle{}, drawCycle{};
MainClass mainClass;
gob::CombinedFiles gcf;
FileList list;

enum class PlayType : int8_t { Single, RepeatSingle, RepeatAll, Shuffle, Max };
PlayType& operator++(PlayType& pt)
{
    pt = static_cast<PlayType>( static_cast<int8_t>(pt) + 1 );
    if(static_cast<int8_t>(pt) >= static_cast<int8_t>(PlayType::Max)) { pt = static_cast<PlayType>(0); }
    return pt;
}
PlayType& operator--(PlayType& pt)
{
    pt = static_cast<PlayType>( static_cast<int8_t>(pt) - 1 );
    if(static_cast<int8_t>(pt) < 0) { pt = static_cast<PlayType> ( static_cast<int8_t>(PlayType::Max) - 1); }
    return pt;
}
PlayType playType{PlayType::RepeatAll};

PROGMEM const char ptSingle[] = "play single";
PROGMEM const char ptRepeatSingle[] = "repeat single";
PROGMEM const char ptRepeatAll[] = "repeat all";
PROGMEM const char ptRepeatSfuffle[] = "repeat shuffle";
PROGMEM const char* ptTable[] = { ptSingle, ptRepeatSingle, ptRepeatAll, ptRepeatSfuffle };

class Wave
{
  public:
    ~Wave() { if(_buf) { free(_buf); } }

    // WARNING: BUS must be released by endWrite()
    bool load(const char* path)
    {
        if(_buf) { free(_buf); }
        stop();
        FsFile f = sd.open(path);
        if(f)
        {
            size_t fsz = f.size();
            Serial.printf("WAV:%zu\n", fsz);
            _buf = (uint8_t*)malloc(fsz); // Allocated at PSRAM in most cases if exists PSRAM
            if(_buf) {  _size = f.read(_buf, fsz); }
            f.close();
            return _buf && _size == fsz;
        }
        return false;
    }
    void play() { if(_buf) { M5.Speaker.playWav(_buf, _size); }}
    void stop() { M5.Speaker.stop(); }
    
  private:
    uint8_t* _buf{};
    uint32_t _size{};
};
Wave wav;
//
}

// Load one of the jpg from GCF.
// WARNING: BUS must be released by endWrite()
static bool loadJpg()
{
    ScopedProfile(loadCycle);
#ifndef FIXED_FRAME
    if(!gcf.eof())
    {
        auto sz = gcf.read(buffer, JPG_BUFFER_SIZE);
        currentFrame = gcf.readed();
        return sz > 0;
    }
    return false;
#else
    if(FIXED_FRAME < gcf.files())
    {
        while(!gcf.eof() && currentFrame < FIXED_FRAME)
        {
            gcf.read(buffer, JPG_BUFFER_SIZE);
            currentFrame = gcf.readed();
        }
    }
    return true;
#endif
}

// WARNING: BUS must be released by endWrite()
static bool playGCF(const String& path, const bool bus = true)
{
    currentFrame = maxFrames = 0;

    // path "foo.{num}.gcf"
    // num is frame rate
    auto idx = path.lastIndexOf('.');
    if(idx < 2) { return false; }
    auto idx2 = path.lastIndexOf('.', idx -1);
    if(idx2 < 0) { return false; }
    String frs = path.substring(idx2+ 1, idx);
    int fr = frs.toInt();
    if(fr <= 0) { return false; }

    // durationTime is calculated from frame rate.
    durationTime = UpdateDuration((float)BASE_FPS / fr);

    // gcf open and load 1st frame
    if(!gcf.open(path.c_str()) || !loadJpg()) {return false; }
    maxFrames = gcf.files();
    Serial.printf("[%s] MaxFrames:%u FrameRate:%u\n", path.c_str(), maxFrames, fr);
    
    String wpath = FileList::changeExt(path, "wav");
    wav.load(wpath.c_str());
    
    if(bus) { display.startWrite(); }

    wav.play();
    return true;
}

void setup()
{
    // Incrase internal heap (Not effective in some environments)
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    esp_bt_mem_release(ESP_BT_MODE_BTDM);

    // M5Unified
    auto cfg = M5.config();
#if defined(__M5GFX_M5MODULEDISPLAY__)
    cfg.module_display.logical_width = 320;
    cfg.module_display.logical_height = 240;
#endif
    cfg.external_speaker.module_display = true;
    M5.begin(cfg);
    M5.setPrimaryDisplayType(m5::board_t::board_M5ModuleDisplay);

#if defined(FBSD_ENABLE_SD_UPDATER)
    // SD-Updater
    SDUCfg.setAppName("M5Stack_FlipBookSDSD");
    SDUCfg.setBinFileName("/M5Stack_FlipBookSD.bin");
    auto SdFatSPIConfig = SdSpiConfig( TFCARD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(25) );
    checkSDUpdater(sd, String(MENU_BIN), 2000, &SdFatSPIConfig);
#endif

    // SdFat
    int retry = 32;
    bool mounted{};
    while(retry-- && !(mounted = sd.begin((unsigned)TFCARD_CS_PIN, SD_SCK_MHZ(25))) ) { delay(500); }
    if(!mounted) { Serial.printf("%s\n", "Failed to mount file system\n"); abort(); }

    // 
    if(display.width() < display.height()) { display.setRotation(display.getRotation() ^ 1); }
    //display.setBrightness(64);
    display.setFont(&Font2);
    display.setTextDatum(textdatum_t::top_center);
    display.clear(0);
    display.setAddrWindow(0, 0, display.width(), display.height());

    M5.Speaker.setVolume(volume);
    M5.BtnA.setHoldThresh(500);
    M5.BtnB.setHoldThresh(500);
    M5.BtnC.setHoldThresh(500);

    // Allocate buffer
    buffer = (uint8_t*)heap_caps_malloc(JPG_BUFFER_SIZE, MALLOC_CAP_DMA); // Must allocate on SRAM
    assert(buffer);
    Serial.printf("Buffer:%p\n", buffer);

    mainClass.setup(&display);

    // file list
    list.make("/gcf");

    // Memory usage
    Serial.printf("End of setup free:%u internal:%u large:%u large internal:%u\n",
             esp_get_free_heap_size(), esp_get_free_internal_heap_size(),
             heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    Serial.printf("PSRAM:size:%u free:%u\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("ESP-IDF Version %d.%d.%d\n",
           (ESP_IDF_VERSION>>16) & 0xFF, (ESP_IDF_VERSION>>8)&0xFF, ESP_IDF_VERSION & 0xFF);

    
    lastTime = ESP32Clock::now();
}

using loop_function = void(*)();
static void loopMenu();
static void loopRender();
loop_function loop_f = loopMenu;

// Menu
static void loopMenu()
{
    M5.update();

    auto prevType = playType;
    auto prevCur = list.current();

    // Decide and play
    if(M5.BtnB.wasClicked())
    {
        display.clear(0);
        if(playGCF(list.getCurrentFullpath()))
        {
            loop_f = loopRender;
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

    if(prevType != playType || prevCur != list.current()) { display.clear(0); }
    
    display.drawString(list.getCurrent().c_str(), display.width()/2, display.height()/2);
    char str[128];
    snprintf(str, sizeof(str), "%d/%d", list.current(), list.files()); str[sizeof(str)-1] = '\0';
    display.drawString(str, display.width()/2, display.height()/2 + 16);
    display.drawString(ptTable[(int8_t)playType], display.width()/2, display.height()/2 + 48);
}

// Render to lcd directly with DMA
static void loopRender()
{
    {
        ScopedProfile(drawCycle);
        mainClass.drawJpg(buffer, JPG_BUFFER_SIZE); // Process on multiple cores
        //mainClass.drawJpg(buffer, JPG_BUFFER_SIZE, false); // Process on single core. Try it, if If assert occurs on xQueueSend call. (However, FPS will be reduced)
    }

    // Keep FPS
    sleepUntil(lastTime + durationTime);
    auto now = ESP32Clock::now();
    auto delta = now - lastTime;
    lastTime = now;
    fps = BASE_FPS / std::chrono::duration_cast<UpdateDuration>(delta).count();
    Serial.printf("%2.2f %5d/%5d %u/%u\n", fps, currentFrame, maxFrames, loadCycle, drawCycle);

    //
    M5.update();
    // Change volume
    if(M5.BtnA.isPressed()) { if(volume >   0) { --volume; } M5.Speaker.setVolume(volume); }
    if(M5.BtnC.isPressed()) { if(volume < 255) { ++volume; } M5.Speaker.setVolume(volume); }
    // Stop
    if(M5.BtnB.wasClicked())
    {
        loop_f = loopMenu;
        wav.stop();
        display.clear(0);
        display.endWrite();
        return;
    }
    display.endWrite();

    if(currentFrame < maxFrames - 1)
    {
        loadJpg();
    }
    // If the end of frames...
    else
    {
        switch(playType)
        {
        case PlayType::Single: break; // Nop
        case PlayType::RepeatSingle: // Repeat single
            currentFrame = 0;
            gcf.rewind();
            if(!loadJpg()) { wav.stop(); loop_f = loopMenu; return; }
            wav.stop();
            wav.play();
            lastTime = ESP32Clock::now();
            break;
        default: // Repeat all / shuffle
            list.next();
            display.clear(0);
            if(!playGCF(list.getCurrentFullpath(), false)) { wav.stop(); loop_f = loopMenu; return; }
            lastTime = ESP32Clock::now();
            break;
        }
    }
    display.startWrite();
}

//
void loop()
{
    loop_f();
}


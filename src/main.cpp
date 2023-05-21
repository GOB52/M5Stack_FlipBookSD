/*
  M5Stack_FlipBookSD

  This application is similar to video playback by playing back images from SD like a flip book.
  SD からパラパラ漫画のように画像を音声と共に再生することで、動画再生に近い事をするアプリケーションです。  

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
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_idf_version.h>

#include "scoped_profiler.hpp"
#include "MainClass.h"
#include "gob_combined_files.hpp"
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
uint8_t* buffer; // For 1 of JPEG file
uint32_t currentFrame{}, maxFrames{};
uint32_t loadCycle{}, drawCycle{};
MainClass mainClass;
bool primaryHDMI{};
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

PROGMEM const char ptSingle[] = "Play single";
PROGMEM const char ptRepeatSingle[] = "Repeat single";
PROGMEM const char ptRepeatAll[] = "Repeat all";
PROGMEM const char ptRepeatSfuffle[] = "Repeat shuffle";
PROGMEM const char* ptTable[] = { ptSingle, ptRepeatSingle, ptRepeatAll, ptRepeatSfuffle };

class Wave
{
  public:
    ~Wave() { free(_buf); }

    // WARNING: BUS must be released by endWrite()
    bool load(const char* path)
    {
        // Allocate largest heap and keep it. (At PSRAM if exists)
        if(!_buf)
        {
            _memSize = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
            M5_LOGI("Allocate for wave %u", _memSize);
            _buf = (uint8_t*)malloc(_memSize);
            if(!_buf)  {M5_LOGE("Failed to allocate wave buffer"); return false; }
        }

        stop();
        
        FsFile f = sd.open(path);
        if(!f) { M5_LOGE("Failed to open [%s]", path); return false; }

        size_t fsz = f.size();
        _dataSize = 0;
        if(fsz <= _memSize)
        {
            _dataSize = f.read(_buf, fsz);
        }
        f.close();
        return _dataSize == fsz;
    }
    void play() { if(_buf && _dataSize) { M5.Speaker.playWav(_buf, _dataSize); }}
    void stop() { M5.Speaker.stop(); }
    
  private:
    uint8_t* _buf{};
    size_t _memSize{}, _dataSize{};
};
Wave wav;

gob::UnifiedButton unfiedButton;
//
}

// Load one of the JPEG imagefrom GCF.
// WARNING: BUS must be released
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

// WARNING: BUS must be released
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
    if(fr < 1) { return false; }

    // durationTime is calculated from frame rate.
    durationTime = UpdateDuration((float)BASE_FPS / fr);

    // gcf open and load 1st frame
    if(!gcf.open(path.c_str()) || !loadJpg()) {return false; }
    maxFrames = gcf.files();
    M5_LOGI("[%s] MaxFrames:%u FrameRate:%u", path.c_str(), maxFrames, fr);
    
    String wpath = FileList::changeExt(path, "wav");
    auto wb = wav.load(wpath.c_str());
    if(!wb) { M5_LOGE("Failed to load wav"); }
    
    if(bus) { display.startWrite(); }

    wav.play();
    return true;
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
    M5.Log.setLogLevel(m5::log_target_display, (esp_log_level_t)-1); // Disable output to display
    display.clear(TFT_DARKGRAY);
    
#if defined(__M5GFX_M5MODULEDISPLAY__)

    // Choose HDMI if HDMI module and cable connected
    int32_t idx = M5.getDisplayIndex(m5gfx::board_M5ModuleDisplay);
    M5_LOGI("ModuleDisplay?:%d",idx);
    if (idx >= 0)
    {
        uint8_t buf[256];
        //auto dsp = &M5.Displays(idx);
        if (0 < ((lgfx::Panel_M5HDMI*)(M5.Displays(idx).panel()))->readEDID(buf, sizeof(buf)))
        {
            M5_LOGI("Detected the display, Set HDMI primary");            
            primaryHDMI = true;
            M5.setPrimaryDisplay(idx);
            // Speaker settings for ModuleDisplay
            auto spk_cfg = M5.Speaker.config();
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
            M5.Speaker.config(spk_cfg);
        }
    }
#endif
    M5_LOGI("Output to %s", primaryHDMI ? "HDMI" : "Lcd");
    if(primaryHDMI) { volume = 128; }
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
    
    // Allocate buffer
    buffer = (uint8_t*)heap_caps_malloc(JPG_BUFFER_SIZE, MALLOC_CAP_DMA); // For DMA transfer
    assert(buffer);
    M5_LOGI("Buffer:%p", buffer);

    mainClass.setup(&display);

    // file list
    list.make("/gcf");

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
        if(playGCF(list.getCurrentFullpath()))
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
    wav.stop();
    loop_f = loopMenu;
    unfiedButton.changeAppearance(gob::UnifiedButton::appearance_t::bottom);
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
    M5_LOGI("%2.2f %5d/%5d %u/%u", fps, currentFrame, maxFrames, loadCycle, drawCycle);
    
    // Change volume
    if(M5.BtnA.isPressed()) { if(volume >   0) { --volume; M5.Speaker.setVolume(volume); } }
    if(M5.BtnC.isPressed()) { if(volume < 255) { ++volume; M5.Speaker.setVolume(volume); } }
    // Stop
    if(M5.BtnB.wasClicked())
    {
        loop_f = loopMenu;
        unfiedButton.changeAppearance(gob::UnifiedButton::appearance_t::bottom);
        wav.stop();
        display.clear(0);
        display.endWrite();
        return;
    }

    display.endWrite(); // Must be call when access to the SD.
    if(currentFrame < maxFrames - 1)
    {
        loadJpg();
    }
    // If the end of frames
    else
    {
        switch(playType)
        {
        case PlayType::Single: break; // Nop
        case PlayType::RepeatSingle:  // Repeat single
            M5_LOGI("Repeat single");
            currentFrame = 0;
            gcf.rewind();
            if(!loadJpg()) { changeToMenu(); return; }
            wav.play();
            lastTime = ESP32Clock::now();
            break;
        default: // Repeat all / shuffle
            M5_LOGI("To next");
            list.next();
            display.clear(0);
            if(!playGCF(list.getCurrentFullpath(), false)) { changeToMenu(); return; }
            lastTime = ESP32Clock::now();
            break;
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


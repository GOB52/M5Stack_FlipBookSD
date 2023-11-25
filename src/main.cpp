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
#include "gob_gmv_file.hpp"
#include "file_list.hpp"
#include <gob_unifiedButton.hpp>

#include <deque>
#include <numeric>

#ifndef TFCARD_CS_PIN
# define TFCARD_CS_PIN (4)
#endif
#ifndef BASE_FPS
# define BASE_FPS (30)
#endif
#ifndef JPG_BUFFER_SIZE
# define JPG_BUFFER_SIZE (1024 * 10)  // WARNING: Must match the maximum JPEG size of the script for data creation.
#endif
#ifndef WAV_BLOCK_BUFFER_SIZE
#define WAV_BLOCK_BUFFER_SIZE (1024 * 8)
#endif

#define BUFFER_SIZE (JPG_BUFFER_SIZE + WAV_BLOCK_BUFFER_SIZE)

#ifndef NUMBER_OF_BUFFERS
#define NUMBER_OF_BUFFERS (3)  // Circular buffer
#endif

// For debug
//#define FIXED_FRAME (100)
//#define START_FRAME (2317)

namespace
{
using UpdateDuration = std::chrono::duration<float, std::ratio<1, BASE_FPS> >;
//UpdateDuration durationTime{1}; // 1 == (1 / BASE_FPS)
ESP32Clock::time_point lastTime{};
float fps{};
std::deque<float> fpsQueue{BASE_FPS/*reserve size*/};

float averageFps()
{
    if(fpsQueue.empty()) { return 0.0f; }
    return std::accumulate(fpsQueue.cbegin(), fpsQueue.cend(), 0.0f) / fpsQueue.size();
}
void clearFpsQueue() { fpsQueue.clear(); }
void pushFpsQueue(const float f)
{
    if(fpsQueue.size() >= BASE_FPS) { fpsQueue.pop_front(); }
    fpsQueue.emplace_back(f);

}
#if 0
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
#endif

auto& display = M5.Display;
SdFs sd;

uint8_t volume{}; // 0~255
uint32_t currentFrame{}, maxFrames{};
uint32_t loadCycle{}, drawCycle{}, wavCycle{};
uint32_t loadCycleTotal{}, drawCycleTotal{}, wavCycleTotal{};
bool primaryDisplay{};

MainClass mainClass;

FileList list;
gob::GMVFile gmv{};

uint8_t* buffers[NUMBER_OF_BUFFERS]; // For 1 of JPEG and wav block
uint32_t bufferIndex{}, outIndex{}, jpegSize{}, wavSize{}, wavTotal{};

goblib::UnifiedButton unifiedButton;

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

// Load one of the JPEG image and wav block
// WARNING: BUS must be released
static bool load1Frame()
{
    if(!gmv) { return false; }

    auto buf = buffers[bufferIndex];
#if !defined(FIXED_FRAME) && !defined(START_FRAME)
    if(!gmv.eof())
    {
        {
            ScopedProfile(loadCycle);
            std::tie(jpegSize, wavSize) = gmv.readBlock(buf, BUFFER_SIZE);
            outIndex = bufferIndex;
            ++bufferIndex;
            bufferIndex %= 3;
            currentFrame = gmv.readCount();
        }
        return jpegSize || wavSize;
    }
    return false;
#elif defined(FIXED_FRAME)
    auto frame = (FIXED_FRAME < gmv.blocks()) ? FIXED_FRAME : gmv.blocks() - 1;
    while(!gmv.eof() && currentFrame < frame)
    {
        std::tie(jpegSize, wavSize) = gmv.readBlock(buf, BUFFER_SIZE);
        currentFrame = gmv.readCount();
    }
    return true;
#elif defined(START_FRAME)
    auto frame = (START_FRAME < gmv.blocks()) ? START_FRAME : gmv.blocks() - 1;
    if(currentFrame < frame)
    {
        M5_LOGI("Goto %u", frame);
        while(!gmv.eof() && currentFrame < frame)
        {
            std::tie(jpegSize, wavSize) = gmv.readBlock(buf, BUFFER_SIZE);
            currentFrame = gmv.readCount();
        }
        return true;
    }
    if(!gmv.eof())
    {
        {
            ScopedProfile(loadCycle);
            std::tie(jpegSize, wavSize) = gmv.readBlock(buf, BUFFER_SIZE);
            outIndex = bufferIndex;
            ++bufferIndex;
            bufferIndex %= 3;
            currentFrame = gmv.readCount();
        }
        return jpegSize || wavSize;
    }
    return false;
#endif
}

// WARNING: BUS must be released
static bool playMovie(const String& path, const bool bus = true)
{
    M5.Speaker.stop();
    wavTotal = currentFrame = maxFrames = 0;
    loadCycleTotal = wavCycleTotal = drawCycleTotal = 0;
    clearFpsQueue();
    
    if(gmv) { gmv.close(); }
    if(!gmv.open(path) || gmv.fps() == 0)
    {
        M5_LOGE("Failed to open %s : %u", path.c_str(), gmv.fps()); return false;
    }

    maxFrames = gmv.blocks();
    M5_LOGI("[%s] MaxFrames:%u FrameRate:%f", path.c_str(), maxFrames, gmv.fps());
    
    // durationTime is calculated from frame rate.
    //durationTime = UpdateDuration((float)BASE_FPS / gmv.fps());

    const auto& wh = gmv.wavHeader();
    M5_LOGI("Wav rate:%u bit_per_sample:%u ch:%u blocksize:%u byte_per_sec:%u",
            wh.sample_rate, wh.bit_per_sample, wh.channel, wh.block_size, wh.byte_per_sec);

    if(bus) { display.startWrite(); }
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
    volume = M5.Speaker.getVolume();
    
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
    unifiedButton.begin(&display);

    // file list (Search "/gcf" if "/gmv" is empty or not exists.)
    if(list.make("/gmv", "gmv") == 0)
    {
        M5_LOGI("Research directory gcf");
        list.make("/gcf", "gmv");
    }
    
    // Allocate buffer
    for(auto& buf : buffers)
    {
        buf = (uint8_t*)heap_caps_malloc(BUFFER_SIZE,  MALLOC_CAP_DMA); // For DMA transfer
        assert(buf);
        M5_LOGI("Buffer:%p", buf);
    }
    
    mainClass.setup(&display);
    
    // Information
    M5_LOGI("ESP-IDF Version %d.%d.%d",
           (ESP_IDF_VERSION>>16) & 0xFF, (ESP_IDF_VERSION>>8)&0xFF, ESP_IDF_VERSION & 0xFF);
    M5_LOGI("End of setup free:%u internal:%u large internal:%u",
            esp_get_free_heap_size(), esp_get_free_internal_heap_size(),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    
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
            unifiedButton.changeAppearance(goblib::UnifiedButton::appearance_t::transparent_all);
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

    unifiedButton.draw(dirty);
}

static void changeToMenu()
{
    M5.Speaker.stop();
    loop_f = loopMenu;
    unifiedButton.changeAppearance(goblib::UnifiedButton::appearance_t::bottom);
    display.clear(0);

}

// Render to lcd directly with DMA
static void loopRender()
{
    static float afps{};

#if defined(DEBUG)
    display.setCursor(0, 4);
    display.printf("F:%2.2f C:%u", afps, currentFrame);
#endif
    
    // 0:Wait dma done (Completed rendering to lcd?)
    while(mainClass.isBusy()) { delay(1); }
    
    // 1:Release BUS
    display.endWrite();

    // Change volume
    if(M5.BtnA.isPressed()) { if(volume >   0) { M5.Speaker.setVolume(--volume); }}
    if(M5.BtnC.isPressed()) { if(volume < 255) { M5.Speaker.setVolume(++volume); }}
    // Stop
    if(M5.BtnB.wasClicked()) { changeToMenu(); return; }

    // 2:Load one block of the image and wav from SDg
    if(currentFrame >= maxFrames - 1) // End of file
    {
        switch(playType)
        {
        case PlayType::RepeatAll:
        case PlayType::Shuffle:
            M5_LOGD("Total: %u / %u / %u", loadCycleTotal, wavCycleTotal, drawCycleTotal);
            M5_LOGI("To next file");
            list.next();
            // fallthrough
        case PlayType::RepeatSingle:
            M5_LOGI("Playback from top");
            display.clear();
            if(!playMovie(list.getCurrentFullpath(), false)) { changeToMenu(); return; }
            lastTime = ESP32Clock::now();
            break;
        default:
            break; // Nop
        }
    }
    load1Frame();
    
    // 3:Occupy BUS
    display.startWrite();

    // 4:Start rendering image with DMA
    {
        ScopedProfile(drawCycle);
        mainClass.drawJpg(buffers[outIndex], jpegSize); // Process on multiple cores
    }

    // 5:Playback audio (Wait for the playback audio queue to empty)
    {
        ScopedProfile(wavCycle);
        auto& wh = gmv.wavHeader();
        const uint8_t* buf = buffers[outIndex] + jpegSize;
        if(wh.bit_per_sample >> 4)
        {
            M5.Speaker.playRaw((const int16_t*)(buf), wavSize >> 1, wh.sample_rate, wh.channel >= 2, 1, 0);
        }
        else
        {
            M5.Speaker.playRaw(buf, wavSize, wh.sample_rate, wh.channel >= 2, 1, 0);
        }
        wavTotal += wavSize;
        M5_LOGV("outIdx:%u jsz:%u wsz:%u/%u", outIndex, jpegSize, wavSize, wavTotal);
    }

    auto now = ESP32Clock::now();
    auto delta = now - lastTime;
    lastTime = now;
    fps = BASE_FPS / std::chrono::duration_cast<UpdateDuration>(delta).count();
    pushFpsQueue(fps);
    afps = averageFps();
    uint32_t addCycle = loadCycle + wavCycle + drawCycle;
    loadCycleTotal += loadCycle;
    wavCycleTotal += wavCycle;
    drawCycleTotal += drawCycle;
    M5_LOGD("%5d/%5d %2.2f/%2.2f %u/%u/%u [%u]", currentFrame, maxFrames, fps, afps, loadCycle, wavCycle, drawCycle, addCycle);
}

//
void loop()
{
    M5.update();
    unifiedButton.update();
    loop_f();
}


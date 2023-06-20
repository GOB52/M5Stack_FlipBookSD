/*
  MainClass.h
  This is a modified version based on
  https://github.com/lovyan03/M5Stack_JpgLoopAnime/blob/master/JpgLoopAnime/src/MainClass.h (Thanks Lovyan03-san)
 */
#ifndef _MAINCLASS_H_
#define _MAINCLASS_H_

#include <esp_heap_caps.h>
#include <M5GFX.h>
#include "tjpgdClass.h"

// Decode jpg and push to diaplay with DMA.
class MainClass
{
  public:
    bool setup(LovyanGFX* lcd);
    bool drawJpg(const uint8_t* buf, int32_t len, const bool multi = true);

    // In rendering?
    bool isBusy() const { return _busy || (_lcd && _lcd->dmaBusy()); }
    
  private:
    LovyanGFX* _lcd{};
    uint_fast8_t _bytesize{};

    static constexpr uint8_t buf_count = 2;
    uint8_t* _dmabufs[buf_count];
    uint8_t* _dmabuf{};
    TJpgD _jdec{};

    int32_t _remain{};
    const uint8_t* _filebuf{};
    uint32_t _fileindex{};

    int32_t _lcd_width{}, _lcd_height{};
    int32_t _out_width{}, _out_height{};
    int32_t _off_x{}, _off_y{};
    int32_t _jpg_x{}, _jpg_y{};

    using FpJpgWrite = uint32_t(*)(TJpgD*,void*,TJpgD::JRECT*);
    FpJpgWrite _fp_jpgWrite{};

    static uint32_t jpgRead(TJpgD *jdec, uint8_t *buf, uint32_t len);
    static uint32_t jpgWrite24(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect);
    static uint32_t jpgWrite16(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect);
    static uint32_t jpgWriteRow(TJpgD *jdec, uint32_t y, uint32_t h);

    bool _busy{};
};

#endif

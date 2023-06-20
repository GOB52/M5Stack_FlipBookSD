/*
  MainClass.cpp
  This is a modified version based on
  https://github.com/lovyan03/M5Stack_JpgLoopAnime/blob/master/JpgLoopAnime/src/MainClass.h (Thanks Lovyan03-san)
 */
#include <SdFat.h>
#include <M5Unified.h> // For Log
#include "MainClass.h"

#pragma GCC optimize ("O3")

bool MainClass::setup(LovyanGFX* lcd)
{
    _lcd = lcd;
    _lcd_width = lcd->width();
    _lcd_height = lcd->height();
    _bytesize = lcd->getColorConverter()->bytes;
    _fp_jpgWrite = (_bytesize == 2)
            ? jpgWrite16
            : jpgWrite24
            ;
    for (int i = 0; i < buf_count; ++i)
    {
        _dmabufs[i] = (uint8_t*)heap_caps_malloc(_lcd_width * 48 * _bytesize, MALLOC_CAP_DMA);
        assert(_dmabufs[i]);
    }
    _dmabuf = _dmabufs[0];

    _jdec.multitask_begin();
    _busy = false;
    return true;
}

bool MainClass::drawJpg(const uint8_t* buf, int32_t len, const bool multi)
{
    _filebuf = buf;
    _fileindex = 0;
    _remain = len;
    TJpgD::JRESULT jres = _jdec.prepare(jpgRead, this);
    if (jres != TJpgD::JDR_OK) {
        M5_LOGE("prepare failed! %d", jres);
        return false;
    }

    _out_width = std::min<int32_t>(_jdec.width, _lcd_width);
    _jpg_x = (_lcd_width - _jdec.width) >> 1;
    if (0 > _jpg_x) {
        _off_x = - _jpg_x;
        _jpg_x = 0;
    } else {
        _off_x = 0;
    }

    _out_height = std::min<int32_t>(_jdec.height, _lcd_height);
    _jpg_y = (_lcd_height- _jdec.height) >> 1;
    if (0 > _jpg_y) {
        _off_y = - _jpg_y;
        _jpg_y = 0;
    } else {
        _off_y = 0;
    }
    //M5_LOGI("j(%d,%d) o(%d,%d) j:[%d,%d] out[%d,%d]", _jpg_x, _jpg_y, _off_x, _off_y, _jdec.width, _jdec.height, _out_width, _out_height);

    _busy = true;
    jres = multi ? _jdec.decomp_multitask(_fp_jpgWrite, jpgWriteRow) :  _jdec.decomp(_fp_jpgWrite, jpgWriteRow);

    // If the value is 1 (JDR_INTR),
    // No problem because the process is stopped by myself.
    // See also [*1]
    if (jres > TJpgD::JDR_INTR)
    {
        M5_LOGE("decomp failed! %d", jres);
        return false;
    }
    //M5_LOGI("==>core:%u dma:%d", xPortGetCoreID(), _lcd && _lcd->dmaBusy());
    return true;
}

uint32_t MainClass::jpgRead(TJpgD *jdec, uint8_t *buf, uint32_t len) {
    MainClass* me = (MainClass*)jdec->device;
    if (len > me->_remain)  len = me->_remain;
    if (buf) {
        memcpy(buf, (const uint8_t *)me->_filebuf + me->_fileindex, len);
    }
    me->_fileindex += len;
    me->_remain -= len;
    return len;
}

// for 24bit color panel
uint32_t MainClass::jpgWrite24(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect) {
    MainClass* me = (MainClass*)jdec->device;

    auto dst = me->_dmabuf;

    uint_fast16_t x = rect->left;
    uint_fast16_t y = rect->top;
    uint_fast16_t w = rect->right + 1 - x;
    uint_fast16_t h = rect->bottom + 1 - y;
    uint_fast16_t outWidth = me->_out_width;
    uint_fast16_t outHeight = me->_out_height;
    uint8_t *src = (uint8_t*)bitmap;
    uint_fast16_t oL = 0, oR = 0;

    if (rect->right < me->_off_x)      return 1;
    if (x >= (me->_off_x + outWidth))  return 1;
    if (rect->bottom < me->_off_y)     return 1;
    if (y >= (me->_off_y + outHeight)) { me->_busy = false; return 0; } // No more rendering. [*1] => decomp failed 1 (Interrupted by output function.

    int32_t src_idx = 0;
    int32_t dst_idx = 0;

    if (me->_off_y > y) {
        uint_fast16_t linesToSkip = me->_off_y - y;
        src_idx += linesToSkip * w;
        h -= linesToSkip;
    }
        
    if (me->_off_x > x) {
        oL = me->_off_x - x;
    }
    if (rect->right >= (me->_off_x + outWidth)) {
        oR = (rect->right + 1) - (me->_off_x + outWidth);
    }

    int_fast16_t line = (w - ( oL + oR )) * 3;
    dst_idx += oL + x - me->_off_x;
    src_idx += oL;
    do {
        memcpy(&dst[dst_idx * 3], &src[src_idx * 3], line);
        dst_idx += outWidth;
        src_idx += w;
    } while (--h);

    return 1;
}

// for 16bit color panel
uint32_t MainClass::jpgWrite16(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect) {
    MainClass* me = (MainClass*)jdec->device;

    uint16_t *dst = (uint16_t *)me->_dmabuf;

    uint_fast16_t x = rect->left;
    uint_fast16_t y = rect->top;
    uint_fast16_t w = rect->right + 1 - x;
    uint_fast16_t h = rect->bottom + 1 - y;
    uint_fast16_t outWidth = me->_out_width;
    uint_fast16_t outHeight = me->_out_height;
    uint8_t *src = (uint8_t*)bitmap;
    uint_fast16_t oL = 0, oR = 0;

    if (rect->right < me->_off_x)      return 1;
    if (x >= (me->_off_x + outWidth))  return 1;
    if (rect->bottom < me->_off_y)     return 1;
    if (y >= (me->_off_y + outHeight)) { me->_busy = false; return 0; } // No more rendering. [*1] => decomp failed 1 (Interrupted by output function.
        
    if (me->_off_y > y) {
        uint_fast16_t linesToSkip = me->_off_y - y;
        src += linesToSkip * w * 3;
        h -= linesToSkip;
    }

    if (me->_off_x > x) {
        oL = me->_off_x - x;
    }
    if (rect->right >= (me->_off_x + outWidth)) {
        oR = (rect->right + 1) - (me->_off_x + outWidth);
    }
    int_fast16_t line = (w - ( oL + oR ));
    dst += oL + x - me->_off_x;
    src += oL * 3;

    do {
        int i = 0;
        do {
#define ALGO (1)
#if ALGO == 0
            // Orignal
            uint_fast8_t r8 = src[i*3+0] & 0xF8;
            uint_fast8_t g8 = src[i*3+1];
            uint_fast8_t b5 = src[i*3+2] >> 3;
            r8 |= g8 >> 5;
            g8 &= 0x1C;
            b5 = (g8 << 3) + b5;
            dst[i] = r8 | b5 << 8;
#elif ALGO == 1
            // Modified (Just a little faster than the original)
            uint32_t r = src[i*3+0] & 0xF8;
            uint32_t g = src[i*3+1] >> 2;
            uint32_t b = src[i*3+2] >> 3;
            r +=  (g >> 3);
            b +=  (g << 5);
            dst[i] = r | b << 8;
#else
#error "No code"
#endif
        } while (++i != line);
        dst += outWidth;
        src += w * 3;
    } while (--h);
    return 1;
}

uint32_t MainClass::jpgWriteRow(TJpgD *jdec, uint32_t y, uint32_t h) {
    static int flip = 0;
    MainClass* me = (MainClass*)jdec->device;
    int_fast16_t oy = me->_off_y;
    int_fast16_t bottom = y + h;
    int_fast16_t yy = y;
    
    if(bottom < oy) { return 1; /* continue */ }
    if(y >= oy + me->_lcd_height) { me->_busy = false; return 0; /* cutoff [*1] */}

    //M5_LOGI("core:%u dma:%d y:%d, h:%d", xPortGetCoreID(), me->_lcd && me->_lcd->dmaBusy(), y, h);

    // Adjust transfer height if there is a positive offset in the y direction
    if(oy > 0)
    {
        yy = y - oy;
        if(oy > y && oy < bottom) { h -= oy % h; yy = 0; } // First block
        if(oy + me->_lcd_height > y && oy + me->_lcd_height < y + h) // Last block
        {
            h -= (y + h) - (me->_lcd_height + oy);
        } 
    }
    //M5_LOGI("oy:%d y:%d h:%d yy:%d", oy, y, h, yy);
    me->_lcd->pushImageDMA(me->_jpg_x,  me->_jpg_y + yy,
                           me->_out_width, h,
                           reinterpret_cast<::lgfx::swap565_t*>(me->_dmabuf));

    flip = !flip;
    me->_dmabuf = me->_dmabufs[flip];

    if(y + h >= (me->_off_y + me->_out_height)) { me->_busy = false; }
    return 1;
}

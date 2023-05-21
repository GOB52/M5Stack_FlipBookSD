/*!
  @file jpg_sprite.cpp
  @brief Class for drawing from JPEG file on memory to sprite derived from LGFX_Sprite
  @note Using multi-core
 */
#include "gob_jpg_sprite.hpp"

#pragma GCC optimize ("O3")

namespace
{
static void write888(uint8_t* dst, const uint8_t* src, uint32_t line, uint32_t outWidth, uint32_t w, uint32_t h)
{
    line *= 3;
    outWidth *= 3;
    w *= 3;
    do {
        memcpy(dst, src, line);
        src += w;
        dst += outWidth;
    } while (--h);
}
    
static void write565(uint8_t* dst, const uint8_t* src, uint32_t line, uint32_t outWidth, uint32_t w, uint32_t h)
{
    do
    {
        auto s = src;
        auto d = dst;
        uint32_t l = line;
        while (l--)
        {
            uint32_t r = s[0];
            uint32_t g = s[1];
            uint32_t b = s[2];
            s += 3;
            g = g >> 2;
            r = (r >> 3) & 0x1F;
            r = (r << 3) + (g >> 3);
            b = (b >> 3) + (g << 5);
            d[0] = r;
            d[1] = b;
            d += 2;
        }
        dst += outWidth * 2;
        src += w * 3;
    }
    while (--h);
}

static void write332(uint8_t* dst, const uint8_t* src, uint32_t line, uint32_t outWidth, uint32_t w, uint32_t h)
{
    do {
        auto s = src;
        auto d = dst;
        uint32_t l = line;
        while (l--) {
            uint32_t r = s[0];
            uint32_t g = s[1];
            uint32_t b = s[2];
            s += 3;
            r >>= 5;
            g >>= 5;
            b >>= 6;
            g += r << 3;
            b += g << 2;
            d[0] = b;
            ++d;
        }
        dst += outWidth;
        src += w * 3;
    } while (--h);
}
//
}

namespace gob
{

JpgSprite:: JpgSprite(LovyanGFX* parent) : LGFX_Sprite(parent)
{
    _jdec.multitask_begin();
    setup();
}

void JpgSprite::setup()
{
    _fp_write = nullptr;
    _bytesize = 0;
    switch (getColorDepth())
    {
    case lgfx::color_depth_t::rgb565_2Byte:
      _fp_write = write565;
      _bytesize = 2;
      break;
    case lgfx::color_depth_t::rgb888_3Byte:
      _fp_write = write888;
      _bytesize = 3;
      break;
    case lgfx::color_depth_t::rgb332_1Byte:
      _fp_write = write332;
      _bytesize = 1;
      break;
    default:
        //Serial.printf("Unsupport format\n");
        break;
    }
}

JpgSprite::~JpgSprite()
{
    _jdec.multitask_end();
}

bool JpgSprite::drawJpgEx(const uint8_t* buf, const int32_t len, const int32_t ox, const int32_t oy)
{
    if(!_fp_write)
    {
        //Serial.printf("fp_write NULL\n");
        return false;
    }
    _filebuf = buf;
    _fileindex = 0;
    _remain = len;

    TJpgD::JRESULT jres = _jdec.prepare(jpgRead, this);
    if (jres != TJpgD::JDR_OK)
    {
        Serial.printf("prepare failed! %d\r\n", jres);
        return false;
    }

    _out_width = std::min<int32_t>(_jdec.width, width());
    _out_height = std::min<int32_t>(_jdec.height, height());

    if(ox >= _jdec.width || oy >= _jdec.height
       || ox + _jdec.width < 0 || oy + _jdec.height < 0)
    {
        Serial.printf("Out of range d(%d,%d)\r\n", ox, oy);
        return false;

    }
    _off_x = ox;
    _off_y = oy;

    jres = _jdec.decomp_multitask(jpgWrite, jpgWriteRow);
    if (jres != TJpgD::JDR_OK)
    {
        Serial.printf("decomp failed! %d\r\n", jres); // 1 is terminated myself.
        return false;
    }
    return true;
}

uint32_t JpgSprite::jpgRead(TJpgD *jdec, uint8_t *buf, uint32_t len)
{
    JpgSprite* me = (JpgSprite*)jdec->device;
    if (len > me->_remain)  len = me->_remain;
    if (buf)
    {
        memcpy(buf, (const uint8_t *)me->_filebuf + me->_fileindex, len);
    }
    me->_fileindex += len;
    me->_remain -= len;
    return len;
}

uint32_t JpgSprite::jpgWrite(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect)
{
    JpgSprite* me = (JpgSprite*)jdec->device;

    int32_t x = rect->left;
    int32_t y = rect->top;
    int32_t w = rect->right + 1 - x;
    int32_t h = rect->bottom + 1 - y;
    int32_t dx = x + me->_off_x;
    int32_t dy = y + me->_off_y;
    int32_t sWidth = me->width();
    int32_t sHeight = me->height();

    if (dx >= sWidth) return 1;
    if (dy >= sHeight) return 1;
    if(dx + w <= 0) return 1;
    if(dy + h <= 0) return 1;
        
    uint8_t *src = (uint8_t*)bitmap;
    uint_fast16_t oL = 0, oR = 0;

    uint8_t* dst = (uint8_t*)me->_panel_sprite.getBuffer();
    dst += dy * (sWidth * me->_bytesize) + dx * me->_bytesize;

    if (-me->_off_y > y)
    {
        auto skip = -me->_off_y - y;
        src += skip * w * 3;
        dst += skip * (sWidth * me->_bytesize);
        h -= skip;
    }
    if(dy + h > sHeight)
    {
        h = sHeight - dy;
    }
    if (-me->_off_x > x)
    {
        oL = -me->_off_x - x;
    }
    if (dx + w > sWidth)
    {
        oR = w - (sWidth - dx);
    }
    int32_t line = (w - ( oL + oR ));

    dst += oL * me->_bytesize;
    src += oL * 3;
    me->_fp_write(dst, src, line, sWidth, w, h);
    return 1;
}
//
}

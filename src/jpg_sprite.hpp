#ifndef JPG_SPRITE_HPP
#define JPG_SPRITE_HPP

#include <M5GFX.h>
#include "tjpgdClass.h"

// LGFX_Sprite with jpg decode drawing function specialized for 16-bit sprite (Same as LCD format)
class JpgSprite : public LGFX_Sprite
{
  public:
    explicit JpgSprite(LovyanGFX* parent = nullptr);
    virtual ~JpgSprite();

    void setup();
    bool drawJpgEx(const uint8_t* buf, const int32_t len,
                   const int32_t ox = 0, const int32_t oy = 0);

  protected:
    static uint32_t jpgRead(TJpgD *jdec, uint8_t *buf, uint32_t len);
    static uint32_t jpgWrite(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect);
    static uint32_t jpgWriteRow(TJpgD *jdec, uint32_t y, uint32_t h)
    {
        JpgSprite* me = (JpgSprite*)jdec->device;
        return y < me->_out_height; // Cutoff if y is over the height
    }

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
    
  protected:
    TJpgD _jdec;

    int32_t _remain{};
    const uint8_t* _filebuf{};
    uint32_t _fileindex{};

    uint_fast8_t _bytesize{};
    int32_t _out_width{};
    int32_t _out_height{};
    int32_t _off_x{}, _off_y{};
    
    void (*_fp_write)(uint8_t* dst, const uint8_t* src, uint32_t line, uint32_t outWidth, uint32_t w, uint32_t h);
};

#endif

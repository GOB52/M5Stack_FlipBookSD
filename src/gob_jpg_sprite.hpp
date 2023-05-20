/*!
  @file jpg_sprite.hpp
  @brief Class for drawing from JPEG file on memory to sprite derived from LGFX_Sprite
  @note Using multi-core
 */
#ifndef GOB_JPG_SPRITE_HPP
#define GOB_JPG_SPRITE_HPP

#include <M5GFX.h>
#include "tjpgdClass.h"

namespace gob
{

class JpgSprite : public LGFX_Sprite
{
  public:
    explicit JpgSprite(LovyanGFX* parent = nullptr);
    virtual ~JpgSprite();

    /*!
      @brief Setup for JPEG decode
      @warning After calling a function that changes Depth, you need to call this function.
      @warning e.g) setColorepth()
     */
    void setup();

    /*!
      @brief drawJpeg to self buffer
      @param buf Buffer of JPEG
      @param len Length of buffer
      @param ox Horizontal drawing offset
      @param oy Vertical drawing offset
      @retval ==true Success
      @retval ==false Failure
     */
    bool drawJpgEx(const uint8_t* buf, const int32_t len,
                   const int32_t ox = 0, const int32_t oy = 0);

  protected:
    static uint32_t jpgRead(TJpgD *jdec, uint8_t *buf, uint32_t len);
    static uint32_t jpgWrite(TJpgD *jdec, void *bitmap, TJpgD::JRECT *rect);
    static uint32_t jpgWriteRow(TJpgD *jdec, uint32_t y, uint32_t h) { return 1; }
    
  protected:
    TJpgD _jdec{};

    int32_t _remain{};
    const uint8_t* _filebuf{};
    uint32_t _fileindex{};

    uint_fast8_t _bytesize{};
    int32_t _out_width{};
    int32_t _out_height{};
    int32_t _off_x{}, _off_y{};

    using FpWrite = void(*)(uint8_t* dst, const uint8_t* src, uint32_t line, uint32_t outWidth, uint32_t w, uint32_t h);
    FpWrite _fp_write{};
};
//
}
#endif

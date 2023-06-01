/*!
  @file gob_gcf.hpp
  @brief GCF (Gob Combined File) format
*/
#ifndef GOB_GCF_HPP
#define GOB_GCF_HPP

#include <cstdint>

namespace gob
{
/*!
  @struct GCFHeader
  @brief Header of the GCF file
 */
struct __attribute__((packed)) GCFHeader
{
    static constexpr uint32_t Signature = 0x30464347;

    uint32_t signature{}; //!< @brief Signature "GCF0" 0x30464347
    uint32_t blocks{};    //!< @brief  Number of the GCF blocks
    uint32_t reserved[2];
};

/*!
  @struct GCFBlock
  @brief Data block
 */
struct __attribute__((packed)) GCFBlock
{
    static constexpr uint32_t Terminator = 0xFFFFFFFF; //!< @brief Terminator

    uint32_t size{Terminator};  //!< @brief Size of data
    //uint8_t data[1];            //!< @brief Data
};
//
}
#endif

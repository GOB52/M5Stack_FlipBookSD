/*!
  @file gob_gmv.hpp
  @brief GMV (Gob MoVie file) format
*/
#ifndef GOB_GMV_HPP
#define GOB_GMV_HPP

#include <cstdint>

namespace gob
{

struct __attribute__((packed)) wav_header_t
{
    char RIFF[4]; // "RIFF"
    uint32_t chunk_size;
    char WAVEfmt[8]; // "WAVEfmt "
    uint32_t fmt_chunk_size; // 16 if linear PCM
    uint16_t audiofmt; // 1 is linear PCM
    uint16_t channel;
    uint32_t sample_rate;
    uint32_t byte_per_sec;
    uint16_t block_size;
    uint16_t bit_per_sample;
};

struct __attribute__((packed)) sub_chunk_t
{
    char identifier[4];
    uint32_t chunk_size;
    uint8_t data[1];
};

/*!
  @struct GMVHeader
  @brief Header of the GMV file
 */
struct __attribute__((packed)) GMVHeader
{
    static constexpr uint32_t Signature = 0x30564d47;

    uint32_t signature{}; //!< @brief Signature "GMV0" 0x30564d47
    uint32_t blocks{};    //!< @brief Number of the data blocks
    uint32_t gcfOffset{}; //!< @brief data block Offset from the beginning of the file
    float fps{};          //!< @brief Frame per second
    wav_header_t wavHeader{}; //!< @brief wav header
};

/*!
  @struct GMVBlock
  @brief Data block
 */
struct __attribute__((packed)) GMVBlock
{
    static constexpr uint32_t Terminator = 0xFFFFFFFF; //!< @brief Terminator
    uint32_t sizes[2]; //!< @brief [0]:Size of image [1]:Size of wav
    // uint8_t data[1];   //!< @brief data[sizes[0]] + data[sizes[1]]  (Data lebgth is sizes[0] + sizes[1])
};

//
}
#endif

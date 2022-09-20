#ifndef TinyFrame_CRC_HPP
#define TinyFrame_CRC_HPP

#include "TinyFrame_Types.hpp"

namespace TinyFrame_n{
//region Checksums

// Custom checksum functions

/**
 * Initialize a checksum
 *
 * @return initial checksum value
 */
template<CKSUM_t CKSUM_TYPE>
CKSUM<CKSUM_TYPE> CksumStart(void);

/**
 * Update a checksum with a byte
 *
 * @param cksum - previous checksum value
 * @param byte - byte to add
 * @return updated checksum value
 */
template<CKSUM_t CKSUM_TYPE>
CKSUM<CKSUM_TYPE> CksumAdd(CKSUM<CKSUM_TYPE> cksum, uint8_t byte);

/**
 * Finalize the checksum calculation
 *
 * @param cksum - previous checksum value
 * @return final checksum value
 */
template<CKSUM_t CKSUM_TYPE>
CKSUM<CKSUM_TYPE> CksumEnd(CKSUM<CKSUM_TYPE> cksum);

#define CKSUM_RESET(cksum)     do { (cksum) = CksumStart<CKSUM_TYPE>(); } while (0)
#define CKSUM_ADD(cksum, byte) do { (cksum) = CksumAdd<CKSUM_TYPE>((cksum), (byte)); } while (0)
#define CKSUM_FINALIZE(cksum)  do { (cksum) = CksumEnd<CKSUM_TYPE>((cksum)); } while (0)

static constexpr uint8_t bit_reflect_lut[16U] = {
    /* OUT,    IN */
    0b0000, // 0b0000
    0b1000, // 0b0001
    0b0100, // 0b0010
    0b1100, // 0b0011
    0b0010, // 0b0100
    0b1010, // 0b0101
    0b0110, // 0b0110
    0b1110, // 0b0111
    0b0001, // 0b1000
    0b1001, // 0b1001
    0b0101, // 0b1010
    0b1101, // 0b1011
    0b0011, // 0b1100
    0b1011, // 0b1101
    0b0111, // 0b1110
    0b1111, // 0b1111
    };

static constexpr uint8_t reflect_bits(uint8_t byte) {
   return (bit_reflect_lut[byte & 0b1111] << 4) | (bit_reflect_lut[byte >> 4]);
}

static inline void reflect_bytes(uint8_t* p_start, size_t size, bool bit_reflect = false){
    uint8_t* p_end = p_start + size - 1;
    uint8_t temp = 0;
    while(p_start <= p_end){
        temp = *p_start;
        if(bit_reflect){
            *p_start = reflect_bits(*p_end);
            *p_end = reflect_bits(temp);
        }else{
            *p_start = *p_end;
            *p_end = temp;
        }
        p_start++;
        p_end--;
    }
}

/* Tableless CRC */
template <typename DATA_TYPE>
constexpr static DATA_TYPE TablelessCrc_Function(DATA_TYPE poly, DATA_TYPE inital_crc, uint8_t byte)
{

    /* extract relevant byte references */
    DATA_TYPE msbMask = static_cast<DATA_TYPE>(1U << ((sizeof(DATA_TYPE) * 8U) - 1));

    /* init crc byte */
    inital_crc = inital_crc ^ (byte << ((sizeof(DATA_TYPE) - 1) * 8U));

    /* calculate byte CRC using XOR */
    for (uint8_t bitIndex = 0U; bitIndex < 8U; bitIndex++)
    {
        if ((inital_crc & msbMask) != 0U)
        {
            inital_crc = (inital_crc << 1U) ^ poly;
        }
        else
        {
            inital_crc = (inital_crc << 1U);
        }
    }

    /* return modified crc */
    return inital_crc; 
}

//endregion
}

#endif // TinyFrame_CRC_HPP
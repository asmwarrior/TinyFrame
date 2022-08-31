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

/* Tableless CRC */

template<typename DATA_TYPE>
DATA_TYPE TablelessCrc_Function(DATA_TYPE poly, DATA_TYPE inital_value, uint8_t byte, uint8_t byte_index) {

  /* setup local variables */
  constexpr DATA_TYPE msbMask = static_cast<DATA_TYPE>(1 << ((sizeof(DATA_TYPE) * 8U) - 1));
  DATA_TYPE crc = inital_value ^ (byte << (byte_index * 8U));

  /* actual CRC calulation using XOR */
  for (uint8_t bitIndex = byte_index * 8U; bitIndex < ((byte_index + 1U) * 8U); bitIndex++) {
    if (crc & msbMask) {
      crc = static_cast<DATA_TYPE>((crc << 1U) ^ poly);
    } else {
      crc = static_cast<DATA_TYPE>(crc << 1U);
    }
  }
  return crc;
}

//endregion
}

#endif // TinyFrame_CRC_HPP
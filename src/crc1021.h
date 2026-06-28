#ifndef __CRC1021_H__
#define __CRC1021_H__

#include <stdint.h>

uint16_t crc1021(uint16_t CRC, uint8_t Byte);
uint16_t crc1021(uint16_t CRC, const uint8_t *Data, int Size);
uint16_t crc1021(uint16_t CRC, const char *Data);

#endif // __CRC1021_H__


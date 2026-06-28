#include "crc1021.h"

uint16_t crc1021(uint16_t CRC, uint8_t Byte)
{ uint16_t X;
  X = ((CRC>>8) ^ Byte) & 0xFF;
  X ^= X>>4;
  CRC = (CRC<<8) ^ (X<<12) ^ (X<<5) ^ X;
  return CRC; }

uint16_t crc1021(uint16_t CRC, const uint8_t *Data, int Size)
{ for(int Idx=0; Idx<Size; Idx++)
    CRC = crc1021(CRC, Data[Idx]);
  return CRC; }

uint16_t crc1021(uint16_t CRC, const char *Str)
{ for(int Idx=0; ; Idx++)
  { uint8_t Byte=Str[Idx]; if(Byte==0) break;
    CRC = crc1021(CRC, Byte); }
  return CRC; }


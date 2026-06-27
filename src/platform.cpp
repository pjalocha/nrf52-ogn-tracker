#include "main.h"

// FlashParameters Parameters;

uint8_t PowerMode = 2;
Word32x2 Random = { .Word = 0x123456789ABCDEF0ULL };
HardItems HardwareStatus = { .Flags = 0 };

/*
static uint32_t gps_baud_rate = 115200;

int CONS_UART_Read(uint8_t &Byte)
{ if (!Serial.available()) return 0;
  Byte = (uint8_t)Serial.read(); return 1; }

void CONS_UART_Write(char Byte)
{ Serial.write((uint8_t)Byte); }

int CONS_UART_Free(void)
{ return Serial.availableForWrite(); }

void GPS_UART_Init(uint32_t BaudRate)
{ gps_baud_rate = BaudRate;
  // Serial1.end();
  Serial1.setPins(GPS_PinRx, GPS_PinTx);
  Serial1.begin(gps_baud_rate);
  // HardwareStatus.GPS=1;
}

int GPS_UART_Full(void)
{ return Serial1.available(); }

int GPS_UART_Read(uint8_t &Byte)
{ if (!Serial1.available()) return 0;
  Byte = (uint8_t)Serial1.read(); return 1; }

int GPS_UART_Read(uint8_t *Data, int Max)
{ int Count=0;
  while (Count<Max && Serial1.available())
  { Data[Count++] = (uint8_t)Serial1.read(); }
  return Count; }

void GPS_UART_Write(char Byte)
{ Serial1.write((uint8_t)Byte); }

void GPS_UART_Flush(int MaxWait)
{ uint32_t start = millis();
  Serial1.flush();
  while (Serial1.available() && ((int)(millis() - start) < MaxWait))
  { (void)Serial1.read(); }
}

void GPS_UART_SetBaudrate(int BaudRate)
{ gps_baud_rate = BaudRate;
  Serial1.end();
  Serial1.setPins(GPS_PinRx, GPS_PinTx);
  Serial1.begin(gps_baud_rate); }

bool GPS_PPS_isOn(void)
{
#if defined(GPS_PinPPS)
  if(GPS_PinPPS<0) return false;
  return digitalRead(GPS_PinPPS) != LOW;
#else
  return false;
#endif
}
*/

void LED_PCB_On(bool ON)
{ digitalWrite(LED_Pin, ON ? LED_StateOn : LED_StateOff); }

void LED_PCB_Off(void)
{ LED_PCB_On(false); }

void LED_PCB_Flash(uint8_t Time)
{ LED_PCB_On(true);
  if(Time>10) Time=10;
  delay(Time);
  LED_PCB_On(false); }

uint16_t BatterySense(int Samples)
{ (void)Samples;
  return 0; }

uint16_t BatterySenseRaw(int Samples)
{ (void)Samples;
  return 0; }

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout, bool LogOnly)
{
  (void)Timestamp;
  (void)msTimeout;
  (void)LogOnly;
  if (LogOnly || !Line || LineLen <= 0) {
    return;
  }
  Serial.write((const uint8_t *)Line, LineLen);
}

void SysLog_Line(const char *Line, bool Timestamp, int msTimeout, bool LogOnly)
{ if (!Line) return;
  SysLog_Line(Line, (int)strlen(Line), Timestamp, msTimeout, LogOnly); }

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout)
{ SysLog_Line(Line, LineLen, Timestamp, msTimeout, false); }

void SysLog_Line(const char *Line, bool Timestamp, int msTimeout)
{ SysLog_Line(Line, Timestamp, msTimeout, false); }

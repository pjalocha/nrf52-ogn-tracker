#pragma once

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <rtos.h>
#include <event_groups.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WITH_OGN1                          // OGN protocol version 1
#define OGN_Packet OGN1_Packet

#define DEFAULT_AcftType        1         // [0..15] default aircraft-type: glider
#define DEFAULT_GeoidSepar     40         // [m]
#define DEFAULT_CONbaud    115200         // [bps]
#define DEFAULT_PPSdelay      100         // [ms]
#define DEFAULT_FreqPlan        0

#include "ogn.h"

uint64_t getUniqueMAC(void);
uint64_t getUniqueID(void);
uint32_t getUniqueAddress(void);

#include "parameters.h"

#if defined(WITH_WIO_TRACKER)
#include "wio-tracker-pins.h"
#elif defined(WITH_T_ECHO)
#include "t-echo-pins.h"
#else
#error "No board pin definition selected"
#endif

#ifndef HARD_NAME
#define HARD_NAME "nRF52"
#endif

#ifndef SOFT_NAME
#define SOFT_NAME "nRF52-OGN"
#endif

// #define WITH_OGN1
// #define OGN_Packet OGN1_Packet

// #define DEFAULT_GeoidSepar 40
// #define DEFAULT_PPSdelay 100

// #ifndef ESP_OK
// #define ESP_OK 0
// #endif
// typedef int esp_err_t;

/*
class FlashParameters
{
public:
  uint8_t AddrType = 1;
  uint32_t Address = 0x123456;
  uint8_t NavRate = 1;
  uint8_t NavMode = 6;
  uint8_t GNSS = 0;
  uint8_t EnableGPS = 1;
  uint8_t EnableGLO = 1;
  uint8_t EnableGAL = 1;
  uint8_t EnableBEI = 1;
  uint8_t EnableSBAS = 1;
  int16_t TimeCorr = 0;
  int16_t PPSdelay = DEFAULT_PPSdelay;
  int16_t GeoidSepar = DEFAULT_GeoidSepar * 10;
  uint8_t manGeoidSepar = 0;
  uint8_t Verbose = 1;

  int WritePOGNS(char *line) const
  {
    return sprintf(line, "$POGNS,Address=%06lX,AddrType=%u\r\n",
                   (unsigned long)Address, (unsigned)AddrType);
  }
};
*/

extern FlashParameters Parameters;

extern SemaphoreHandle_t CONS_Mutex;
extern SemaphoreHandle_t I2C_Mutex;
// extern SemaphoreHandle_t WIFI_Mutex;

extern uint8_t PowerMode;

typedef union
{
  uint64_t Word;
  struct
  {
    uint32_t RX;
    uint32_t GPS;
  };
} Word32x2;

extern Word32x2 Random;

typedef union
{
  uint32_t Flags;
  struct
  {
    bool AXP192 : 1;
    bool AXP202 : 1;
    bool AXP210 : 1;
    bool BMP280 : 1;
    bool BME280 : 1;
    bool Magn : 1;
    bool IMU : 1;
    bool Radio : 1;
    bool GPS : 1;
    bool SPIFFS : 1;
  };
} HardItems;

extern HardItems HardwareStatus;

inline void XorShift32(uint32_t &word)
{ word ^= word << 13;
  word ^= word >> 17;
  word ^= word << 5; }

int CONS_UART_Read(uint8_t &Byte);
void CONS_UART_Write(char Byte);
int CONS_UART_Free(void);

int GPS_UART_Full(void);
int GPS_UART_Read(uint8_t &Byte);
int GPS_UART_Read(uint8_t *Data, int Max);
void GPS_UART_Write(char Byte);
void GPS_UART_Flush(int MaxWait);
void GPS_UART_SetBaudrate(int BaudRate);
void GPS_UART_Init(uint32_t BaudRate = 115200);

bool GPS_PPS_isOn(void);
#ifdef GPS_PinEna
void GPS_ENABLE(void);
void GPS_DISABLE(void);
#endif

void LED_PCB_On(bool ON = 1);
void LED_PCB_Off(void);
void LED_PCB_Flash(uint8_t Time = 100);

uint16_t BatterySense(int Samples = 4);
uint16_t BatterySenseRaw(int Samples = 4);

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout, bool LogOnly);
void SysLog_Line(const char *Line, bool Timestamp, int msTimeout, bool LogOnly);
void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout);
void SysLog_Line(const char *Line, bool Timestamp, int msTimeout);

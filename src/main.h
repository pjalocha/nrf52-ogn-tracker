#pragma once

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <rtos.h>
#include <event_groups.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const uint8_t KNOB_Tick = 15;
#include "play.h"

#ifndef VERSION
#define VERSION "0.1.27"
#endif

#ifndef SOFT_NAME
#define SOFT_NAME "OGNv" VERSION
#endif

#define HARDWARE_ID 0x05
#define SOFTWARE_ID 0x01

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

bool CONS_UART_isConnected(void);
int  CONS_UART_Read(uint8_t &Byte);
void CONS_UART_Write(char Byte);
int  CONS_UART_Free(void);

int GPS_UART_Full(void);
int GPS_UART_Read(uint8_t &Byte);
int GPS_UART_Read(uint8_t *Data, int Max);
void GPS_UART_Write(char Byte);
void GPS_UART_Flush(int MaxWait);
void GPS_UART_SetBaudrate(int BaudRate);
void GPS_UART_Init(uint32_t BaudRate = 115200);


bool GPS_PPS_isOn(void);
#ifdef GPS_PinPPS
extern uint32_t PPS_Intr_usTime;   // [us] micros() counter at the time of the PPS
extern uint32_t PPS_Intr_msTime;   // [ms] millis() counter at the time of the PPS

extern uint32_t PPS_usPrecTime;
extern uint32_t PPS_usTimeRMS;

extern uint32_t PPS_Intr_usFirst;  // [us] the time of the first interrupt in a series
extern uint32_t PPS_Intr_Count;    // [count] of good PPS interrupts in the series
extern uint32_t PPS_Intr_Missed;   // [count] of missed PPS interrupts

extern  int32_t PPS_usPeriodErr;   // [1/16us] PPS period systematic error
extern uint32_t PPS_usPeriodRMS;   // [ ]

int PPS_Print(char *Line);
#endif

#ifdef GPS_PinEna
void GPS_ENABLE(void);
void GPS_DISABLE(void);
#endif

void LED_OGN_RX(uint8_t ms);
void LED_OGN_TX(uint8_t ms);

void LED_PCB_On(bool ON = 1);
void LED_PCB_Off(void);
void LED_PCB_Flash(uint8_t Time = 100);

uint16_t BatterySense(int Samples = 4);
uint16_t BatterySenseRaw(int Samples = 4);

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout, bool LogOnly);
void SysLog_Line(const char *Line, bool Timestamp, int msTimeout, bool LogOnly);
void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout);
void SysLog_Line(const char *Line, bool Timestamp, int msTimeout);

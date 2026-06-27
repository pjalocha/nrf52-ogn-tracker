#pragma once

#if !defined(_PINNUM)
#define _PINNUM(port, pin) ((port) * 32 + (pin))
#endif

#define Pin_NotUsed   (-1)

// User input
#define Button_Pin    _PINNUM(0, 8)  // active low

// Trackball / joystick
#define Trackball_PinUp     _PINNUM(1, 4)
#define Trackball_PinDown   _PINNUM(0, 12)
#define Trackball_PinLeft   _PINNUM(0, 11)
#define Trackball_PinRight  _PINNUM(1, 3)
#define Trackball_PinPress  _PINNUM(1, 5)

// L76K GNSS
#define GPS_UART      Serial1
#define GPS_PinTx     _PINNUM(0, 27) // nRF TX -> GNSS RX
#define GPS_PinRx     _PINNUM(0, 26) // nRF RX <- GNSS TX
#define GPS_PinPPS    Pin_NotUsed
#define GPS_PinWake   _PINNUM(1,  9) // D0, GNSS standby/wake
#define GPS_PinReset  Pin_NotUsed

// SX1262 RF chip
#define Radio_PinRST  _PINNUM(1,  7) // reset
#define Radio_PinSCK  _PINNUM(0, 30) // SCK
#define Radio_PinMOSI _PINNUM(0, 28) // MOSI
#define Radio_PinMISO _PINNUM(0,  3) // MISO
#define Radio_PinCS   _PINNUM(1, 14) // CS/NSS
#define Radio_PinIRQ1 _PINNUM(0,  7) // DIO1/IRQ
#define Radio_PinBusy _PINNUM(1, 10) // BUSY
#define Radio_PinRXEN _PINNUM(1,  8) // RF switch RX enable
#define Radio_PinTXEN Pin_NotUsed

// I2C / OLED
#define I2C_PinSDA    _PINNUM(0, 6) // SDA
#define I2C_PinSCL    _PINNUM(0, 5) // SCL
#define OLED_PinRST   Pin_NotUsed
#define OLED_Model    "SH1106"

// UI LED
#define LED_Pin        _PINNUM(1, 1) // green LED, active high
#define LED_StateOn    HIGH
#define LED_StateOff   LOW

// Battery ADC and enable
#define Battery_Enable_Pin _PINNUM(0, 4)
#define Battery_Pin        _PINNUM(0, 31) // AIN7
#define Battery_Divider    2.0f

// Buzzer
#define Buzzer_Pin     _PINNUM(1, 0)

// External P25Q16H QSPI flash
#define Flash_PinSCK   _PINNUM(0, 21)
#define Flash_PinCS    _PINNUM(0, 25)
#define Flash_PinMOSI  _PINNUM(0, 20) // QSPI IO0
#define Flash_PinMISO  _PINNUM(0, 24) // QSPI IO1
#define Flash_PinWP    _PINNUM(0, 22) // QSPI IO2
#define Flash_PinHOLD  _PINNUM(0, 23) // QSPI IO3

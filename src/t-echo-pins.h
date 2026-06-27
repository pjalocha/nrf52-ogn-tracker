#pragma once

#if !defined(_PINNUM)
#define _PINNUM(port, pin) ((port) * 32 + (pin))
#endif

#define Pin_NotUsed   (-1)

// User input
#define Button_Pin    _PINNUM(1, 10) // user button: LOW when pushed
#define Touch_Pin     _PINNUM(0, 11) // capacitive pad / touch input

// GPS / GNSS
#define GPS_UART      Serial1
#define GPS_PinTx     _PINNUM(1,  8) // nRF TX -> GNSS RX
#define GPS_PinRx     _PINNUM(1,  9) // nRF RX <- GNSS TX
#define GPS_PinPPS    _PINNUM(1,  4) // PPS
#define GPS_PinWake   _PINNUM(1,  2) // wake/standby
#define GPS_PinReset  _PINNUM(1,  5) // reset, present on later revisions

// SX1262 RF chip
#define Radio_PinRST  _PINNUM(0, 25) // reset
#define Radio_PinSCK  _PINNUM(0, 19) // SCK
#define Radio_PinMOSI _PINNUM(0, 22) // MOSI
#define Radio_PinMISO _PINNUM(0, 23) // MISO
#define Radio_PinCS   _PINNUM(0, 24) // CS/NSS
#define Radio_PinIRQ  _PINNUM(0, 20) // DIO1/IRQ
#define Radio_PinBusy _PINNUM(0, 17) // BUSY
#define Radio_PinDIO0_REV0 Pin_NotUsed
#define Radio_PinDIO0_REV1 _PINNUM(1,  1)
#define Radio_PinDIO0_REV2 _PINNUM(0, 15)

// I2C
#define I2C_PinSDA    _PINNUM(0, 26) // SDA
#define I2C_PinSCL    _PINNUM(0, 27) // SCL

// E-paper display
#define EPD_PinMISO   _PINNUM(1,  7)
#define EPD_PinMOSI   _PINNUM(0, 29)
#define EPD_PinSCK    _PINNUM(0, 31)
#define EPD_PinCS     _PINNUM(0, 30)
#define EPD_PinDC     _PINNUM(0, 28)
#define EPD_PinRST    _PINNUM(0,  2)
#define EPD_PinBusy   _PINNUM(0,  3)
#define EPD_PinLight  _PINNUM(1, 11)

// Power enables
#define IO_Power_Pin  _PINNUM(0, 12) // EPD/RGB/CN1, and GNSS/sensor on rev2
#define RF_Power_Pin  _PINNUM(0, 13) // RF 3V3 on rev2

// RGB LED pins differ by T-Echo revision. Rev2 is the common current mapping.
#define LED_REV0_Green _PINNUM(0, 13)
#define LED_REV0_Red   _PINNUM(0, 14)
#define LED_REV0_Blue  _PINNUM(0, 15)
#define LED_REV1_Green _PINNUM(0, 15)
#define LED_REV1_Red   _PINNUM(0, 13)
#define LED_REV1_Blue  _PINNUM(0, 14)
#define LED_REV2_Green _PINNUM(1,  1)
#define LED_REV2_Red   _PINNUM(1,  3)
#define LED_REV2_Blue  _PINNUM(0, 14)

#define LED_Pin        LED_REV2_Red
#define LED_StateOn    LOW
#define LED_StateOff   HIGH

// Battery ADC
#define Battery_Pin    _PINNUM(0, 4) // AIN2, through 100k + 100k divider

// External SPI flash
#define Flash_PinMOSI  _PINNUM(1, 12)
#define Flash_PinMISO  _PINNUM(1, 13)
#define Flash_PinSCK   _PINNUM(1, 14)
#define Flash_PinCS    _PINNUM(1, 15)
#define Flash_PinHOLD  _PINNUM(0,  5)
#define Flash_PinWP    _PINNUM(0,  7)

// Optional T-Echo Plus peripherals
#define Motor_Pin      _PINNUM(0,  8)
#define Buzzer_Pin     _PINNUM(0,  6)

#pragma once

#if !defined(_PINNUM)
#define _PINNUM(port, pin) ((port) * 32 + (pin))
#endif

#define Pin_NotUsed   (-1)

// L76K GNSS
#define GPS_UART      Serial1
#define GPS_PinTx     _PINNUM(1, 11) // D6, nRF TX -> GNSS RX
#define GPS_PinRx     _PINNUM(1, 12) // D7, nRF RX <- GNSS TX
#define GPS_PinPPS    Pin_NotUsed
#define GPS_PinEna    _PINNUM(0,  2) // D0, GNSS standby/wake
#define GPS_PinReset  Pin_NotUsed

// if we want PPS but not Enable
// #define GPS_PinPPS    _PINNUM(0,  2) // D0, PPS interrupt
// #define GPS_PinEna    Pin_NotUsed

// SX1262 RF chip
#define Radio_PinRST  _PINNUM(0, 28) // D2, RESET
#define Radio_PinSCK  _PINNUM(1, 13) // D8, SCK
#define Radio_PinMOSI _PINNUM(1, 15) // D10, MOSI
#define Radio_PinMISO _PINNUM(1, 14) // D9, MISO
#define Radio_PinCS   _PINNUM(0,  4) // D4, CS/NSS
#define Radio_PinIRQ1 _PINNUM(0,  3) // D1, DIO1/IRQ
#define Radio_PinBusy _PINNUM(0, 29) // D3, BUSY
#define Radio_PinRXEN _PINNUM(0,  5) // D5, RF switch RX enable
#define Radio_PinTXEN Pin_NotUsed
#define Radio_DIO2AsRfSwitch         // SX1262 DIO2 controls the RF switch
#define Radio_TCXO_Voltage 1.8 // SX1262 DIO3 TCXO voltage

// UI LED
#define LED_PinRed     _PINNUM(0, 26) //
#define LED_PinGreen   _PINNUM(0, 30) //
#define LED_PinBlue    _PINNUM(0, 06) //
#define LED_StateOn    LOW            // active low
#define LED_StateOff   HIGH

#define LED_Pin LED_PinRed

// Battery ADC and enable
#define Battery_PinADC   _PINNUM(0, 31) // AIN7_BAT, battery voltage ADC
#define Battery_PinEna   _PINNUM(0, 14) // READ_BAT_ENABLE - active LOW !
#define Battery_Divider    2.0f

// External (P25Q16H ?) QSPI flash
#define Flash_PinSCK   _PINNUM(0, 21)
#define Flash_PinCS    _PINNUM(0, 25)
#define Flash_PinMOSI  _PINNUM(0, 20) // QSPI IO0
#define Flash_PinMISO  _PINNUM(0, 24) // QSPI IO1
#define Flash_PinWP    _PINNUM(0, 22) // QSPI IO2
#define Flash_PinHOLD  _PINNUM(0, 23) // QSPI IO3

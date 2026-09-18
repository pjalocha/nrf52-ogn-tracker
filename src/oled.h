#pragma once

#ifdef WITH_OLED
#include <U8g2lib.h>

#ifdef WITH_BIGOLED
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C OLED;
#else
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C OLED;
#endif
extern uint8_t OLED_Rotate;

void OLED_ButtonSingle(void);
#if defined(WITH_WIO_TRACKER)
void OLED_ButtonLong(void);
#endif
void OLED_Task(void *Parms);
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
void OLED_MenuButtonClick(void);
void OLED_MenuButtonLong(void);
#endif

void OLED_DrawLogo     (u8g2_t *OLED, const GPS_Position *GPS=0);  // draw logo and hardware options in software
void OLED_DrawStatusBar(u8g2_t *OLED, const GPS_Position *GPS=0);  // status bar on top of the OLED
void OLED_DrawGPS      (u8g2_t *OLED, const GPS_Position *GPS=0);  // GNSS time, position, altitude
void OLED_DrawSatSNR   (u8g2_t *OLED, const GPS_Position *GPS=0);  // GNSS SNR
void OLED_DrawID       (u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawBaro     (u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawRF       (u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawRFcounts (u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawRelayOGN (u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawRelayADSL(u8g2_t *OLED, const GPS_Position *GPS=0);
void OLED_DrawPower    (u8g2_t *OLED, const GPS_Position *GPS=0);
#ifdef WITH_LOOKOUT
void OLED_DrawLookOut  (u8g2_t *OLED, const GPS_Position *GPS=0);
#endif
void OLED_DrawCompass  (u8g2_t *OLED, const GPS_Position *GPS=0);

#endif // WITH_OLED

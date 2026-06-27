#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#if defined(WITH_WIO_TRACKER)
#include "wio-tracker-pins.h"
#elif defined(WITH_T_ECHO)
#include "t-echo-pins.h"
#else
#error "No board pin definition selected"
#endif

#if !defined(HARD_NAME)
#define HARD_NAME "nRF52"
#endif

static uint32_t last_report_ms = 0;
static uint32_t tick_count = 0;

static void print_pin_check()
{
  Serial.println("Pins:");

  Serial.print("  Button/LED: ");
  Serial.print(Button_Pin);
  Serial.print('/');
  Serial.println(LED_Pin);

  Serial.print("  GPS RX/TX/PPS: ");
  Serial.print(GPS_PinRx);
  Serial.print('/');
  Serial.print(GPS_PinTx);
  Serial.print('/');
  Serial.println(GPS_PinPPS);

  Serial.print("  Radio CS/SCK/MOSI/MISO/RST/IRQ/BUSY: ");
  Serial.print(Radio_PinCS);
  Serial.print('/');
  Serial.print(Radio_PinSCK);
  Serial.print('/');
  Serial.print(Radio_PinMOSI);
  Serial.print('/');
  Serial.print(Radio_PinMISO);
  Serial.print('/');
  Serial.print(Radio_PinRST);
  Serial.print('/');
  Serial.print(Radio_PinIRQ);
  Serial.print('/');
  Serial.println(Radio_PinBusy);

#if defined(Radio_PinRXEN)
  Serial.print("  Radio RXEN/TXEN: ");
  Serial.print(Radio_PinRXEN);
  Serial.print('/');
  Serial.println(Radio_PinTXEN);
#endif

  Serial.print("  I2C SDA/SCL: ");
  Serial.print(I2C_PinSDA);
  Serial.print('/');
  Serial.println(I2C_PinSCL);

#if defined(OLED_Model)
  Serial.print("  OLED: ");
  Serial.print(OLED_Model);
  Serial.print(" RST=");
  Serial.println(OLED_PinRST);
#endif

#if defined(Battery_Enable_Pin)
  Serial.print("  Battery EN/ADC: ");
  Serial.print(Battery_Enable_Pin);
  Serial.print('/');
  Serial.println(Battery_Pin);
#endif

#if defined(Buzzer_Pin)
  Serial.print("  Buzzer: ");
  Serial.println(Buzzer_Pin);
#endif
}

void setup()
{
  pinMode(LED_Pin, OUTPUT);
  digitalWrite(LED_Pin, LED_StateOff);

#if defined(Battery_Enable_Pin)
  pinMode(Battery_Enable_Pin, OUTPUT);
  digitalWrite(Battery_Enable_Pin, HIGH);
#endif

  Serial.begin(115200);

  uint32_t start_ms = millis();
  while (!Serial && (millis() - start_ms) < 5000) {
    delay(10);
  }

  Serial.println();
  Serial.print("nrf52-ogn-tracker ");
  Serial.print(HARD_NAME);
  Serial.println(" console check");
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);
  print_pin_check();
}

void loop()
{
  uint32_t now_ms = millis();

  if ((now_ms - last_report_ms) >= 1000) {
    last_report_ms = now_ms;
    tick_count++;

    digitalWrite(LED_Pin, (tick_count & 1) ? LED_StateOn : LED_StateOff);

    Serial.print(HARD_NAME);
    Serial.print(" alive ");
    Serial.print(tick_count);
    Serial.print(" uptime_ms=");
    Serial.println(now_ms);
  }
}

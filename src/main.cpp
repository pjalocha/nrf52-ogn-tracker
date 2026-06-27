#include "main.h"
#include "gps.h"
#include "ogn-radio.h"

SemaphoreHandle_t CONS_Mutex;
SemaphoreHandle_t I2C_Mutex;
// SemaphoreHandle_t WIFI_Mutex;

// =======================================================================================================

uint32_t RxProc_Count[8];  // temperary, the real is in PROC task

void LED_OGN_RX(uint8_t ms) { }
void LED_OGN_TX(uint8_t ms) { }

// =======================================================================================================

uint64_t getUniqueMAC(void) { uint64_t MAC = ((uint64_t)NRF_FICR->DEVICEID[1] << 32) | NRF_FICR->DEVICEID[0]; return MAC; }

uint64_t getUniqueID(void)      { return getUniqueMAC(); }         // get unique serial ID of the CPU/chip
uint32_t getUniqueAddress(void) { return getUniqueMAC()&0x00FFFFFF; } // get unique OGN address

// =======================================================================================================

FlashParameters Parameters;  // parameters stored in Flash: address, aircraft type, etc.

// =======================================================================================================

int CONS_UART_Read(uint8_t &Byte)
{ if (!Serial.available()) return 0;
  Byte = (uint8_t)Serial.read(); return 1; }

void CONS_UART_Write(char Byte)
{ Serial.write((uint8_t)Byte); }

int CONS_UART_Free(void)
{ return Serial.availableForWrite(); }

// =======================================================================================================

// static uint32_t gps_baud_rate = 115200;

void GPS_UART_Init(uint32_t BaudRate)
{ // gps_baud_rate = BaudRate;
  // Serial1.end();
  Serial1.setPins(GPS_PinRx, GPS_PinTx);
  Serial1.begin(BaudRate);
#if defined(GPS_PinEna)
  if(GPS_PinEna>=0)
  { pinMode(GPS_PinEna, OUTPUT);
    digitalWrite(GPS_PinEna, HIGH); }
#endif
#if defined(GPS_PinReset)
  if(GPS_PinReset>=0)
  { pinMode(GPS_PinReset, OUTPUT);
    digitalWrite(GPS_PinReset, HIGH); }
#endif
#if defined(GPS_PinPPS)
  if(GPS_PinPPS>=0)
  { pinMode(GPS_PinPPS, INPUT); }
#endif
}

#ifdef GPS_PinEna
void GPS_DISABLE(void) { digitalWrite(GPS_PinEna, LOW); }
void GPS_ENABLE (void) { digitalWrite(GPS_PinEna, HIGH); }
#endif

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
{ // gps_baud_rate = BaudRate;
  Serial1.end();
  Serial1.setPins(GPS_PinRx, GPS_PinTx);
  Serial1.begin(BaudRate); }

bool GPS_PPS_isOn(void)
{
#if defined(GPS_PinPPS)
  if(GPS_PinPPS<0) return false;
  return digitalRead(GPS_PinPPS) != LOW;
#else
  return false;
#endif
}

// =======================================================================================================

void setup()
{
  pinMode(LED_Pin, OUTPUT);
  digitalWrite(LED_Pin, 0);  // LED is low-active

/*
#if defined(Battery_Enable_Pin)
  pinMode(Battery_Enable_Pin, OUTPUT);
  digitalWrite(Battery_Enable_Pin, HIGH);
#endif

*/

  CONS_Mutex = xSemaphoreCreateMutex();
  I2C_Mutex = xSemaphoreCreateMutex();
  // WIFI_Mutex = xSemaphoreCreateMutex();

  Parameters.setDefault(getUniqueAddress()); // set default parameter values

  Serial.begin(115200);
  Serial.println();
  delay(1000);
  digitalWrite(LED_Pin, 1);

  // for (uint8_t i = 0; i < 6; i++)
  // { digitalWrite(LED_Pin, (i & 1) ? LED_StateOff : LED_StateOn);
  //   delay(80); }
  // digitalWrite(LED_Pin, LED_StateOff);

  // Serial.println("BOOT setup");

  // console_lock();
  Serial.print("nrf52-ogn-tracker ");
  Serial.print(HARD_NAME);
  Serial.println(" GPS/FreeRTOS check");
  delay(1000);
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);
  delay(1000);
  Serial.print("FreeRTOS tick [Hz] = ");
  Serial.println(configTICK_RATE_HZ);
  // print_pin_check();
  // console_unlock();

  GPS_UART_Init(GPS_getBaudRate());
  xTaskCreate(vTaskGPS    ,  "GPS"  ,  1000, NULL, 1, NULL);  // read data from GPS
  // xTaskCreate(vTaskPROC   ,  "PROC" ,  1200, NULL, 0, NULL);  // process received packets, prepare packets for transmission
  xTaskCreate(Radio_Task  ,  "RF"   ,  1200, NULL, 1, NULL);  // transmit/receive packets

/*
  create_task(vTaskLED, "LED", 256, TASK_PRIO_LOW, &led_task_handle);
  create_task(vTaskPROC, "PROC", 384, TASK_PRIO_LOW, &proc_task_handle);
  create_task(Radio_Task, "RF", 384, TASK_PRIO_LOW, &radio_task_handle);
  create_task(vTaskCONSOLE, "CONS", 640, TASK_PRIO_LOW, &console_task_handle);
  gps_start_state = 1;
  gps_task_create_result = create_task(vTaskGPSStart, "GPS0", 1024, TASK_PRIO_NORMAL, &gps_start_task_handle);
  if (gps_task_create_result != pdPASS) {
    gps_start_state = 9;
  }
*/
}

void loop()
{ vTaskDelay(configTICK_RATE_HZ/10);
  // Serial.println("loop() ...");
}

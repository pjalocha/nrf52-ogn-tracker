#include "main.h"
#include "gps.h"
#include "proc.h"
#include "ogn-radio.h"

SemaphoreHandle_t CONS_Mutex;
SemaphoreHandle_t I2C_Mutex;
// SemaphoreHandle_t WIFI_Mutex;

// uint32_t RxProc_Count[8];  // temperary, the real is in PROC task

// =======================================================================================================

void LED_PCB_On(bool ON)
{ digitalWrite(LED_PinRed, ON ? LED_StateOn : LED_StateOff); }

void LED_PCB_Off(void)
{ LED_PCB_On(false); }

void LED_PCB_Flash(uint8_t Time)
{ LED_PCB_On(true);
  if(Time>10) Time=10;
  delay(Time);
  LED_PCB_On(false); }

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

#ifdef GPS_PinPPS
uint32_t PPS_Intr_msTime = 0;   // [ms] xTaskGetTickCount() counter at the time of the PPS
uint32_t PPS_Intr_usTime = 0;   // [us] micros() counter at the time of the PPS

uint32_t PPS_usPrecTime  = 0;   // [1/16us] precise time of the PPS
uint32_t PPS_usTimeRMS   = 0;   // [1/4us]  precise PPS time residue time error RMS

uint32_t PPS_Intr_usFirst= 0;   // [us] micros() counter at the first interrupt in a series
uint32_t PPS_Intr_Count  = 0;   // [count] good PPS interrupts in the series
uint32_t PPS_Intr_Missed = 0;   // [count] missed PPS interrupts

 int32_t PPS_usPeriodErr = 0;   // [1/16us] PPS period average systematic error
uint32_t PPS_usPeriodRMS = 0;   // [(1/4us)^2] mean square of statistical error

const uint32_t PPS_usPeriod = 1000000;  // [usec] expected PPS period = 1sec = 10000000usec

static void PPS_Intr(void *Context)
{ uint32_t usTime = micros();                                     // [usec] usec-clock at interrupt time
  uint32_t msTime = xTaskGetTickCount();                          // [msec] mses-clock at interrupt time
  uint32_t usDelta = usTime - PPS_Intr_usTime;                    // difference from the previous PPS
  PPS_Intr_usTime = usTime;                                       // [usec]
  PPS_Intr_msTime = msTime;                                       // [ms]
  uint32_t Cycles = (usDelta+PPS_usPeriod/2)/PPS_usPeriod;        // how many PPS periods past
   int32_t usResid = usDelta-Cycles*PPS_usPeriod;                 // [usec]
  if(Cycles>0 && Cycles<=10 && abs(usResid)<Cycles*500)           // condition to accept the PPS edge
  { int32_t ResidErr = (usResid<<4)-PPS_usPeriodErr;              // [1/16us]
    if(Cycles>1) ResidErr/=Cycles;
    PPS_usPeriodErr += (ResidErr+2)>>2;                         // [1/16us] average the PPS period error
    uint32_t ErrSqr = ResidErr*ResidErr;
    if(PPS_Intr_Count)                                            // if not the first edge in the series
    { PPS_usPrecTime += ((PPS_usPeriod<<4)+PPS_usPeriodErr)*Cycles;  // [1/16us] forcast the PPS time to the current PPS
      PPS_usPeriodRMS += ((int32_t)((ErrSqr>>4)-PPS_usPeriodRMS)+2)>>2;
      int32_t usTimeErr = (usTime<<4)-PPS_usPrecTime;                // [1/16us] difference between current PPs and the forecast
      PPS_usPrecTime += (usTimeErr+2)>>2;                            // [1/16us]
      uint32_t ErrSqr = usTimeErr*usTimeErr;
      PPS_usTimeRMS += ((int32_t)((ErrSqr>>4)-PPS_usTimeRMS)+2)>>2; }
    else                                                          // if this was the very first edge in the series
    { PPS_usPrecTime  = PPS_Intr_usTime<<4;
      PPS_usTimeRMS   = 4<<4;
      PPS_usPeriodErr = ResidErr;
      PPS_usPeriodRMS = 4<<4; }                                   // set statistical error RMS to 2 usec
    PPS_Intr_Count += Cycles;
    PPS_Intr_Missed+= Cycles-1; }                                 // count PPS cycles
  else                                                            // if edge is not accepted
  { PPS_Intr_Count=0;                                             // then we start all from scratch
    PPS_Intr_Missed=0;
    PPS_Intr_usFirst= usTime; }
}

int PPS_Print(char *Line)
{ int Len=0;
  uint32_t msTime = millis();
  uint32_t PPSage = msTime-PPS_Intr_msTime;                         // [ms] time since the last PPS interrupt
  uint32_t UTC    = GPS_TimeSync.UTC;                               // [sec] time since last ref. UTC
  uint32_t UTCage = msTime-GPS_TimeSync.sysTime;                    // [ms] time since last ref. UTC
  // Serial.printf("PPS: PPSage:%u UTCage:%u [ms] UTC:%u", PPSage, UTCage, UTC);
  PPSage -= UTCage;                                                 //
  PPSage += 500;
  UTC -= PPSage/1000;
  // Serial.printf(" => PPSage:%u UTC:%u\n", PPSage, UTC);
  if(PPS_Intr_Count>=10 && PPSage<=20000)
    Len=sprintf(Line, "SatPPS: %08X:%08X/16MHz/%3.1fus %+3.1fppm %3.1fus %ds",
              UTC, PPS_usPrecTime,
              (1.0/4)*IntSqrt(PPS_usTimeRMS),
              (-1.0/16)*PPS_usPeriodErr, (1.0/4)*IntSqrt(PPS_usPeriodRMS),
              PPS_Intr_Count );
  return Len; }
#endif

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
  { pinMode(GPS_PinPPS, INPUT);
    // attachInterrupt(digitalPinToInterrupt(GPS_PinPPS), PPS_Intr, RISING);
  }
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
  pinMode(LED_PinRed, OUTPUT);
  digitalWrite(LED_PinRed  , LED_StateOn);
#ifdef LED_PinGreen
  pinMode(LED_PinGreen, OUTPUT);
  digitalWrite(LED_PinGreen, LED_StateOff);
#endif
#ifdef LED_PinBlue
  pinMode(LED_PinBlue, OUTPUT);
  digitalWrite(LED_PinBlue , LED_StateOff);
#endif

#ifdef IO_Power_Pin
  pinMode(IO_Power_Pin, OUTPUT);
  digitalWrite(IO_Power_Pin, HIGH);
#endif
#ifdef RF_Power_Pin
  pinMode(RF_Power_Pin, OUTPUT);
  digitalWrite(RF_Power_Pin, HIGH);
#endif

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
  digitalWrite(LED_PinRed, LED_StateOff);

  Serial.print("nrf52-ogn-tracker ");
  Serial.print(HARD_NAME);
  Serial.println(" GPS/FreeRTOS check");
  delay(1000);
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);
  // delay(1000);
  // Serial.print("FreeRTOS tick [Hz] = ");
  // Serial.println(configTICK_RATE_HZ);

  GPS_UART_Init(GPS_getBaudRate());
  xTaskCreate(vTaskGPS    ,  "GPS"  ,  1000, NULL, 1, NULL);  // read data from GPS
  xTaskCreate(Radio_Task  ,  "RF"   ,  1200, NULL, 1, NULL);  // transmit/receive packets
  xTaskCreate(vTaskPROC   ,  "PROC" ,  1200, NULL, 0, NULL);  // process received packets, prepare packets for transmission

}

void loop()
{ vTaskDelay(configTICK_RATE_HZ/10);
  ///
}

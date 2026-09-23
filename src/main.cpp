#include "main.h"
#include "gps.h"
#include "proc.h"
#include "ogn-radio.h"

#include "external_flash_fs.h"
#ifdef WITH_LOG
#include "log.h"
#endif
#include "epd.h"
#include "oled.h"
#ifdef WITH_TASK_STATS
#include "taskstats.h"
#endif

#include <Wire.h>

#include "Button2.h"

#ifdef WITH_BLE_SPP
#include <bluefruit.h>
#include "nrf_sdm.h"
#include "nrf_soc.h"
#endif

// =======================================================================================================

SemaphoreHandle_t CONS_Mutex;
SemaphoreHandle_t I2C_Mutex;
#ifdef WITH_BLE_SPP
SemaphoreHandle_t BLE_Mutex;
#endif

// =======================================================================================================

uint8_t PowerMode = 2;
Word32x2 Random = { .Word = 0x123456789ABCDEF0ULL };
HardItems HardwareStatus = { .Flags = 0 };

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

static uint8_t I2C_Scan(TwoWire &Wire, const char *Title)
{ Serial.printf(Title);
  uint32_t Time=millis();
  uint8_t I2Cdev=0;
  for(uint8_t Addr=0x08; Addr<=0x77; Addr++)
  { delay(1);
    Wire.beginTransmission(Addr);
    if(Wire.endTransmission()==0)
    { Serial.printf(" 0x%02X", Addr); I2Cdev++; }
  }
  Time=millis()-Time;
  Serial.printf(" %d devices (%dms)\n", I2Cdev, Time);
  return I2Cdev; }

// =======================================================================================================

int16_t readMCUtemperature(void)
{
#ifdef WITH_BLE_SPP
  uint8_t SoftDeviceEnabled = 0;
  if((sd_softdevice_is_enabled(&SoftDeviceEnabled)==NRF_SUCCESS) && SoftDeviceEnabled)
  { int32_t Temp = 0;
    if(sd_temp_get(&Temp)==NRF_SUCCESS) return (Temp*10+2)/4; } // [0.1degC]
#endif
  NRF_TEMP->TASKS_START = 1;
  while(!NRF_TEMP->EVENTS_DATARDY);
  int32_t Temp = NRF_TEMP->TEMP;
  NRF_TEMP->EVENTS_DATARDY = 0;
  NRF_TEMP->TASKS_STOP = 1;
  return Temp=(Temp*10+2)/4; }             // [0.1degC]

static int ADC_Init(void)
{ // analogReadResolution(12);             // default is 10 bits, no need to change
  analogReference(AR_INTERNAL_3_0); }      // so 1024 ADC counts is 3.0V

uint16_t BatterySense(int Samples)
{ uint32_t Volt=0;
  for( int Idx=0; Idx<Samples; Idx++)
  { Volt += analogRead(Battery_Pin); }
#ifdef WITH_WIO_TRACKER
  Volt = (Volt*579+Samples*50)/(Samples*100); // [mV] calibrated 5.79mV/count for Wio Tracker
#else
  Volt = (Volt*11+Samples)/(Samples*2);
#endif
  return Volt; }                           // [mV]

// =======================================================================================================

FlashParameters Parameters;  // parameters stored in Flash: address, aircraft type, etc.

// =======================================================================================================

#ifdef WITH_BLE_SPP

static BLEService BLE_SPP_Service(0xFFE0);
static BLECharacteristic BLE_SPP_Char(0xFFE1);

static void BLE_StartAdvertising()
{ Bluefruit.Advertising.stop();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(BLE_SPP_Service);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0); }

static void BLE_Init(void)
{ Bluefruit.autoConnLed(false);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  if(!Bluefruit.begin(1, 0)) return;             // 1 client allowed, no connections to external servers
  Bluefruit.setName(Parameters.BTname);
  Bluefruit.setTxPower(0);                       // [dBm] BLE transmitter power

  BLE_SPP_Service.begin();

  BLE_SPP_Char.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY);
  BLE_SPP_Char.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  BLE_SPP_Char.setMaxLen(Bluefruit.getMaxMtu(BLE_GAP_ROLE_PERIPH));
  BLE_SPP_Char.setUserDescriptor("BLE SPP");
  BLE_SPP_Char.begin();

  BLE_StartAdvertising();
}

FIFO<char, 4096> BLE_SPP_TxFIFO;

bool BLE_isConnected(void) { return Bluefruit.connected(); }
bool BLE_SPP_isConnected(void) { return BLE_SPP_Char.notifyEnabled(); }

static void BLE_Loop(void)
{ if( !BLE_isConnected() || !BLE_SPP_isConnected()) { BLE_SPP_TxFIFO.flush(); return; }
  char *Block;
  int Size=BLE_SPP_TxFIFO.getReadBlock(Block); if(Size<=0) return;
  uint16_t ConnHandle = Bluefruit.connHandle();
  BLEConnection *Conn = Bluefruit.Connection(ConnHandle);
  int Payload = Conn ? Conn->getMtu()-3 : 20;
  if(Payload<1) Payload=20;
  if(Size>Payload) Size=Payload;
  if(BLE_SPP_Char.notify(ConnHandle, (uint8_t *)Block, Size))
    BLE_SPP_TxFIFO.flushReadBlock(Size); }

void BLE_UART_Write(char Byte) { BLE_SPP_TxFIFO.Write(Byte); }
int  BLE_UART_Free(void) { return BLE_SPP_TxFIFO.Free(); }

#endif

// =======================================================================================================

// =======================================================================================================

#if Button_Pin >= 0
static Button2 Button(Button_Pin);
#endif
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
static Button2 OLED_MenuButton(Trackball_PinPress);
#endif

#if Button_Pin >= 0
static void Button_Single(Button2 Butt)
{
#ifdef WITH_EPAPER
  if(EPD_IsRadarView())
  { EPD_TrafficRange_Next();
    EPD_RequestFullRefresh();
    return; }
#endif
#ifdef WITH_OLED
  OLED_ButtonSingle();
#endif
}

static void Button_Double(Button2 Butt)
{
#ifdef WITH_EPAPER
  EPD_BacklightOn(15000);
  EPD_RequestFullRefresh();
#endif
}

static void Button_Long(Button2 Butt)
{
#if defined(WITH_WIO_TRACKER)
  (void)Butt;
  OLED_ButtonLong();
#else
  PowerMode=0;
// #ifdef WITH_BEEPER
//   Beep_Off();
// #endif
#ifdef WITH_EPAPER
  EPD_BacklightOff();
  uint32_t WaitStart = millis();
  while(!EPD_IsPowerOffReady() && (uint32_t)(millis()-WaitStart)<8000)
  { vTaskDelay(50); }
#endif
#ifdef GPS_PinEna
  GPS_DISABLE();
#endif
#ifdef RF_Power_Pin
  digitalWrite(RF_Power_Pin, LOW);
#endif
#ifdef IO_Power_Pin
  digitalWrite(IO_Power_Pin, LOW);
#endif
  delay(100);
  systemOff(Button_Pin, LOW);   // wake on the active-low button
  while(1) __WFI();             // never returns
#endif
}

#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
static void OLED_MenuButton_Click(Button2 Butt)
{ (void)Butt; OLED_MenuButtonClick(); }

static void OLED_MenuButton_Long(Button2 Butt)
{ (void)Butt; OLED_MenuButtonLong(); }
#endif

static void Button_Init(void)
{ pinMode(Button_Pin, INPUT);
  Button.setLongClickTime(3000);
  Button.setClickHandler(Button_Single);
  Button.setDoubleClickHandler(Button_Double);
  Button.setLongClickDetectedHandler(Button_Long);
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  pinMode(Trackball_PinUp, INPUT_PULLUP);
  pinMode(Trackball_PinDown, INPUT_PULLUP);
  pinMode(Trackball_PinLeft, INPUT_PULLUP);
  pinMode(Trackball_PinRight, INPUT_PULLUP);
  OLED_MenuButton.setClickHandler(OLED_MenuButton_Click);
  OLED_MenuButton.setLongClickTime(2000);
  OLED_MenuButton.setLongClickDetectedHandler(OLED_MenuButton_Long);
#endif
}
#endif



// =======================================================================================================

bool CONS_UART_isConnected(void) // { return 0; } // only for tests
{ return Serial && Serial.dtr(); }

int CONS_UART_Read(uint8_t &Byte)
{ if(!Serial.available()) return 0;
  Byte = (uint8_t)Serial.read(); return 1; }

void CONS_UART_Write(char Byte)
{ if(CONS_UART_Free()<80) return;
  Serial.write((uint8_t)Byte); }

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

static void PPS_Intr(void)
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

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout, bool LogOnly)
{ (void)Timestamp;
  if(!CONS_UART_isConnected()) return;
  if (LogOnly || Line==0 || LineLen <= 0) return;
#ifdef CONS_OUTPUT
  if(!xSemaphoreTake(CONS_Mutex, msTimeout)) return;
  if(CONS_UART_Free()>LineLen) Serial.write((const uint8_t *)Line, LineLen);
  xSemaphoreGive(CONS_Mutex);
#endif
}

void SysLog_Line(const char *Line, bool Timestamp, int msTimeout, bool LogOnly)
{ if (Line==0) return;
  SysLog_Line(Line, (int)strlen(Line), Timestamp, msTimeout, LogOnly); }

void SysLog_Line(const char *Line, int LineLen, bool Timestamp, int msTimeout)
{ SysLog_Line(Line, LineLen, Timestamp, msTimeout, false); }

void SysLog_Line(const char *Line, bool Timestamp, int msTimeout)
{ SysLog_Line(Line, Timestamp, msTimeout, false); }

// =======================================================================================================

static char Line[512];

#if defined(WITH_T_ECHO)
static void T_Echo_StayOffOnChargerWake(void)
{
  uint32_t ResetReason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = ResetReason;

  bool WokeFromSystemOff = ResetReason & POWER_RESETREAS_OFF_Msk;
  bool WokeFromVBUS      = ResetReason & POWER_RESETREAS_VBUS_Msk;
  bool WokeFromResetPin  = ResetReason & POWER_RESETREAS_RESETPIN_Msk;

  pinMode(Button_Pin, INPUT_PULLUP);
  bool ButtonPressed = digitalRead(Button_Pin)==LOW;

  if(WokeFromSystemOff && WokeFromVBUS && !WokeFromResetPin && !ButtonPressed)
  { systemOff(Button_Pin, LOW);
    while(1) __WFI(); }
}
#endif

void setup()
{
#if defined(WITH_T_ECHO)
  T_Echo_StayOffOnChargerWake();
#endif

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

  ADC_Init();
#if defined(Battery_Enable_Pin)
  pinMode(Battery_Enable_Pin, OUTPUT);
  digitalWrite(Battery_Enable_Pin, Battery_Enable_StateOn);
#endif

#if Button_Pin >= 0
  Button_Init();
#endif
#ifdef WITH_INTERNAL_FS
#ifdef WITH_LORAWAN
  LoRaWANnode SavedWAN;
  bool SavedWANValid=false;
#endif
  bool InternalFSReady=InternalFS.begin();
  if(!InternalFSReady)
  { if(InternalFS.format()) InternalFSReady=InternalFS.begin(); }
#else
  InternalFS.begin();
#endif
  HardwareStatus.SPIFFS = LogFS_begin();
#ifdef WITH_INTERNAL_FS
  int ParameterRead=InternalFSReady ? Parameters.ReadFromNVS() : -2;
  if(ParameterRead < -1 && InternalFSReady)
  { // A bad or obsolete parameter record can make the FS unreadable; reformat and recover defaults.
#ifdef WITH_LORAWAN
    // Keep a valid LoRaWAN registration while recovering the parameter file.
    SavedWAN.Reset(getUniqueID());
    SavedWANValid = SavedWAN.ReadFromNVS()>=0;
#endif
    InternalFSReady=InternalFS.format();
    if(InternalFSReady) InternalFSReady=InternalFS.begin();
#ifdef WITH_LORAWAN
    if(InternalFSReady && SavedWANValid) SavedWAN.WriteToNVS();
#endif
    ParameterRead=-1; }
  if(ParameterRead<0)                         // use defaults if parameters are absent or invalid
  { Parameters.setDefault(getUniqueAddress());
    if(InternalFSReady) Parameters.WriteToNVS(); }
#else
  if(Parameters.ReadFromNVS()<0)               // try to get parameters from external flash
  { Parameters.setDefault(getUniqueAddress());
    Parameters.WriteToNVS(); }
#endif
  if(Parameters.BTname[0]==0)                  // for the BT to work
  { Parameters.getAprsCall(Parameters.BTname);
    Parameters.WriteToNVS(); }

#ifdef HARD_NAME
  strcpy(Parameters.Hard, HARD_NAME);
#endif
#ifdef SOFT_NAME
  #ifdef WITH_OTA_HTTPS
    if(Parameters.Soft[0]==0)                  // with OTA, firmware serial number is stored as Parameters.Soft
  #endif
      strcpy(Parameters.Soft, SOFT_NAME);
#endif

  CONS_Mutex = xSemaphoreCreateMutex();
  I2C_Mutex = xSemaphoreCreateMutex();
#ifdef WITH_TASK_STATS
  TaskStats_Init();
#endif
#ifdef WITH_BLE_SPP
  BLE_Mutex = xSemaphoreCreateMutex();
#endif
#if defined(I2C_PinSDA) && defined(I2C_PinSCL)
  Wire.setPins(I2C_PinSDA, I2C_PinSCL);
  Wire.begin();
  Wire.setClock(400000);
#endif
  // I2C_Scan(Wire, "I2C bus:");
  Serial.begin(115200);
  Serial.println();
  // delay(1000);
  digitalWrite(LED_PinRed, LED_StateOff);

#ifdef WITH_BEEPER
  Beep_Init();
#ifdef WITH_BEEPER_GEN
  Play_Morse('S');
#else
#ifdef WITH_WIO_TRACKER
  // The Wio Tracker L1 buzzer is markedly louder around 2.5-2.65kHz.
  // Play(Play_Vol_1 | Play_Oct_2 | 0x03, 250); // 2489Hz
  // Play(Play_Vol_1 | Play_Oct_2 | 0x04, 250); // 2637Hz
  Play(Play_Vol_1 | Play_Oct_0 | 0x05, 250);
  Play(Play_Vol_1 | Play_Oct_0 | 0x08, 250);
#else
  Play(Play_Vol_1 | Play_Oct_0 | 0x05, 250);
  Play(Play_Vol_1 | Play_Oct_0 | 0x08, 250);
#endif
  Play(Play_Vol_0 | Play_Oct_0 | 0x00, 100);
#endif
  // Play_Morse(' ');
  // Play_Morse('O');
  // Play_Morse('G');
  // Play_Morse('N');
#endif

  Serial.print("nrf52-ogn-tracker ");
  Serial.print(HARD_NAME);
  Serial.println(" GPS/FreeRTOS check");
  // delay(1000);
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(' ');
  Serial.println(__TIME__);
  // delay(1000);
  // Serial.print("FreeRTOS tick [Hz] = ");
  // Serial.println(configTICK_RATE_HZ);

  LogFS_printStatus(Serial);
  LogFS_listRoot(Serial);

  GPS_UART_Init(GPS_getBaudRate());
#ifdef WITH_BLE_SPP
  BLE_Init();
#endif

  xTaskCreate(vTaskGPS    ,  "GPS"  ,  1000, NULL, 1, NULL);  // read data from GPS
  xTaskCreate(Radio_Task  ,  "RF"   ,  1200, NULL, 2, NULL);  // transmit/receive packets
  xTaskCreate(vTaskPROC   ,  "PROC" ,  1200, NULL, 1, NULL);  // process received packets, prepare packets for transmission
#ifdef WITH_LOG
  xTaskCreate(vTaskLOG    ,  "LOG"  ,  3000, NULL, 0, NULL);  // write received and own packets to external flash
#endif
#ifdef WITH_EPAPER
  xTaskCreate(EPD_Task    ,  "EPD"  ,  3000, NULL, 0, NULL);  // update e-paper display
#endif
#ifdef WITH_OLED
  xTaskCreate(OLED_Task   ,  "OLED" ,  2000, NULL, 0, NULL);  // update OLED display
#endif

}

// ===================================================================================================================

static NMEA_RxMsg NMEA;

static void PrintParameters(void)                              // print parameters stored in Flash
{ Parameters.Print(Line);
  if(!xSemaphoreTake(CONS_Mutex, 100)) return;                 // ask exclusivity on UART1
  Format_String(CONS_UART_Write, Line);
  xSemaphoreGive(CONS_Mutex); }                                // give back UART1 to other tasks

static void PrintPOGNS(void)                                   // print parameters in the $POGNS form
{ if(!xSemaphoreTake(CONS_Mutex, 100)) return;
  Parameters.WritePOGNS(Line);
  Format_String(CONS_UART_Write, Line);
  Parameters.WritePOGNS_Pilot(Line);
  Format_String(CONS_UART_Write, Line);
  Parameters.WritePOGNS_Acft(Line);
  Format_String(CONS_UART_Write, Line);
  Parameters.WritePOGNS_Comp(Line);
  Format_String(CONS_UART_Write, Line);
#ifdef WITH_AP
  Parameters.WritePOGNS_AP(Line);
  Format_String(CONS_UART_Write, Line);
#endif
#ifdef WITH_STRATUX
  Parameters.WritePOGNS_Stratux(Line);
  Format_String(CONS_UART_Write, Line);
#endif
  xSemaphoreGive(CONS_Mutex); }

#ifdef WITH_CONFIG
static void ReadParameters(void)  // read parameters requested by the user in the NMEA sent.
{ if((!NMEA.hasCheck()) || NMEA.isChecked() )
  { PrintParameters();
    if(NMEA.Parms==0) { PrintPOGNS(); return; }                              // if no parameter given
    Parameters.ReadPOGNS(NMEA);
    PrintParameters();
    Parameters.WriteToNVS();                                                  // erase and write the parameters into >
  }
}
#endif

#ifdef WITH_LOOKOUT
static void ListTraffic(void)
{ char Line[160];
  for( uint8_t Idx=0; Idx<Look.MaxTargets; Idx++)
  { const LookOut_Target *Tgt = Look.Target+Idx; if(!Tgt->Alloc) continue;
    int Len=Tgt->Print(Line);
    Line[Len++]=' ';
    Len+=Tgt->Pos.Print(Line+Len);
    Serial.printf("%2d: %s\n", Idx, Line); }
}
#endif

#ifdef WITH_CONFIG
static void ReadPFLAC(void)  // read parameters requested by the user in the NMEA
{ if((!NMEA.hasCheck()) || NMEA.isChecked() )
  { PrintParameters();
    // if(NMEA.Parms==0) { PrintPOGNS(); return; }                              // if no parameter given
    Parameters.ReadPFLAC(NMEA);
    PrintParameters();
    Parameters.WriteToNVS();                                                  // erase and write the parameters into >
  }
}
#endif

#ifdef WITH_LOG
static void ListLogFile(void)
{
  if(NMEA.Parms!=1) return;
  uint32_t FileTime = FlashLog_ReadShortFileTime((const char *)NMEA.ParmPtr(0), NMEA.ParmLen(0));
  if(FileTime==0) return;
  FlashLog_ListFile(FileTime);
}
#endif

static void ReadPFLA(void)  // read and interprete $PFLAx NMEA
{ if(NMEA.isPFLAC()) return ReadPFLAC();

}

static void ProcessNMEA(void)     // process a valid NMEA that we got to the console
{
#ifdef WITH_CONFIG
  if(NMEA.isPOGNS()) ReadParameters();
  if(NMEA.isPFLA()) ReadPFLA();
#endif
#ifdef WITH_LOG
  if(NMEA.isPOGNL()) ListLogFile();
#endif
}

static void ProcessCtrlC(void)                                  // print system state to the console
{ if(!xSemaphoreTake(CONS_Mutex, 50)) return;
  Parameters.Print(Line);
  Format_String(CONS_UART_Write, Line);
  Format_String(CONS_UART_Write, "GPS: ");
  Format_UnsDec(CONS_UART_Write, GPS_getBaudRate(), 1);
  Format_String(CONS_UART_Write, "bps");
  CONS_UART_Write(',');
  Format_UnsDec(CONS_UART_Write, GPS_PosPeriod, 4, 3);
  CONS_UART_Write('s');
  if(GPS_Status.PPS)         Format_String(CONS_UART_Write, ",PPS");
  if(GPS_Status.NMEA)        Format_String(CONS_UART_Write, ",NMEA");
  if(GPS_Status.UBX)         Format_String(CONS_UART_Write, ",UBX");
  if(GPS_Status.MAV)         Format_String(CONS_UART_Write, ",MAV");
  if(GPS_Status.BaudConfig)  Format_String(CONS_UART_Write, ",BaudOK");
  if(GPS_Status.ModeConfig)  Format_String(CONS_UART_Write, ",ModeOK");
  CONS_UART_Write('\r'); CONS_UART_Write('\n');
  Parameters.Write(CONS_UART_Write);                         // write the parameters to the console

  Serial.printf("Batt:%6.4fV %+4.1fmV/min\n",
     0.0001f*((10*BatteryVoltage+128)>>8), 0.1f*((600*BatteryVoltageRate+128)>>8));
  // Format_String(CONS_UART_Write, "Batt:");
  // Format_UnsDec(CONS_UART_Write, (10*BatteryVoltage+128)>>8, 5, 4);
  // Format_String(CONS_UART_Write, "V ");
  // Format_SignDec(CONS_UART_Write, (600*BatteryVoltageRate+128)>>8, 3, 1);
  // Format_String(CONS_UART_Write, "mV/min\n");

  // lfs_t *lfs = InternalFS._getFS();
  // Serial.printf("InternalFS TotalBlocks:%lu x %lu B\n", lfs->cfg->block_count, lfs->cfg->block_size);

  // uint32_t TotalBytes = lfs->cfg->block_count * lfs->cfg->block_size;
  // uint32_t UsedBytes = lfs_fs_size(lfs) * lfs->cfg->block_size;
  // Serial.printf("InternalFS Total:%3.1f Used:%3.1f [MB]\n", (1.0f/0x1000000)*TotalBytes, (1.0f/0x1000000)*UsedBytes);
  LogFS_printStatus(Serial);

  xSemaphoreGive(CONS_Mutex); }

static void ProcessCtrlX(void)
{ static uint32_t LastTime=0;
  uint32_t Time=millis();
  uint32_t Diff=Time-LastTime;
  if(Diff<1000)
  { // SysLog_Line("Restart from console", 1, 50);
    // ShutDownReq=1;
    vTaskDelay(2000);
    // ESP.restart();
    NVIC_SystemReset(); }
  LastTime=Time; } 

static void ProcessCtrlL(void)
{
#ifdef WITH_LOG
  int Files = FlashLog_ListFiles();
  if(Files<=0) Serial.println("FlashLog: no log files");
#else
  Serial.println("FlashLog: not enabled");
#endif
}

#ifdef WITH_TASK_STATS
static void ProcessCtrlS(void)
{ if(!xSemaphoreTake(CONS_Mutex, 100)) return;
  TaskStats_Print();
  xSemaphoreGive(CONS_Mutex); }
#endif

static void ProcessCtrlO(void)
{ static uint32_t LastTime=0;
  uint32_t Time=millis();
  uint32_t Diff=Time-LastTime;
  if(Diff<1000)
  {
#ifdef WITH_LOG
    if(FlashLog_isOpen())
    { Serial.println("ExternalFlash: log file is open, format skipped");
      LastTime=Time;
      return; }
#endif
    HardwareStatus.SPIFFS = LogFS_format(Serial);
    LogFS_listRoot(Serial); }
  else
  { Serial.println("ExternalFlash: press Ctrl-O again within 1s to format FAT"); }
  LastTime=Time; }

static int ProcessInput(void)
{
  const uint8_t CtrlB = 'B'-'@';
  const uint8_t CtrlC = 'C'-'@';
  const uint8_t CtrlF = 'F'-'@';
  const uint8_t CtrlL = 'L'-'@';
  const uint8_t CtrlO = 'O'-'@';
  const uint8_t CtrlP = 'P'-'@';
  const uint8_t CtrlS = 'S'-'@';
  const uint8_t CtrlT = 'T'-'@';
  const uint8_t CtrlX = 'X'-'@';

  int Count=0;
  for( ; ; )
  { uint8_t Byte; int Err=CONS_UART_Read(Byte); if(Err<=0) break; // get byte from console, if none: exit the loop
    Count++;
    if(Byte==CtrlC) ProcessCtrlC();                                // if Ctrl-C: print parameters
    if(Byte==CtrlF) LogFS_listRoot(Serial);                         // if Ctrl-F: list external flash root
    if(Byte==CtrlL) ProcessCtrlL();                                  // if Ctrl-L: list log files
    if(Byte==CtrlO) ProcessCtrlO();                                  // double Ctrl-O formats external flash FAT
#ifdef WITH_TASK_STATS
    if(Byte==CtrlS) ProcessCtrlS();                                  // print FreeRTOS task statistics
#endif
#ifdef WITH_LOOKOUT
    if(Byte==CtrlT) ListTraffic();                                 // if Ctrl-T: print traffic
#endif
    if(Byte==CtrlX) ProcessCtrlX();                                // double Ctrl-X restarts the system
    NMEA.ProcessByte(Byte);                                       // pass the byte through the NMEA processor
    if(NMEA.isComplete())                                         // if complete NMEA:
    { ProcessNMEA();                                              // interpret the NMEA
      NMEA.Clear(); }                                             // clear the NMEA processor for the next sentence
  }
  return Count; }

void loop()
{ vTaskDelay(1);
#ifdef WITH_BEEPER
  Play_TimerCheck(1);              // handle playing notes on the buzzer
#endif
#ifdef WITH_BLE_SPP
  BLE_Loop();
#endif
#if Button_Pin >= 0
  Button.loop();
#endif
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  OLED_MenuButton.loop();
#endif
  while(ProcessInput()>0);         // handle console input
}

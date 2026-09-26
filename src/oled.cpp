#include <Arduino.h>

#include "main.h"
// #include "ble_spp.h"
#include "oled.h"

#include "gps.h"
#include "proc.h"
// #include "sens.h"
#include "format.h"
#include "intmath.h"

#include "ogn-radio.h"
#ifdef WITH_TASK_STATS
#include "taskstats.h"
#endif

#ifdef WITH_LOG
#include "external_flash_fs.h"
#include "log.h"
#endif

#ifdef WITH_OLED

static char Line[32];

#ifdef WITH_BIGOLED
U8G2_SH1106_128X64_NONAME_F_HW_I2C OLED(U8G2_R0, OLED_PinRST);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C OLED(U8G2_R0, OLED_PinRST);
#endif

static const uint8_t OLED_Page_ID          = 0;
static const uint8_t OLED_Page_GPS         = 1;
static const uint8_t OLED_Page_SatSNR      = 2;
static const uint8_t OLED_Page_Baro        = 3;
static const uint8_t OLED_Page_RF          = 4;
static const uint8_t OLED_Page_RFcounts    = 5;
static const uint8_t OLED_Page_Power       = 6;
static const uint8_t OLED_Page_RelayOGN    = 7;
static const uint8_t OLED_Page_RelayADSL   = 8;
#ifdef WITH_LOOKOUT
static const uint8_t OLED_Page_LookOut     = 9;
#endif
#ifdef WITH_LOG
#ifdef WITH_LOOKOUT
static const uint8_t OLED_Page_Log         = 10;
static const uint8_t OLED_Page_BaseCount   = 11;
#else
static const uint8_t OLED_Page_Log         = 9;
static const uint8_t OLED_Page_BaseCount   = 10;
#endif
#elif defined(WITH_LOOKOUT)
static const uint8_t OLED_Page_BaseCount   = 10;
#else
static const uint8_t OLED_Page_BaseCount   = 9;
#endif
#if 0 && defined(WITH_TASK_STATS) // Temporarily hide the incomplete OLED task-status page.
static const uint8_t OLED_Page_TaskStats   = OLED_Page_BaseCount;
static const uint8_t OLED_Page_AfterStats  = OLED_Page_BaseCount+1;
#else
static const uint8_t OLED_Page_AfterStats  = OLED_Page_BaseCount;
#endif
#ifdef WITH_LORAWAN
static const uint8_t OLED_Page_LoRaWAN      = OLED_Page_AfterStats;
static const uint8_t OLED_Page_Return       = OLED_Page_AfterStats+1;
static const uint8_t OLED_Pages             = OLED_Page_AfterStats+2;
#else
static const uint8_t OLED_Page_Return       = OLED_Page_AfterStats;
static const uint8_t OLED_Pages             = OLED_Page_AfterStats+1;
#endif
static uint8_t OLED_Page                   = 0;
static bool OLED_PageChange                = false;
static bool OLED_PageOFF                   = false;
uint8_t OLED_Rotate                        = 0;
#ifdef WITH_OLED_DIM
static uint32_t OLED_PageActive            = 0;
static const uint32_t OLED_PageTimeout     = (uint32_t)60000*WITH_OLED_DIM;
#endif

static TaskHandle_t OLED_TaskHandle = 0;
static const uint32_t OLED_EventPageButton = 1u<<0;
static const uint32_t OLED_EventTakeoff    = 1u<<4;
#if defined(WITH_WIO_TRACKER)
static const uint32_t OLED_EventPageLong   = 1u<<1;
static bool OLED_KeypadLocked              = false;
#endif

#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
#include "external_flash_fs.h"
#ifdef WITH_LORAWAN
#include <qrcode.h>
#ifdef WITH_BLE_SPP
#include "nrf_soc.h"
#endif
#endif

static const uint32_t OLED_EventMenuClick  = 1u<<2;
static const uint32_t OLED_EventMenuLong   = 1u<<3;

enum OLED_MenuState
{ OLED_MenuClosed,
  OLED_MenuList,
  OLED_MenuAcftType,
  OLED_MenuAddrType,
  OLED_MenuAddress,
  OLED_MenuTxPower,
  OLED_MenuLookOutWarnTime,
  OLED_MenuAlert,
  OLED_MenuGhost,
  OLED_MenuTextEdit,
  OLED_MenuFormatConfirm,
  OLED_MenuDefaultsConfirm,
#ifdef WITH_USB_MEMORY
  OLED_MenuUSBConfirm,
#endif
#ifdef WITH_LORAWAN
  OLED_MenuTTNConfirm,
  OLED_MenuQR
#endif
};

enum OLED_MenuTextField
{ OLED_MenuTextNone,
  OLED_MenuTextReg,
  OLED_MenuTextPilot };

static OLED_MenuState OLED_Menu = OLED_MenuClosed;
static uint8_t OLED_MenuItem = 0;
static uint8_t OLED_MenuAcftTypeValue = 0;
static uint8_t OLED_MenuAddrTypeValue = 0;
static uint32_t OLED_MenuAddressValue = 0;
static uint8_t OLED_MenuAddressNibble = 0;
static uint8_t OLED_MenuTxPowerValue = 0;
static uint8_t OLED_MenuLookOutWarnTimeValue = 0;
static uint8_t OLED_MenuAlertValue = 0;
static uint8_t OLED_MenuGhostValue = 0;
static OLED_MenuTextField OLED_MenuText = OLED_MenuTextNone;
static char OLED_MenuTextValue[FlashParameters::InfoParmLen];
static uint8_t OLED_MenuTextPosition = 0;
static int OLED_MenuSaveResult = 0;
static uint32_t OLED_MenuMessageTime = 0;
static const char *OLED_MenuMessage = 0;

static const uint8_t OLED_MenuItems = 11
#ifdef WITH_USB_MEMORY
  + 1
#endif
#ifdef WITH_LORAWAN
  + 1
#endif
;
static const uint8_t OLED_MenuTextLength = FlashParameters::InfoParmLen-1;
static const uint8_t OLED_MenuLookOutWarnTimes[4] = { 20, 30, 40, 50 };
static const uint8_t OLED_MenuGestureCenter  = 1u<<4;
static const uint8_t OLED_MenuGestureBlocked = 1u<<7;
static uint8_t OLED_MenuGestureOwner = 0;
static bool OLED_MenuCenterGesturePending = false;
static bool OLED_MenuCenterGesturePrincipal = false;

#ifdef WITH_LORAWAN
static const uint8_t OLED_QRVersion = 2;
static const uint8_t OLED_QRPayloadLength = 24; // 8-byte DevEUI + 16-byte AppKey
static uint8_t OLED_QRPayload[OLED_QRPayloadLength];
static uint8_t OLED_QRModules[79];             // ceil((4*2+17)^2/8)
static QRCode OLED_QRCode;
#endif

static const char *OLED_MenuAcftTypeNames[16] =
{ "Unknown", "Glider", "Towplane", "Helicopter",
  "Skydiver", "Drop", "Hangglider", "Paraglider",
  "Powered", "Jet", "Gyroplane", "Balloon",
  "Zeppelin", "UAV", "Car", "Fixed" };

static const char *OLED_MenuAddrTypeNames[4] =
{ "RND", "ICAO", "FLARM", "OGN" };

static const char *OLED_MenuAlertNames[5] =
{ "All", "Level 1+", "Level 2+", "Level 3+", "Off" };

static const char *OLED_MenuGhostNames[3] =
{ "Off", "Traffic", "Altitude" };

static const char OLED_MenuTextCharacters[] =
  " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-+_/@#:";

static void OLED_SetPowerSave(bool PowerSave);
static void OLED_PreviousPage(void);
static void OLED_NextPage(void);

static void OLED_MenuBeepOpen(void)
{ Play(Play_Vol_1 | Play_Oct_0 | 0x05, 80); }

static void OLED_MenuBeepSaved(void)
{ Play(Play_Vol_1 | Play_Oct_0 | 0x05, 70);
  Play(Play_Vol_1 | Play_Oct_0 | 0x08, 110); }

static bool OLED_MenuActive(void)
{ return OLED_Menu!=OLED_MenuClosed; }

static void OLED_MenuRequest(uint32_t Event)
{ if(OLED_TaskHandle) xTaskNotify(OLED_TaskHandle, Event, eSetBits); }

void OLED_MenuButtonClick(void)
{ OLED_MenuRequest(OLED_EventMenuClick); }

void OLED_MenuButtonLong(void)
{ OLED_MenuRequest(OLED_EventMenuLong); }

static void OLED_MenuWake(void)
{ OLED_PageOFF=false;
#ifdef WITH_OLED_DIM
  OLED_PageActive=millis();
#endif
  OLED_SetPowerSave(false);
  OLED_PageChange=true; }

static void OLED_MenuOpen(void)
{ OLED_Menu=OLED_MenuList;
  OLED_MenuItem=0;
  OLED_MenuMessageTime=0;
  OLED_MenuMessage=0;
  OLED_MenuBeepOpen();
  OLED_MenuWake(); }

static void OLED_MenuClose(void)
{ OLED_Menu=OLED_MenuClosed;
  OLED_MenuMessageTime=0;
  OLED_MenuMessage=0;
  OLED_MenuWake(); }

static void OLED_MenuShowMessage(const char *Message, int Result)
{ OLED_MenuMessage=Message;
  OLED_MenuSaveResult=Result;
  OLED_MenuMessageTime=millis(); }

static void OLED_MenuSaveParameters(void)
{ int Result=Parameters.WriteToNVS();
  OLED_MenuShowMessage(Result<0 ? "ERROR" : "Saved", Result);
  if(Result>=0) OLED_MenuBeepSaved(); }

#ifdef WITH_LORAWAN
static bool OLED_MenuBuildQR(const uint8_t *Key)
{ uint64_t DevEUI=getUniqueID();
  for(uint8_t Idx=0; Idx<8; Idx++)
    OLED_QRPayload[Idx]=(uint8_t)(DevEUI>>(56-8*Idx));
  memcpy(OLED_QRPayload+8, Key, 16);
  return qrcode_initBytes(&OLED_QRCode, OLED_QRModules, OLED_QRVersion,
                          ECC_MEDIUM, OLED_QRPayload, OLED_QRPayloadLength)==0; }

static bool OLED_MenuRandomAppKey(uint8_t *Key)
{
#ifdef WITH_BLE_SPP
  uint8_t Offset=0;
  while(Offset<16)
  { uint8_t Available=0;
    if(sd_rand_application_bytes_available_get(&Available)!=NRF_SUCCESS) return false;
    if(Available==0) { vTaskDelay(1); continue; }
    uint8_t Count=Available;
    if(Count>16-Offset) Count=16-Offset;
    uint32_t Result=sd_rand_application_vector_get(Key+Offset, Count);
    if(Result==NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES) continue;
    if(Result!=NRF_SUCCESS) return false;
    Offset+=Count; }
  return true;
#elif defined(NRF_RNG)
  NRF_RNG->EVENTS_VALRDY=0;
  NRF_RNG->TASKS_START=1;
  for(uint8_t Idx=0; Idx<16; Idx++)
  { uint32_t Start=millis();
    while(!NRF_RNG->EVENTS_VALRDY)
    { if((uint32_t)(millis()-Start)>100) { NRF_RNG->TASKS_STOP=1; return false; }
      vTaskDelay(1); }
    Key[Idx]=NRF_RNG->VALUE;
    NRF_RNG->EVENTS_VALRDY=0; }
  NRF_RNG->TASKS_STOP=1;
  return true;
#else
  (void)Key;
  return false;
#endif
}

static void OLED_MenuRegisterTTN(void)
{ uint8_t Key[16];
  if(!OLED_MenuRandomAppKey(Key))
  { OLED_MenuShowMessage("RNG error", -1);
    OLED_Menu=OLED_MenuList; OLED_PageChange=true; return; }
  if(!OLED_MenuBuildQR(Key))
  { OLED_MenuShowMessage("QR error", -1);
    OLED_Menu=OLED_MenuList; OLED_PageChange=true; return; }

  // Let the radio task replace the live LoRaWAN state and write it to flash.
  // LittleFS access stays in one task, avoiding a concurrent radio/OLED write.
  Radio_LoRaWANRegister(Key);
  OLED_Menu=OLED_MenuQR;
  OLED_PageChange=true; }

static bool OLED_MenuTTNConfigured(void)
{ for(uint8_t Idx=0; Idx<16; Idx++)
    if(WANdev.AppKey[Idx]) return true;
  return false; }
#endif

static void OLED_MenuEnterText(OLED_MenuTextField Field);

static void OLED_MenuEnterItem(void)
{ switch(OLED_MenuItem)
  { case 0:
      OLED_MenuAcftTypeValue = Parameters.AcftType<16 ? Parameters.AcftType : 0;
      OLED_Menu=OLED_MenuAcftType;
      break;
    case 1:
      OLED_MenuAddrTypeValue = Parameters.AddrType<4 ? Parameters.AddrType : 0;
      OLED_Menu=OLED_MenuAddrType;
      break;
    case 2:
      OLED_MenuAddressValue = Parameters.Address&0x00FFFFFF;
      OLED_MenuAddressNibble=0;
      OLED_Menu=OLED_MenuAddress;
      break;
    case 3:
      { int Value=Parameters.TxPower;
        if(Value<0) Value=0;
        if(Value>22) Value=22;
        OLED_MenuTxPowerValue=Value; }
      OLED_Menu=OLED_MenuTxPower;
      break;
    case 4:
      OLED_MenuLookOutWarnTimeValue = Parameters.LookOutWarnTime<4 ? Parameters.LookOutWarnTime : 0;
      OLED_Menu=OLED_MenuLookOutWarnTime;
      break;
    case 5:
      OLED_MenuAlertValue = Parameters.AlertThresh<=4 ? Parameters.AlertThresh : 4;
      OLED_Menu=OLED_MenuAlert;
      break;
    case 6:
      OLED_MenuGhostValue = Parameters.GhostMode>=2 ? 2 : Parameters.GhostMode;
      OLED_Menu=OLED_MenuGhost;
      break;
    case 7:
      OLED_MenuEnterText(OLED_MenuTextReg);
      break;
    case 8:
      OLED_MenuEnterText(OLED_MenuTextPilot);
      break;
    case 9:
      OLED_Menu=OLED_MenuFormatConfirm;
      break;
    case 10:
      OLED_Menu=OLED_MenuDefaultsConfirm;
      break;
#ifdef WITH_USB_MEMORY
    case 11:
      OLED_Menu=OLED_MenuUSBConfirm;
      break;
#endif
#ifdef WITH_LORAWAN
    case 11
#ifdef WITH_USB_MEMORY
      + 1
#endif
      :
      OLED_Menu=OLED_MenuTTNConfirm;
      break;
#endif
    default: break; }
  OLED_PageChange=true; }

static char *OLED_MenuTextTarget(void)
{ if(OLED_MenuText==OLED_MenuTextReg) return Parameters.Reg;
  if(OLED_MenuText==OLED_MenuTextPilot) return Parameters.Pilot;
  return 0; }

static const char *OLED_MenuTextTitle(void)
{ return OLED_MenuText==OLED_MenuTextReg ? "Registration" : "Pilot name"; }

static void OLED_MenuEnterText(OLED_MenuTextField Field)
{ OLED_MenuText=Field;
  char *Target=OLED_MenuTextTarget();
  for(uint8_t Idx=0; Idx<OLED_MenuTextLength; Idx++)
    OLED_MenuTextValue[Idx] = (Target && Target[Idx]) ? Target[Idx] : ' ';
  OLED_MenuTextValue[OLED_MenuTextLength]=0;
  OLED_MenuTextPosition=0;
  OLED_Menu=OLED_MenuTextEdit;
  OLED_PageChange=true; }

static void OLED_MenuChangeAcftType(int8_t Step)
{ int Value=OLED_MenuAcftTypeValue+Step;
  if(Value<0) Value=0;
  if(Value>15) Value=15;
  OLED_MenuAcftTypeValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeAddrType(int8_t Step)
{ int Value=OLED_MenuAddrTypeValue+Step;
  if(Value<0) Value=0;
  if(Value>3) Value=3;
  OLED_MenuAddrTypeValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeAddressNibble(int8_t Step)
{ uint8_t Shift=(5-OLED_MenuAddressNibble)*4;
  uint8_t Value=(OLED_MenuAddressValue>>Shift)&0x0F;
  int NewValue=Value+Step;
  if(NewValue<0) NewValue=15;
  if(NewValue>15) NewValue=0;
  OLED_MenuAddressValue=(OLED_MenuAddressValue&~((uint32_t)0x0F<<Shift))|
                        ((uint32_t)NewValue<<Shift);
  OLED_PageChange=true; }

static void OLED_MenuChangeTxPower(int8_t Step)
{ int Value=OLED_MenuTxPowerValue+Step;
  if(Value<0) Value=0;
  if(Value>22) Value=22;
  OLED_MenuTxPowerValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeLookOutWarnTime(int8_t Step)
{ int Value=OLED_MenuLookOutWarnTimeValue+Step;
  if(Value<0) Value=0;
  if(Value>3) Value=3;
  OLED_MenuLookOutWarnTimeValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeAlert(int8_t Step)
{ int Value=OLED_MenuAlertValue+Step;
  if(Value<0) Value=0;
  if(Value>4) Value=4;
  OLED_MenuAlertValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeGhost(int8_t Step)
{ int Value=OLED_MenuGhostValue+Step;
  if(Value<0) Value=0;
  if(Value>2) Value=2;
  OLED_MenuGhostValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeText(int8_t Step)
{ const char *Characters=OLED_MenuTextCharacters;
  uint8_t CharacterCount=strlen(Characters);
  uint8_t Character=0;
  while(Character<CharacterCount && Characters[Character]!=OLED_MenuTextValue[OLED_MenuTextPosition]) Character++;
  if(Character>=CharacterCount) Character=0;
  int NewCharacter=Character+Step;
  if(NewCharacter<0) NewCharacter=CharacterCount-1;
  if(NewCharacter>=CharacterCount) NewCharacter=0;
  OLED_MenuTextValue[OLED_MenuTextPosition]=Characters[NewCharacter];
  OLED_PageChange=true; }

static void OLED_MenuCommitItem(void)
{ switch(OLED_Menu)
  { case OLED_MenuAcftType:
      if(Parameters.AcftType!=OLED_MenuAcftTypeValue)
      { Parameters.AcftType=OLED_MenuAcftTypeValue;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuAddrType:
      if(Parameters.AddrType!=OLED_MenuAddrTypeValue)
      { // Keep the same address-generation rules as the text parameter parser.
        if(Parameters.AddrType==3) Parameters.Address=getUniqueAddress();
        else if(Parameters.AddrType==0)
          Parameters.Address=(Parameters.calcCheckSum()*1664525+1013904223)^getUniqueAddress();
        Parameters.AddrType=OLED_MenuAddrTypeValue;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuAddress:
      if(Parameters.Address!=(OLED_MenuAddressValue&0x00FFFFFF))
      { Parameters.Address=OLED_MenuAddressValue&0x00FFFFFF;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuTxPower:
      if(Parameters.TxPower!=OLED_MenuTxPowerValue)
      { Parameters.TxPower=OLED_MenuTxPowerValue;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuLookOutWarnTime:
      if(Parameters.LookOutWarnTime!=OLED_MenuLookOutWarnTimeValue)
      { Parameters.LookOutWarnTime=OLED_MenuLookOutWarnTimeValue;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuAlert:
      if(Parameters.AlertThresh!=OLED_MenuAlertValue)
      { Parameters.AlertThresh=OLED_MenuAlertValue;
        OLED_MenuSaveParameters(); }
      break;
    case OLED_MenuGhost:
      if(Parameters.GhostMode!=OLED_MenuGhostValue)
      { Parameters.GhostMode=OLED_MenuGhostValue;
        OLED_MenuSaveParameters(); }
      else
      { OLED_MenuShowMessage("Saved", 0);
        OLED_MenuBeepSaved(); }
      break;
    case OLED_MenuTextEdit:
      { char Value[FlashParameters::InfoParmLen];
        memcpy(Value, OLED_MenuTextValue, OLED_MenuTextLength);
        Value[OLED_MenuTextLength]=0;
        for(int Idx=OLED_MenuTextLength-1; Idx>=0 && Value[Idx]==' '; Idx--)
          Value[Idx]=0;
        char *Target=OLED_MenuTextTarget();
        if(Target && strcmp(Target, Value)!=0)
        { strcpy(Target, Value);
          OLED_MenuSaveParameters(); }
        else
        { OLED_MenuShowMessage("Saved", 0);
          OLED_MenuBeepSaved(); } }
      break;
    default: break; }
  OLED_Menu=OLED_MenuList;
  OLED_PageChange=true; }

static void OLED_MenuFormatFlash(void)
{
#ifdef WITH_LOG
  if(FlashLog_isOpen())
  { OLED_MenuShowMessage("Log open", -1);
    OLED_Menu=OLED_MenuList;
    OLED_PageChange=true;
    return; }
#endif
  bool Formatted=LogFS_format(Serial);
#ifdef WITH_LOG
  FlashLog_RequestStorageUpdate();
#endif
  HardwareStatus.SPIFFS=Formatted;
  int Result=Formatted ? Parameters.WriteToNVS() : -1;
  if(Formatted) LogFS_listRoot(Serial);
  OLED_MenuShowMessage(Result<0 ? "ERROR" : "Formatted", Result);
  if(Result>=0) OLED_MenuBeepSaved();
  OLED_Menu=OLED_MenuList;
  OLED_PageChange=true;
}

static void OLED_MenuResetDefaults(void)
{ Parameters.setDefault();
  int Result=Parameters.WriteToNVS();
  OLED_MenuShowMessage(Result<0 ? "ERROR" : "Defaults", Result);
  if(Result>=0) OLED_MenuBeepSaved();
  OLED_Menu=OLED_MenuList;
  OLED_PageChange=true;
}

static void OLED_MenuHandleEvent(uint32_t Event)
{ if(Event&(OLED_EventMenuClick|OLED_EventMenuLong))
  { if(!OLED_MenuCenterGesturePending && OLED_MenuGestureOwner!=0) return;
    if(OLED_MenuCenterGesturePending)
    { bool Allowed=OLED_MenuCenterGesturePrincipal &&
                   (OLED_MenuGestureOwner==0 || OLED_MenuGestureOwner==OLED_MenuGestureCenter);
      OLED_MenuCenterGesturePending=false;
      if(!Allowed) return; }
  }
  if(Event&OLED_EventMenuLong)
  { if(OLED_Menu==OLED_MenuClosed) OLED_MenuOpen();
    else if(OLED_Menu==OLED_MenuFormatConfirm) OLED_MenuFormatFlash();
    else if(OLED_Menu==OLED_MenuDefaultsConfirm) OLED_MenuResetDefaults();
#ifdef WITH_USB_MEMORY
    else if(OLED_Menu==OLED_MenuUSBConfirm)
    { if(USBMemory_Enter())
      { OLED_Menu=OLED_MenuClosed;
        OLED_MenuMessage=0; }
      else
      { OLED_MenuShowMessage("Flash unavailable", -1);
        OLED_Menu=OLED_MenuList;
        OLED_PageChange=true; } }
#endif
#ifdef WITH_LORAWAN
    else if(OLED_Menu==OLED_MenuTTNConfirm) OLED_MenuRegisterTTN();
#endif
    else if(OLED_Menu!=OLED_MenuList) OLED_MenuCommitItem();
  }
  if(Event&OLED_EventMenuClick)
  { if(OLED_Menu==OLED_MenuList) OLED_MenuEnterItem();
    else if(OLED_Menu!=OLED_MenuClosed)
    { OLED_Menu=OLED_MenuList; OLED_PageChange=true; } }
}

static void OLED_MenuPollJoystick(void)
{ static bool First=true;
  static uint8_t Previous=0;
  static uint8_t TextRepeatKey=0;
  static uint32_t TextRepeatStart=0;
  static uint32_t TextRepeatNext=0;
  uint8_t Current=0;
  if(digitalRead(Trackball_PinUp)==LOW)    Current|=1u<<0;
  if(digitalRead(Trackball_PinDown)==LOW)  Current|=1u<<1;
  if(digitalRead(Trackball_PinLeft)==LOW)  Current|=1u<<2;
  if(digitalRead(Trackball_PinRight)==LOW) Current|=1u<<3;
  if(digitalRead(Trackball_PinPress)==LOW) Current|=OLED_MenuGestureCenter;
  if(OLED_MenuGestureOwner==0 && Current)
  { if((Current&(Current-1))==0) OLED_MenuGestureOwner=Current;
    else OLED_MenuGestureOwner=OLED_MenuGestureBlocked; }
  if(Current==0) OLED_MenuGestureOwner=0;
  if(First) { Previous=Current; First=false; return; }
  uint8_t Pressed=Current&~Previous;
  Previous=Current;
  if(Pressed&OLED_MenuGestureCenter)
  { OLED_MenuCenterGesturePending=true;
    OLED_MenuCenterGesturePrincipal=OLED_MenuGestureOwner==OLED_MenuGestureCenter; }
#if defined(WITH_WIO_TRACKER)
  if(OLED_KeypadLocked)
  { TextRepeatKey=0;
    OLED_MenuCenterGesturePending=false;
    return; }
#endif
  if(OLED_MenuGestureOwner==OLED_MenuGestureBlocked ||
     OLED_MenuGestureOwner==OLED_MenuGestureCenter)
    Pressed=0;
  else Pressed&=OLED_MenuGestureOwner;
  if(OLED_Menu==OLED_MenuClosed)
  { uint8_t PagePresses=Pressed&((1u<<2)|(1u<<3));
    if(PagePresses)
    { bool WasOff=OLED_PageOFF;
      OLED_PageOFF=false;
      if(WasOff)
      { OLED_SetPowerSave(false);
        OLED_PageChange=true; }
      else if(PagePresses&(1u<<2)) OLED_PreviousPage();
      else OLED_NextPage();
      OLED_PageActive=millis(); } }
  else if(OLED_Menu==OLED_MenuList)
  { if(Pressed&(1u<<0))
    { if(OLED_MenuItem==0) OLED_MenuItem=OLED_MenuItems-1; else OLED_MenuItem--; OLED_PageChange=true; }
    if(Pressed&(1u<<1))
    { OLED_MenuItem++; if(OLED_MenuItem>=OLED_MenuItems) OLED_MenuItem=0; OLED_PageChange=true; }
  }
  else if(OLED_Menu==OLED_MenuAcftType)
  { if(Pressed&(1u<<0)) OLED_MenuChangeAcftType(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeAcftType(-1); }
  else if(OLED_Menu==OLED_MenuAddrType)
  { if(Pressed&(1u<<0)) OLED_MenuChangeAddrType(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeAddrType(-1); }
  else if(OLED_Menu==OLED_MenuAddress)
  { if(Pressed&(1u<<0)) OLED_MenuChangeAddressNibble(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeAddressNibble(-1);
    if(Pressed&(1u<<2))
    { if(OLED_MenuAddressNibble==0) OLED_MenuAddressNibble=5; else OLED_MenuAddressNibble--; OLED_PageChange=true; }
    if(Pressed&(1u<<3))
    { OLED_MenuAddressNibble++; if(OLED_MenuAddressNibble>=6) OLED_MenuAddressNibble=0; OLED_PageChange=true; } }
  else if(OLED_Menu==OLED_MenuTxPower)
  { if(Pressed&(1u<<0) || Pressed&(1u<<1)) OLED_MenuChangeTxPower(Pressed&(1u<<0) ? +1 : -1); }
  else if(OLED_Menu==OLED_MenuLookOutWarnTime)
  { if(Pressed&(1u<<0)) OLED_MenuChangeLookOutWarnTime(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeLookOutWarnTime(-1); }
  else if(OLED_Menu==OLED_MenuAlert)
  { if(Pressed&(1u<<0)) OLED_MenuChangeAlert(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeAlert(-1); }
  else if(OLED_Menu==OLED_MenuGhost)
  { if(Pressed&(1u<<0)) OLED_MenuChangeGhost(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeGhost(-1); }
  else if(OLED_Menu==OLED_MenuTextEdit)
  { uint8_t Direction=0;
    if(OLED_MenuGestureOwner==(1u<<0) && (Current&(1u<<0))) Direction=1;
    if(OLED_MenuGestureOwner==(1u<<1) && (Current&(1u<<1))) Direction=2;
    uint8_t PressedDirection=Pressed&((1u<<0)|(1u<<1));
    uint32_t Now=millis();
    if(PressedDirection && Direction)
    { OLED_MenuChangeText(Direction==1 ? +1 : -1);
      TextRepeatKey=Direction;
      TextRepeatStart=Now;
      TextRepeatNext=Now+350; }
    else if(Direction && Direction==TextRepeatKey && (int32_t)(Now-TextRepeatNext)>=0)
    { OLED_MenuChangeText(Direction==1 ? +1 : -1);
      uint32_t Held=Now-TextRepeatStart;
      uint32_t Interval=Held>1500 ? 50 : (Held>800 ? 90 : 140);
      TextRepeatNext=Now+Interval; }
    else if(Direction==0) TextRepeatKey=0;
    if(Pressed&(1u<<2))
    { if(OLED_MenuTextPosition==0) OLED_MenuTextPosition=OLED_MenuTextLength-1;
      else OLED_MenuTextPosition--; OLED_PageChange=true; }
    if(Pressed&(1u<<3))
    { OLED_MenuTextPosition++;
      if(OLED_MenuTextPosition>=OLED_MenuTextLength) OLED_MenuTextPosition=0;
      OLED_PageChange=true; } }
  else TextRepeatKey=0;
}

static void OLED_MenuDraw(u8g2_t *Display, const GPS_Position *GPS)
{
#ifdef WITH_LORAWAN
  if(OLED_Menu==OLED_MenuQR)
  { const uint8_t Scale=2;
    const uint8_t Size=OLED_QRCode.size;
    const uint8_t X=(64-Size*Scale)/2;
    const uint8_t Y=(64-Size*Scale)/2;
    u8g2_SetDrawColor(Display, 1);
    u8g2_DrawBox(Display, 0, 0, 128, 64);
    u8g2_SetDrawColor(Display, 0);
    for(uint8_t Row=0; Row<Size; Row++)
      for(uint8_t Col=0; Col<Size; Col++)
        if(qrcode_getModule(&OLED_QRCode, Col, Row))
          u8g2_DrawBox(Display, X+Col*Scale, Y+Row*Scale, Scale, Scale);
    // Keep the identity and the GPS UTC time next to the QR code.  The
    // identity is split over two lines because the complete 16-digit MAC
    // does not fit in the right half of the OLED.
    char MAC[17];
    Format_Hex(MAC, getUniqueID());
    MAC[16]=0;
    char MACtail[9];
    memcpy(MACtail, MAC+8, 8);
    MACtail[8]=0;
    u8g2_SetFont(Display, u8g2_font_5x8_tr);
    u8g2_DrawStr(Display, 66, 8,  "TTN CONFIG");
    // u8g2_DrawStr(Display, 66, 16, "MAC");
    MAC[8]=0;
    u8g2_DrawStr(Display, 66, 24, MAC);
    u8g2_DrawStr(Display, 66, 32, MACtail);

    char Date[10];
    if(GPS && GPS->isDateValid())
    { GPS->FormatDate_DDMMYY(Date);
      u8g2_DrawStr(Display, 66, 40, Date); }
    else u8g2_DrawStr(Display, 66, 40, "GPS wait");

    char Time[10];
    if(GPS && GPS->isTimeValid())
    { GPS->FormatTime(Time);
      // FormatTime() includes milliseconds; the compact QR screen only
      // needs whole seconds and marks the value as UTC.
      Time[8]='Z'; Time[9]=0;
      u8g2_DrawStr(Display, 66, 48, Time); }
    else u8g2_DrawStr(Display, 66, 48, "GPS wait");
    u8g2_DrawStr(Display, 66, 60, "SEND PHOTO");
    u8g2_SetDrawColor(Display, 1);
    return;
  }
#endif
  u8g2_SetFont(Display, u8g2_font_7x13_tf);
  if(OLED_Menu==OLED_MenuList)
  { static const char *ItemNames[OLED_MenuItems] =
    { "AcftType", "AddrType", "Address", "Tx power", "Warn time", "Alerts", "Ghost", "Reg", "Pilot", "Format flash", "Reset defaults"
#ifdef WITH_USB_MEMORY
      , "USB memory"
#endif
#ifdef WITH_LORAWAN
      , "Register TTN"
#endif
    };
    uint8_t First=OLED_MenuItem>1 ? OLED_MenuItem-1 : 0;
    if(First+3>OLED_MenuItems) First=OLED_MenuItems-3;
    u8g2_DrawStr(Display, 0, 22, "OGN settings");
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    for(uint8_t Row=0; Row<3; Row++)
    { uint8_t Item=First+Row;
      char Value[32];
      if(Item==OLED_MenuItem) strcpy(Value, ">"); else strcpy(Value, " ");
      strcat(Value, ItemNames[Item]);
      if(Item==0) { strcat(Value, " "); strcat(Value, OLED_MenuAcftTypeNames[Parameters.AcftType<16 ? Parameters.AcftType : 0]); }
      if(Item==1) { strcat(Value, " "); strcat(Value, OLED_MenuAddrTypeNames[Parameters.AddrType<4 ? Parameters.AddrType : 0]); }
      if(Item==2) { sprintf(Value+strlen(Value), " %06X", Parameters.Address&0x00FFFFFF); }
      if(Item==3) { int TxPower=Parameters.TxPower; if(TxPower<0) TxPower=0; if(TxPower>22) TxPower=22; sprintf(Value+strlen(Value), " %ddBm", TxPower); }
      if(Item==4) { sprintf(Value+strlen(Value), " %ds", OLED_MenuLookOutWarnTimes[Parameters.LookOutWarnTime<4 ? Parameters.LookOutWarnTime : 0]); }
      if(Item==5) { strcat(Value, " "); strcat(Value, OLED_MenuAlertNames[Parameters.AlertThresh<=4 ? Parameters.AlertThresh : 4]); }
      if(Item==6) { strcat(Value, " "); strcat(Value, OLED_MenuGhostNames[Parameters.GhostMode>=2 ? 2 : Parameters.GhostMode]); }
      if(Item==7) { strcat(Value, " "); strcat(Value, Parameters.Reg); }
      if(Item==8) { strcat(Value, " "); strcat(Value, Parameters.Pilot); }
      u8g2_DrawStr(Display, 0, 34+12*Row, Value); }
  }
  else if(OLED_Menu==OLED_MenuAcftType)
  { u8g2_DrawStr(Display, 0, 25, "Aircraft type");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    u8g2_DrawStr(Display, 0, 45, OLED_MenuAcftTypeNames[OLED_MenuAcftTypeValue]);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    sprintf(Line, "%d/15 Up/Dn Long=save", OLED_MenuAcftTypeValue);
    u8g2_DrawStr(Display, 0, 61, Line); }
  else if(OLED_Menu==OLED_MenuAddrType)
  { u8g2_DrawStr(Display, 0, 25, "Address type");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    u8g2_DrawStr(Display, 0, 45, OLED_MenuAddrTypeNames[OLED_MenuAddrTypeValue]);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "U/D Long=OK"); }
  else if(OLED_Menu==OLED_MenuAddress)
  { char Address[7];
    for(uint8_t Idx=0; Idx<6; Idx++) Address[Idx]=HexDigit((OLED_MenuAddressValue>>((5-Idx)*4))&0x0F);
    Address[6]=0;
    u8g2_DrawStr(Display, 0, 25, "Address");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    u8g2_DrawStr(Display, 20, 45, Address);
    u8g2_DrawHLine(Display, 20+9*OLED_MenuAddressNibble, 48, 8);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "L/R digit U/D Long OK"); }
  else if(OLED_Menu==OLED_MenuTxPower)
  { u8g2_DrawStr(Display, 0, 25, "Tx power");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    sprintf(Line, "%d dBm", OLED_MenuTxPowerValue);
    u8g2_DrawStr(Display, 0, 45, Line);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "U/D Long=OK"); }
  else if(OLED_Menu==OLED_MenuLookOutWarnTime)
  { u8g2_DrawStr(Display, 0, 25, "Warning time");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    sprintf(Line, "%d sec", OLED_MenuLookOutWarnTimes[OLED_MenuLookOutWarnTimeValue]);
    u8g2_DrawStr(Display, 0, 45, Line);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "U/D Long=OK"); }
  else if(OLED_Menu==OLED_MenuAlert)
  { u8g2_DrawStr(Display, 0, 25, "Alert level");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    u8g2_DrawStr(Display, 0, 45, OLED_MenuAlertNames[OLED_MenuAlertValue]);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "U/D Long=OK"); }
  else if(OLED_Menu==OLED_MenuGhost)
  { u8g2_DrawStr(Display, 0, 25, "Ghost mode");
    u8g2_SetFont(Display, u8g2_font_9x15_tr);
    u8g2_DrawStr(Display, 0, 45, OLED_MenuGhostNames[OLED_MenuGhostValue]);
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 61, "U/D Long=OK"); }
  else if(OLED_Menu==OLED_MenuTextEdit)
  { u8g2_DrawStr(Display, 0, 25, OLED_MenuTextTitle());
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 45, OLED_MenuTextValue);
    u8g2_DrawHLine(Display, 6*OLED_MenuTextPosition, 48, 6);
    u8g2_DrawStr(Display, 0, 61, "L/R char U/D Long OK"); }
  else if(OLED_Menu==OLED_MenuFormatConfirm)
  { u8g2_DrawStr(Display, 0, 25, "FORMAT EXT FLASH?");
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 42, "Long=YES");
    u8g2_DrawStr(Display, 0, 58, "Short=cancel"); }
  else if(OLED_Menu==OLED_MenuDefaultsConfirm)
  { u8g2_DrawStr(Display, 0, 25, "RESET DEFAULTS?");
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 42, "Long=YES");
    u8g2_DrawStr(Display, 0, 58, "Short=cancel"); }
#ifdef WITH_USB_MEMORY
  else if(OLED_Menu==OLED_MenuUSBConfirm)
  { u8g2_DrawStr(Display, 0, 22, "USB MEMORY MODE?");
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 36, "Stops tracker");
    u8g2_DrawStr(Display, 0, 48, "Full read/write flash");
    u8g2_DrawStr(Display, 0, 61, "Long=YES  Short=cancel"); }
#endif
#ifdef WITH_LORAWAN
  else if(OLED_Menu==OLED_MenuTTNConfirm)
  { u8g2_DrawStr(Display, 0, 25, "REGISTER TTN?");
    u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 0, 38, OLED_MenuTTNConfigured() ? "Current TTN erased" : "New TTN setup");
    u8g2_DrawStr(Display, 0, 50, "Long=YES");
    u8g2_DrawStr(Display, 0, 62, "Short=cancel"); }
#endif
  if(OLED_MenuMessage && (uint32_t)(millis()-OLED_MenuMessageTime)<2500)
  { u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 82, 22, OLED_MenuMessage); }
}
#endif

void OLED_ButtonSingle(void)
{ if(OLED_TaskHandle) xTaskNotify(OLED_TaskHandle, OLED_EventPageButton, eSetBits); }

void OLED_TakeoffDetected(void)
{ if(OLED_TaskHandle) xTaskNotify(OLED_TaskHandle, OLED_EventTakeoff, eSetBits); }

#if defined(WITH_WIO_TRACKER)
void OLED_ButtonLong(void)
{ if(OLED_TaskHandle) xTaskNotify(OLED_TaskHandle, OLED_EventPageLong, eSetBits); }
#endif

static bool OLED_PageAvailable(uint8_t Page)
{ switch(Page)
  { case OLED_Page_ID:
    case OLED_Page_GPS:
    case OLED_Page_SatSNR:
    case OLED_Page_RF:
    case OLED_Page_RFcounts:
#if 0 && defined(WITH_TASK_STATS) // Temporarily hide the incomplete OLED task-status page.
    case OLED_Page_TaskStats:
#endif
    case OLED_Page_Power:
    case OLED_Page_RelayOGN:
    case OLED_Page_RelayADSL:
#ifdef WITH_LOOKOUT
    case OLED_Page_LookOut:
#endif
#ifdef WITH_LOG
    case OLED_Page_Log:
#endif
#ifdef WITH_LORAWAN
    case OLED_Page_LoRaWAN:
#endif
    case OLED_Page_Return:
      return true;
    case OLED_Page_Baro:
#if defined(WITH_BMP180) || defined(WITH_BMP280) || defined(WITH_MS5607) || defined(WITH_BME280) || defined(WITH_MS5611)
      return true;
#else
      return false;
#endif
    default:
      return false; } }

static void OLED_NextPage(void)
{ for(uint8_t Idx=0; Idx<OLED_Pages; Idx++)
  { OLED_Page++;
    if(OLED_Page>=OLED_Pages) OLED_Page=0;
    if(OLED_PageAvailable(OLED_Page)) break; }
  OLED_PageChange=true; }

static void OLED_PreviousPage(void)
{ for(uint8_t Idx=0; Idx<OLED_Pages; Idx++)
  { if(OLED_Page==0) OLED_Page=OLED_Pages-1; else OLED_Page--;
    if(OLED_PageAvailable(OLED_Page)) break; }
  OLED_PageChange=true; }

static void OLED_SetPowerSave(bool PowerSave)
{ if(xSemaphoreTake(I2C_Mutex, 50))
  { OLED.setPowerSave(PowerSave ? 1 : 0);
    xSemaphoreGive(I2C_Mutex); } }

static int OLED_DrawPage(const GPS_Position *GPS)
{
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  if(OLED_MenuActive())
  { OLED.clearBuffer();
#ifdef WITH_LORAWAN
    if(OLED_Menu==OLED_MenuQR)
      OLED_MenuDraw(OLED.getU8g2(), GPS);
    else
#endif
    { OLED_DrawStatusBar(OLED.getU8g2(), GPS);
      OLED_MenuDraw(OLED.getU8g2(), GPS); }
    if(xSemaphoreTake(I2C_Mutex, 50))
    { OLED.sendBuffer();
      xSemaphoreGive(I2C_Mutex); }
    return 1; }
#endif
  if(OLED_PageOFF) return 1;
  if(!OLED_PageAvailable(OLED_Page)) return 0;
  OLED.clearBuffer();
  switch(OLED_Page)
  { case OLED_Page_ID:        OLED_DrawID       (OLED.getU8g2(), GPS); break;
    case OLED_Page_GPS:       OLED_DrawGPS      (OLED.getU8g2(), GPS); break;
    case OLED_Page_SatSNR:    OLED_DrawSatSNR   (OLED.getU8g2(), GPS); break;
    case OLED_Page_Baro:      OLED_DrawBaro      (OLED.getU8g2(), GPS); break;
    case OLED_Page_RF:        OLED_DrawRF        (OLED.getU8g2(), GPS); break;
    case OLED_Page_RFcounts:  OLED_DrawRFcounts  (OLED.getU8g2(), GPS); break;
#if 0 && defined(WITH_TASK_STATS) // Temporarily hide the incomplete OLED task-status page.
    case OLED_Page_TaskStats: OLED_DrawTaskStats  (OLED.getU8g2(), GPS); break;
#endif
    case OLED_Page_Power:     OLED_DrawPower     (OLED.getU8g2(), GPS); break;
    case OLED_Page_RelayOGN:  OLED_DrawRelayOGN  (OLED.getU8g2(), GPS); break;
    case OLED_Page_RelayADSL: OLED_DrawRelayADSL (OLED.getU8g2(), GPS); break;
#ifdef WITH_LOOKOUT
    case OLED_Page_LookOut:   OLED_DrawLookOut   (OLED.getU8g2(), GPS); break;
#endif
#ifdef WITH_LOG
    case OLED_Page_Log:       OLED_DrawLogPage   (OLED.getU8g2(), GPS); break;
#endif
#ifdef WITH_LORAWAN
    case OLED_Page_LoRaWAN:   OLED_DrawLoRaWAN   (OLED.getU8g2(), GPS); break;
#endif
    case OLED_Page_Return:    OLED_DrawReturn    (OLED.getU8g2(), GPS); break;
    default: return 0; }
  OLED_DrawStatusBar(OLED.getU8g2(), GPS);
  if(xSemaphoreTake(I2C_Mutex, 50))
  { OLED.sendBuffer();
    xSemaphoreGive(I2C_Mutex); }
  return 1; }

static void OLED_Init(void)
{ if(xSemaphoreTake(I2C_Mutex, 100))
  { OLED.setI2CAddress(0x3D<<1);
    OLED.begin();
    xSemaphoreGive(I2C_Mutex); }
  OLED.clearBuffer();
  OLED_DrawLogo(OLED.getU8g2(), 0);
  if(xSemaphoreTake(I2C_Mutex, 50))
  { OLED.sendBuffer();
    xSemaphoreGive(I2C_Mutex); } }

static void OLED_HandleButton(void)
{
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  if(OLED_MenuActive())
  { OLED_MenuClose();
    return; }
#endif
  if(OLED_PageOFF)
  { OLED_PageOFF=false;
    OLED_SetPowerSave(false);
    OLED_PageChange=true; }
  else OLED_NextPage();
#ifdef WITH_OLED_DIM
  OLED_PageActive=millis();
#endif
}

static void OLED_HandleTakeoff(void)
{
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  if(OLED_MenuActive()) OLED_MenuClose();
#endif
  OLED_Page=OLED_Page_Return;
  OLED_PageOFF=false;
#ifdef WITH_OLED_DIM
  OLED_PageActive=millis();
#endif
  OLED_SetPowerSave(false);
  OLED_PageChange=true;
}

#if defined(WITH_WIO_TRACKER)
static void OLED_HandleKeypadLock(void)
{ OLED_KeypadLocked = !OLED_KeypadLocked;
#if defined(WITH_OLED_MENU)
  if(OLED_KeypadLocked && OLED_MenuActive()) OLED_MenuClose();
#endif
  if(OLED_KeypadLocked)
  { Play(Play_Vol_1 | Play_Oct_0 | 0x03, 100); }
  else
  { Play(Play_Vol_1 | Play_Oct_0 | 0x08, 100);
    Play(Play_Vol_1 | Play_Oct_0 | 0x05, 70); }
  OLED_PageChange=true;
}
#endif

#ifdef WITH_USB_MEMORY
static void OLED_DrawUSBMemoryMode(u8g2_t *Display)
{ u8g2_SetFont(Display, u8g2_font_7x13_tf);
  u8g2_DrawStr(Display, 0, 18, "USB MEMORY MODE");
  u8g2_SetFont(Display, u8g2_font_6x12_tr);
  u8g2_DrawStr(Display, 0, 34, "External flash FAT");
  u8g2_DrawStr(Display, 0, 44, "read/write enabled");
  u8g2_DrawStr(Display, 0, 54, "Reset or repower");
  u8g2_DrawStr(Display, 0, 64, "to resume tracker"); }
#endif

void OLED_Task(void *Parms)
{
  (void)Parms;
  OLED_TaskHandle = xTaskGetCurrentTaskHandle();
  OLED_Init();
  vTaskDelay(pdMS_TO_TICKS(2000)); // leave the startup logo visible briefly

  GPS_Position *PrevGPS=0;
  for( ; ; )
  {
    TaskWatchdog_Heartbeat(TaskWatchdog_OLED);
    uint32_t Events=0;
    xTaskNotifyWait(0, 0xFFFFFFFF, &Events, 0);
#if defined(WITH_WIO_TRACKER)
    if(Events&OLED_EventPageLong) OLED_HandleKeypadLock();
    if(!OLED_KeypadLocked)
    { if(Events&OLED_EventPageButton) OLED_HandleButton(); }
    else if((Events&OLED_EventPageButton) && OLED_PageOFF)
    { OLED_PageOFF=false;
      OLED_SetPowerSave(false);
      OLED_PageChange=true;
#ifdef WITH_OLED_DIM
      OLED_PageActive=millis();
#endif
    }
#else
    if(Events&OLED_EventPageButton) OLED_HandleButton();
#endif
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
    OLED_MenuPollJoystick();
    if(!OLED_KeypadLocked) OLED_MenuHandleEvent(Events);
#endif
    if(Events&OLED_EventTakeoff) OLED_HandleTakeoff();

#ifdef WITH_USB_MEMORY
    if(USBMemory_IsActive())
    { OLED.clearBuffer();
      OLED_DrawUSBMemoryMode(OLED.getU8g2());
      if(xSemaphoreTake(I2C_Mutex, 50))
      { OLED.sendBuffer();
        xSemaphoreGive(I2C_Mutex); }
      vTaskSuspend(NULL);                         // remain in USB mode until reset
    }
#endif

    GPS_Position *GPS = GPS_getPosition();
    if(GPS==0) GPS = GPS_Pos+GPS_PosIdx;
    if(GPS!=PrevGPS)
    {
      OLED_PageChange=true;
#ifdef WITH_OLED_DIM
      uint32_t msTime = millis();
      bool USBpowered = (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk)!=0;
      bool GPSlocked = GPS && GPS->isValid();
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
      if(OLED_MenuActive())
      { OLED_PageOFF=false;
        OLED_PageActive=msTime; }
      else
#endif
      { if(USBpowered || !GPSlocked) OLED_PageActive = msTime;
        uint32_t Age = msTime-OLED_PageActive;
        OLED_PageOFF = Age>OLED_PageTimeout; }
#else
      OLED_PageOFF = false;
#endif
      OLED_SetPowerSave(OLED_PageOFF);
      PrevGPS=GPS;
    }

    if(OLED_PageChange)
    { OLED_PageChange=false;
      if(OLED_DrawPage(GPS)==0) OLED_NextPage(); }
#if 0 && defined(WITH_TASK_STATS) // Temporarily hide the incomplete OLED task-status page.
    if((int32_t)(millis()-TaskStatsUpdateTime)>=0)
    { TaskStats_Update();
      TaskStatsUpdateTime=millis()+10000;
      if(OLED_Page==OLED_Page_TaskStats) OLED_PageChange=true; }
#endif
    vTaskDelay(50);
  }
}

void OLED_DrawLogo(u8g2_t *OLED, const GPS_Position *GPS)  // draw logo and hardware options in software
{ u8g2_DrawCircle(OLED, 96, 32, 30, U8G2_DRAW_ALL);
  u8g2_DrawCircle(OLED, 96, 32, 34, U8G2_DRAW_UPPER_RIGHT);
  u8g2_DrawCircle(OLED, 96, 32, 38, U8G2_DRAW_UPPER_RIGHT);
  // u8g2_SetFont(OLED, u8g2_font_open_iconic_all_4x_t);
  // u8g2_DrawGlyph(OLED, 64, 32, 0xF0);
  u8g2_SetFont(OLED, u8g2_font_ncenB14_tr);
  u8g2_DrawStr(OLED, 74, 31, "OGN");
  u8g2_SetFont(OLED, u8g2_font_8x13_tr);
  u8g2_DrawStr(OLED, 69, 43, "Tracker");

  u8g2_DrawStr(OLED,  0, 16 ,"nRF52840");
#ifdef WITH_GPS_PCAS
  u8g2_DrawStr(OLED,  0, 28 ,"PCAS GPS");
#endif
#ifdef WITH_GPS_MTK
  u8g2_DrawStr(OLED,  0, 28 ,"MTK GPS");
#endif
#ifdef WITH_GPS_UBX
  u8g2_DrawStr(OLED,  0, 28 ,"UBX GPS");
#endif
#ifdef WITH_SX1262
  u8g2_DrawStr(OLED,  0, 40 ,"SX1262");
#endif
#ifdef WITH_SX1276
  u8g2_DrawStr(OLED,  0, 40 ,"SX1276");
#endif
#ifdef WITH_BME280
  u8g2_DrawStr(OLED,  0, 52 ,"BME280");
#endif
#ifdef WITH_BLE_SPP
  u8g2_DrawStr(OLED,  0, 52 ,"BLE SPP");
#endif
}

static int8_t BattCapacity(uint16_t mVolt) // deduce battery capacity from its voltage
{ if(mVolt>=4100) return 100;              // if 4.1V or more then full
  if(mVolt<=1000) return  -1;              // if below 1.0V then no-battery
  if(mVolt<=3600) return   0;              // if below 3.6V then empty
  return (mVolt-3600+2)/5; }               // otherwise a linear function from 3.6V to 4.1V

void OLED_DrawStatusBar(u8g2_t *OLED, const GPS_Position *GPS)   // status bar on top of the OLED
{ static bool Odd=0;
  int8_t Cap = BattCapacity(BatteryVoltage>>8);           // [%] est. battery capacity
  uint8_t BattLev = (Cap+10)/20;                          // [0..5] convert to display scale
  uint8_t Charging = BatteryVoltageRate>0;                // charging or not changing ?
  static uint8_t DispLev = 0;
  if(Charging==1 || Charging==2) { DispLev++; if(DispLev>5) DispLev = BattLev?BattLev-1:0; }
                           else  { DispLev = BattLev; }
  if(Cap>=0)
  { if(BattLev==0 && !Charging && Odd)                // when battery is empty, then flash it at 0.5Hz
    { }                                               // thus here avoid printing the battery symbol for flashing effect
    else                                              // print the battery symbol with DispLev
    { u8g2_SetFont(OLED, u8g2_font_battery19_tn);
      u8g2_SetFontDirection(OLED, 3);
      u8g2_DrawGlyph(OLED, 20, 10, '0'+DispLev);
      u8g2_SetFontDirection(OLED, 0); }
    Odd=!Odd; }

#ifdef WITH_BLE_SPP
  if(BLE_isConnected())
  { u8g2_SetFont(OLED, u8g2_font_open_iconic_all_1x_t);
    u8g2_DrawGlyph(OLED, 36, 11, 0x5E); } // 0x4A
#endif

#if defined(WITH_WIO_TRACKER)
  if(OLED_KeypadLocked)
  { u8g2_SetFont(OLED, u8g2_font_open_iconic_all_1x_t);
    u8g2_DrawGlyph(OLED, 42, 11, 0xC1); } // Open Iconic: key
#endif

/*
#ifdef WITH_SD
  if(SD_isMounted())
  { u8g2_SetFont(OLED, u8g2_font_twelvedings_t_all);
    u8g2_DrawGlyph(OLED, 24, 12, 0x73); }
#endif
#ifdef WITH_WIFI
  if(WIFI_isConnected())
  { u8g2_SetFont(OLED, u8g2_font_open_iconic_all_1x_t);
    u8g2_DrawGlyph(OLED, 43, 11, 0x119); } // 0x50
#endif
#ifdef WITH_AP
  if(WIFI_isAP())
  { u8g2_SetFont(OLED, u8g2_font_open_iconic_all_1x_t);
    u8g2_DrawGlyph(OLED, 43, 11, 0xF8); } // 0x50
#endif
*/
  static uint8_t Sec=0;
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);
  strcpy(Line, "--sat --:--Z");
  if(GPS && GPS->isTimeValid())
  { Format_UnsDec (Line+6, (uint32_t)GPS->Hour,  2, 0); Line[8]=':';
    Format_UnsDec (Line+9, (uint32_t)GPS->Min,   2, 0);
  } else Format_String(Line+6, "--:--");
  if(GPS)
  { if(Sec)
    { Format_UnsDec(Line, (uint32_t)GPS->Satellites,  2); memcpy(Line+2, "sat", 3); }
    else
    { Format_UnsDec(Line, (uint32_t)(GPS_SatSNR+2)/4,  2); memcpy(Line+2, "dB ", 3);}
  }
  else Format_String(Line, "--sat");
  u8g2_DrawStr(OLED, 52, 10, Line);
  Sec++; if(Sec>=3) Sec=0; }

void OLED_DrawSatSNR(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];

  // u8g2_SetFont(OLED, u8g2_font_ncenB14_tr);
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);              // 5 lines, 12 pixels/line

  int Vert=24;
  for(uint8_t Sys=1; Sys<=4; Sys++)
  { int Len=sprintf(Line, "%s:%2d:%2d", GPS_Sat::SysName(Sys), GPS_SatMon.FixSats[Sys], GPS_SatMon.VisSats[Sys]);
    uint8_t SNR=GPS_SatMon.VisSNR[Sys];
    if(SNR>0) Len+=sprintf(Line+Len, " %4.1fdB", 0.25*SNR);
         // else Len+=sprintf(Line+Len, " --.-dB");
    Line[Len]=0;
    u8g2_DrawStr(OLED, 0, Vert, Line);
    Vert+=12; }
}

void OLED_DrawGPS(u8g2_t *OLED, const GPS_Position *GPS)  // GPS time, position, altitude
{ // u8g2_SetFont(OLED, u8g2_font_ncenB14_tr);
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);              // 5 lines, 12 pixels/line
  uint8_t Len=0;
  if(GPS && GPS->isDateValid())
  { Format_UnsDec (Line   , (uint32_t)GPS->Day,   2, 0); Line[2]='.';
    Format_UnsDec (Line+ 3, (uint32_t)GPS->Month, 2, 0); Line[5]='.';
    Format_UnsDec (Line+ 6, (uint32_t)GPS->Year , 2, 0); Line[8]=' ';
  } else Format_String(Line, "  .  .   ");
  if(GPS && GPS->isTimeValid())
  { Format_UnsDec (Line+ 9, (uint32_t)GPS->Hour,  2, 0); Line[11]=':';
    Format_UnsDec (Line+12, (uint32_t)GPS->Min,   2, 0); Line[14]=':';
    Format_UnsDec (Line+15, (uint32_t)GPS->Sec,   2, 0);
  } else Format_String(Line+9, "  :  :  ");
  Line[17]=0;
  u8g2_DrawStr(OLED, 0, 24, Line);

  Len=0;
  Len+=Format_String(Line+Len, "Lat:  ");
  if(GPS && GPS->isValid())
  { Len+=Format_SignDec(Line+Len,  GPS->Latitude /6, 7, 5);
    Line[Len++]=0xB0; }
  else Len+=Format_String(Line+Len, "---.-----");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 36, Line);
  Len=0;
  Len+=Format_String(Line+Len, "Lon: ");
  if(GPS && GPS->isValid())
  { Len+=Format_SignDec(Line+Len,  GPS->Longitude /6, 8, 5);
    Line[Len++]=0xB0; }
  else Len+=Format_String(Line+Len, "----.-----");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 48, Line);

  // const bool isAltitudeUnitMeter=1;
  Len=0;
  Len+=Format_String(Line+Len, "Alt: ");
  if(GPS && GPS->isValid())
  { int32_t Alt = GPS->Altitude;
    if(Alt>=0) Line[Len++]=' ';
    // if(isAltitudeUnitMeter)                                       // display altitude in meters
    { Len+=Format_SignDec(Line+Len,  Alt, 1, 1, 1);               // [0.1m]
      Line[Len++]='m'; }
    // else if(isAltitudeUnitFeet)                                   // display altitude in feet
    // { Alt = (Alt*336+512)>>10;                                    // [0.1m] => [feet]
    //   Len+=Format_SignDec(Line+Len,  Alt, 1, 0, 1);               // [feet]
    //   Line[Len++]='f'; Line[Len++]='t'; }
    for( ; Len<14; ) Line[Len++]=' ';                             // tail of spaces to cover older printouts
  }
  else Len+=Format_String(Line+Len, "-----.-  ");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 60, Line); }

void OLED_DrawID(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[128];
  u8g2_SetFont(OLED, u8g2_font_9x15_tr);
  sprintf(Line, "%s:%c:%06X", Parameters.AcftTypeName(), Parameters.AddrTypeChar(), Parameters.Address);
  u8g2_DrawStr(OLED, 0, 25, Line);
  // Parameters.Print(Line); Line[10]=0;
  // u8g2_DrawStr(OLED, 26, 25, Line);
  // u8g2_SetFont(OLED, u8g2_font_10x20_tr);
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);
  // u8g2_DrawStr(OLED, 0, 24, "ID:");
  if(Parameters.Pilot[0] || Parameters.Reg[0])
  { strcpy(Line, "Pilot: "); strcat(Line, Parameters.Pilot);
    u8g2_DrawStr(OLED, 0, 37, Line);
    strcpy(Line, "Reg: "); strcat(Line, Parameters.Reg);
    u8g2_DrawStr(OLED, 0, 49, Line); }
  else
  { u8g2_DrawStr(OLED, 20, 37, "OGN-Tracker");
    u8g2_DrawStr(OLED,  0, 50, "(c) Pawel Jalocha"); }
 u8g2_SetFont(OLED, u8g2_font_6x12_tr);
  uint64_t ID=getUniqueID();
  uint8_t Len=Format_String(Line, "#");
  Len+=Format_Hex(Line+Len, (uint16_t)(ID>>32));
  Len+=Format_Hex(Line+Len, (uint32_t)ID);
  // Line[Len++]=' ';
  // Line[Len++]='v';
    Len+=Format_String(Line+Len," " VERSION);
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 62, Line); }

void OLED_DrawBaro(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);              // 5 lines, 12 pixels/line
  uint8_t Len=Format_String(Line+Len, "BME280 ");
  if(GPS && GPS->hasBaro)
  { Len+=Format_UnsDec(Line+Len, GPS->Pressure/4, 5, 2);
    Len+=Format_String(Line+Len, "hPa "); }
  else Len+=Format_String(Line+Len, "----.--hPa ");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 24, Line);
  Len=0;
  if(GPS && GPS->hasBaro)
  { Len+=Format_SignDec(Line+Len, (int32_t)GPS->StdAltitude, 5, 1);
    Len+=Format_String(Line+Len, "m ");
    Len+=Format_SignDec(Line+Len, (int32_t)GPS->ClimbRate, 2, 1);
    Len+=Format_String(Line+Len, "m/s "); }
  else
  { Len+=Format_String(Line+Len, "-----.-m");
    Len+=Format_String(Line+Len, " --.-m/s "); }
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 36, Line);
  Len=0;
  if(GPS && GPS->hasBaro)
  { Len+=Format_SignDec(Line+Len, (int32_t)GPS->Temperature, 2, 1);
    Line[Len++]=0xB0;
    Line[Len++]='C';
    Line[Len++]=' ';
    Len+=Format_SignDec(Line+Len, (int32_t)GPS->Humidity, 2, 1);
    Line[Len++]='%'; }
  else Len+=Format_String(Line+Len, "---.- C --.-% ");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 48, Line);
  if(GPS && GPS->hasBaro)
  { float Dew = DewPoint(0.1f*GPS->Temperature, 0.1f*GPS->Humidity);
    sprintf(Line, "%+5.1f C dew point", Dew);
    Line[5]=0xB0;
    u8g2_DrawStr(OLED, 0, 60, Line); }
}

void OLED_DrawRF(u8g2_t *OLED, const GPS_Position *GPS) // RF 868MHz
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);            // 5 lines. 12 pixels/line
  uint8_t Len=0;
#ifdef WITH_SX1262
  Len+=Format_String(Line+Len, "SX1262");
#endif
#ifdef WITH_SX1276
  Len+=Format_String(Line+Len, "SX1276");
#endif
  Line[Len++]=':';
  Len+=Format_SignDec(Line+Len, (int32_t)Parameters.TxPower);              // Tx power
  Len+=Format_String(Line+Len, "dBm");
  Line[Len++]=' ';
  if(Parameters.RFchipFreqCorr!=0)
  { Len+=Format_SignDec(Line+Len, (int32_t)Parameters.RFchipFreqCorr, 2, 1); // frequency correction
    Len+=Format_String(Line+Len, "ppm"); }
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 24, Line);
  sprintf(Line, "Rx: %+4.1fdBm", Radio_BkgRSSI);
  u8g2_DrawStr(OLED, 0, 36, Line);
  // uint32_t Sum=0;
  // for(int Idx=0; Idx<8; Idx++)
  //   Sum+=Radio_RxCount[Idx];
  // sprintf(Line, "Rx: %d pkts", Sum);
  sprintf(Line, "Rx: %3.1f pkt/s", Radio_PktRate);
  u8g2_DrawStr(OLED, 0, 48, Line);
  Len=0;
  Len+=Format_String(Line+Len, Radio_FreqPlan.getPlanName());               // name of the frequency plan
  Line[Len++]=' ';
  Len+=Format_UnsDec(Line+Len, (uint32_t)(Radio_FreqPlan.getCenterFreq()/100000), 3, 1); // center frequency
  Len+=Format_String(Line+Len, "MHz");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 60, Line); }

void OLED_DrawRFcounts(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);         // small font
  int Vert=28;
  u8g2_DrawStr(OLED, 40, Vert-8, "Tx       Rx");
  sprintf(Line, "FLR:%7d %9d", Radio_TxCount[0], Radio_RxCount[0]);
  u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
  sprintf(Line, "OGN:%7d %9d", Radio_TxCount[1], Radio_RxCount[1]);
  u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
  sprintf(Line, "MDR:%7d %9d", Radio_TxCount[2], Radio_RxCount[2]);
  u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
  sprintf(Line, "LDR:%7d %9d", Radio_TxCount[5], Radio_RxCount[5]);
  u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
  sprintf(Line, "HDR:%7d %9d", Radio_TxCount[6], Radio_RxCount[6]);
  u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
// #ifdef WITH_FANET
//   sprintf(Line, "FNT:%7d %9d", Radio_TxCount[4], Radio_RxCount[4]);
//   u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=9;
// #endif
}

#ifdef WITH_TASK_STATS
static const TaskStats_Record *OLED_FindTaskStats(const TaskStats_Record *Records, uint8_t Count, const char *Name)
{ for(uint8_t Idx=0; Idx<Count; Idx++)
  { const TaskStats_Record *Record=Records+Idx;
    if(strcmp(Record->Name, Name)==0) return Record; }
  return 0; }

void OLED_DrawTaskStats(u8g2_t *OLED, const GPS_Position *GPS)
{ (void)GPS;
  static const char *const Names[] = { "loop", "GPS", "RF", "PROC", "LOG", "OLED" };
  TaskStats_Record Records[16];
  uint8_t Count=TaskStats_Copy(Records, 16);
  char Line[32];
  u8g2_SetFont(OLED, u8g2_font_5x8_tr);
  for(uint8_t Idx=0; Idx<sizeof(Names)/sizeof(Names[0]); Idx++)
  { const TaskStats_Record *Record=OLED_FindTaskStats(Records, Count, Names[Idx]);
    if(Record) sprintf(Line, "%-4s %3u%% %5u", Names[Idx], Record->CPU, Record->StackFree);
    else      sprintf(Line, "%-4s ---   ---", Names[Idx]);
    u8g2_DrawStr(OLED, 0, 16+8*Idx, Line); }
}
#endif

void OLED_DrawRelayOGN(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);         // small font
  uint8_t LineIdx=1;
  bool Displayed=false;
  for( uint8_t Idx=0; Idx<RelayQueueSize; Idx++)
  { OGN_RxPacket<OGN_Packet> *Packet = OGN_RelayQueue.Packet+Idx; if(Packet->Alloc==0) continue;
    if(Packet->Packet.Header.NonPos) continue;
    uint32_t Dist= IntDistance(Packet->LatDist, Packet->LonDist);      // [m]
    uint32_t Dir = IntAtan2(Packet->LonDist, Packet->LatDist);         // [16-bit cyclic]
    Dir &= 0xFFFF; Dir = (Dir*360)>>16;                                // [deg]
    uint8_t Len=0;
    Len+=Format_String(Line+Len, Parameters.AcftTypeName(Packet->Packet.Position.AcftType));
    Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, (uint32_t)Packet->Packet.DecodeAltitude(), 4); // [m] altitude
    Line[Len++]='m'; Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, Dir, 3);                             // [deg] direction to target
    Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, (Dist+50)/100, 3, 1);                // [km] distance to target
    Len+=Format_String(Line+Len, "km");
    Line[Len]=0;
    u8g2_DrawStr(OLED, 0, (LineIdx+3)*8, Line);
    Displayed=true;
    LineIdx++; if(LineIdx>=8) break;
  }
  if(!Displayed) u8g2_DrawStr(OLED, 0, 32, "No OGN relays");
}

void OLED_DrawRelayADSL(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);         // small font
  uint8_t LineIdx=1;
  bool Displayed=false;
  for( uint8_t Idx=0; Idx<RelayQueueSize; Idx++)
  { ADSL_RxPacket *Packet = ADSL_RelayQueue.Packet+Idx; if(Packet->Alloc==0) continue;
    if(!Packet->Packet.isPosition()) continue;
    uint32_t Dist= IntDistance(Packet->LatDist, Packet->LonDist);      // [m]
    uint32_t Dir = IntAtan2(Packet->LonDist, Packet->LatDist);         // [16-bit cyclic]
    Dir &= 0xFFFF; Dir = (Dir*360)>>16;                                // [deg]
    uint8_t Len=0;
    Len+=Format_String(Line+Len, Parameters.AcftTypeName(Packet->Packet.getAcftTypeOGN()));
    Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, (uint32_t)Packet->Packet.getAlt(), 4); // [m] altitude /// take care of negative !
    Line[Len++]='m'; Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, Dir, 3);                             // [deg] direction to target
    Line[Len++]=' ';
    Len+=Format_UnsDec(Line+Len, (Dist+50)/100, 3, 1);                // [km] distance to target
    Len+=Format_String(Line+Len, "km");
    Line[Len]=0;
    u8g2_DrawStr(OLED, 0, (LineIdx+3)*8, Line);
    Displayed=true;
    LineIdx++; if(LineIdx>=8) break;
  }
  if(!Displayed) u8g2_DrawStr(OLED, 0, 32, "No ADS-L relays");
}

#ifdef WITH_LOG
void OLED_DrawLogPage(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_5x8_tr);
  u8g2_DrawStr(OLED, 0, 21, "FLIGHT LOGS");

  if(!LogFS_isMounted())
  { u8g2_DrawStr(OLED, 0, 39, "External flash");
    if(LogFS_isDetected())
    { u8g2_DrawStr(OLED, 0, 48, "not mounted");
      u8g2_DrawStr(OLED, 0, 57, "Please format flash"); }
    else
    { u8g2_DrawStr(OLED, 0, 48, "not detected");
      u8g2_DrawStr(OLED, 0, 57, "Check hardware"); }
    return; }

  uint32_t Total=0, Free=0;
  FlashLog_GetStorage(Total, Free);
  uint32_t Used=Total>Free ? Total-Free : 0;
  sprintf(Line, "Used: %lukB", (unsigned long)(Used/1024));
  u8g2_DrawStr(OLED, 0, 30, Line);
  sprintf(Line, "Free: %lukB", (unsigned long)(Free/1024));
  u8g2_DrawStr(OLED, 0, 39, Line);

  if(FlashLog_FileTime)
  { strcpy(Line, "Start: ");
    Format_HHMMSS(Line+7, FlashLog_FileTime);
    Line[13]=0; }
  else strcpy(Line, "No log this flight");
  u8g2_DrawStr(OLED, 0, 48, Line);

  uint32_t Size=(FlashLog_FileFlush+512)>>10;
  if(FlashLog_Files>=0)
    sprintf(Line, "Size:%lukB Logs:%d", (unsigned long)Size, FlashLog_Files);
  else sprintf(Line, "Size:%lukB Logs:--", (unsigned long)Size);
  u8g2_DrawStr(OLED, 0, 57, Line); }
#endif

void OLED_DrawPower(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  int16_t BattVolt=(BatteryVoltage+128)>>8;          // [mV] measured and averaged battery voltage
  int8_t BattLev=BattCapacity(BattVolt);             // [%] estimated battery capacity

  // Large battery symbol on the left half of the 128x64 OLED.
  const int16_t X=12, Y=20, W=32, H=44;
  u8g2_DrawFrame(OLED, X, Y, W, H);
  u8g2_DrawBox(OLED, X+W/2-7, Y-5, 14, 5);
  if(BattLev>=0)
  { int16_t FillH=(BattLev*(H-6)+50)/100;
    if(FillH>0) u8g2_DrawBox(OLED, X+3, Y+H-3-FillH, W-6, FillH); }

  // Battery percentage, voltage, voltage rate, and MCU temperature on the right.
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);
  if(BattLev>=0) sprintf(Line, "%3d%%", BattLev);
           else sprintf(Line, " --%%");
  u8g2_DrawStr(OLED, 68, 24, Line);

  uint8_t Len=Format_SignDec(Line, (int32_t)BattVolt, 4, 3);
  Line[Len++]='V'; Line[Len]=0;
  u8g2_DrawStr(OLED, 64, 36, Line);

  u8g2_SetFont(OLED, u8g2_font_6x12_tr);
  sprintf(Line, "%+4.1fmV/min", 0.1f*((600*BatteryVoltageRate+128)>>8));
  u8g2_DrawStr(OLED, 64, 48, Line);
  sprintf(Line, "%4.1fdegC", 0.1f*readMCUtemperature());
  u8g2_DrawStr(OLED, 70, 60, Line);
}

#ifdef WITH_LORAWAN
void OLED_DrawLoRaWAN(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  const char *StateName[4] = { "Not-conn.", "Join-Req", "+Joined+", "Pkt-Sent" };
  uint8_t Len=Format_String(Line, "TTN: ");
  if(WANdev.Enable)
  { if(WANdev.State==2) Len+=Format_Hex(Line+Len, WANdev.DevAddr);
    else if(WANdev.State<=3) Len+=Format_String(Line+Len, StateName[WANdev.State]);
    else Len+=Format_Hex(Line+Len, WANdev.State); }
  else Len+=Format_String(Line+Len, "Disabled");
  Line[Len]=0;

  u8g2_SetFont(OLED, u8g2_font_6x12_tr);
  u8g2_DrawStr(OLED, 0, 23, Line);

  if(WANdev.State>=2)
  { Len=0;
    Len+=Format_Hex(Line+Len, (uint16_t)WANdev.UpCount);
    Len+=Format_String(Line+Len, " >> ");
    Len+=Format_Hex(Line+Len, (uint16_t)WANdev.DnCount);
    Line[Len]=0;
    u8g2_DrawStr(OLED, 0, 35, Line);

    Len=0;
    Len+=Format_SignDec(Line+Len, ((int32_t)WANdev.RxSNR*10+2)>>2, 2, 1);
    Len+=Format_String(Line+Len, "dB ");
    Len+=Format_SignDec(Line+Len, (int32_t)WANdev.RxRSSI, 3);
    Len+=Format_String(Line+Len, "dBm");
    Line[Len]=0;
    u8g2_DrawStr(OLED, 0, 47, Line);
  }

  Len=Format_String(Line, "Key: ");
  Len+=Format_HexBytes(Line+Len, WANdev.AppKey, 2);
  Line[Len++]='.'; Line[Len++]='.';
  Len+=Format_Hex(Line+Len, WANdev.AppKey[15]);
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 59, Line);
}
#endif

#ifdef WITH_LOOKOUT
void OLED_DrawLookOut(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  const char *AlertName;
  switch(Parameters.AlertThresh)
  { case 0: AlertName="All";  break;
    case 1: AlertName="L1+";  break;
    case 2: AlertName="L2+";  break;
    case 3: AlertName="L3+";  break;
    case 4: AlertName="Off";  break;
    default: AlertName="?"; }

  u8g2_SetFont(OLED, u8g2_font_6x12_tr);
  u8g2_DrawStr(OLED, 0, 23, "LOOKOUT");
  sprintf(Line, "Alert %s  Warn %ds", AlertName, Look.WarnTime);
  u8g2_DrawStr(OLED, 0, 35, Line);

  uint8_t TgtIdx=Look.WorstTgtIdx;
  if(Look.WarnLevel && TgtIdx<LookOut<32>::MaxTargets)
  { const LookOut_Target *Tgt=Look.Target+TgtIdx;
    if(Tgt->Alloc && Tgt->WarnLevel)
    { if(Tgt->Call[0]) sprintf(Line, "Threat L%d %.10s", Tgt->WarnLevel, Tgt->Call);
      else              sprintf(Line, "Threat L%d %06X", Tgt->WarnLevel, Tgt->Address&0x00FFFFFF);
      u8g2_DrawStr(OLED, 0, 47, Line);
      sprintf(Line, "CPA %ds %dm %+dm", Tgt->MissTime/2, Tgt->MissDist/2, Tgt->dZ/2);
      u8g2_DrawStr(OLED, 0, 59, Line);
      return; }
  }

  sprintf(Line, "No threat  Targets %d", Look.Targets);
  u8g2_DrawStr(OLED, 0, 47, Line);
  u8g2_DrawStr(OLED, 0, 59, "CPA --s ---m"); }
#endif

static int16_t OLED_ClipInt16(int32_t Value)
{ if(Value> 32767) return  32767;
  if(Value<-32768) return -32768;
  return Value; }

static uint16_t OLED_ReturnBearing(int32_t East, int32_t North)
{ while((East>16000) || (East< -16000) || (North>16000) || (North< -16000))
  { East/=2; North/=2; }
  return (uint16_t)IntAtan2((int16_t)East, (int16_t)North); }

static void OLED_DrawReturnCardinal(u8g2_t *Display, char Cardinal, uint16_t Angle)
{ const int16_t Xc=29, Yc=39, Radius=15;
  int16_t X=(int16_t)(((int32_t)Radius*Isin((int16_t)Angle)+0x800)>>12);
  int16_t Y=(int16_t)(((int32_t)Radius*Icos((int16_t)Angle)+0x800)>>12);
  char Text[2]={Cardinal, 0};
  u8g2_DrawStr(Display, Xc+X-4, Yc-Y+4, Text); }

static void OLED_DrawReturnPointer(u8g2_t *Display, uint16_t Angle)
{ const int16_t Xc=29, Yc=39, TipRadius=13, BaseRadius=4, HalfWidth=3;
  int16_t Sin=Isin((int16_t)Angle), Cos=Icos((int16_t)Angle);
  int16_t TipX = Xc+(((int32_t)TipRadius*Sin+0x800)>>12);
  int16_t TipY = Yc-(((int32_t)TipRadius*Cos+0x800)>>12);
  int16_t TailX = Xc-(TipX-Xc), TailY = Yc-(TipY-Yc);
  int16_t BaseX=Xc+(((int32_t)BaseRadius*Sin+0x800)>>12);
  int16_t BaseY=Yc-(((int32_t)BaseRadius*Cos+0x800)>>12);
  int16_t SideX=((int32_t)HalfWidth*Cos+0x800)>>12;
  int16_t SideY=((int32_t)HalfWidth*Sin+0x800)>>12;
  u8g2_DrawLine(Display, TailX, TailY, TipX, TipY);
  u8g2_DrawTriangle(Display, TipX, TipY, BaseX+SideX, BaseY+SideY,
                     BaseX-SideX, BaseY-SideY); }

void OLED_DrawReturn(u8g2_t *Display, const GPS_Position *GPS)
{ char Line[24];
  const int16_t Xc=27, Yc=39, Radius=22;
  const bool GPSvalid = GPS && GPS->isValid();
  int32_t Heading=GPSvalid ? GPS->Heading : 0;
  Heading%=3600; if(Heading<0) Heading+=3600;
  uint16_t TrackAngle=(uint16_t)(((uint32_t)Heading*65536u+1800u)/3600u);
  uint16_t Rotation=GPSvalid ? TrackAngle : 0;

  u8g2_SetFont(Display, u8g2_font_7x13_tf);
  u8g2_DrawCircle(Display, Xc, Yc, Radius, U8G2_DRAW_ALL);
  OLED_DrawReturnCardinal(Display, 'N', (uint16_t)(0x0000-Rotation));
  OLED_DrawReturnCardinal(Display, 'E', (uint16_t)(0x4000-Rotation));
  OLED_DrawReturnCardinal(Display, 'S', (uint16_t)(0x8000-Rotation));
  OLED_DrawReturnCardinal(Display, 'W', (uint16_t)(0xC000-Rotation));

  u8g2_SetFont(Display, u8g2_font_6x12_tr);
  if(!GPSvalid)
  { u8g2_DrawStr(Display, 67, 22, Flight.Takeoff.isValid() ? "WAIT GPS" : "NO TAKEOFF");
    u8g2_DrawStr(Display, 67, 32, "---\xB0/---");
    u8g2_DrawStr(Display, 67, 42, "---\xB0/---kt");
    u8g2_DrawStr(Display, 67, 52, "----m AMSL");
    u8g2_DrawStr(Display, 67, 62, "ETE --:--");
    return; }

  u8g2_DrawStr(Display, 67, 22, Flight.Takeoff.isValid() ? "RETURN" : "NO TAKEOFF");

  uint32_t Speed=(GPS->Speed>0) ? (uint32_t)GPS->Speed : 0;
  uint32_t SpeedKts=(Speed*1944u+5000u)/10000u;
  sprintf(Line, "%03ld%c/%lukt", (long)(((Heading+5)/10)%360), 0xB0,
                                  (unsigned long)SpeedKts);
  u8g2_DrawStr(Display, 67, 42, Line);

  int32_t AltitudeMeters=(GPS->Altitude>=0) ? (GPS->Altitude+5)/10 : (GPS->Altitude-5)/10;
  sprintf(Line, "%ldm AMSL", (long)AltitudeMeters);
  u8g2_DrawStr(Display, 67, 52, Line);

  if(!Flight.Takeoff.isValid())
  { u8g2_DrawStr(Display, 67, 32, "---\xB0/---");
    u8g2_DrawStr(Display, 67, 62, "ETE --:--");
    return; }

  int32_t North=GPS_Position::calcLatDistance(GPS->Latitude, Flight.Takeoff.Latitude);
  int32_t East=GPS_Position::calcLonDistance(GPS->Longitude, Flight.Takeoff.Longitude,
                                             GPS->LatitudeCosine);
  uint32_t Distance=IntDistance(East, North);
  uint16_t Bearing=(Distance>=10) ? OLED_ReturnBearing(East, North) : 0;
  if(Distance>=10) OLED_DrawReturnPointer(Display, (uint16_t)(Bearing-Rotation));

  uint32_t BearingDegrees=(((uint32_t)Bearing*360u+32768u)>>16)%360u;
  if(Distance<1000u)
  { if(Distance<10u) sprintf(Line, "---\xB0/%lum", (unsigned long)Distance);
    else sprintf(Line, "%03lu%c/%lum", (unsigned long)BearingDegrees, 0xB0,
                                        (unsigned long)Distance); }
  else
  { uint32_t TenthKm=(Distance+50u)/100u;
    sprintf(Line, "%03lu%c/%lu.%lukm", (unsigned long)BearingDegrees, 0xB0,
            (unsigned long)(TenthKm/10u), (unsigned long)(TenthKm%10u));
    if(u8g2_GetStrWidth(Display, Line)>61)
    { uint32_t Km=(Distance+500u)/1000u;
      sprintf(Line, "%03lu%c/%lukm", (unsigned long)BearingDegrees, 0xB0,
                                       (unsigned long)Km); }
    if(u8g2_GetStrWidth(Display, Line)>61)
    { uint32_t Nm=(Distance+926u)/1852u;
      sprintf(Line, "%03lu%c/%lunm", (unsigned long)BearingDegrees, 0xB0,
                                       (unsigned long)Nm); } }
  u8g2_DrawStr(Display, 67, 32, Line);

  if(Distance==0) sprintf(Line, "ETE 00:00");
  else if(Speed==0) sprintf(Line, "ETE --:--");
  else
  { uint32_t ETEseconds=(Distance*10u+Speed/2u)/Speed;
    uint32_t ETEminutes=(ETEseconds+30u)/60u;
    uint32_t ETEhours=ETEminutes/60u;
    if(ETEhours<100u)
      sprintf(Line, "ETE %02lu:%02lu", (unsigned long)ETEhours,
                                        (unsigned long)(ETEminutes%60u));
    else sprintf(Line, "ETE >99h"); }
  u8g2_DrawStr(Display, 67, 62, Line); }

void OLED_DrawCompass(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);
#ifdef WITH_QMC63XX
  if(!HardwareStatus.Magn) { u8g2_DrawStr(OLED, 0, 28, "No magn. sensor"); return; }
  if(Magn_Calibrate)
  { u8g2_SetFont(OLED, u8g2_font_7x13_tf);
    u8g2_DrawStr(OLED, 0, 24, "Calibrating...");
    u8g2_DrawStr(OLED, 0, 36, "Turn device");
    u8g2_DrawStr(OLED, 0, 48, "along all axes");
    sprintf(Line, "Samples: %02d", Magn_Calibrate);
    u8g2_DrawStr(OLED, 0, 60, Line);
    return; }
  int Vert=24;
  u8g2_DrawStr(OLED, 0, Vert, "   [uT]"); Vert+=12;
  const char *AxisName = "XYZ";
  for(int Idx=0; Idx<3; Idx++)
  { sprintf(Line, "%c: %+6.1f", AxisName[Idx], (1.0f/150.0f)*Magn.A[Idx]);
    u8g2_DrawStr(OLED, 0, Vert, Line); Vert+=12; }

  const int16_t Xc =  96;
  const int16_t Yc =  37;
  const int16_t R  =  26;
  const int16_t dR =  10;
  const int16_t dHead = 0x800;
  // uint16_t Heading = IntAtan2(OLED_ClipInt16(Magn.Y), OLED_ClipInt16(Magn.X));
  uint16_t Heading = IntAtan2(OLED_ClipInt16(Magn.X), OLED_ClipInt16(Magn.Y));
  if(!OLED_Rotate) Heading+=0x8000;
  uint16_t Deg = ((uint32_t)Heading*45+0x1000)>>13;
  u8g2_SetFont(OLED, u8g2_font_fub20_tr);
  int Len=Format_UnsDec(Line, (uint32_t)Deg, 3); Line[Len]=0;
  uint8_t TextWidth = u8g2_GetStrWidth(OLED, Line);
  u8g2_DrawStr(OLED, Xc-TextWidth/2, Yc+10, Line);
  u8g2_DrawCircle(OLED, Xc, Yc, R, U8G2_DRAW_ALL);
  int16_t Xm = ((int32_t)R*Icos(Heading)+0x800)>>12;
  int16_t Ym = ((int32_t)R*Isin(Heading)+0x800)>>12;
  int16_t XmL = ((int32_t)(R-dR)*Icos(Heading-dHead)+0x800)>>12;
  int16_t YmL = ((int32_t)(R-dR)*Isin(Heading-dHead)+0x800)>>12;
  int16_t XmR = ((int32_t)(R-dR)*Icos(Heading+dHead)+0x800)>>12;
  int16_t YmR = ((int32_t)(R-dR)*Isin(Heading+dHead)+0x800)>>12;
  u8g2_DrawTriangle(OLED, Xc-Ym, Yc-Xm, Xc-YmL, Yc-XmL, Xc-YmR, Yc-XmR);
#endif
}

#endif // WITH_OLED

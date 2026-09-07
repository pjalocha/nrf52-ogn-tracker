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
static const uint8_t OLED_Pages             = 9;
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

#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
#include "external_flash_fs.h"
#ifdef WITH_LOG
#include "log.h"
#endif

static const uint32_t OLED_EventMenuClick  = 1u<<1;
static const uint32_t OLED_EventMenuLong   = 1u<<2;

enum OLED_MenuState
{ OLED_MenuClosed,
  OLED_MenuList,
  OLED_MenuAcftType,
  OLED_MenuAddrType,
  OLED_MenuAddress,
  OLED_MenuAlert,
  OLED_MenuGhost,
  OLED_MenuTextEdit,
  OLED_MenuFormatConfirm,
  OLED_MenuDefaultsConfirm };

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
static uint8_t OLED_MenuAlertValue = 0;
static uint8_t OLED_MenuGhostValue = 0;
static OLED_MenuTextField OLED_MenuText = OLED_MenuTextNone;
static char OLED_MenuTextValue[FlashParameters::InfoParmLen];
static uint8_t OLED_MenuTextPosition = 0;
static int OLED_MenuSaveResult = 0;
static uint32_t OLED_MenuMessageTime = 0;
static const char *OLED_MenuMessage = 0;

static const uint8_t OLED_MenuItems = 9;
static const uint8_t OLED_MenuTextLength = FlashParameters::InfoParmLen-1;

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
      OLED_MenuAlertValue = AlarmThresh<=4 ? AlarmThresh : 4;
      OLED_Menu=OLED_MenuAlert;
      break;
    case 4:
      OLED_MenuGhostValue = Parameters.GhostMode>=2 ? 2 : Parameters.GhostMode;
      OLED_Menu=OLED_MenuGhost;
      break;
    case 5:
      OLED_MenuEnterText(OLED_MenuTextReg);
      break;
    case 6:
      OLED_MenuEnterText(OLED_MenuTextPilot);
      break;
    case 7:
      OLED_Menu=OLED_MenuFormatConfirm;
      break;
    case 8:
      OLED_Menu=OLED_MenuDefaultsConfirm;
      break;
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
  if(Value<0) Value=15;
  if(Value>15) Value=0;
  OLED_MenuAcftTypeValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeAddrType(int8_t Step)
{ int Value=OLED_MenuAddrTypeValue+Step;
  if(Value<0) Value=3;
  if(Value>3) Value=0;
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

static void OLED_MenuChangeAlert(int8_t Step)
{ int Value=OLED_MenuAlertValue+Step;
  if(Value<0) Value=4;
  if(Value>4) Value=0;
  OLED_MenuAlertValue=Value;
  OLED_PageChange=true; }

static void OLED_MenuChangeGhost(int8_t Step)
{ int Value=OLED_MenuGhostValue+Step;
  if(Value<0) Value=2;
  if(Value>2) Value=0;
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
    case OLED_MenuAlert:
      AlarmThresh=OLED_MenuAlertValue;
      OLED_MenuShowMessage("Set", 0);
      OLED_MenuBeepSaved();
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
  AlarmThresh=0;
  int Result=Parameters.WriteToNVS();
  OLED_MenuShowMessage(Result<0 ? "ERROR" : "Defaults", Result);
  if(Result>=0) OLED_MenuBeepSaved();
  OLED_Menu=OLED_MenuList;
  OLED_PageChange=true;
}

static void OLED_MenuHandleEvent(uint32_t Event)
{ if(Event&OLED_EventMenuLong)
  { if(OLED_Menu==OLED_MenuClosed) OLED_MenuOpen();
    else if(OLED_Menu==OLED_MenuFormatConfirm) OLED_MenuFormatFlash();
    else if(OLED_Menu==OLED_MenuDefaultsConfirm) OLED_MenuResetDefaults();
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
  if(First) { Previous=Current; First=false; return; }
  uint8_t Pressed=Current&~Previous;
  Previous=Current;
  if(OLED_Menu==OLED_MenuList)
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
  else if(OLED_Menu==OLED_MenuAlert)
  { if(Pressed&(1u<<0)) OLED_MenuChangeAlert(+1);
    if(Pressed&(1u<<1)) OLED_MenuChangeAlert(-1); }
  else if(OLED_Menu==OLED_MenuGhost)
  { if(Pressed&(1u<<0) || Pressed&(1u<<1)) OLED_MenuChangeGhost(+1); }
  else if(OLED_Menu==OLED_MenuTextEdit)
  { uint8_t Direction=0;
    if((Current&(1u<<0)) && !(Current&(1u<<1))) Direction=1;
    if((Current&(1u<<1)) && !(Current&(1u<<0))) Direction=2;
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

static void OLED_MenuDraw(u8g2_t *Display)
{ u8g2_SetFont(Display, u8g2_font_7x13_tf);
  if(OLED_Menu==OLED_MenuList)
    { static const char *ItemNames[OLED_MenuItems] =
    { "AcftType", "AddrType", "Address", "Alerts", "Ghost", "Reg", "Pilot", "Format flash", "Reset defaults" };
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
      if(Item==3) { strcat(Value, " "); strcat(Value, OLED_MenuAlertNames[AlarmThresh<=4 ? AlarmThresh : 4]); }
      if(Item==4) { strcat(Value, " "); strcat(Value, OLED_MenuGhostNames[Parameters.GhostMode>=2 ? 2 : Parameters.GhostMode]); }
      if(Item==5) { strcat(Value, " "); strcat(Value, Parameters.Reg); }
      if(Item==6) { strcat(Value, " "); strcat(Value, Parameters.Pilot); }
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
  if(OLED_MenuMessage && (uint32_t)(millis()-OLED_MenuMessageTime)<2500)
  { u8g2_SetFont(Display, u8g2_font_6x12_tr);
    u8g2_DrawStr(Display, 82, 22, OLED_MenuMessage); }
}
#endif

void OLED_ButtonSingle(void)
{ if(OLED_TaskHandle) xTaskNotify(OLED_TaskHandle, OLED_EventPageButton, eSetBits); }

static bool OLED_PageAvailable(uint8_t Page)
{ switch(Page)
  { case OLED_Page_ID:
    case OLED_Page_GPS:
    case OLED_Page_SatSNR:
    case OLED_Page_RF:
    case OLED_Page_RFcounts:
    case OLED_Page_Power:
    case OLED_Page_RelayOGN:
    case OLED_Page_RelayADSL:
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

static void OLED_SetPowerSave(bool PowerSave)
{ if(xSemaphoreTake(I2C_Mutex, 50))
  { OLED.setPowerSave(PowerSave ? 1 : 0);
    xSemaphoreGive(I2C_Mutex); } }

static int OLED_DrawPage(const GPS_Position *GPS)
{
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
  if(OLED_MenuActive())
  { OLED.clearBuffer();
    OLED_DrawStatusBar(OLED.getU8g2(), GPS);
    OLED_MenuDraw(OLED.getU8g2());
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
    case OLED_Page_Power:     OLED_DrawPower     (OLED.getU8g2(), GPS); break;
    case OLED_Page_RelayOGN:  OLED_DrawRelayOGN  (OLED.getU8g2(), GPS); break;
    case OLED_Page_RelayADSL: OLED_DrawRelayADSL (OLED.getU8g2(), GPS); break;
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

void OLED_Task(void *Parms)
{
  (void)Parms;
  OLED_TaskHandle = xTaskGetCurrentTaskHandle();
  OLED_Init();

  GPS_Position *PrevGPS=0;
  for( ; ; )
  {
    uint32_t Events=0;
    xTaskNotifyWait(0, 0xFFFFFFFF, &Events, 0);
    if(Events&OLED_EventPageButton) OLED_HandleButton();
#if defined(WITH_OLED_MENU) && defined(WITH_WIO_TRACKER)
    OLED_MenuHandleEvent(Events);
    OLED_MenuPollJoystick();
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

void OLED_DrawRelayOGN(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);         // small font
  uint8_t LineIdx=1;
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
    LineIdx++; if(LineIdx>=8) break;
  }
}

void OLED_DrawRelayADSL(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_6x12_tr);         // small font
  uint8_t LineIdx=1;
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
    LineIdx++; if(LineIdx>=8) break;
  }
}

void OLED_DrawPower(u8g2_t *OLED, const GPS_Position *GPS)
{ char Line[32];
  u8g2_SetFont(OLED, u8g2_font_7x13_tf);              // 5 lines, 12 pixels/line
  uint8_t Len=Format_String(Line+Len, " USB       Batt ");
  Line[Len]=0;
  u8g2_DrawStr(OLED, 0, 24, Line);
  int16_t BattVolt=(BatteryVoltage+128)>>8; // [mV] measured and averaged  battery voltage
  Len=Format_SignDec(Line, (int32_t)BattVolt, 4, 3); Line[Len++]='V'; Line[Len]=0;
  u8g2_DrawStr(OLED, 64, 36, Line);
#ifdef WITH_AXP
  if(HardwareStatus.AXP192 || HardwareStatus.AXP202)
  { if(xSemaphoreTake(I2C_Mutex, 10))
    { sprintf(Line, "%5.3fV", 0.001f*AXP.getVbusVoltage());
      u8g2_DrawStr(OLED,  0, 36, Line);
      sprintf(Line, "%5.3fA", 0.001f*AXP.getVbusCurrent());
      u8g2_DrawStr(OLED,  0, 48, Line);
      int32_t BattCurr = AXP.getBattChargeCurrent()-AXP.getBattDischargeCurrent();
      sprintf(Line, "%5.3fA", 0.001f*BattCurr);
      u8g2_DrawStr(OLED, 64, 48, Line);
      // int BattLev = PMU->getBattPercent(); // only AXP202
      xSemaphoreGive(I2C_Mutex); }
  }
#endif
#ifdef WITH_XPOWERS
  if(HardwareStatus.AXP192 || HardwareStatus.AXP210)
  { if(xSemaphoreTake(I2C_Mutex, 10))
    { sprintf(Line, "%5.3fV", 0.001f*PMU->getVbusVoltage());
      u8g2_DrawStr(OLED,  0, 36, Line);
      int BattLev = PMU->getBatteryPercent();
      if(BattLev>=0) sprintf(Line, "   %3d%%", BattLev);
               else  sprintf(Line, "   ---%%");
      u8g2_DrawStr(OLED, 64, 48, Line);
      xSemaphoreGive(I2C_Mutex); }
  }
#endif // WITH_XPOWERS


}

static int16_t OLED_ClipInt16(int32_t Value)
{ if(Value> 32767) return  32767;
  if(Value<-32768) return -32768;
  return Value; }

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

#include <stdint.h>

#include "main.h"
#include "log.h"

#include "adsl.h"
#include "external_flash_fs.h"
#include "format.h"
#include "gps.h"
#include "nmea.h"
#include "timesync.h"

void AddPath(char *Name, const char *FileName, const char *Path)
{ if(Path==0) { strcpy(Name, FileName); return; }
  strcpy(Name, Path);
  int Len=strlen(Name);
  if(Len && Name[Len-1]!='/') Name[Len++]='/';
  strcpy(Name+Len, FileName); }

#ifdef WITH_LOG

const char *FlashLog_Path = "/";
const char *FlashLog_Ext  = ".TLG";

static const uint32_t FlashLog_MaxTime = 3600;
static const uint32_t FlashLog_MaxSize = 0x10000;
static const uint32_t FlashLog_SaveSize = 4096;

bool     FlashLog_SaveReq = 0;
uint32_t FlashLog_FileTime = 0;
char     FlashLog_FileName[32] = { 0 };
uint32_t FlashLog_FileFlush = 0;
int      FlashLog_Files = 0;
static uint32_t FlashLog_TotalSpace = 0;
static uint32_t FlashLog_FreeSpace = 0;
static volatile uint8_t FlashLog_StorageUpdateRequest = 0;
static volatile bool FlashLog_USBRequest = false;
static volatile bool FlashLog_USBReady = false;

static FatFile FlashLog_File;

FIFO<OGN_LogPacket<OGN_Packet>, 32> FlashLog_FIFO;

bool FlashLog_isOpen(void) { return FlashLog_File.isOpen(); }

int FlashLog_ShortFileName(char *FileName, uint32_t Time)
{ int Len = Format_Hex(FileName, Time);
  strcpy(FileName+Len, FlashLog_Ext);
  Len += strlen(FlashLog_Ext);
  FileName[Len] = 0;
  return Len; }

int FlashLog_FullFileName(char *FileName, uint32_t Time)
{ strcpy(FileName, FlashLog_Path);
  int Len = strlen(FileName);
  if(Len && FileName[Len-1]!='/') FileName[Len++]='/';
  return Len + FlashLog_ShortFileName(FileName+Len, Time); }

uint32_t FlashLog_ReadShortFileTime(const char *FileName, int Len)
{ if(Len!=12) return 0;
  if(memcmp(FileName+8, FlashLog_Ext, 4)!=0) return 0;
  uint32_t Time = 0;
  if(Read_Hex(Time, FileName)!=8) return 0;
  return Time; }

uint32_t FlashLog_ReadShortFileTime(const char *FileName)
{ return FlashLog_ReadShortFileTime(FileName, strlen(FileName)); }

static bool FlashLog_OpenRoot(FatFile &Root)
{ return LogFS_isMounted() && Root.open(&LogFS, "/", O_RDONLY); }

static bool FlashLog_OpenFile(FatFile &File, const char *Name, oflag_t Mode)
{ return LogFS_isMounted() && File.open(&LogFS, Name, Mode); }

static bool FlashLog_GetFileSize(const char *Name, uint32_t &Size)
{ FatFile File;
  if(!FlashLog_OpenFile(File, Name, O_RDONLY)) return false;
  Size = File.fileSize();
  File.close();
  return true; }

int FlashLog_FindOldestFile(uint32_t &Oldest, uint32_t After)
{ int Files = 0;
  Oldest = 0xFFFFFFFF;
  FatFile Root;
  if(!FlashLog_OpenRoot(Root)) return -1;
  FatFile File;
  while(File.openNext(&Root, O_RDONLY))
  { vTaskDelay(1);
    if(!File.isDir())
    { char Name[32];
      File.getName(Name, sizeof(Name));
      uint32_t Time = FlashLog_ReadShortFileTime(Name);
      if(Time>After)
      { if(Time<Oldest) Oldest = Time;
        Files++; }
    }
    File.close();
  }
  Root.close();
  FlashLog_Files = Files;
  return Files; }

int FlashLog_ListFiles(void)
{ if(!LogFS_isMounted()) return 0;

  int Files = 0;
  uint32_t PrevTime = 0;
  for( ; ; )
  { vTaskDelay(1);
    uint32_t Time = 0;
    FlashLog_FindOldestFile(Time, PrevTime);
    if(Time==0xFFFFFFFF) break;
    PrevTime = Time;
    char FullName[32];
    uint32_t Size = 0;
    FlashLog_FullFileName(FullName, Time);
    if(!FlashLog_GetFileSize(FullName, Size)) continue;

    char Line[64];
    uint8_t Len = Format_String(Line, "$POGNL,");
    const char *Name = strrchr(FullName, '/');
    Name = Name ? Name+1 : FullName;
    Len += Format_String(Line+Len, Name);
    Line[Len++] = ',';
    Len += Format_HHMMSS(Line+Len, Time);
    Line[Len++] = ',';
    Len += Format_UnsDec(Line+Len, Size/OGN_LogPacket<OGN_Packet>::Bytes);
    Len += NMEA_AppendCheckCRNL(Line, Len);

    if(xSemaphoreTake(CONS_Mutex, 25))
    {
      Format_String(CONS_UART_Write, Line, 0, Len);
      xSemaphoreGive(CONS_Mutex);
    }
    Files++;
  }
  return Files; }

int FlashLog_ListFile(const char *FileName, uint32_t FileTime)
{ FatFile File;
  if(!FlashLog_OpenFile(File, FileName, O_RDONLY)) return -1;

  char Line[128];
  OGN_LogPacket<OGN_Packet> Packet;
  ADSL_Packet AdslPacket;
  int Packets = 0;

  for( ; ; )
  { if(File.read(&Packet, Packet.Bytes)!=(int)Packet.Bytes) break;
    if(!Packet.isCorrect()) continue;
    uint32_t Time = Packet.getTime(FileTime);
    uint8_t Len = 0;
    if(Packet.Prot)
    { AdslPacket.Version = 0x00;
      memcpy(AdslPacket.Byte, Packet.PktByte(), 20);
      Len = AdslPacket.WriteAPRS(Line, Time, GPS_GeoidSepar/10); }
    else
    { Len = Packet.Packet.WriteAPRS(Line, Time); }

    if(Len==0) continue;
    Line[Len++] = '\n';
    Line[Len] = 0;
    if(xSemaphoreTake(CONS_Mutex, 25))
    { Format_String(CONS_UART_Write, Line, 0, Len);
      xSemaphoreGive(CONS_Mutex); }
    vTaskDelay(10);
    Packets++;
  }

  File.close();
  if(xSemaphoreTake(CONS_Mutex, 25))
  { Format_String(CONS_UART_Write, FileName);
    Format_String(CONS_UART_Write, " => ");
    Format_UnsDec(CONS_UART_Write, (uint32_t)Packets);
    Format_String(CONS_UART_Write, " packets\n");
    xSemaphoreGive(CONS_Mutex); }
  return Packets; }

int FlashLog_ListFile(uint32_t FileTime)
{ char FileName[32];
  FlashLog_FullFileName(FileName, FileTime);
  return FlashLog_ListFile(FileName, FileTime); }

static int FlashLog_CleanEmpty(int MinSize=0)
{ const int MaxDelFiles = 4;
  uint32_t DelFile[MaxDelFiles];
  int DelFiles = 0;

  FatFile Root;
  if(!FlashLog_OpenRoot(Root)) return -1;

  FatFile File;
  while(File.openNext(&Root, O_RDONLY))
  { vTaskDelay(1);
    if(!File.isDir())
    { char Name[32];
      File.getName(Name, sizeof(Name));
      uint32_t Time = FlashLog_ReadShortFileTime(Name);
      if(Time)
      { bool Delete = true;
        if(Time>=0x10000000 && Time<0x80000000)
        { Delete = File.fileSize() < (uint32_t)MinSize; }
        if(Delete)
        { DelFile[DelFiles++] = Time;
          if(DelFiles==MaxDelFiles)
          { File.close();
            break; }
        }
      }
    }
    File.close();
  }
  Root.close();

  char FullName[32];
  for(int Idx=0; Idx<DelFiles; Idx++)
  { FlashLog_FullFileName(FullName, DelFile[Idx]);
    LogFS.remove(FullName);
    vTaskDelay(1); }
  return DelFiles; }

static uint32_t FlashLog_FreeBytes(void)
{ if(!LogFS_isMounted()) return 0;
  int32_t FreeClusters = LogFS.freeClusterCount();
  if(FreeClusters<0) return 0;
  return (uint32_t)FreeClusters * LogFS.bytesPerCluster(); }

static void FlashLog_UpdateStorage(void)
{ uint32_t Total=0;
  uint32_t Free=0;
  if(LogFS_isMounted())
  { Total=(uint32_t)LogFS.clusterCount() * LogFS.bytesPerCluster();
    Free=FlashLog_FreeBytes();
    if(Free>Total) Free=Total; }
  FlashLog_TotalSpace=Total;
  FlashLog_FreeSpace=Free; }

void FlashLog_GetStorage(uint32_t &Total, uint32_t &Free)
{ Total=FlashLog_TotalSpace;
  Free=FlashLog_FreeSpace; }

void FlashLog_RequestStorageUpdate(void)
{ FlashLog_StorageUpdateRequest++; }

bool FlashLog_IsUSBPreparing(void)
{ return FlashLog_USBRequest; }

bool FlashLog_PrepareUSB(uint32_t TimeoutMS)
{
  FlashLog_USBReady=false;
  FlashLog_USBRequest=true;
  uint32_t Start=millis();
  while(!FlashLog_USBReady && (uint32_t)(millis()-Start)<TimeoutMS)
    vTaskDelay(1);
  if(!FlashLog_USBReady) FlashLog_USBRequest=false;
  return FlashLog_USBReady;
}

static int FlashLog_Clean(size_t MinFree=0)
{ if(!LogFS_isMounted()) return -1;
  uint32_t Total = (uint32_t)LogFS.clusterCount() * LogFS.bytesPerCluster();
  uint32_t Free = FlashLog_FreeBytes();
  if(MinFree) { if(Free>=MinFree) return 0; }
         else { if(Free>=(Total/4)) return 0; }

  uint32_t Oldest = 0xFFFFFFFF;
  int Files = FlashLog_FindOldestFile(Oldest, 0);
  if(Files<0) return Files;
  if(Files<=2) return 0;

  char FullName[32];
  FlashLog_FullFileName(FullName, Oldest);
  if(!LogFS.remove(FullName)) return -1;
  FlashLog_Files = Files-1;
  return 1; }

static int FlashLog_Clean(size_t MinFree, int Loops)
{ int Count = 0;
  for( ; Loops>0; Loops--)
  { if(FlashLog_Clean(MinFree)<=0) break;
    vTaskDelay(1);
    Count++; }
  return Count; }

static bool FlashLog_SetTimestamp(uint8_t Flags, uint32_t Time)
{ if(!FlashLog_File.isOpen() || Time<946771200UL) return false; // 2000-01-02
  GPS_Time DateTime;
  DateTime.setUnixTime(Time);
  if(DateTime.Year<0 || DateTime.Year>99 || DateTime.Month<1 || DateTime.Month>12 ||
     DateTime.Day<1 || DateTime.Day>31 || DateTime.Hour<0 || DateTime.Hour>23 ||
     DateTime.Min<0 || DateTime.Min>59 || DateTime.Sec<0 || DateTime.Sec>59) return false;
  return FlashLog_File.timestamp(Flags, 2000+(uint8_t)DateTime.Year,
                                 (uint8_t)DateTime.Month, (uint8_t)DateTime.Day,
                                 (uint8_t)DateTime.Hour, (uint8_t)DateTime.Min,
                                 (uint8_t)DateTime.Sec); }

static void FlashLog_Close(uint32_t Time)
{ if(!FlashLog_File.isOpen()) return;
  FlashLog_SetTimestamp(T_WRITE, Time);
  FlashLog_File.sync();
  FlashLog_File.close(); }

static int FlashLog_Open(uint32_t Time)
{ if(FlashLog_File.isOpen()) FlashLog_Close(Time);
  FlashLog_CleanEmpty(32);
  FlashLog_Clean(2*FlashLog_MaxSize, 2);
  FlashLog_FullFileName(FlashLog_FileName, Time);
  FlashLog_FileTime = Time;
  FlashLog_FileFlush = 0;

  if(!FlashLog_OpenFile(FlashLog_File, FlashLog_FileName, O_WRONLY | O_CREAT | O_TRUNC))
  { FlashLog_Clean(0, 4);
    return 0; }
  FlashLog_SetTimestamp(T_CREATE|T_WRITE, Time);
  return 1; }

static void FlashLog_Reopen(uint32_t Time)
{ if(FlashLog_File.isOpen())
  { FlashLog_Close(Time);
    if(FlashLog_OpenFile(FlashLog_File, FlashLog_FileName, O_WRONLY | O_CREAT | O_APPEND | O_AT_END))
    { FlashLog_SetTimestamp(T_WRITE, Time);
      FlashLog_FileFlush = FlashLog_File.curPosition(); }
  }
  FlashLog_SaveReq = 0; }

static int FlashLog_Record(OGN_LogPacket<OGN_Packet> *Packet, int Packets, uint32_t Time)
{ if(FlashLog_File.isOpen())
  { uint32_t TimeSinceStart = Time-FlashLog_FileTime;
    uint32_t WritePos = FlashLog_File.curPosition();
    uint32_t WriteSize = Packets*OGN_LogPacket<OGN_Packet>::Bytes;
    if((TimeSinceStart>=FlashLog_MaxTime) || ((WritePos+WriteSize)>FlashLog_MaxSize))
    { FlashLog_Close(Time); }
  }

  if(!FlashLog_File.isOpen()) FlashLog_Open(Time);
  if(!FlashLog_File.isOpen()) return -1;

  int Written = FlashLog_File.write((const uint8_t *)Packet, Packets*Packet->Bytes);
  if(Written!=(Packets*Packet->Bytes))
  { FlashLog_Close(Time);
    FlashLog_Clean(0, 4);
    return -1; }

  uint32_t WritePos = FlashLog_File.curPosition();
  if(WritePos-FlashLog_FileFlush>FlashLog_SaveSize) FlashLog_Reopen(Time);
  return Packets; }

static int Copy(void)
{ OGN_LogPacket<OGN_Packet> *Packet;
  size_t Packets = FlashLog_FIFO.getReadBlock(Packet);
  if(Packets==0) return 0;

  uint32_t Time = TimeSync_Time();
  int Err = FlashLog_Record(Packet, Packets, Time);
  if(Err<0)
  { FlashLog_Clean(0, 4);
    Err = FlashLog_Record(Packet, Packets, Time); }
  FlashLog_FIFO.flushReadBlock(Packets);
  return Err; }

extern "C" void vTaskLOG(void* pvParameters)
{ (void)pvParameters;
  FlashLog_FIFO.Clear();

  LogFS_begin();
  FlashLog_UpdateStorage();
  uint32_t Oldest;
  int Files = FlashLog_FindOldestFile(Oldest, 0);

  if(xSemaphoreTake(CONS_Mutex, 200))
  { Format_String(CONS_UART_Write, "TaskLOG() ");
    if(LogFS_isMounted())
    { uint32_t Used = ((uint32_t)LogFS.clusterCount() * LogFS.bytesPerCluster()) - FlashLog_FreeBytes();
      Format_UnsDec(CONS_UART_Write, Used/1024);
      Format_String(CONS_UART_Write, "kB used, ");
      Format_UnsDec(CONS_UART_Write, ((uint32_t)LogFS.clusterCount() * LogFS.bytesPerCluster())/1024);
      Format_String(CONS_UART_Write, "kB total, "); }
    Format_SignDec(CONS_UART_Write, (int32_t)Files, 1, 0, 1);
    Format_String(CONS_UART_Write, " files\n");
    xSemaphoreGive(CONS_Mutex); }

  TickType_t PrevTick = 0;
  uint32_t StorageUpdateTime = millis();
  uint8_t StorageUpdateRequest = FlashLog_StorageUpdateRequest;
  static bool PrevFlying = 0;
  for( ; ; )
  {
    TaskWatchdog_Heartbeat(TaskWatchdog_LOG);
    vTaskDelay(1);

    if(FlashLog_USBRequest)
    {
      bool Ready=true;
      if(LogFS_isMounted())
      { while(FlashLog_FIFO.Full()>0)
        { if(Copy()<=0) { Ready=false; break; }
          vTaskDelay(1); }
        if(Ready && FlashLog_File.isOpen())
          FlashLog_Close(TimeSync_Time());
      }
      else FlashLog_FIFO.Clear();
      if(Ready)
      { LogFS_end();
        FlashLog_USBReady=true;
        vTaskSuspend(NULL);                       // USB owns the flash until reset
      }
    }

    uint32_t Now=millis();
    bool StorageUpdateRequested=FlashLog_StorageUpdateRequest!=StorageUpdateRequest;
    if(StorageUpdateRequested) StorageUpdateRequest=FlashLog_StorageUpdateRequest;
    if(StorageUpdateRequested || (FlashLog_isOpen() && (uint32_t)(Now-StorageUpdateTime)>=10000))
    { FlashLog_UpdateStorage();
      StorageUpdateTime=Now; }
    bool Flying = Flight.inFlight(); // GPS_TimeSinceLock>=10;
    Flying = Flying && PowerMode>0;
    bool Landed = PrevFlying && !Flying;
    PrevFlying = Flying;
    size_t Packets = FlashLog_FIFO.Full();

    if(Flying && LogFS_isMounted())
    { if(FlashLog_SaveReq) FlashLog_Reopen(TimeSync_Time());
      TickType_t Tick = xTaskGetTickCount();
      if(Packets==0) { PrevTick=Tick; vTaskDelay(50); continue; }
      if(Packets>=8) { Copy(); PrevTick=Tick; continue; }
      TickType_t Diff = Tick-PrevTick;
      if(Diff>=8000) { Copy(); PrevTick=Tick; continue; }
    }
    else
    { if(Landed && LogFS_isMounted())
      { while(FlashLog_FIFO.Full()>0)
        { if(Copy()<=0) break;
          vTaskDelay(1); }
      }
      if(FlashLog_File.isOpen()) FlashLog_Close(TimeSync_Time());
      while(FlashLog_FIFO.Full()>=FlashLog_FIFO.Len/2)
      { FlashLog_FIFO.Read();
        vTaskDelay(1); }
    }
    if(Landed) FlashLog_UpdateStorage();
    vTaskDelay(50);
  }
}

#endif

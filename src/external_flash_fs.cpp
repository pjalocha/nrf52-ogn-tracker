#include "external_flash_fs.h"

#if defined(EXTERNAL_FLASH_USE_QSPI) || defined(EXTERNAL_FLASH_USE_SPI)

#if defined(EXTERNAL_FLASH_USE_QSPI)
static Adafruit_FlashTransport_QSPI FlashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
static Adafruit_FlashTransport_SPI FlashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
#endif

Adafruit_SPIFlash ExternalFlash(&FlashTransport);
FatVolume LogFS;

static bool ExternalFlashBegun = false;
static bool LogFSMounted = false;
static uint32_t LastRawJEDEC = 0;

static const SPIFlash_Device_t FlashDevices[] = {
  ZD25WQ16B,
  P25Q16H,
  GD25Q16C,
  MX25R1635F,
  S25FL116K,
  W25Q16FW,
  W25Q16JV_IQ,
  GD25Q32C,
  W25Q32JV_IQ,
};

static uint32_t LogFS_readRawJEDEC(void)
{
  uint8_t JEDEC[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
  FlashTransport.readCommand(SFLASH_CMD_READ_JEDEC_ID, JEDEC, sizeof(JEDEC));
  return ((uint32_t)JEDEC[0] << 16) | ((uint32_t)JEDEC[1] << 8) | JEDEC[2];
}

static void putLE16(uint8_t *Data, uint32_t Ofs, uint16_t Value)
{
  Data[Ofs+0] = Value;
  Data[Ofs+1] = Value >> 8;
}

static void putLE32(uint8_t *Data, uint32_t Ofs, uint32_t Value)
{
  Data[Ofs+0] = Value;
  Data[Ofs+1] = Value >> 8;
  Data[Ofs+2] = Value >> 16;
  Data[Ofs+3] = Value >> 24;
}

static bool LogFS_writeZeroSectors(uint32_t Sector, uint32_t Count)
{
  uint8_t Buffer[512];
  memset(Buffer, 0, sizeof(Buffer));
  for(uint32_t Idx=0; Idx<Count; Idx++)
  { if(!ExternalFlash.writeBlocks(Sector+Idx, Buffer, 1)) return false; }
  return true;
}

static bool LogFS_formatFAT12(Print &Out)
{
  const uint32_t DevSectors = ExternalFlash.size() / 512;
  if(DevSectors < 256) return false;

  const uint32_t PartStart = 1;
  const uint32_t PartSectors = DevSectors - PartStart;
  const uint16_t RootEntries = 512;
  const uint16_t RootSectors = (RootEntries * 32 + 511) / 512;

  uint8_t SectorsPerCluster = 1;
  uint16_t SectorsPerFAT = 1;
  uint32_t Clusters = 0;
  for( ; SectorsPerCluster<=128; SectorsPerCluster<<=1)
  { SectorsPerFAT = 1;
    for(uint8_t Iter=0; Iter<8; Iter++)
    { uint32_t DataSectors = PartSectors - 1 - 2*SectorsPerFAT - RootSectors;
      Clusters = DataSectors / SectorsPerCluster;
      uint32_t FatBytes = ((Clusters + 2) * 3 + 1) / 2;
      uint16_t NeededSectors = (FatBytes + 511) / 512;
      if(NeededSectors == SectorsPerFAT) break;
      SectorsPerFAT = NeededSectors; }
    if(Clusters>0 && Clusters<=4084) break; }

  if(Clusters==0 || Clusters>4084) return false;

  const uint32_t FatStart = PartStart + 1;

  uint8_t Sector[512];
  memset(Sector, 0, sizeof(Sector));
  Sector[446+0] = 0x00;
  Sector[446+1] = 0x00; Sector[446+2] = 0x02; Sector[446+3] = 0x00;
  Sector[446+4] = 0x01;
  Sector[446+5] = 0xFE; Sector[446+6] = 0xFF; Sector[446+7] = 0xFF;
  putLE32(Sector, 446+8, PartStart);
  putLE32(Sector, 446+12, PartSectors);
  Sector[510] = 0x55; Sector[511] = 0xAA;
  if(!ExternalFlash.writeBlocks(0, Sector, 1)) return false;

  memset(Sector, 0, sizeof(Sector));
  Sector[0] = 0xEB; Sector[1] = 0x3C; Sector[2] = 0x90;
  memcpy(Sector+3, "MSDOS5.0", 8);
  putLE16(Sector, 11, 512);
  Sector[13] = SectorsPerCluster;
  putLE16(Sector, 14, 1);
  Sector[16] = 2;
  putLE16(Sector, 17, RootEntries);
  if(PartSectors <= 0xFFFF) putLE16(Sector, 19, PartSectors);
  Sector[21] = 0xF8;
  putLE16(Sector, 22, SectorsPerFAT);
  putLE16(Sector, 24, 63);
  putLE16(Sector, 26, 255);
  putLE32(Sector, 28, PartStart);
  if(PartSectors > 0xFFFF) putLE32(Sector, 32, PartSectors);
  Sector[36] = 0x80;
  Sector[38] = 0x29;
  putLE32(Sector, 39, 0x20260630);
  memcpy(Sector+43, "EXT FLASH  ", 11);
  memcpy(Sector+54, "FAT12   ", 8);
  Sector[510] = 0x55; Sector[511] = 0xAA;
  if(!ExternalFlash.writeBlocks(PartStart, Sector, 1)) return false;

  if(!LogFS_writeZeroSectors(FatStart, 2*SectorsPerFAT + RootSectors)) return false;

  memset(Sector, 0, sizeof(Sector));
  Sector[0] = 0xF8; Sector[1] = 0xFF; Sector[2] = 0xFF;
  if(!ExternalFlash.writeBlocks(FatStart, Sector, 1)) return false;
  if(!ExternalFlash.writeBlocks(FatStart + SectorsPerFAT, Sector, 1)) return false;

  ExternalFlash.syncBlocks();
  Out.print("ExternalFlash: FAT12 clusters=");
  Out.print(Clusters);
  Out.print(" sectors/cluster=");
  Out.print(SectorsPerCluster);
  Out.print(" sectors/FAT=");
  Out.println(SectorsPerFAT);
  return true;
}

bool LogFS_begin(void)
{
  if(LogFSMounted) return true;

  ExternalFlashBegun = ExternalFlash.begin(FlashDevices, sizeof(FlashDevices)/sizeof(FlashDevices[0]));
  LastRawJEDEC = ExternalFlashBegun ? ExternalFlash.getJEDECID() : LogFS_readRawJEDEC();
  if(!ExternalFlashBegun) return false;

  LogFSMounted = LogFS.begin(&ExternalFlash);
  return LogFSMounted;
}

bool LogFS_isMounted(void)
{
  return LogFSMounted;
}

int LogFS_readFile(const char *Name, void *Data, size_t Size)
{
  if(!LogFSMounted) return -1;
  FatFile File;
  if(!File.open(&LogFS, Name, O_RDONLY)) return -2;
  if(File.fileSize() != Size)
  { File.close(); return -3; }
  int Read = File.read(Data, Size);
  File.close();
  return Read == (int)Size ? Read : -4;
}

int LogFS_writeFile(const char *Name, const void *Data, size_t Size)
{
  if(!LogFSMounted) return -1;
  LogFS.remove(Name);
  FatFile File;
  if(!File.open(&LogFS, Name, O_WRONLY | O_CREAT | O_TRUNC)) return -2;
  int Written = File.write((const uint8_t *)Data, Size);
  File.sync();
  File.close();
  return Written == (int)Size ? Written : -3;
}

bool LogFS_format(Print &Out)
{
  if(!ExternalFlashBegun)
  { ExternalFlashBegun = ExternalFlash.begin(FlashDevices, sizeof(FlashDevices)/sizeof(FlashDevices[0]));
    LastRawJEDEC = ExternalFlashBegun ? ExternalFlash.getJEDECID() : LogFS_readRawJEDEC();
    if(!ExternalFlashBegun)
    { Out.println("ExternalFlash: not detected");
      return false; }
  }

  Out.println("ExternalFlash: formatting FAT");
  LogFSMounted = false;

  if(!LogFS_formatFAT12(Out))
  { Out.println("ExternalFlash: format failed");
    return false; }

  ExternalFlash.syncBlocks();
  LogFSMounted = LogFS.begin(&ExternalFlash);
  Out.println(LogFSMounted ? "ExternalFlash: format complete" : "ExternalFlash: formatted but mount failed");
  return LogFSMounted;
}

void LogFS_printStatus(Print &Out)
{
  Out.print("ExternalFlash: ");
  if(!ExternalFlashBegun)
  { Out.print("not detected");
    if(LastRawJEDEC)
    { Out.print(" RawJEDEC=0x");
      Out.print(LastRawJEDEC, HEX); }
    Out.println();
    return; }

  Out.print("JEDEC=0x");
  Out.print(LastRawJEDEC, HEX);
  Out.print(" Size=");
  Out.print(ExternalFlash.size() >> 10);
  Out.print("kB FAT=");
  Out.println(LogFSMounted ? "mounted" : "not mounted");
}

void LogFS_listRoot(Print &Out)
{
  if(!ExternalFlashBegun || !LogFSMounted)
  { LogFS_printStatus(Out);
    return; }

  FatFile Root;
  FatFile File;
  if(!Root.open("/"))
  { Out.println("ExternalFlash: open root failed");
    return; }

  Out.println("ExternalFlash root:");
  bool Empty = true;
  while(File.openNext(&Root, O_RDONLY))
  { Empty = false;
    File.printFileSize(&Out);
    Out.write(' ');
    File.printName(&Out);
    if(File.isDir()) Out.write('/');
    Out.println();
    File.close(); }

  Root.close();
  if(Empty) Out.println("  <empty>");
}

#endif

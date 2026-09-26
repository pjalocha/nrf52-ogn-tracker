#include "main.h"
#include "usb_memory.h"

#if defined(WITH_USB_MEMORY)

#include "external_flash_fs.h"
#include "log.h"

static volatile bool USBMemory_ActiveFlag=false;
static bool USBMemory_Started=false;
static Adafruit_USBD_MSC USBMemory_MSC;

bool USBMemory_IsActive(void)
{ return USBMemory_ActiveFlag; }

static int32_t USBMemory_Read(uint32_t LBA, void *Buffer, uint32_t Size)
{
  if(!USBMemory_ActiveFlag || (Size&511)) return -1;
  return ExternalFlash.readBlocks(LBA, (uint8_t *)Buffer, Size/512) ? (int32_t)Size : -1;
}

static int32_t USBMemory_Write(uint32_t LBA, uint8_t *Buffer, uint32_t Size)
{
  if(!USBMemory_ActiveFlag || (Size&511)) return -1;
  return ExternalFlash.writeBlocks(LBA, Buffer, Size/512) ? (int32_t)Size : -1;
}

static void USBMemory_Flush(void)
{ ExternalFlash.syncBlocks(); }

bool USBMemory_Enter(void)
{
  if(USBMemory_ActiveFlag) return true;
  if(!LogFS_isDetected()) return false;

  // The LOG task owns FatFile and FatVolume.  Let it flush the FIFO, close
  // the current file, and release the FAT cache before USB gets ownership.
  if(!FlashLog_PrepareUSB(5000)) return false;

  if(!USBMemory_Started)
  {
    USBMemory_MSC.setID("OGN", "Flight logs", "1.0");
    USBMemory_MSC.setCapacity(ExternalFlash.size()/512, 512);
    USBMemory_MSC.setReadWriteCallback(USBMemory_Read, USBMemory_Write, USBMemory_Flush);
    USBMemory_MSC.setUnitReady(false);
    if(!USBMemory_MSC.begin()) return false;
    USBMemory_Started=true;
  }

  TaskWatchdog_EnterMaintenance();
  USBMemory_ActiveFlag=true;
  USBMemory_MSC.setUnitReady(true);

  // CDC was already enumerated during startup.  Re-enumerate so the host
  // sees the newly added MSC interface together with the console interface.
  if(TinyUSBDevice.mounted())
  { TinyUSBDevice.detach();
    vTaskDelay(pdMS_TO_TICKS(20));
    TinyUSBDevice.attach(); }
  return true;
}

#else

bool USBMemory_IsActive(void) { return false; }
bool USBMemory_Enter(void) { return false; }

#endif

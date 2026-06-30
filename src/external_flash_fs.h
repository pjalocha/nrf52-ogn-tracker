#pragma once

#include <Arduino.h>

#if defined(EXTERNAL_FLASH_USE_QSPI) || defined(EXTERNAL_FLASH_USE_SPI)

#include "Adafruit_SPIFlash.h"
#include "SdFat_Adafruit_Fork.h"

extern Adafruit_SPIFlash ExternalFlash;
extern FatVolume LogFS;

bool LogFS_begin(void);
bool LogFS_isMounted(void);
bool LogFS_format(Print &Out);
void LogFS_printStatus(Print &Out);
void LogFS_listRoot(Print &Out);

#else

inline bool LogFS_begin(void) { return false; }
inline bool LogFS_isMounted(void) { return false; }
inline bool LogFS_format(Print &Out) { Out.println("ExternalFlash: not configured"); return false; }
inline void LogFS_printStatus(Print &Out) { Out.println("ExternalFlash: not configured"); }
inline void LogFS_listRoot(Print &Out) { LogFS_printStatus(Out); }

#endif

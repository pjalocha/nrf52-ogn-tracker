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
int LogFS_readFile(const char *Name, void *Data, size_t Size);
int LogFS_writeFile(const char *Name, const void *Data, size_t Size);

#else

inline bool LogFS_begin(void) { return false; }
inline bool LogFS_isMounted(void) { return false; }
inline bool LogFS_format(Print &Out) { Out.println("ExternalFlash: not configured"); return false; }
inline void LogFS_printStatus(Print &Out) { Out.println("ExternalFlash: not configured"); }
inline void LogFS_listRoot(Print &Out) { LogFS_printStatus(Out); }
inline int LogFS_readFile(const char *, void *, size_t) { return -1; }
inline int LogFS_writeFile(const char *, const void *, size_t) { return -1; }

#endif

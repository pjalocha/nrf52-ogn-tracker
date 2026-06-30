#pragma once

#include "fifo.h"
#include "ogn.h"

extern FIFO<OGN_LogPacket<OGN_Packet>, 32> FlashLog_FIFO;

extern const char *FlashLog_Path;
extern const char *FlashLog_Ext;

void AddPath(char *Name, const char *FileName, const char *Path);

extern bool     FlashLog_SaveReq;
extern uint32_t FlashLog_FileTime;
extern char     FlashLog_FileName[32];
extern uint32_t FlashLog_FileFlush;
extern int      FlashLog_Files;

bool FlashLog_isOpen(void);
int  FlashLog_FullFileName(char *FileName, uint32_t Time);
int  FlashLog_ShortFileName(char *FileName, uint32_t Time);
uint32_t FlashLog_ReadShortFileTime(const char *FileName, int Len);
uint32_t FlashLog_ReadShortFileTime(const char *FileName);
int  FlashLog_FindOldestFile(uint32_t &Oldest, uint32_t After=0);
int  FlashLog_ListFiles(void);
int  FlashLog_ListFile(uint32_t FileTime);
int  FlashLog_ListFile(const char *FileName, uint32_t FileTime);

#ifdef __cplusplus
extern "C"
#endif
void vTaskLOG(void* pvParameters);

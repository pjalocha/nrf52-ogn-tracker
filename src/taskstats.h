#pragma once

#ifdef WITH_TASK_STATS

#include <stdint.h>

struct TaskStats_Record
{
  char     Name[9];
  uint8_t  CPU;             // [%] since the previous snapshot
  uint16_t StackFree;       // FreeRTOS stack words at the low-water mark
};

void TaskStats_Update(void);
void TaskStats_Print(void);
void TaskStats_Init(void);
uint8_t TaskStats_Copy(TaskStats_Record *Records, uint8_t MaxRecords);
uint32_t TaskStats_WindowUS(void);

#endif

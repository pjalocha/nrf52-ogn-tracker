#pragma once

#include <stdint.h>

// Application tasks which must continue to run for the hardware watchdog to
// be reloaded.  The list is independent of task handles: each task reports
// from its own loop.
enum TaskWatchdog_Id : uint8_t
{
  TaskWatchdog_Loop = 0,
  TaskWatchdog_GPS,
  TaskWatchdog_RF,
  TaskWatchdog_PROC,
  TaskWatchdog_LOG,
  TaskWatchdog_EPD,
  TaskWatchdog_OLED,
  TaskWatchdog_Max
};

static inline uint32_t TaskWatchdog_Bit(TaskWatchdog_Id Id)
{ return 1UL << (uint8_t)Id; }

#ifdef WITH_TASK_WATCHDOG
void TaskWatchdog_Heartbeat(TaskWatchdog_Id Id);
void TaskWatchdog_Start(uint32_t ExpectedTasks);
void TaskWatchdog_EnterMaintenance(void);
#else
static inline void TaskWatchdog_Heartbeat(TaskWatchdog_Id) { }
static inline void TaskWatchdog_Start(uint32_t) { }
static inline void TaskWatchdog_EnterMaintenance(void) { }
#endif

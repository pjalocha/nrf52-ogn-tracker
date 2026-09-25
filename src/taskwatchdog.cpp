#include "main.h"
#include "taskwatchdog.h"

#ifdef WITH_TASK_WATCHDOG

#include <task.h>

// The WDT is intentionally given a long timeout.  The supervisor normally
// reloads it once per second, but a long flash operation or radio transaction
// must not cause a spurious reset.  A genuinely stuck task is still recovered
// without relying on the USB console.
static const uint32_t TaskWatchdog_TimeoutMS = 30000;
static const uint32_t TaskWatchdog_StartupGraceMS = 15000;
static const uint32_t TaskWatchdog_StaleMS = 10000;

static volatile uint32_t TaskWatchdog_Seen = 0;
static volatile uint32_t TaskWatchdog_Last[TaskWatchdog_Max] = { 0 };
static uint32_t TaskWatchdog_Expected = 0;
static uint32_t TaskWatchdog_StartTime = 0;
static bool TaskWatchdog_Started = false;

static void TaskWatchdog_Reload(void)
{ NRF_WDT->RR[0] = WDT_RR_RR_Reload; }

static bool TaskWatchdog_Healthy(uint32_t Now)
{
  if((uint32_t)(Now-TaskWatchdog_StartTime)<TaskWatchdog_StartupGraceMS)
    return true;                              // allow all tasks to start

  if((TaskWatchdog_Seen & TaskWatchdog_Expected)!=TaskWatchdog_Expected)
    return false;                             // a task never reached its loop

  for(uint8_t Id=0; Id<TaskWatchdog_Max; Id++)
  { uint32_t Bit=1UL<<Id;
    if((TaskWatchdog_Expected&Bit) &&
       (uint32_t)(Now-TaskWatchdog_Last[Id])>TaskWatchdog_StaleMS)
      return false;
  }
  return true;
}

static void TaskWatchdog_Task(void *Parms)
{
  (void)Parms;
  for(;;)
  { vTaskDelay(pdMS_TO_TICKS(1000));
    uint32_t Now=millis();
    if(TaskWatchdog_Healthy(Now)) TaskWatchdog_Reload();
    // If a task is missing or stale, deliberately stop reloading.  The
    // hardware watchdog then resets the MCU instead of trying to recover from
    // a possibly corrupted FreeRTOS state in software.
  }
}

void TaskWatchdog_Heartbeat(TaskWatchdog_Id Id)
{
  if(Id>=TaskWatchdog_Max) return;
  TaskWatchdog_Last[Id]=millis();
  TaskWatchdog_Seen |= TaskWatchdog_Bit(Id);
}

void TaskWatchdog_Start(uint32_t ExpectedTasks)
{
  if(TaskWatchdog_Started) return;
  TaskWatchdog_Expected=ExpectedTasks;
  TaskWatchdog_StartTime=millis();
  TaskWatchdog_Seen=0;

  // Run during sleep, but pause while halted by a debugger.
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
                    (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
  NRF_WDT->CRV = (TaskWatchdog_TimeoutMS*32768UL)/1000UL;
  NRF_WDT->RREN = WDT_RREN_RR0_Msk;
  NRF_WDT->TASKS_START = 1;
  TaskWatchdog_Started=true;

  // Highest application priority ensures the supervisor still gets CPU time
  // if a lower-priority task spins without yielding.
  xTaskCreate(TaskWatchdog_Task, "WDT", 256, NULL,
              configMAX_PRIORITIES-1, NULL);
}

#endif

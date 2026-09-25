#include "main.h"
#include "taskstats.h"

#ifdef WITH_TASK_STATS

#include <task.h>
#include <string.h>

static const uint8_t TaskStats_MaxTasks = 16;

struct TaskStats_Previous
{
  UBaseType_t  Number;
  uint32_t     RunTime;
};

static TaskStats_Record   TaskStats_Records[TaskStats_MaxTasks];
static TaskStats_Previous TaskStats_PreviousRecords[TaskStats_MaxTasks];
static uint8_t            TaskStats_RecordCount = 0;
static uint8_t            TaskStats_PreviousCount = 0;
static uint32_t           TaskStats_PreviousTotal = 0;
static uint32_t           TaskStats_Window = 0; // [FreeRTOS ticks]
static bool                TaskStats_HavePrevious = false;
static SemaphoreHandle_t  TaskStats_Mutex = 0;

static int TaskStats_FindPrevious(UBaseType_t Number)
{ for(uint8_t Idx=0; Idx<TaskStats_PreviousCount; Idx++)
    if(TaskStats_PreviousRecords[Idx].Number==Number) return Idx;
  return -1; }

void TaskStats_Init(void)
{ if(!TaskStats_Mutex) TaskStats_Mutex=xSemaphoreCreateMutex(); }

static void TaskStats_UpdateLocked(void)
{
  TaskStatus_t Status[TaskStats_MaxTasks];
  uint32_t TotalRunTime=0;
  UBaseType_t Count=uxTaskGetSystemState(Status, TaskStats_MaxTasks, &TotalRunTime);
  if(Count==0 || Count>TaskStats_MaxTasks) return;

  uint32_t Window=TaskStats_HavePrevious ? TotalRunTime-TaskStats_PreviousTotal : TotalRunTime;
  TaskStats_Window=Window;
  TaskStats_RecordCount=(uint8_t)Count;
  uint32_t Deltas[TaskStats_MaxTasks];
  uint64_t SumDelta=0;

  for(uint8_t Idx=0; Idx<TaskStats_RecordCount; Idx++)
  { TaskStats_Record &Record=TaskStats_Records[Idx];
    const TaskStatus_t &Current=Status[Idx];
    strncpy(Record.Name, Current.pcTaskName ? Current.pcTaskName : "?", sizeof(Record.Name)-1);
    Record.Name[sizeof(Record.Name)-1]=0;
    Record.StackFree=Current.usStackHighWaterMark;

    uint32_t Delta=Current.ulRunTimeCounter;
    int PrevIdx=TaskStats_FindPrevious(Current.xTaskNumber);
    if(TaskStats_HavePrevious && PrevIdx>=0)
      Delta-=TaskStats_PreviousRecords[PrevIdx].RunTime;
    /* A task cannot consume more than the elapsed single-CPU window. */
    if(Window && Delta>Window) Delta=0;
    Deltas[Idx]=Delta;
    SumDelta+=Delta;

    TaskStats_PreviousRecords[Idx].Number=Current.xTaskNumber;
    TaskStats_PreviousRecords[Idx].RunTime=Current.ulRunTimeCounter;
  }
  for(uint8_t Idx=0; Idx<TaskStats_RecordCount; Idx++)
  { TaskStats_Record &Record=TaskStats_Records[Idx];
    if(SumDelta) Record.CPU=(uint8_t)(((uint64_t)Deltas[Idx]*100+SumDelta/2)/SumDelta);
    else         Record.CPU=0;
    if(Record.CPU>100) Record.CPU=100; }
  TaskStats_PreviousCount=TaskStats_RecordCount;
  TaskStats_PreviousTotal=TotalRunTime;
  TaskStats_HavePrevious=true;
}

void TaskStats_Update(void)
{ if(TaskStats_Mutex && xSemaphoreTake(TaskStats_Mutex, portMAX_DELAY))
  { TaskStats_UpdateLocked();
    xSemaphoreGive(TaskStats_Mutex); }
}

uint8_t TaskStats_Copy(TaskStats_Record *Records, uint8_t MaxRecords)
{ if(!Records || !MaxRecords) return 0;
  if(TaskStats_Mutex && !xSemaphoreTake(TaskStats_Mutex, portMAX_DELAY)) return 0;
  uint8_t Count=TaskStats_RecordCount;
  if(Count>MaxRecords) Count=MaxRecords;
  memcpy(Records, TaskStats_Records, Count*sizeof(TaskStats_Record));
  if(TaskStats_Mutex) xSemaphoreGive(TaskStats_Mutex);
  return Count; }

uint32_t TaskStats_WindowUS(void)
{ uint32_t Window=0;
  if(TaskStats_Mutex && !xSemaphoreTake(TaskStats_Mutex, portMAX_DELAY)) return 0;
  Window=TaskStats_Window;
  if(TaskStats_Mutex) xSemaphoreGive(TaskStats_Mutex);
  return (uint32_t)(((uint64_t)Window*1000000UL)/configTICK_RATE_HZ); }

void TaskStats_Print(void)
{
  TaskStats_Update();
  TaskStats_Record Records[TaskStats_MaxTasks];
  uint8_t Count=TaskStats_Copy(Records, TaskStats_MaxTasks);
  Serial.printf("%u tasks over %3.1fs\n",
                Count, 1e-6f*TaskStats_WindowUS());
  Serial.println("Task       CPU  stack-free(words)");
  for(uint8_t Idx=0; Idx<Count; Idx++)
  { const TaskStats_Record *Record=Records+Idx;
    Serial.printf("%-8s %3u%% %5u\n", Record->Name, Record->CPU, Record->StackFree); }
}

#endif

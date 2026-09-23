/* Project-owned FreeRTOS run-time statistics configuration. */
#ifndef OGN_TASKSTATS_FREERTOS_H
#define OGN_TASKSTATS_FREERTOS_H

/* Load the Adafruit nRF52 configuration, then enable its optional statistics. */
#include_next "FreeRTOSConfig.h"

#undef configGENERATE_RUN_TIME_STATS
#define configGENERATE_RUN_TIME_STATS 1

/* Use the FreeRTOS tick counter as the run-time clock.  The nRF52 DWT
   counter wraps after about 67 seconds at 64MHz, which is too short for
   the 32-bit FreeRTOS run-time counters. */
#undef portCONFIGURE_TIMER_FOR_RUN_TIME_STATS
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() do { } while(0)

#undef portGET_RUN_TIME_COUNTER_VALUE
#define portGET_RUN_TIME_COUNTER_VALUE()                                      \
  ((uint32_t)xTaskGetTickCount())

#endif

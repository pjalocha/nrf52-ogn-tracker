#pragma once

#ifdef WITH_LOOKOUT                   // traffic awareness and warnings
#include "lookout.h"
extern LookOut<32> Look;
#endif

extern uint32_t BatteryVoltage;       // [1/256 mV] averaged
extern  int32_t BatteryVoltageRate;   // [1/256 mV] averaged

extern uint8_t               LookOut_AlarmLevel;    // 0:no LookOut alarm
extern const LookOut_Target *LookOut_AlarmTgt;      // the most alarming target (when AlarmLevel>0)

extern uint32_t RxProc_Count[8];      // counters for correctly received packets

// extern FlightMonitor Flight;

#ifdef WITH_ESP32
const uint8_t RelayQueueSize = 32;
#else
const uint8_t RelayQueueSize = 16;
#endif

// received packets and candidates to be relayed
extern Relay_PrioQueue<OGN_RxPacket<OGN_Packet>, RelayQueueSize> OGN_RelayQueue;
// received ADSL packets and candidates to be relayed
extern Relay_PrioQueue<ADSL_RxPacket, RelayQueueSize>           ADSL_RelayQueue;

#ifdef __cplusplus
  extern "C"
#endif
 void vTaskPROC(void* pvParameters);

#ifndef __TIMESYNC_H__
#define __TIMESYNC_H__

#include <stdint.h>

#include "hal.h"

class TimeSync
{ public:
   static constexpr uint32_t ClockHz = 1000; // millis() time base, independent of the 1024 Hz RTOS tick
   uint32_t     UTC;       // [sec] UTC Unix time
   uint32_t sysTime;       // [ms]  system time which corresponds to the above UTC

  public:
   uint16_t getFracTime(uint32_t msTime) const
   { msTime = msTime+ClockHz-sysTime;
     if(msTime<ClockHz) return msTime;
     if(msTime<ClockHz*2) return msTime-ClockHz;
     if(msTime<ClockHz*3) return msTime-ClockHz*2;
     return msTime%ClockHz; }

   void Norm(uint32_t msTime)
   { int32_t msDiff = msTime-sysTime;
     if(msDiff>=ClockHz*2) { sysTime+=ClockHz*2; UTC+=2; return; }
     if(msDiff>=ClockHz  ) { sysTime+=ClockHz  ; UTC++; }
   }

   // uint32_t getUTC(uint32_t msTime) const
   // { int32_t msDiff = msTime-sysTime;
   //   if(msDiff>=0) return UTC+msDiff/1000;
   // }

} ;

extern TimeSync GPS_TimeSync;

void TimeSync_HardPPS(TickType_t Tick);                                     // hardware PPS at the give system tick
void TimeSync_HardPPS(void);

void TimeSync_SoftPPS(TickType_t Tick, uint32_t Time, int32_t msOfs=100);   // software PPS: from GPS burst start or from MAV

TickType_t TimeSync_msTime(TickType_t Tick);                                // [ms] get fractional time which corresponds to given system tick
TickType_t TimeSync_msTime(void);

uint32_t TimeSync_Time(TickType_t Tick);                                    // [sec] get Time which  corresponds to given system tick
uint32_t TimeSync_Time(void);

void TimeSync_Time(uint32_t &Time, TickType_t &msTime, TickType_t Tick);
void TimeSync_Time(uint32_t &Time, TickType_t &msTime);

void TimeSync_CorrRef(int16_t Corr);                                        // [ms] correct the time reference [RTOS tick]

#endif // __TIMESYNC_H__

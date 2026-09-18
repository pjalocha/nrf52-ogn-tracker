#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>

// #define DEBUG_PRINT

#include "ogn.h"
#include "lookout.h"

uint8_t RefTime =                      0; // [sec]
int32_t RefLat  = +45.557567/(0.0001/60); // [0.0001/60deg]
int32_t RefLon  =  +5.980333/(0.0001/60); // [0.0001/60deg]
int32_t RefAlt  =                    290; // [m]
uint16_t LatCos =                   2845; // [ ] 4096*cos(46.0deg) = need not to be exact
 int16_t GeoidSepar =                 48;
int32_t MaxDist =                  20000; // [m]

int StopReq=0;

void SigHandler(int signum) // Signal handler, when user pressed Ctrl-C or process stops for whatever reason
{ StopReq=1; }

int main(int argc, char *argv[])
{
/*
  struct sigaction SigAction;
  SigAction.sa_handler = SigHandler;              // setup the signal handler (for Ctrl-C or when process is stopped)
  sigemptyset(&SigAction.sa_mask);
  SigAction.sa_flags = 0;

  struct sigaction SigIgnore;
  SigIgnore.sa_handler = SIG_IGN;
  sigemptyset(&SigIgnore.sa_mask);
  SigIgnore.sa_flags = 0;

  sigaction(SIGINT,  &SigAction, 0);
  sigaction(SIGTERM, &SigAction, 0);
  sigaction(SIGQUIT, &SigAction, 0);
  sigaction(SIGPIPE, &SigIgnore, 0);              // we want to ignore pipe/fifo read/write errors, we handle them by return codes
*/

  const int Targets = 5;

  const char *Call[Targets] = { "Acft0", "Acft1", "Acft2", "Acft3", "Acft4" } ;
  Acft_RelPos  Me,    Target   [Targets];         // position of own and other target aircrafts
  OGN1_Packet  MePkt, TargetPkt[Targets];         // OGN packets
  ADSL_Packet           AdslPkt[Targets];         // ADS-L packets
  LookOut<32>                       Look;         // the algorithm

  MePkt.Clear();                                  // my own ID
  MePkt.Header.Address = 0xE01234;
  MePkt.Header.AddrType = 3;
  MePkt.Position.AcftType = 1;
  MePkt.calcAddrParity();

  for(int Tgt=0; Tgt<Targets; Tgt++)
  { TargetPkt[Tgt].Clear();                             // Targer1 ID
      AdslPkt[Tgt].Init();
    TargetPkt[Tgt].Header.AddrType = 2+(Tgt&1);
      AdslPkt[Tgt].setAddrTable(2+(Tgt&1));
    TargetPkt[Tgt].Header.Address = 0xC01234 + (Tgt<<16);
      AdslPkt[Tgt].setAddress(TargetPkt[Tgt].Header.Address);
    TargetPkt[Tgt].Position.AcftType = 1+Tgt;
      AdslPkt[Tgt].setAcftTypeOGN(1+Tgt);
    TargetPkt[Tgt].calcAddrParity(); }

  Me.Clear();                                     // my own position/speed
  Me.dStdAlt      =      50; // [0.5m]
  Me.hasStdAlt    =       1;
  Me.Speed        =      60; // [0.5m/s]
  Me.Climb        =       4; // [0.5m/s]
  Me.hasClimb     =       1;
  Me.Heading      =  0x0000; // [cordic] north
  Me.Turn         = +0x0200; // [] right turn, about standard ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Me.hasTurn      =       1;
  Me.calcDir();

  Target[0].Clear();
  Target[0].dStdAlt =      50;
  Target[0].hasStdAlt =     1;
  Target[0].Speed   =     100; // [0.5m/s]
  Target[0].Climb   =       4; // [0.5m/s]
  Target[0].hasClimb  =     1;
  Target[0].Heading =  0x8000; // [cordic] head south
  Target[0].Turn    = +0x0200; // [] right turn, about standard ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Target[0].hasTurn   =     1;
  Target[0].calcDir();

  Target[1].Clear();
  Target[1].dStdAlt =      50;
  Target[1].hasStdAlt =     1;
  Target[1].Speed   =      60; // [] [0.5m/s]
  Target[1].Climb   =       4; // [0.5m/s]
  Target[1].hasClimb  =     1;
  Target[1].Heading =  0x4000; // [cordic] head east
  Target[1].Turn    = -0x0200; // [] left turn, about standart ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Target[1].hasTurn   =     1;
  Target[1].calcDir();

  Target[2].Clear();
  Target[2].dStdAlt =      50;
  Target[2].hasStdAlt =     1;
  Target[2].Speed   =      40; // [] [0.5m/s]
  Target[2].Climb   =       2; // [0.5m/s]
  Target[2].hasClimb  =     1;
  Target[2].Heading =  0xC000; // [cordic] head west
  Target[2].Turn    = +0x0100; // [] left turn, about standart ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Target[2].hasTurn   =     1;
  Target[2].calcDir();

  Target[3].Clear();
  Target[3].dStdAlt =      50;
  Target[3].hasStdAlt =     1;
  Target[3].Speed   =      80; // [] [0.5m/s]
  Target[3].Climb   =       2; // [0.5m/s]
  Target[3].hasClimb  =     1;
  Target[3].Heading =  0xA000; // [cordic] head west
  Target[3].Turn    = -0x0200; // [] left turn, about standart ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Target[3].hasTurn   =     1;
  Target[3].calcDir();

  Target[4].Clear();
  // Target[4].dStdAlt =      50;
  // Target[4].hasStdAlt =     1;
  Target[4].Speed   =       1; // [] [0.5m/s]
  Target[4].Climb   =       0; // [0.5m/s]
  Target[4].hasClimb  =     1;
  Target[4].Heading =  0x2000; // [cordic]
  Target[4].Turn    = -0x0010; // [] left turn, about standart ROT, 0x0100 = 1.4deg/s => full circle in 4*64sec
  Target[4].hasTurn   =     1;
  Target[4].calcDir();

  time_t Now = time(&Now); srand(Now);                                        // initialize random number gen. with current time

  Look.Clear();               // clear the algorithm
  Look.GeoidSepar=GeoidSepar; // [m]
  uint32_t Time=0;            // [sec] count time
  // for( ; !StopReq; Time++)
  for( ; Time<520; Time++)
  {
    uint32_t Min=Time/60;
    printf("%02d:%02d\n", Min, Time-60*Min);                                  // print the time
    // Me.Print();
    // Target[0].Print();
    // Target[1].Print();
           Me.Write(    MePkt,    RefTime, RefLat, RefLon, RefAlt, LatCos);   // write position/speed into OGN packets
    for(int Tgt=0; Tgt<Targets; Tgt++)
    { Target[Tgt].Write(TargetPkt[Tgt], RefTime, RefLat, RefLon, RefAlt, LatCos);
      Target[Tgt].Write(AdslPkt[Tgt], RefTime, RefLat, RefLon, RefAlt, LatCos, GeoidSepar); }
    MePkt.Print();
    for(int Tgt=0; Tgt<Targets; Tgt++)
    { TargetPkt[Tgt].Print();
        AdslPkt[Tgt].Print(); }

    for(int Tgt=0; Tgt<Targets; Tgt++)
    { if(rand()&0x100)                                    // 50% packet loss
      { // TargetPkt[Tgt].Print();
        const LookOut_Target *TgtPtr = 0;
        if(rand()&0x1000) TgtPtr = Look.ProcessTarget(TargetPkt[Tgt], Time);   // process Target OGN packet
                     else TgtPtr = Look.ProcessTarget(AdslPkt[Tgt], Time);     // process Target ADS-L packet
      }
    }

    const LookOut_Target *TgtPtr = Look.ProcessOwn(MePkt, Time);               // process my own packet
    if(TgtPtr)
    { printf("Look[%3d]: ID=%08X, Warn=%d, TimeMargin=%4.1fs, MissTime=%4.1fs, MissDist=%5.1fm, HorDist=%6.1f, RelBearing=%+6.1fdeg\n",
      Time, TgtPtr->ID, TgtPtr->WarnLevel, 0.5*TgtPtr->TimeMargin, 0.5*TgtPtr->MissTime, 0.5*TgtPtr->MissDist, 0.5*TgtPtr->HorDist, 180.0/0x8000*Look.getRelBearing(TgtPtr)); }
    else
    { printf("Look[%3d]: none\n", Time); }

    Look.Print();                                                             // debug print
    Look.PrintPFLA();                                                         // print the PFLAU for status and PFLAA for targets
/*
    GDL90_REPORT Report;
    Look.Write(Report); Report.Print();
    for( int Idx=0; Idx<Look.MaxTargets; Idx++)
    { LookOut_Target *Tgt = Look.Target+Idx; if(!Tgt->Alloc) continue;
      Look.Write(Report, Tgt); Report.Print(); }
*/
    Me.StepFwd1sec();                                                         // move positions by 1 second
    for(int Tgt=0; Tgt<Targets; Tgt++)
      Target[Tgt].StepFwd1sec();
  }

  return 0;
}

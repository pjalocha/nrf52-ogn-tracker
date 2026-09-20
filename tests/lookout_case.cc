#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "ogn.h"
#include "lookout.h"

static const double Pi = 3.14159265358979323846;

static const uint8_t RefTime = 0;                    // [sec]
static const int32_t RefLat = +45.557567/(0.0001/60); // [0.0001/60deg]
static const int32_t RefLon =  +5.980333/(0.0001/60); // [0.0001/60deg]
static const int32_t RefAlt = 290;                   // [m]
static const uint16_t LatCos = 2845;

struct CollisionScenario
{ const char *Name;
  double CollisionX;                                 // [m] local X, relative to RefLat/RefLon
  double CollisionY;                                 // [m] local Y, relative to RefLat/RefLon
  double CollisionAlt;                               // [m] above RefAlt
  int TimeToCollision;                               // [sec]
  double MeDistance;                                 // [m] initial distance from the collision point
  double TargetDistance;                             // [m] initial distance from the collision point
  double CrossingAngle;                              // [deg] target heading relative to own heading
  double MeSpeed;                                    // [m/s]
  double TargetSpeed;                                // [m/s]
  double MeClimb;                                    // [m/s]
  double TargetClimb;                                // [m/s]
  double MeTurn;                                     // [deg/s]
  double TargetTurn;                                 // [deg/s]
  int WarnTime;                                      // [sec]
};

struct SimAircraft
{ Acft_RelPos State;
  OGN1_Packet Packet;
};

struct CollisionSetup
{ CollisionScenario Scenario;
  SimAircraft Me;
  SimAircraft Target;
  double MeEndpointError;                            // [m]
  double TargetEndpointError;                        // [m]
};

struct LookoutResult
{ double ActualMissDistance;                         // [m]
  int FirstWarningLead;                               // [sec], -1 if no warning
  int FirstLevelLead[4];                              // [sec], -1 if not reached
  uint8_t MaximumWarningLevel;
  int Score;
  bool Passed;
};

static uint16_t HeadingFromDegrees(double Degrees)
{ int32_t Value=(int32_t)llround(Degrees*65536.0/360.0);
  return (uint16_t)Value; }

static int16_t TurnFromDegrees(double Degrees)
{ int32_t Value=(int32_t)llround(Degrees*65536.0/360.0);
  return (int16_t)Value; }

static int16_t Fixed(double Value)
{ return (int16_t)llround(2.0*Value); }

static double HeadingRadians(double Degrees)
{ return Degrees*Pi/180.0; }

static void InitAircraft(SimAircraft &Aircraft, uint32_t Address,
                         double Speed, double Climb, double Turn)
{ Aircraft.State.Clear();
  Aircraft.State.Speed=Fixed(Speed);
  Aircraft.State.Climb=Fixed(Climb);
  Aircraft.State.Turn=TurnFromDegrees(Turn);
  Aircraft.State.hasClimb=1;
  Aircraft.State.hasTurn=1;
  Aircraft.State.calcDir();

  Aircraft.Packet.Clear();
  Aircraft.Packet.Header.Address=Address;
  Aircraft.Packet.Header.AddrType=3;
  Aircraft.Packet.Position.AcftType=1;
  Aircraft.Packet.calcAddrParity(); }

static double EndpointError(const Acft_RelPos &Initial, double CollisionX,
                            double CollisionY, double CollisionAlt,
                            int TimeToCollision, double Heading)
{ Acft_RelPos Test=Initial;
  Test.Heading=HeadingFromDegrees(Heading);
  Test.calcDir();
  for(int Sec=0; Sec<TimeToCollision; Sec++) Test.StepFwd1sec();

  double dX=0.5*Test.X-CollisionX;
  double dY=0.5*Test.Y-CollisionY;
  double dZ=0.5*Test.Z-CollisionAlt;
  return sqrt(dX*dX+dY*dY+dZ*dZ); }

static double FindInitialHeading(const Acft_RelPos &Initial, double NominalHeading,
                                 double CollisionX, double CollisionY,
                                 double CollisionAlt, int TimeToCollision)
{ double Best=NominalHeading;
  double BestError=EndpointError(Initial, CollisionX, CollisionY, CollisionAlt,
                                 TimeToCollision, Best);

  // Coarse-to-fine local search. The starting position remains fixed and
  // only the initial direction is adjusted for the turn rate.
  for(double Span=90.0; Span>=0.05; Span*=0.25)
  { double Step=Span/5.0;
    double LocalBest=Best;
    double LocalError=BestError;
    for(int Idx=-5; Idx<=5; Idx++)
    { double Candidate=Best+Idx*Step;
      double Error=EndpointError(Initial, CollisionX, CollisionY, CollisionAlt,
                                 TimeToCollision, Candidate);
      if(Error<LocalError)
      { LocalBest=Candidate;
        LocalError=Error; } }
    Best=LocalBest;
    BestError=LocalError; }
  return Best; }

static void SetInitialState(Acft_RelPos &State, double CollisionX,
                            double CollisionY, double CollisionAlt,
                            double Distance, double NominalHeading,
                            double Climb, int TimeToCollision)
{ double Heading=HeadingRadians(NominalHeading);
  State.X=Fixed(CollisionX-cos(Heading)*Distance);
  State.Y=Fixed(CollisionY-sin(Heading)*Distance);
  State.Z=Fixed(CollisionAlt-Climb*TimeToCollision);
  State.T=0;
  State.Heading=HeadingFromDegrees(NominalHeading);
  State.calcDir(); }

// Set up one encounter. The returned aircraft states are the initial states
// from which TraceCollision() will generate packets.
static bool SetupCollision(const CollisionScenario &Scenario,
                           CollisionSetup &Setup, bool PrintSetup=true)
{ Setup.Scenario=Scenario;
  InitAircraft(Setup.Me, 0xE01234, Scenario.MeSpeed,
               Scenario.MeClimb, Scenario.MeTurn);
  InitAircraft(Setup.Target, 0xC01234, Scenario.TargetSpeed,
               Scenario.TargetClimb, Scenario.TargetTurn);

  SetInitialState(Setup.Me.State, Scenario.CollisionX, Scenario.CollisionY,
                  Scenario.CollisionAlt, Scenario.MeDistance, 0.0,
                  Scenario.MeClimb, Scenario.TimeToCollision);
  SetInitialState(Setup.Target.State, Scenario.CollisionX, Scenario.CollisionY,
                  Scenario.CollisionAlt, Scenario.TargetDistance,
                  Scenario.CrossingAngle, Scenario.TargetClimb,
                  Scenario.TimeToCollision);

  double MeHeading=FindInitialHeading(Setup.Me.State, 0.0,
                                      Scenario.CollisionX, Scenario.CollisionY,
                                      Scenario.CollisionAlt, Scenario.TimeToCollision);
  double TargetHeading=FindInitialHeading(Setup.Target.State,
                                          Scenario.CrossingAngle,
                                          Scenario.CollisionX, Scenario.CollisionY,
                                          Scenario.CollisionAlt, Scenario.TimeToCollision);
  Setup.Me.State.Heading=HeadingFromDegrees(MeHeading);
  Setup.Me.State.calcDir();
  Setup.Target.State.Heading=HeadingFromDegrees(TargetHeading);
  Setup.Target.State.calcDir();

  Setup.MeEndpointError=EndpointError(Setup.Me.State, Scenario.CollisionX,
                                      Scenario.CollisionY, Scenario.CollisionAlt,
                                      Scenario.TimeToCollision, MeHeading);
  Setup.TargetEndpointError=EndpointError(Setup.Target.State, Scenario.CollisionX,
                                          Scenario.CollisionY, Scenario.CollisionAlt,
                                          Scenario.TimeToCollision, TargetHeading);

  if(PrintSetup)
    printf("Setup %-16s: headings me=%7.2f target=%7.2f deg, endpoint errors %5.1fm/%5.1fm\n",
           Scenario.Name, MeHeading, TargetHeading,
           Setup.MeEndpointError, Setup.TargetEndpointError);
  return Setup.MeEndpointError<50.0 && Setup.TargetEndpointError<50.0; }

static double ActualDistance(const Acft_RelPos &Me, const Acft_RelPos &Target)
{ double dX=0.5*(double)(Target.X-Me.X);
  double dY=0.5*(double)(Target.Y-Me.Y);
  double dZ=0.5*(double)(Target.Z-Me.Z);
  return sqrt(dX*dX+dY*dY+dZ*dZ); }

// Trace one setup through the actual LookOut packet interface and score it.
static LookoutResult TraceCollision(const CollisionSetup &Setup, bool PrintTrace,
                                    bool PrintResult=true)
{ LookOut<32> Look;
  Look.WarnTime=Setup.Scenario.WarnTime;
  Look.GeoidSepar=48;
  SimAircraft Me=Setup.Me;
  SimAircraft Target=Setup.Target;
  LookoutResult Result;
  Result.ActualMissDistance=1e9;
  Result.FirstWarningLead=-1;
  for(int Idx=0; Idx<4; Idx++) Result.FirstLevelLead[Idx]=-1;
  Result.MaximumWarningLevel=0;

  for(int Time=0; Time<=Setup.Scenario.TimeToCollision+10; Time++)
  { Me.State.Write(Me.Packet, RefTime, RefLat, RefLon, RefAlt, LatCos);
    Target.State.Write(Target.Packet, RefTime, RefLat, RefLon, RefAlt, LatCos);

    const LookOut_Target *Worst=0;
    if(Time==0)
    { Look.ProcessOwn(Me.Packet, Time);                         // establish reference first
      Look.ProcessTarget(Target.Packet, Time); }
    else
    { Look.ProcessTarget(Target.Packet, Time);                 // target packet first, as in the tracker
      Worst=Look.ProcessOwn(Me.Packet, Time); }

    double Distance=ActualDistance(Me.State, Target.State);
    if(Distance<Result.ActualMissDistance) Result.ActualMissDistance=Distance;
    uint8_t Warn=Look.WarnLevel;
    if(Warn>Result.MaximumWarningLevel) Result.MaximumWarningLevel=Warn;
    if(Warn && Result.FirstWarningLead<0)
      Result.FirstWarningLead=Setup.Scenario.TimeToCollision-Time;
    for(uint8_t Level=1; Level<=3; Level++)
      if(Warn>=Level && Result.FirstLevelLead[Level]==-1)
        Result.FirstLevelLead[Level]=Setup.Scenario.TimeToCollision-Time;

    if(PrintTrace)
    { printf("  t=%3d lead=%3d actual=%6.1fm warn=%d",
             Time, Setup.Scenario.TimeToCollision-Time, Distance, Warn);
      if(Worst)
        printf(" margin=%5.1fs miss=%5.1fs/%5.1fm",
               0.5*Worst->TimeMargin, 0.5*Worst->MissTime,
               0.5*Worst->MissDist);
      printf("\n"); }

    if(Time<Setup.Scenario.TimeToCollision+10)
    { Me.State.StepFwd1sec();
      Target.State.StepFwd1sec(); }
  }

  Result.Score=0;
  if(Result.FirstWarningLead>=5) Result.Score+=25;
  if(Result.MaximumWarningLevel>=1) Result.Score+=20;
  if(Result.MaximumWarningLevel>=2) Result.Score+=20;
  if(Result.MaximumWarningLevel>=3) Result.Score+=20;
  if(Result.ActualMissDistance<200.0) Result.Score+=15;
  Result.Passed=Result.Score>=80;

  if(PrintResult)
    printf("Result %-16s: miss=%5.1fm, first warning lead=%3ds, levels=%d/%d/%d, max=%d, score=%d %s\n",
           Setup.Scenario.Name, Result.ActualMissDistance,
           Result.FirstWarningLead, Result.FirstLevelLead[1],
           Result.FirstLevelLead[2], Result.FirstLevelLead[3],
           Result.MaximumWarningLevel, Result.Score,
           Result.Passed ? "PASS" : "FAIL");
  return Result; }

static CollisionScenario BaseScenario(void)
{ return CollisionScenario(
  { "scan case",
    0.0, 0.0, 100.0,
    30,
    900.0, 900.0,
    180.0,
    30.0, 30.0,
    1.0, 1.5,
    0.5, -0.5,
    20 }); }

typedef void (*ScenarioSetter)(CollisionScenario &, double);

static void SetCrossingAngle(CollisionScenario &Scenario, double Value)
{ Scenario.CrossingAngle=Value; }

static void SetTargetClimb(CollisionScenario &Scenario, double Value)
{ Scenario.TargetClimb=Value; }

static void SetWarnTime(CollisionScenario &Scenario, double Value)
{ Scenario.WarnTime=(int)Value; }

static void SetSymmetricTurn(CollisionScenario &Scenario, double Value)
{ Scenario.MeTurn=Value;
  Scenario.TargetTurn=-Value; }

static int ScanScenarios(const char *Title, const CollisionScenario &Base,
                         const double *Values, int Count, ScenarioSetter Set)
{ int Passed=0;
  printf("\n%s\n", Title);
  for(int Idx=0; Idx<Count; Idx++)
  { CollisionScenario Scenario=Base;
    Set(Scenario, Values[Idx]);
    CollisionSetup Setup;
    bool SetupOK=SetupCollision(Scenario, Setup, false);
    if(!SetupOK)
    { printf("  value=%7.2f setup FAIL (endpoint error %.1f/%.1fm)\n",
             Values[Idx], Setup.MeEndpointError, Setup.TargetEndpointError);
      continue; }
    LookoutResult Result=TraceCollision(Setup, false, false);
    if(Result.Passed) Passed++;
    printf("  value=%7.2f miss=%5.1fm lead=%3ds levels=%d/%d/%d score=%3d %s\n",
           Values[Idx], Result.ActualMissDistance, Result.FirstWarningLead,
           Result.FirstLevelLead[1], Result.FirstLevelLead[2],
           Result.FirstLevelLead[3], Result.Score,
           Result.Passed ? "PASS" : "FAIL"); }
  printf("  summary: %d/%d passed\n", Passed, Count);
  return Passed==Count ? 0 : 1; }

static int RunScans(void)
{ const CollisionScenario Base=BaseScenario();
  CollisionScenario WarningBase=Base;
  WarningBase.TimeToCollision=70;
  WarningBase.MeDistance=2100.0;
  WarningBase.TargetDistance=2100.0;
  WarningBase.MeTurn=0.0;
  WarningBase.TargetTurn=0.0;
  const double Angles[] = { 30.0, 60.0, 90.0, 120.0, 150.0, 180.0 };
  const double Climbs[] = { -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 };
  const double Turns[]  = { -1.0, -0.5, 0.0, 0.5, 1.0 };
  const double WarnTimes[] = { 20.0, 30.0, 40.0, 50.0 };
  int Status=0;
  Status|=ScanScenarios("Crossing-angle scan", Base, Angles,
                        sizeof(Angles)/sizeof(Angles[0]), SetCrossingAngle);
  Status|=ScanScenarios("Target-climb scan", Base, Climbs,
                        sizeof(Climbs)/sizeof(Climbs[0]), SetTargetClimb);
  Status|=ScanScenarios("Symmetric-turn scan", Base, Turns,
                        sizeof(Turns)/sizeof(Turns[0]), SetSymmetricTurn);
  Status|=ScanScenarios("Warning-time scan", WarningBase, WarnTimes,
                        sizeof(WarnTimes)/sizeof(WarnTimes[0]), SetWarnTime);
  return Status; }

static void SetCandidateState(Acft_RelPos &Pos, int16_t Time, int16_t X, uint16_t Speed)
{ Pos.Clear();
  Pos.Flags=0;
  Pos.T=Time;
  Pos.X=X;
  Pos.Speed=Speed;
  Pos.Heading=0;
  Pos.hasClimb=1;
  Pos.isMoving=1;
  Pos.calcDir(); }

static bool TestOtherMeCandidateGate(void)
{ LookOut<1> Look;
  Look.hasPosition=1;
  SetCandidateState(Look.Pos, 20, 320, 20); // 160m east, 10m/s

  LookOut_Target Candidate;
  Candidate.Clear();
  SetCandidateState(Candidate.Pos, 12, 240, 20); // four seconds earlier, should align to own position
  if(!Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: timestamp-aligned matching track rejected\n");
    return 0; }
  Candidate.PreOtherMe=1;
  Look.calcRelPos(&Candidate);
  if(!Look.isOtherMeFineMatch(&Candidate))
  { printf("OtherMe fine gate: close moving co-track rejected\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 1000, 20); // well beyond the loose position limit
  if(Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: distant track accepted\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 320, 60); // 30m/s versus own 10m/s
  if(Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: different-speed track accepted\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 320, 20); // same speed, opposite direction
  Candidate.Pos.Heading=0x8000;
  Candidate.Pos.calcDir();
  if(Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: divergent velocity vector accepted\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 320, 20);
  Candidate.Pos.Climb=40; // 20m/s vertical relative speed
  if(Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: different climb rate accepted\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 10, 320, 20); // five-second timestamp skew
  if(Look.isPreOtherMeCandidate(&Candidate))
  { printf("OtherMe candidate gate: excessive timestamp skew accepted\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 420, 20); // 50m away: coarse candidate, not fine match
  Candidate.PreOtherMe=1;
  Look.calcRelPos(&Candidate);
  if(!Look.isPreOtherMeCandidate(&Candidate) || Look.isOtherMeFineMatch(&Candidate))
  { printf("OtherMe fine gate: loose-distance track accepted as a fine match\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 320, 20);
  Candidate.Pos.Z=120; // 60m altitude separation: coarse candidate, not a fine match
  Candidate.PreOtherMe=1;
  Look.calcRelPos(&Candidate);
  if(!Look.isPreOtherMeCandidate(&Candidate) || Look.isOtherMeFineMatch(&Candidate))
  { printf("OtherMe fine gate: vertically separated track accepted as a fine match\n");
    return 0; }

  SetCandidateState(Candidate.Pos, 20, 320, 20);
  Candidate.Pos.isMoving=0;
  Candidate.PreOtherMe=1;
  Look.calcRelPos(&Candidate);
  if(Look.isOtherMeFineMatch(&Candidate))
  { printf("OtherMe fine gate: stationary tracks accepted as evidence\n");
    return 0; }

  LookOut_Target Votes;
  Votes.Clear();
  for(uint8_t Idx=0; Idx<LookOut<1>::OtherMeConfirmVotes; Idx++) Look.updateOtherMeHistory(&Votes,1);
  if(!Votes.OtherMe)
  { printf("OtherMe history: failed to confirm at positive-vote threshold\n");
    return 0; }
  for(uint8_t Idx=0; Idx<LookOut<1>::OtherMeConfirmVotes-1; Idx++) Look.updateOtherMeHistory(&Votes,0);
  if(!Votes.OtherMe)
  { printf("OtherMe history: confirmation cleared before release threshold\n");
    return 0; }
  Look.updateOtherMeHistory(&Votes,0);
  if(Votes.OtherMe)
  { printf("OtherMe history: confirmation not cleared at release threshold\n");
    return 0; }

  printf("OtherMe candidate, fine gate and history: PASS\n");
  return 1; }

static bool TestOtherMeProcessPipeline(void)
{ LookOut<1> Look;
  Look.hasPosition=1;
  SetCandidateState(Look.Pos, 0, 0, 20);

  LookOut_Target Update;
  for(uint8_t Idx=0; Idx<LookOut<1>::OtherMeConfirmVotes; Idx++)
  { if(Idx)
    { Look.Pos.T+=2;
      Look.Pos.X+=20; }
    Update.Clear();
    Update.ID=0x123456;
    SetCandidateState(Update.Pos, Look.Pos.T, Look.Pos.X+4, 20); // 2m installation separation
    Look.ProcessTarget(&Update); }
  if(!Look.Target[0].PreOtherMe || !Look.Target[0].OtherMe)
  { printf("OtherMe pipeline: repeated close updates did not confirm\n");
    return 0; }

  for(uint8_t Idx=0; Idx<LookOut<1>::OtherMeConfirmVotes; Idx++)
  { Look.Pos.T+=2;
    Look.Pos.X+=20;
    Update.Clear();
    Update.ID=0x123456;
    SetCandidateState(Update.Pos, Look.Pos.T, Look.Pos.X+200, 20); // 100m: coarse-only, no fine votes
    Look.ProcessTarget(&Update); }
  if(Look.Target[0].OtherMe)
  { printf("OtherMe pipeline: sustained mismatch did not release confirmation\n");
    return 0; }

  printf("OtherMe pipeline: PASS\n");
  return 1; }

int main(int argc, char **argv)
{ if(!TestOtherMeCandidateGate() || !TestOtherMeProcessPipeline()) return 1;
  if(argc>1 && strcmp(argv[1], "scan")==0) return RunScans();

  CollisionScenario Scenario =
  { "turning climb",
    0.0, 0.0, 100.0,
    30,
    900.0, 900.0,
    180.0,
    30.0, 30.0,
    1.0, 1.5,
    0.5, -0.5,
    20 };
  CollisionSetup Setup;
  if(!SetupCollision(Scenario, Setup))
  { printf("Setup failed\n");
    return 1; }
  LookoutResult Result=TraceCollision(Setup, true);
  return Result.Passed ? 0 : 1; }

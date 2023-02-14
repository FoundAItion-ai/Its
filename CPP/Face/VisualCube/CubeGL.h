/*******************************************************************************
 FILE         :   CubeGL.h

 COPYRIGHT    :   DMAlex, 2020

 DESCRIPTION  :   Tracking visual representation

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   05/29/2020

 LAST UPDATE  :   05/29/2020
*******************************************************************************/
#pragma once

#include <windows.h>

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <tuple>


// Deviation off mean for normal distribution used by tracker
constexpr auto DeviationMax = 7;
constexpr auto DeviationMin = 1;
constexpr auto DeviationSigma = 12;


/*

1) With direction reversal (L -> R) ratio leftFoodLevel / rightFoodLevel
inversed, like 5 : 20 -> 20 : 5 2) With globalPeriod increase leftFoodLevel is
getting closer to rightFoodLevel, like 5 : 20 -> 15 -> 20 as L and R input are
leveling up.

*/

using ItsPeriod = unsigned int;
using ItsFrequency = unsigned int;
using ItsFood = unsigned int;

struct Pos {
  Pos() = default;
  Pos(int _x, int _y) : x(_x), y(_y) {}
  int x{};
  int y{};
};

inline bool operator <(const Pos& left, const Pos& right) {
    if (left.x < right.x) {
        return true;
    }

    if (left.x == right.x) {
        return left.y < right.y;
    }

    return false;
}


enum IOChange { Gain, Loss, Unchanged };
enum Thrust { Fast, Slow, Nothing };
enum class Input { Food, Empty, Wall };
enum class Output { Signal, NoSignal };
enum class Side { LeftGain, LeftLoss, RightGain, RightLoss };

class PeriodTracker;


// This state machine could probably better be implemented using C++ 20 coroutines
// but it's still not fully supported there and requires a lot of plumbing
class TrackerState {
public:
  virtual ~TrackerState() = default;
  virtual TrackerState *HandleInput(PeriodTracker &tracker, Input inputLeft,
                                    Input inputRight) = 0;
};

// Incapsulates two states for simplicity:
//
// 1) collect data over the first periodGainLoss
// 2) collect data over the second periodGainLoss
// 3) compare data and check if gain/loss occured in either or both channels
//
// tracking input indefinetely if no changes, not switching state
//

class GainLossState : public TrackerState {
public:
  GainLossState() = default;

  TrackerState *HandleInput(PeriodTracker &tracker, Input inputLeft,
                            Input inputRight) override;
};

// Same as GainLossState but switching to other states if periodCollectionLimit
// reached
//
class GainLossStateLimited : public GainLossState {
public:
  GainLossStateLimited(Side _secondFlip);

  TrackerState *HandleInput(PeriodTracker &tracker, Input inputLeft,
                            Input inputRight) override;

private:
  // what we're waiting for after first flip
  Side secondFlip{Side::LeftGain};

  ItsPeriod period{};
};

class WaitingState : public GainLossState {
public:
  WaitingState(PeriodTracker& tracker, bool _needFlip, bool _needSmartFlip, Side _secondFlip);

  TrackerState *HandleInput(PeriodTracker &tracker, Input inputLeft,
                            Input inputRight) override;

private:
  // Should we flip after waiting?
  bool needFlip{false};
  bool needSmartFlip{false};
  Side secondFlip{Side::LeftGain};

  ItsPeriod period{};
};

class TurningState : public TrackerState {
public:
  TurningState() = default;

  TrackerState *HandleInput(PeriodTracker &tracker, Input inputLeft,
                            Input inputRight) override;

private:
  ItsPeriod period{};
};


class WaveTracker {
public:
  void Turn();
  void Tick();

  bool IsChanged() {
    return isChanged;
  }

private:
  bool isChanged {false};
  ItsPeriod period {};
  std::optional<ItsPeriod> point1 {};
  std::optional<ItsPeriod> point2 {};
};


//
//  Redesign 2022:
//
// 1) remove Thrust leftThrust / rightThrust / leftThrustUpLevel / rightThrustUpLevel
// 
// 2) waitResult should be private friend to states, not interface
//    as well as Reverse / Turn?
// 
// 3) add freqLeft / freqRight (relative to heartbeat), if implemented as glob freq divider 1/1, 1/2, 1/3...
//    think about too steep change 1/1 - 1/2 like 50% freq change? Need smooth variation somehow.
//    Also think about sync - is it the model restriction if all waves will be in phase or with phase 
//    we can pass another (valuable) info? Likely! So for example 1/3 freq wave in Cell1 may or may not
//    be in sync with Cell2 / Cell3 / etc? TBD, waveTracker.IsChanged() => wave restart? both L/R back to min?
//    phaseLeft / phaseRight?  Low/High freq limit needed? As top level is increasing/decreasing our freq.
// 
//    WaveTracker is actually phase change tracker between L and R signals? Right, not period as those are two
//    different/independent signals.
// 
//    freqLeft and freqRight are dependent. As well as freq and detection period globalPeriodGainLoss_1 / etc
//    Parent backfeed changes freqLeft and freqRight at the same time, just increasing/decreasing average
//    amplitude. Is it enough for spyral? Do we need spyral?
// 
// 4) HanleInput -> 2 ins / 2 outs
// 
// 5) New class - Connectom to connect trackers (or rename PeriodTracker -> Cell?)
//    and may be split connections Cell1-LeftOut -> Cell2-RightIn?
//    Also need to be cetain about the order in which we process all cells/signals/connections
//    top -> down or?
// 
// 6) If works ok, re-implement it using Basic/CudaCube with gravity / probability (growth) / reflections?
//
// 7) ?


class PeriodTrackerEx {
public:
  PeriodTrackerEx(ItsPeriod periodGainLoss, ItsFrequency frequency);

	std::tuple<Output, Output> HanleInput(Input inputLeft, Input inputRight);

	void Excite();
	void Inhibit();
	void Reverse();

	static void Tick();
	static ItsPeriod Time();

private:
  ItsFrequency frequency {};
	int deviation {DeviationMax};

	// random_device is generally only used to seed a PRNG such as mt19937
	std::random_device random_device;
	std::mt19937 generator;

  static ItsPeriod globalTime;
};


class PeriodTracker {
public:
  PeriodTracker(ItsPeriod periodGainLoss);

  bool HanleInput(Input inputLeft, Input inputRight);

  // With top levels
  void Sync(Thrust leftThrustUpLevel, Thrust rightThrustUpLevel);
  void Reverse(bool smartFlip, Side secondFlip);
  void Turn();

  Thrust leftThrust{Fast};
  Thrust rightThrust{Slow};

  // Where upper level is pushing us
  Thrust leftThrustUpLevel{Fast};
  Thrust rightThrustUpLevel{Slow};

  using WaitResult = std::tuple</*changed=*/bool, IOChange, IOChange>;
  WaitResult waitResult{};
  std::string GetCurrentState();

  ItsPeriod periodGainLoss{};
  ItsPeriod periodVariance{};

protected:
  using DataPoint = std::pair<std::optional<ItsFood>, std::optional<ItsFood>>;

  WaitResult WaitForChange(Input inputLeft, Input inputRight);
  IOChange GetChange(DataPoint data);

private:
  // Data stream is:
  //
  // 1) continous
  // 2) shared across states
  //
  ItsPeriod period{};
  DataPoint dataLeft{};
  DataPoint dataRight{};

  TrackerState *state{nullptr};
  WaveTracker waveTracker;
};

class It {
public:
  It(const Pos &initialPosition);

  void Update(Pos *posLeft, Pos *posRight);
	void HanleInput(Input inputLeft, Input inputRight);

  PeriodTracker tracker_1;
  PeriodTracker tracker_2;
	PeriodTrackerEx tracker_1_ex;

  static std::string ThrustAsString(Thrust thrust) {
    switch (thrust) {
    case Fast:
      return "Fast";
    case Slow:
      return "Slow";
    case Nothing:
      return "Nothing";
    default:
      // assert :-)
      Beep(5000, 300);
      return "Error";
    }
    return "";
  }

  static std::string ChangeAsString(IOChange change) {
    switch (change) {
    case IOChange::Gain:
      return "Gain";
    case IOChange::Loss:
      return "Loss";
    case IOChange::Unchanged:
      return "Same";
    default:
      // assert :-)
      Beep(5000, 300);
      return "Error";
    }
    return "";
  }

  std::string leftThrustMode() { return ThrustAsString(leftThrust); }
  std::string rightThrustMode() { return ThrustAsString(rightThrust); }

private:
  typedef std::queue<int> ReflectionQueue;

	Pos lastPositionLeft {};
	Pos lastPositionRight {};

	long timeSinceLastInput {};

	Thrust leftThrust {Fast};
	Thrust rightThrust {Slow};

	Output outputLeft {Output::NoSignal};
	Output outputRight {Output::NoSignal};

  // precise position
	float x {};
	float y {};
	float heading {}; // angle, radian
};

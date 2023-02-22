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
#include <string>
#include <tuple>

/*

1) With direction reversal (L -> R) ratio leftFoodLevel / rightFoodLevel
inversed, like 5 : 20 -> 20 : 5 2) With globalPeriod increase leftFoodLevel is
getting closer to rightFoodLevel, like 5 : 20 -> 15 -> 20 as L and R input are
leveling up.

*/

using ItsPeriod = unsigned int;
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

  bool Update(Pos *posLeft, Pos *posRight);
  void HanleInput(Input inputLeft, Input inputRight);

  PeriodTracker tracker_1;
  PeriodTracker tracker_2;

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

  long timer;      // redraw cycles
  long inputTimer; // movement cycles
  int speedFactor;

  std::string leftThrustMode() { return ThrustAsString(leftThrust); }
  std::string rightThrustMode() { return ThrustAsString(rightThrust); }

private:
  typedef std::queue<int> ReflectionQueue;

  Pos lastPositionLeft;
  Pos lastPositionRight;

  long timeSinceLastInput;

  Thrust leftThrust;
  Thrust rightThrust;

  // precise position
  float x;
  float y;
  float heading; // angle, radian
};

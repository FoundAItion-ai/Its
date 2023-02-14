/*******************************************************************************
 FILE         :   CubeGL.cpp

 COPYRIGHT    :   DMAlex, 2020

 DESCRIPTION  :   Tracking visual representation

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   05/29/2020

 LAST UPDATE  :   05/29/2020
*******************************************************************************/
#include "stdafx.h"

#include "CubeGL.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/freeglut.h>
#include <GL/gl.h>

const int width = 1400;
const int height = 800;

const float speed = 0.4f; //0.04f; //0.08f;
const float drift = 0.06f; // 0.006f;
const float deviation = 1.0f ; // 0.785f;
const float waveChangeLimit = 100.0;



/*

****************************************** TODO ******************************************



TODO: redesign PeriodTracker to conform the model above and simplify connections for many cells





one of the problems - we can stuck in GainLossState forever
and question is how do we move in the state, like ideal move - outward spyral
but maybe just simple sine wave is ok with last/average period * 2 from WaveTracker?

^^^^ should level 3 action help? no, need spyral/wave
but if we generate regular wave - like 3L/1R (for one direction and 3R/1L for opposite) 
how does it help? what is "sub period" compare to current one? 1/3? 1/X?
each wave naturally subsides

another one - not registering "wave" sometimes and so not reducing period when running into different level

is "wave tracker"  technically 2nd level cell as getting IO from 1st one? "turns"?

so we have two 1st level: short period / long period tracker and 
2nd level - wave tracker for short period 
do we need wave tracker for long period ? it's there already, what is it doing in fact?

*/



/*

1) With direction reversal (L -> R) ratio leftFoodLevel / rightFoodLevel
inversed, like 5 : 20 -> 20 : 5 2) With globalPeriod increase leftFoodLevel is
getting closer to rightFoodLevel, like 5 : 20 -> 15 -> 20 as L and R input are
leveling up.

*/

using ItsPeriod = unsigned int;
using ItsFood = unsigned int;

constexpr static ItsPeriod globalPeriodGainLoss_1 = 3;
constexpr static ItsPeriod globalPeriodGainLoss_2 = 50;

constexpr ItsPeriod GetPeriodCollectionLimit(ItsPeriod periodGainLoss) {
  return periodGainLoss * 5;
}

constexpr ItsPeriod GetPeriodWait(ItsPeriod periodGainLoss) {
  return periodGainLoss * 3;
}

constexpr ItsPeriod GetPeriodTurn(ItsPeriod periodGainLoss) {
  return periodGainLoss;
}

constexpr ItsPeriod GetPeriodDelay(ItsPeriod periodGainLoss) {
  return periodGainLoss * 3;
}


// Need sharply raising function initially, slowly later (but still increasing)
// ex:  1 -> 9 -> 25 -> 35 -> 40 -> 43 -> 44 -> 45 -> 46 -> 47?
constexpr ItsPeriod GetPeriodExtended(ItsPeriod periodGainLoss, ItsPeriod periodVariance) {
  ItsPeriod period{GetPeriodWait(periodGainLoss)};

  // Lazy approximation :-)
  switch (periodVariance) {
  case 0: return period;
  case 1: return period * 2;
  case 2: return period * 3;
  default:
      return period * 5;
  }

  // change Sync too?
  //return period + 3 * (periodVariance * periodVariance);
}


// sort it
using Food = std::vector<Pos>;
Food food;

bool gRender = true;

Food::iterator FindFood(const Pos &pos) {
    auto found {std::equal_range(food.begin(), food.end(), pos)};
    auto distance {std::distance(found.first, found.second)};
    if (distance == 0) {
        return food.end();
    }

    assert(found.first != found.second);
    assert(distance == 1);
    
    return found.first;
}

bool IsFood(const Pos &pos) { return FindFood(pos) != food.end(); }

// Incapsulates two states for simplicity:
//
// 1) collect data over the first periodGainLoss
// 2) collect data over the second periodGainLoss
// 3) compare data and check if gain/loss occured in either or both channels
//
// tracking input indefinetely if no changes, not switching state
//

IOChange changeLeft;  // for debugging only
IOChange changeRight; // for debugging only



PeriodTrackerEx::PeriodTrackerEx(ItsPeriod periodGainLoss, ItsFrequency _frequency):
  frequency(_frequency),
  generator(random_device()) {
}


void PeriodTrackerEx::Excite() {
  if (deviation < 0) {
    if (deviation < /**/DeviationMax) {
      ++deviation;
    }
  } else {
    if (deviation < DeviationMax) {
      ++deviation;
    }
  }
}


void PeriodTrackerEx::Inhibit() {
  if (deviation > DeviationMin) {
    --deviation;
  }
}

void PeriodTrackerEx::Reverse() {
//	deviation = -deviation;????????
  //-7 -> -3 -> 0 -> 2 -> 7?
}

std::tuple<Output, Output> PeriodTrackerEx::HanleInput(Input inputLeft, Input inputRight)
{
  Output outputLeft {Output::NoSignal};
  Output outputRight {Output::NoSignal};

  if (globalTime % frequency == 0) {
    std::normal_distribution<> distrib(deviation, DeviationSigma);

    if (distrib(generator) < 0) {
      outputLeft = Output::Signal;
    }

    if (distrib(generator) > 0) {
      outputRight = Output::Signal;
    }
  }

  return {outputLeft, outputRight};
}


// static 
void PeriodTrackerEx::Tick() {
  ++globalTime;
}

// static 
ItsPeriod PeriodTrackerEx::Time() {
  return globalTime;
}

// static
ItsPeriod PeriodTrackerEx::globalTime {};


TrackerState *GainLossState::HandleInput(PeriodTracker &tracker,
                                         Input inputLeft, Input inputRight) {
  auto [changed, leftChange, rightChange] = tracker.waitResult;

  if (!changed) {
    return nullptr;
  }

  // Start turning and flip if needed
  if ((leftChange == IOChange::Gain && rightChange == IOChange::Gain) ||
      leftChange == IOChange::Loss && rightChange == IOChange::Loss) {
    // Change direction after waiting
    // TODO: try random flipping
    return new WaitingState(tracker, /*needFlip=*/true, /*needSmartFlip=*/false,
                            /*ignored*/ {Side::LeftGain});
  }

  if ((leftChange == IOChange::Gain && rightChange == IOChange::Loss) ||
      leftChange == IOChange::Loss && rightChange == IOChange::Gain) {
    // Rotating, continue turning in the same direction after waiting
    return new WaitingState(tracker, /*needFlip=*/false, /*needSmartFlip=*/false,
                            /*ignored*/ {Side::LeftGain});
  }

  // Wait for specific second flip
  if (leftChange == IOChange::Unchanged && rightChange == IOChange::Gain) {
    return new GainLossStateLimited(Side::LeftGain);
  }

  if (leftChange == IOChange::Gain && rightChange == IOChange::Unchanged) {
    return new GainLossStateLimited(Side::RightGain);
  }

  if (leftChange == IOChange::Unchanged && rightChange == IOChange::Loss) {
    return new GainLossStateLimited(Side::LeftLoss);
  }

  if (leftChange == IOChange::Loss && rightChange == IOChange::Unchanged) {
    return new GainLossStateLimited(Side::RightLoss);
  }

  return nullptr;
}

// Same as GainLossState but switching to other states if periodCollectionLimit
// reached
//
GainLossStateLimited::GainLossStateLimited(Side _secondFlip)
    : secondFlip(_secondFlip) {}

TrackerState *GainLossStateLimited::HandleInput(PeriodTracker &tracker,
                                                Input inputLeft,
                                                Input inputRight) {
  ++period;

  if (period >= GetPeriodCollectionLimit(tracker.periodGainLoss)) {
    return new GainLossState();
  }

  auto [changed, leftChange, rightChange] = tracker.waitResult;

  if (!changed) {
    return nullptr;
  }

  // TODO: Always wait for the full periodCollectionLimit?
  // if we wait, our turns will be more periodical, less random
  switch (secondFlip) {
  case Side::LeftGain:
    if (leftChange == IOChange::Gain) {
      return new WaitingState(tracker, /*needFlip=*/true, /*needSmartFlip=*/true,
                              secondFlip);
    }
    break;
  case Side::LeftLoss:
    if (leftChange == IOChange::Loss) {
      return new WaitingState(tracker, /*needFlip=*/true, /*needSmartFlip=*/true,
                              secondFlip);
    }
    break;
  case Side::RightGain:
    if (rightChange == IOChange::Gain) {
      return new WaitingState(tracker, /*needFlip=*/true, /*needSmartFlip=*/true,
                              secondFlip);
    }
    break;
  case Side::RightLoss:
    if (rightChange == IOChange::Loss) {
      return new WaitingState(tracker, /*needFlip=*/true, /*needSmartFlip=*/true,
                              secondFlip);
    }
    break;
  }

  return nullptr;
}

WaitingState::WaitingState(PeriodTracker& tracker, bool _needFlip, bool _needSmartFlip,
                           Side _secondFlip)
    : needFlip(_needFlip), needSmartFlip(_needSmartFlip),
      secondFlip(_secondFlip) {
    //if (needFlip) {
      tracker.Turn();
    //}
}

TrackerState *WaitingState::HandleInput(PeriodTracker &tracker, Input inputLeft,
                                        Input inputRight) {
  // Even in the waiting state we should be vigilant about unexpected IO (maybe other level)
  // ...well after a little bit of waiting :-)
  if (period > GetPeriodDelay(tracker.periodGainLoss)) {
    TrackerState* glState {GainLossState::HandleInput(tracker, inputLeft, inputRight)};
  
    if (glState != nullptr) {
      return glState;
    }
  }

  ++period;

  if (period < GetPeriodExtended(tracker.periodGainLoss, tracker.periodVariance)) {
    return nullptr;
  }

/*
  ItsPeriod periodExtended{GetPeriodExtended(tracker.periodGainLoss)};
  ItsPeriod periodAdjustment {};

  if (tracker.leftThrustUpLevel == Fast) {
    if (tracker.leftThrust == Fast) {
      periodAdjustment = periodExtended;
    }
  } else {
    if (tracker.rightThrust == Fast) {
      periodAdjustment = periodExtended;
    }
  }

  if (period < GetPeriodWait(tracker.periodGainLoss) + periodAdjustment) {
    return nullptr;
  }
*/

  if (needFlip) {
    tracker.Reverse(needSmartFlip, secondFlip);
  }

  return new TurningState();
}

TrackerState *TurningState::HandleInput(PeriodTracker &tracker, Input inputLeft,
                                        Input inputRight) {
  ++period;

  if (period < GetPeriodTurn(tracker.periodGainLoss)) {
    return nullptr;
  }

  return new GainLossState();
}


void WaveTracker::Turn() {
  if (!point1.has_value()) {
    point1 = period;
    isChanged = false;
    return;
  }
  
  if (!point2.has_value()) {
    point2 = period;
    isChanged = false;
    return;
  }

  assert(*point2 > *point1);
  assert(period > *point2);

  ItsPeriod period1 {*point2 - *point1};
  ItsPeriod period2 { period - *point2};

  ItsPeriod change {(ItsPeriod)abs((int)(period1 - period2))};
  float changePercent { };

  if (period2 > period1) {
    changePercent = (abs((int)(period1 - period2)) * 100.0f) / period1;
  } else {
    changePercent = (abs((int)(period1 - period2)) * 100.0f) / period2;
  }

  isChanged = changePercent > waveChangeLimit;
      
  if (isChanged) {
    point1.reset();
    point2.reset();
  } else {
    point1 = *point2;
    point2 = period;
  }
}

void WaveTracker::Tick() {
  ++period;
}


PeriodTracker::PeriodTracker(ItsPeriod _periodGainLoss)
    : state(new GainLossState()), periodGainLoss(_periodGainLoss) {
  assert(periodGainLoss > 0);
}

void PeriodTracker::Sync(Thrust _leftThrustUpLevel,
                         Thrust _rightThrustUpLevel) {
  leftThrustUpLevel = _leftThrustUpLevel;
  rightThrustUpLevel = _rightThrustUpLevel;
}


void PeriodTracker::Reverse(bool smartFlip, Side secondFlip) {
  if (!smartFlip) {
    leftThrust = leftThrust == Fast ? Slow : Fast;
    rightThrust = rightThrust == Fast ? Slow : Fast;
    return;
  }

  //if (periodGainLoss > globalPeriodGainLoss_1) {
  //  if (smartFlip) {
  //    Beep(3000, 100);  // debugging
  //  } else {
  //    Beep(1000, 100);  // debugging
  //  }
  //}

  // Different periods("density") and / or phase shift("shapes") handled with
  // tracking:
  //
  //  left  gain->right gain = reverse if left  thrust
  //  left  loss->right loss = reverse if left  thrust
  //
  //  right gain->left  gain = reverse if right thrust
  //  right loss->left  loss = reverse if right thrust
  //
  //  ==> co - directional arrows seems to be working great.Only watch for Gain
  //  / Loss per channel and once flipped
  //  ==> in the same direction check if action should be reversed.
  bool reverse = false;

  switch (secondFlip) {
  case Side::LeftGain:
  case Side::LeftLoss:
    if (leftThrust == Fast) {
      reverse = true;
    }
    break;
  case Side::RightGain:
  case Side::RightLoss:
    if (rightThrust == Fast) {
      reverse = true;
    }
    break;
  }

  if (reverse) {
    Reverse(/*smartFlip=*/false, secondFlip);
  }
}

void PeriodTracker::Turn() {
    waveTracker.Turn();

    if (waveTracker.IsChanged() && periodGainLoss == globalPeriodGainLoss_1) {
      Beep(500, 1000);
    }

}


ItsFood dataLeftLevel2 = 0;
ItsFood dataRightLevel2 = 0;

PeriodTracker::WaitResult PeriodTracker::WaitForChange(Input inputLeft,
                                                       Input inputRight) {
  WaitResult NoResult{false, IOChange::Unchanged, IOChange::Unchanged};
  int foodLeft = inputLeft == Input::Food ? 1 : 0;
  int foodRight = inputRight == Input::Food ? 1 : 0;
  ++period;

  if (!dataLeft.first) {
    dataLeft.first = foodLeft;
    dataRight.first = foodRight;
    return NoResult;
  }

  if (!dataLeft.second) {
    if (period < periodGainLoss) {
      *dataLeft.first += foodLeft;
      *dataRight.first += foodRight;
    } else {
      dataLeft.second = foodLeft;
      dataRight.second = foodRight;
      period = 0;
    }
    return NoResult;
  }

  if (period < periodGainLoss) {
    *dataLeft.second += foodLeft;
    *dataRight.second += foodRight;
    return NoResult;
  }

  // Now we can compare results
  WaitResult result{true, GetChange(dataLeft), GetChange(dataRight)};

  if (periodGainLoss > globalPeriodGainLoss_1) {
    changeLeft = std::get<1>(result);  // debugging only
    changeRight = std::get<2>(result); // debugging only

    dataLeftLevel2 = *dataLeft.second;
    dataRightLevel2 = *dataRight.second;
  }

  period = periodGainLoss;

  dataLeft.first = dataLeft.second;
  dataLeft.second = {};
  dataRight.first = dataRight.second;
  dataRight.second = {};

  // But there may be situation when nothing changed
  if (std::get<1>(result) == IOChange::Unchanged &&
      std::get<2>(result) == IOChange::Unchanged) {
    return NoResult;
  }

  return result;
}

IOChange PeriodTracker::GetChange(DataPoint data) {
  assert(data.first && data.second);

  if (*data.first == 0 && *data.second > 0) {
    return IOChange::Gain;
  }

  if (*data.first > 0 && *data.second == 0) {
    return IOChange::Loss;
  }

  return IOChange::Unchanged;
}

bool PeriodTracker::HanleInput(Input inputLeft, Input inputRight) {
  // 3) capturing - back to tracking 1st level when found, controlled by "wave founder" to distinguish
  // this track from another one by period violations ? should get back(reduce deviation) quickly / immediately
  waveTracker.Tick();
  if (waveTracker.IsChanged()) {
    periodVariance = 0;
  }


  // 1) tracking, follow 1st level path, controlled by IO
  waitResult = WaitForChange(inputLeft, inputRight);
  TrackerState *newState = state->HandleInput(*this, inputLeft, inputRight);

  if (newState != nullptr) {
    delete state;
    state = newState;

    // 3) capturing - back to tracking 1st level when found, controlled by "wave founder" to distinguish
    // this track from another one by period violations ? should get back(reduce deviation) quickly / immediately
    if (waveTracker.IsChanged()) {
      periodVariance = 0;
    }
    else {
      // 2) going to 2nd / higher level, controlled by longer period IO to deviate from path if needed
      // also should get back but slowly (when running out of resources, so level 2 control - reversed)
      if (typeid(*state) == typeid(TurningState)) {
        if (leftThrustUpLevel == Fast) {
          periodVariance += 1;
        } else if (periodVariance > 0) {
          // Could be steeper curve to drop
          periodVariance -= 1;
        }
      }
    }
  }

  return true;
}

std::string PeriodTracker::GetCurrentState() {
  if (state == nullptr) {
    return "NULL";
  }

  if (typeid(*state) == typeid(GainLossState)) {
    return "GL--";
  } else if (typeid(*state) == typeid(GainLossStateLimited)) {
    return "GL-L";
  } else if (typeid(*state) == typeid(WaitingState)) {
    return "WAIT";
  } else if (typeid(*state) == typeid(TurningState)) {
    return "TURN";
  }

  return "NONE";
}

It::It(const Pos &initialPosition)
    : x((float)initialPosition.x), y((float)initialPosition.y),
      tracker_1(globalPeriodGainLoss_1), tracker_2(globalPeriodGainLoss_2), tracker_1_ex(globalPeriodGainLoss_1, 2)
  /*,
      lastPositionLeft(initialPosition.x, initialPosition.y),
      lastPositionRight(initialPosition.x, initialPosition.y)*/ {}

// At least one IN position should change so we check if there's food there.
// And if second position is the same we might eat food there already!
void It::Update(Pos *posLeft, Pos *posRight) {
  if (!posLeft || !posRight) {
    return;
  }

  if (outputLeft == Output::Signal && outputRight == Output::Signal) {
    x += (float)sin(heading) * speed;
    y += (float)cos(heading) * speed;
  } else if (outputLeft == Output::Signal) {
    x += (float)sin(heading - deviation) * speed;
    y += (float)cos(heading - deviation) * speed;

    heading += drift;
    if (heading > 6.283f) { // 2 * Pi
      heading = 0.0f;
    }
  } else if (outputRight == Output::Signal) {
    x += (float)sin(heading + deviation) * speed;
    y += (float)cos(heading + deviation) * speed;

    heading -= drift;
    if (heading <= 0.0f) {
      heading = 6.283f; // 2 * Pi
    }
  }

  // IO left side and right
  float xLeft = x + (float)sin(heading + 1.5f) * 2.0f;
  float yLeft = y + (float)cos(heading + 1.5f) * 2.0f;
  float xRight = x + (float)sin(heading - 1.5f) * 2.0f;
  float yRight = y + (float)cos(heading - 1.5f) * 2.0f;

  posLeft->x = (int)xLeft;
  posLeft->y = (int)yLeft;
  posRight->x = (int)xRight;
  posRight->y = (int)yRight;
}


void It::HanleInput(Input inputLeft, Input inputRight) {
  if (inputLeft == Input::Wall || inputRight == Input::Wall) {
    return;
  }

  PeriodTrackerEx::Tick();
  std::tie<Output, Output> (outputLeft, outputRight) = tracker_1_ex.HanleInput(inputLeft, inputRight);


  if (tracker_1.HanleInput(inputLeft, inputRight)) {
    // Thrust is flipped
    //leftThrust = tracker_1.leftThrust;
    //rightThrust = tracker_1.rightThrust;
  }

  // ***************************************************************
  //
  // IMPORTANT:  (??)
  //
  // ***************************************************************
  //
  // 1) This tracker is higher level and "have more say" so it's after the first
  // one to override one's decisions
  //
  // 2) Looks like we also need to synchronize all low level trackers with our
  // decision
  //
  //
  
  if (tracker_2.HanleInput(inputLeft, inputRight)) {
    // Thrust is flipped
    // leftThrust = tracker_2.leftThrust;
    // rightThrust = tracker_2.rightThrust;
    // Beep(3000, 300);

    // ====> Low level sync
    tracker_1.Sync(tracker_2.leftThrust, tracker_2.rightThrust);
  }
  

  /*
  *
  * We should not force "outward spyral", it's natural, "last remembered wave"
  * although we may still need to increase period over time
  *
  *
  if (timeSinceLastInput == 100) {
    speedFactor = 3;
    leftThrust = Fast;
    rightThrust = Slow;
    Beep(3000, 300);
  }
  */
}

std::unique_ptr<It> it;

void onDisplay(void) {
  if (!gRender) {
    return;
  }

  glClear(GL_COLOR_BUFFER_BIT);

  glBegin(GL_POINTS);
  glColor3f(1.00, 1.00, 1.00);
  for (auto item : food) {
    glVertex2f((GLfloat)item.x, (GLfloat)item.y);
  }
  glEnd();

  if (it.get()) {
    Pos posLeft{}, posRight{};
    Input inputLeft {Input::Empty};
    Input inputRight {Input::Empty};

    it->Update(&posLeft, &posRight);

    if (posLeft.x > 5 && posLeft.x < width - 5 && posLeft.y > 5 || posLeft.y < height - 5) {
        Food::iterator found = FindFood(posLeft);
        if (found != food.end()) {
          food.erase(found);
          inputLeft = Input::Food;
        }

        found = FindFood(posRight);
        if (found != food.end()) {
          food.erase(found);
          inputRight = Input::Food;
        }
    }

    it->HanleInput(inputLeft, inputRight);

    GLfloat x = ((GLfloat)posLeft.x + (GLfloat)posRight.x) / 2.0f;
    GLfloat y = ((GLfloat)posLeft.y + (GLfloat)posRight.y) / 2.0f;

    glBegin(GL_TRIANGLE_STRIP);
    glColor3f(0.00, 1.00, 1.00);

    glVertex2f(x + (GLfloat)0.0, y + (GLfloat)0.0);
    glVertex2f(x + (GLfloat)0.0, y + (GLfloat)5.0);
    glVertex2f(x + (GLfloat)5.0, y + (GLfloat)0.0);
    glEnd();
  }

  if (it.get()) {
    auto printString = [](Pos pos, std::string_view str) {
      glColor3f(0.00, 1.00, 1.00);
      glRasterPos2f((GLfloat)pos.x, (GLfloat)pos.y);
      for (unsigned int i = 0; i < str.size(); i++) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, str[i]);
      }
    };

    char buf[256] = {};
    sprintf_s(buf, sizeof(buf), "Time %d, L1 State: %s, L2 State: %s, (%lu - %lu)",
              PeriodTrackerEx::Time(), it->tracker_1.GetCurrentState().c_str(),
              it->tracker_2.GetCurrentState().c_str(),
              it->tracker_1.periodVariance,
              it->tracker_2.periodVariance);
    printString(Pos(50, 30), buf);

    sprintf_s(
        buf, sizeof(buf), "L2 %s / %s : data %d",
        It::ChangeAsString(changeLeft).c_str(),
        // sprintf_s(buf, sizeof(buf), "%d : Tr2 %s - Tr1 %s", PeriodTrackerEx::Time(),
        It::ThrustAsString(it->tracker_2.leftThrust).c_str(), dataLeftLevel2);
    printString(Pos(50, 50), buf);

    sprintf_s(
        buf, sizeof(buf), "R2 %s / %s : data %d",
        It::ChangeAsString(changeRight).c_str(),
        It::ThrustAsString(it->tracker_2.rightThrust).c_str(), dataRightLevel2);
    printString(Pos(50, 70), buf);
  }

  glutSwapBuffers();
}

void onMouse(int button, int state, int x, int y) {
  if (button == 0 && state == 1) {
    it.reset(new It(Pos(x, y)));
  }

  if (button == 2 && state == 1) {
    gRender = !gRender;
  }
}

void onMotion(int x, int y) {
  if (!IsFood(Pos(x, y))) {
    food.push_back(Pos(x + 0, y + 0));
    food.push_back(Pos(x + 0, y + 1));
    food.push_back(Pos(x + 1, y + 0));
    food.push_back(Pos(x + 1, y + 1));
    sort(food.begin(), food.end());
  }
}

void onReshape(int width, int height) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void onIdle(void) { glutPostRedisplay(); }

void DrawCircle(const int radius, const int width, const int centerPosX,
                const int centerPosY, float pieStart, float pieEnd,
                const int density) {
  // circle, one quadrant
  const int posStartX = centerPosX - radius;
  const int posStartY = centerPosY - radius;

  for (int posX = posStartX; posX < posStartX + (radius * 2); posX++) {
    for (int posY = posStartY; posY < posStartY + (radius * 2); posY++) {
      int distance = (int)sqrt(pow(abs(posX - centerPosX), 2) +
                               pow(abs(posY - centerPosY), 2));
      float rad = (float)atan2(posY - centerPosY, posX - centerPosX);

      if (distance <= radius && distance > radius - width && rad < pieStart &&
          rad > pieEnd) {
        if (posX * posY % density == 0) {
          food.push_back(Pos(posX, posY));
        }
      }
    }
  }
}

void DrawLine(const int startPosX, const int startPosY, const int endPosX,
              const int endPosY, const int density) {

  for (int posX = startPosX; posX < endPosX; posX++) {
    for (int posY = startPosY; posY < endPosY; posY++) {
      if (posX * posY % density == 0) {
        food.push_back(Pos(posX, posY));
      }
    }
  }
}

void VisualCubeMain() {
  int argc = 0;

  // move to VisualCubeUnitTest.cpp?
#if 0
  {
    // Unit testing :-)
    It it(Pos(0, 0));
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);

    It::IOChange leftChange;
    It::IOChange rightChange;

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Invalid);
    assert(rightChange == It::IOChange::Invalid);

    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);

    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Steady);
    assert(rightChange == It::IOChange::Loss);

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Invalid);
    assert(rightChange == It::IOChange::Invalid);

    it.SetInputLeft(It::Empty);
    it.SetInputLeft(It::Empty);
    it.SetInputLeft(It::Empty);
    it.SetInputLeft(It::Empty);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food); // extra
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food); // extra

    it.SetInputRight(It::Empty);
    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);
    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food); // extra

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Gain);
    assert(rightChange == It::IOChange::Loss);

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Invalid);
    assert(rightChange == It::IOChange::Invalid);

    it.SetInputLeft(It::Empty);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);
    food.push_back(Pos(0, 0)); it.SetInputLeft(It::Food);

    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);
    food.push_back(Pos(0, 0)); it.SetInputRight(It::Food);
    it.SetInputRight(It::Empty);
    it.SetInputRight(It::Empty);

    it.ProcessInput(&leftChange, &rightChange);
    assert(leftChange == It::IOChange::Gain);
    assert(rightChange == It::IOChange::Steady);
  }
#endif

  // circle, one quadrant
  //
  // X - horizontal
  // Y - vertical
  //
  // DrawCircle(200, 30, /*centerPosX=*/200, /*centerPosY=*/200, 3, 1,
  // /*density=*/2); DrawCircle(200, 30, /*centerPosX=*/300, /*centerPosY=*/300,
  // -1.5, -4, /*density=*/2);

  // DrawCircle(200, 30, /*centerPosX=*/500, /*centerPosY=*/300, 3,  2,
  // /*density=*/2);

  // DrawCircle(200, 2, /*centerPosX=*/500, /*centerPosY=*/300, 3, 2,
  // /*density=*/2);

  // dotted-line
  // DrawLine(100, 200, 200, 220, 2);
  // DrawLine(300, 200, 400, 220, 2);
  // DrawLine(500, 200, 600, 220, 2);

  // DrawLine(100, 200, 500, 202, 2);
  // DrawLine(100, 200, 200, 300, 2);

  // straight lines
  //
  // DrawLine(500, 50, 503, 800, 1);
  // DrawLine(10, 200, 1300, 203, 1);

  // rectangle
  //DrawLine(50, 100, 1000, 150, 1);

  // Curved dotted-line, food is decreasing
  // size_t units = 7;
  // size_t step = 0;// +15;

  // for (size_t i = 0; i < units; i++) {
  //  size_t startPosX = 100 + (i * 70);
  //  size_t startPosY = 200 + (i * step);

  //  DrawLine(startPosX, startPosY, startPosX + 50, startPosY + 20, 2/*units -
  //  i*/);
  //}

  //
  // ************* Second level *************
  //
  // Split-line
  // for (size_t i = 0; i <= 3; ++i) {
  //  size_t startPosX = 400 + (i * 25);
  //  DrawLine(startPosX, 100, startPosX + 1, 700, 1);
  //}

  for (size_t i = 0; i < 3; ++i) {
    size_t startPosY = 200 + (i * 30);
    // Should not be <2 dots thin, can miss detection
    //DrawLine(100, startPosY, 700, startPosY + 2, 1);
    DrawLine(50, startPosY, 1350, startPosY + 2, 1);
  }


  sort(food.begin(), food.end());

  glutInit(&argc, NULL);
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
  glutInitWindowSize(width, height);
  glutCreateWindow("Playground");

  glutDisplayFunc(onDisplay);
  glutReshapeFunc(onReshape);
  glutIdleFunc(onIdle);
  glutMouseFunc(onMouse);
  glutMotionFunc(onMotion);
  glutMainLoop();
}

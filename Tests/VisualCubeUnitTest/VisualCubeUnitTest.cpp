#include "pch.h"
#include "CppUnitTest.h"

#include "CubeGL.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace VisualCubeUnitTest
{

class GainLossStateTest : public PeriodTracker {
public:
  GainLossStateTest(ItsPeriod periodGainLoss) : PeriodTracker(periodGainLoss, /*irrelevant*/0, /*irrelevant*/0) {}

  using DataPoint = PeriodTracker::DataPoint;

  IOChange GetChangeTest(DataPoint data) { return GetChange(data); }
};

TEST_CLASS(VisualCubeUnitTest) {
  public :
    TEST_METHOD(TestGainLossState) {
      {
        GainLossStateTest state(3);

        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {0, 0}));
        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {1, 1}));
        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {2, 2}));

        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 1}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {1, 0}));
        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {1, 2}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {2, 1}));

        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 2}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {2, 0}));
        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 3}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {3, 0}));
      }
      {
        GainLossStateTest state(5);

        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {0, 0}));
        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {1, 1}));
        Assert::IsTrue(IOChange::Unchanged == state.GetChangeTest(GainLossStateTest::DataPoint {2, 2}));

        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 1}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {1, 0}));
        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {1, 2}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {2, 1}));

        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 2}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {2, 0}));
        Assert::IsTrue(IOChange::Gain == state.GetChangeTest(GainLossStateTest::DataPoint {0, 3}));
        Assert::IsTrue(IOChange::Loss == state.GetChangeTest(GainLossStateTest::DataPoint {3, 0}));
      }
    }
};

} // namespace VisualCubeUnitTest

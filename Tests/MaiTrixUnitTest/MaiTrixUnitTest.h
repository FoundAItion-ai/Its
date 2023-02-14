/*******************************************************************************
 FILE         :   MaiTrixUnitTest.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   MaiTrix unit testing class

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   06/12/2012

 LAST UPDATE  :   06/12/2012
*******************************************************************************/


#ifndef __MAITRIXUNITTEST_H
#define __MAITRIXUNITTEST_H


#include "PatternAnalyzer.h"


using namespace std;
using namespace DCommon;


namespace Its
{

const string defaultMaiTrixName( "First" );


class MaiTrixUnitTest
{
public:
  struct HitsStatistic
  {
    HitsStatistic():
      runs( 0 ),
      completeRuns( 0 ),
      predictionRuns( 0 ),
      nontrivialRuns( 0 )
    {
      for( int loopIdentityLevel = 0; loopIdentityLevel < Its::MaxIdentityLevel; loopIdentityLevel++ )
      {
        periodHits  [ loopIdentityLevel ] = 0;
        countHits   [ loopIdentityLevel ] = 0;
      }
    }

    // to accumulate statistics in a long run
    void operator += ( HitsStatistic& that )
    {
      runs            += that.runs;
      completeRuns    += that.completeRuns;
      predictionRuns  += that.predictionRuns;
      nontrivialRuns  += that.nontrivialRuns;

      for( int loopIdentityLevel = 0; loopIdentityLevel < Its::MaxIdentityLevel; loopIdentityLevel++ )
      {
        periodHits  [ loopIdentityLevel ] += that.periodHits  [ loopIdentityLevel ];
        countHits   [ loopIdentityLevel ] += that.countHits   [ loopIdentityLevel ];
      }
    }

    PeriodIt runs;
    PeriodIt completeRuns;    // must be = runs when delay is 0
    PeriodIt predictionRuns;  // when see out pattern with no input pattern
    PeriodIt nontrivialRuns;  // when see out pattern with search is not complete 

    PeriodIt periodHits [ Its::MaxIdentityLevel ];
    PeriodIt countHits  [ Its::MaxIdentityLevel ];
  };


  // call to run once and get results
  typedef void CallMaiTrixBack( PeriodIt tick, int rf, IFrame& textFrame, IFrame& audioFrame, IFrame& videoFrame, void* callMaiTrixBackArguments );

  // call to reset maiTrix in between tests, to do 'clean tests' for example 
  typedef void CallMaiTrixBackAfter( void* callMaiTrixBackArguments ); 

  // maxDuration - one test duration in ticks
  // delay - before starting output analysys
  //
  MaiTrixUnitTest(  CapacityIt maiTrixCapacity, PeriodIt maxDuration, PeriodIt delay,
                    CallMaiTrixBack& callMaiTrixBack, CallMaiTrixBackAfter& callMaiTrixBackAfter, void* callMaiTrixBackArguments );

  HitsStatistic& Run( SizeIt maxTestTraits, SizeIt maxTestPatterns, const char* maiTrixPath, const char* maiTrixDebPath, int rf );

protected:

  bool RunForPattern( IFrame& patternedFrame, IFrame& emptyFrame,
                      TextPatternAnalyzer& inputPatternAnalyzer, TextPatternAnalyzer& outputPatternAnalyzer,
                      int rf );

  bool RunMaiTrixAndDoAnalysis( TextPatternAnalyzer&  inputPatternAnalyzer, 
                                TextPatternAnalyzer&  outputPatternAnalyzer,
                                PeriodIt tick, 
                                int rf );

  void UpdateStatistics(  bool searchComplete, ImpulseFramesPattern& outputPattern, int rf );
  void PatternsSanityCheck( ImpulseFramesPattern& inputPattern );

  void OutputViewByLevel( TextPatternAnalyzer& patternAnalyzer, ostream& os, Identity_t identity, bool searchComplete );

  void OutputExtendedAnalysisResultsToLog( bool searchComplete, 
                                           ImpulseFramesPattern& outputPattern, 
                                           int rf );
  
  void OutputPatternsToLog( ImpulseFramesPattern& comparePattern, const char* comparePatternName );

private:
  static const int minimalPatternGoodCount = 3;

  SizeIt                testUid;  // current test's unique Id
  SizeIt                frameSize;
  PeriodIt              maxDuration;
  PeriodIt              delay;
  CapacityIt            maiTrixCapacity;

  IFrame                textFrame;
  IFrame                audioFrame;
  IFrame                videoFrame;

  CallMaiTrixBack&      callMaiTrixBack;
  CallMaiTrixBackAfter& callMaiTrixBackAfter;
  void*                 callMaiTrixBackArguments;

  HitsStatistic         statistic;
  TextPatternGenerator  patternGenerator;
  ImpulseFramesPattern  generatedPattern;
};



}; // namespace Its


#endif // __MAITRIXUNITTEST_H

// MaiTrixUnitTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "PatternAnalyzer.h"
#include "MaiTrixUnitTest.h"


using namespace Its;



MaiTrixUnitTest::MaiTrixUnitTest( CapacityIt maiTrixCapacity, PeriodIt _maxDuration, PeriodIt _delay,
                                  CallMaiTrixBack& _callMaiTrixBack, CallMaiTrixBackAfter& _callMaiTrixBackAfter, 
                                  void* _callMaiTrixBackArguments ):
  patternGenerator         ( maiTrixCapacity.sizeText, _maxDuration ),
  frameSize                ( maiTrixCapacity.sizeText               ),
  maiTrixCapacity          ( maiTrixCapacity                        ),
  maxDuration              ( _maxDuration                           ),
  delay                    ( _delay                                 ),
  textFrame                ( maiTrixCapacity.sizeText               ),
  audioFrame               ( maiTrixCapacity.sizeAudio              ),
  videoFrame               ( maiTrixCapacity.sizeVideo              ),
  generatedPattern         ( Its::MaxIdentityLevel                  ),
  callMaiTrixBack          ( _callMaiTrixBack                       ),
  callMaiTrixBackAfter     ( _callMaiTrixBackAfter                  ),
  callMaiTrixBackArguments ( _callMaiTrixBackArguments              ),
  testUid                  ( 0                                      )
{
}



void MaiTrixUnitTest::OutputPatternsToLog( ImpulseFramesPattern& comparePattern, const char* comparePatternName )
{
  DLog::msg( DLog::LevelVerbose, "" );
  DLog::msg( DLog::LevelVerbose, "> Generated pattern [start %3d / period %3d / count %3d, identity = %s]",                generatedPattern.start, generatedPattern.period,  generatedPattern.count, TextPatternAnalyzer::IdentityToString( generatedPattern.identity ));
  DLog::msg( DLog::LevelVerbose, "> %-9s pattern [start %3d / period %3d / count %3d, identity = %s]", comparePatternName, comparePattern.start,   comparePattern.period,    comparePattern.count,   TextPatternAnalyzer::IdentityToString( comparePattern.identity   ));
  DLog::msg( DLog::LevelVerbose, "" );
}



void MaiTrixUnitTest::OutputExtendedAnalysisResultsToLog( 
              bool searchComplete, 
              ImpulseFramesPattern& outputPattern, 
              int rf )
{
  DLog::msg( DLog::LevelVerbose, "===========> Interim extended analysis results ===========>" );

  if( searchComplete )
  {
    DLog::msg( DLog::LevelDebug, "Seach is complete, Total runs = %d, rf = %d", statistic.runs, rf );
    DLog::msg( DLog::LevelDebug, "" );
  }
  else
  {
    DLog::msg( DLog::LevelDebug, "Seach is NOT complete, Total runs = %d, rf = %d", statistic.runs, rf );

    // extra check, just in case
    // Exclude almost trivial runs 
    //
    //if( outputPattern.count >= minimalPatternGoodCount )
    //
    if( outputPattern.period > 1 && outputPattern.count >= minimalPatternGoodCount )
    {
      DLog::msg( DLog::LevelWarning, "Unit test %d, Rf = %d, seach is NOT complete, but non-trivial Output pattern is observed, period = %d, count = %d", testUid, rf, outputPattern.period, outputPattern.count );

      statistic.nontrivialRuns++;
      //DLog::msg(  DLog::LevelWarning, "Unit test %d, Rf = %d, seach is NOT complete for Input pattern [start %d / period %d / count %d]", 
      //            testUid, rf, generatedPattern.start, generatedPattern.period, generatedPattern.count );
      //DLog::msg(  DLog::LevelWarning, "But non-trivial Output pattern is observed, [start %3d / period %3d / count %3d, identity = %s]", 
      //            outputPattern.start, outputPattern.period, outputPattern.count, TextPatternAnalyzer::IdentityToString( outputPattern.identity ));
    }

    DLog::msg( DLog::LevelDebug, "" );
  }

  OutputPatternsToLog( outputPattern, "Output" );

  DLog::msg( DLog::LevelVerbose, "" );
  DLog::msg( DLog::LevelVerbose, "Identity Level 0 [ Content   ], period hits = %4d, count hits = %4d", statistic.periodHits[ Its::Content   ], statistic.countHits[ Its::Content   ] );
  DLog::msg( DLog::LevelVerbose, "Identity Level 1 [ Width     ], period hits = %4d, count hits = %4d", statistic.periodHits[ Its::Width     ], statistic.countHits[ Its::Width     ] );
  DLog::msg( DLog::LevelVerbose, "Identity Level 2 [ CharTypes ], period hits = %4d, count hits = %4d", statistic.periodHits[ Its::CharTypes ], statistic.countHits[ Its::CharTypes ] );
  DLog::msg( DLog::LevelVerbose, "Identity Level 3 [ Present   ], period hits = %4d, count hits = %4d", statistic.periodHits[ Its::Present   ], statistic.countHits[ Its::Present   ] );
  DLog::msg( DLog::LevelVerbose, "" );

  DLog::msg( DLog::LevelVerbose, "<=========== Interim extended analysis results <===========" );
}


void MaiTrixUnitTest::PatternsSanityCheck( ImpulseFramesPattern& inputPattern )
{
  // what do we do about 'time delay'?
  //
  // what if we get some pitch before official delay is over???

  if( generatedPattern.start    != inputPattern.start     ||
      generatedPattern.period   != inputPattern.period    ||
      generatedPattern.count    != inputPattern.count     ||
      generatedPattern.identity != inputPattern.identity    )
  {
    OutputPatternsToLog( inputPattern, "Input" );

    throw UnitTestException( DException::Error, "Original pattern from generator and analyzed one do not match, pattern analyzer failed" );
  }
  else
  {
    DLog::msg( DLog::LevelVerbose, "Passed sanity check, analyzed input pattern matches the generated one." );
  }
}



void MaiTrixUnitTest::UpdateStatistics( bool searchComplete, ImpulseFramesPattern& outputPattern, int rf )
{
  statistic.runs++;

  if( searchComplete )
  {
    statistic.completeRuns++;

    bool patternsMatch = false;
    
    // Let's exclude period 0, count 0 out pattern from statistics
    // as this may coincide with period 0, count 1 in pattern
    // and generate false statistics
    //
    //if( !( outputPattern.period == 0 && outputPattern.count == 0 ))
    //
    // Exclude almost trivial runs 
    //
    if( outputPattern.period > 1 && outputPattern.count > 1 )
    {
      if( generatedPattern.period == outputPattern.period )
      {
        statistic.periodHits[ outputPattern.identity ]++;
        patternsMatch = true;
      }

      if( generatedPattern.count == outputPattern.count )
      {
        statistic.countHits[ outputPattern.identity ]++;
        patternsMatch = true;
      }
    }

    if( !patternsMatch )
    {
      DLog::msg( DLog::LevelVerbose, "Discovered Output pattern doesn't match Input pattern [start %d / period %d / count %d]. Statistics not changed. Rf = %d", 
                                      generatedPattern.start, generatedPattern.period, generatedPattern.count, rf );

      // Exclude almost trivial runs 
      //
      //if( outputPattern.count >= minimalPatternGoodCount )
      //
      if( outputPattern.period > 1 && outputPattern.count >= minimalPatternGoodCount )
      {
        DLog::msg( DLog::LevelWarning, "Unit test %d, Rf = %d, while Output pattern doesn't match Input pattern a non-trivial Output pattern is observed, period = %d, cound = %d", testUid, rf, outputPattern.period, outputPattern.count );

        statistic.nontrivialRuns++;

        //DLog::msg(  DLog::LevelWarning, "Unit test %d, Rf = %d, while Output pattern doesn't match Input pattern [start %d / period %d / count %d]", 
        //            testUid, rf, generatedPattern.start, generatedPattern.period, generatedPattern.count );
        //DLog::msg(  DLog::LevelWarning, "But non-trivial Output pattern is observed, [start %3d / period %3d / count %3d, identity = %s]", 
        //            outputPattern.start, outputPattern.period, outputPattern.count, TextPatternAnalyzer::IdentityToString( outputPattern.identity ));
      }
    }
  }

  OutputExtendedAnalysisResultsToLog( searchComplete, outputPattern, rf );
}


bool MaiTrixUnitTest::RunMaiTrixAndDoAnalysis( 
                        TextPatternAnalyzer&  inputPatternAnalyzer, 
                        TextPatternAnalyzer&  outputPatternAnalyzer,
                        PeriodIt tick, 
                        int rf )
{
  bool searchComplete = false;

  string message;

  DebugBytesToText(( ByteIt* )textFrame, textFrame.Size(), message );
  DLog::msg( DLog::LevelVerbose, "In tick %3d providing TEXT input =>   '%s' of data with size '%d'", tick, message.c_str(), textFrame.Size());

  bool discoveredInputPattern  = inputPatternAnalyzer.AnalyzePitch( textFrame, true );
  bool discoveredOutputPattern = false;

  if( discoveredInputPattern )
  {
    ImpulseFramesPattern inputPattern = inputPatternAnalyzer.GetPattern();

    PatternsSanityCheck( inputPattern );
  }


  //
  // Do we void maiTrix in between Unit tests?
  //
  callMaiTrixBack( tick, rf, textFrame, audioFrame, videoFrame, callMaiTrixBackArguments );



  // Wait some "reasonable time" till start analyzing the output 
  // as sometimes Cuda maiTrix returns non empty output before the time.
  //
  // TODO: Should we add another analyzer (#3) to discover it and write a warning, just in case?
  //
  if( tick >= delay )
  {
    DebugBytesToText(( ByteIt* )textFrame, textFrame.Size(), message );
    DLog::msg( DLog::LevelVerbose, "In tick %3d receiving TEXT output <= '%s' of data with size '%d'", tick, message.c_str(), textFrame.Size());

    discoveredOutputPattern = outputPatternAnalyzer.AnalyzePitch( textFrame, false );

    if( discoveredOutputPattern )
    {
      // Second sanity check!
      if( !discoveredInputPattern )
      {
        // Well, consider these predictions ;-)
        statistic.predictionRuns++;
        //
        // extra noise? discard or what?
        //throw UnitTestException( DException::Warning, "Discovered output pattern while no input pattern found yet" );
      }

      searchComplete = true;
    }
  }

  return searchComplete;
}


bool MaiTrixUnitTest::RunForPattern(  IFrame&               patternedFrame, 
                                      IFrame&               emptyFrame,
                                      TextPatternAnalyzer&  inputPatternAnalyzer, 
                                      TextPatternAnalyzer&  outputPatternAnalyzer,
                                      int                   rf )
{
  PeriodIt nextPitchTick  = generatedPattern.start;
  SizeIt   count          = 0;
  //int      rf             = 17199; // loopTrait * loopPattern;
  bool     searchComplete = false;


  //
  // Do or do not update RF in the loop?
  //
  for( PeriodIt loopCycles = 0; loopCycles < maxDuration; loopCycles++ )
  {
    // Generate desired pattern
    if( loopCycles == nextPitchTick && count < generatedPattern.count )
    {
      textFrame     = patternedFrame;
      nextPitchTick = loopCycles + generatedPattern.period; 
      count++;
    }
    else
    {
      textFrame = emptyFrame;
    }
        
    // Analyze it and maiTrix output too
    if( RunMaiTrixAndDoAnalysis( inputPatternAnalyzer, outputPatternAnalyzer, loopCycles, rf ))
    {
      searchComplete = true;
      break;
    }
  }

  UpdateStatistics( searchComplete, outputPatternAnalyzer.GetPattern(), rf );

  return searchComplete;
}



MaiTrixUnitTest::HitsStatistic& MaiTrixUnitTest::Run( SizeIt maxTestTraits, SizeIt maxTestPatterns, const char* maiTrixPath, const char* maiTrixDebPath, int rf )
{
  textFrame .Clear();
  audioFrame.Clear();
  videoFrame.Clear();

  string debugFolderPath( maiTrixDebPath );

  ofstream patternViewIn        ( debugFolderPath + "MaiTrixUnit_InRaw.log"      , ios_base::trunc ); // just in case, control input flow
  ofstream patternViewOutRaw    ( debugFolderPath + "MaiTrixUnit_OutRaw.log"     , ios_base::trunc );
  ofstream patternViewOutLevel0 ( debugFolderPath + "MaiTrixUnit_OutLevel0.log"  , ios_base::trunc );
  ofstream patternViewOutLevel1 ( debugFolderPath + "MaiTrixUnit_OutLevel1.log"  , ios_base::trunc );
  ofstream patternViewOutLevel2 ( debugFolderPath + "MaiTrixUnit_OutLevel2.log"  , ios_base::trunc );
  ofstream patternViewOutLevel3 ( debugFolderPath + "MaiTrixUnit_OutLevel3.log"  , ios_base::trunc );

  testUid = 0;

  for( SizeIt loopTrait = 0; loopTrait < maxTestTraits; loopTrait++ )
  {
    for( SizeIt loopPattern = 0; loopPattern < maxTestPatterns; loopPattern++ )
    {
      testUid++;

      generatedPattern   = patternGenerator.CreateImpulsePattern( loopPattern );  
      PitchTraits traits = patternGenerator.CreateTraits( loopTrait );  

      IFrame patternedFrame = patternGenerator.CreatePatternFrame( traits );  
      IFrame emptyFrame     = patternGenerator.CreateEmptyFrame();  

      if( emptyFrame.Size() != frameSize || patternedFrame.Size() != frameSize )
      {
        throw UnitTestException( DException::Error, "Generated pattern size doesn't match requested one" );
      }

      // Need input analysis for 'sanity check' ;-)
      TextPatternAnalyzer inputPatternAnalyzer  ( "Input ", frameSize, maxDuration, generatedPattern.count );
      TextPatternAnalyzer outputPatternAnalyzer ( "Output", frameSize, maxDuration, generatedPattern.count );

      DLog::msg( DLog::LevelInfo, "Unit test %d, use symbolic %2d pattern of width %2d [start %d / period %d / count %d, identity = %s]", 
                 testUid, traits.symbolic, traits.width, generatedPattern.start, generatedPattern.period,  generatedPattern.count, TextPatternAnalyzer::IdentityToString( generatedPattern.identity ));

      // run single unit test
      bool searchComplete = RunForPattern( patternedFrame, emptyFrame, inputPatternAnalyzer, outputPatternAnalyzer, rf );

      // do some cleanup in between tests
      callMaiTrixBackAfter( callMaiTrixBackArguments );
    
      OutputViewByLevel( inputPatternAnalyzer  , patternViewIn        , Its::MaxIdentityLevel , searchComplete );
      OutputViewByLevel( outputPatternAnalyzer , patternViewOutRaw    , Its::MaxIdentityLevel , searchComplete );
      OutputViewByLevel( outputPatternAnalyzer , patternViewOutLevel0 , Its::Content          , searchComplete );
      OutputViewByLevel( outputPatternAnalyzer , patternViewOutLevel1 , Its::Width            , searchComplete );
      OutputViewByLevel( outputPatternAnalyzer , patternViewOutLevel2 , Its::CharTypes        , searchComplete );
      OutputViewByLevel( outputPatternAnalyzer , patternViewOutLevel3 , Its::Present          , searchComplete );
    }
  }

  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "===========> Analysis results ===========>" );

  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "Total      runs = %d", statistic.runs           );
  DLog::msg( DLog::LevelInfo, "Complete   runs = %d", statistic.completeRuns   );
  DLog::msg( DLog::LevelInfo, "Prediction runs = %d", statistic.predictionRuns );  
  DLog::msg( DLog::LevelInfo, "NonTrivial runs = %d", statistic.nontrivialRuns );    
  DLog::msg( DLog::LevelInfo, "" );

  DLog::msg( DLog::LevelInfo, "Identity Level 0 [ Content   ], period hits = %3d, count hits = %3d", statistic.periodHits[ Its::Content   ], statistic.countHits[ Its::Content   ] );
  DLog::msg( DLog::LevelInfo, "Identity Level 1 [ Width     ], period hits = %3d, count hits = %3d", statistic.periodHits[ Its::Width     ], statistic.countHits[ Its::Width     ] );
  DLog::msg( DLog::LevelInfo, "Identity Level 2 [ CharTypes ], period hits = %3d, count hits = %3d", statistic.periodHits[ Its::CharTypes ], statistic.countHits[ Its::CharTypes ] );
  DLog::msg( DLog::LevelInfo, "Identity Level 3 [ Present   ], period hits = %3d, count hits = %3d", statistic.periodHits[ Its::Present   ], statistic.countHits[ Its::Present   ] );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "<=========== Analysis results <===========" );

  return statistic;
}


  

void MaiTrixUnitTest::OutputViewByLevel( TextPatternAnalyzer& patternAnalyzer, ostream& os, Identity_t identity, bool searchComplete )
{
  string searchCompleteIndicator = searchComplete ? "+" : "-";

  //
  // Output header with extra info
  //
  os << "************** Output begins for [" << searchCompleteIndicator << "] unit test " << testUid << " with identity '" << TextPatternAnalyzer::IdentityToString( identity ) << "' **************" << endl;
  
 
  patternAnalyzer.OutputView( os, "Generated", &generatedPattern );
  patternAnalyzer.OutputView( os, identity );

  os << endl;
  os << "Output ends for identity '" << TextPatternAnalyzer::IdentityToString( identity ) << "'" << endl;
  os << endl << endl;
}


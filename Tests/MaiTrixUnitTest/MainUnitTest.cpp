// MaiTrixUnitTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <time.h>
#include <iostream>

#include "MaiTrixUnitTest.h"


using namespace Its;
using namespace std;


IFrame mirrorFrame( 100 );

Probe  probe;


void MaiTrixUnitTestCallback_Debug( PeriodIt tick, int rf, IFrame& textFrame, IFrame& audioFrame, IFrame& videoFrame, void* callMaiTrixBackArguments )
{
  DLog::msg( DLog::LevelVerbose, "Entering MaiTrix debug callback" );

  //
  // Run 5 traits x 7 patterns x 40 ticks
  //
  //
  // Debugging analyzers
  //
  // 1) Empty test (no patterns discovered expected)
  //
  //textFrame.Clear();
  //return;

  // 2) Same test (all patterns discovered expected)
  //
  //return;

  // 3) Simple pattern test ( 35 / 35 / 10 patterns discovered expected with delay 10, all by Content )
  //                        ( 35 / 35 / 20 patterns discovered expected with delay 0 , all by Content )
  //if( tick % 3 == 0 )
  //{
  //  memset( ( ByteIt* )textFrame, 'f', 13 );
  //}
  //else
  //{
  //  textFrame.Clear();
  //}
  //return;

  // 3) Simple pattern test 2 ( 35 / 35 / 0 patterns discovered expected with delay 0, 
  //                            5 by Content for 1 pitch, 5 by char types for 2 pitches )
  //if( tick == 15 )
  //{
  //  memset( ( ByteIt* )textFrame, 'f', 13 );
  //}
  //else if( tick == 22 )
  //{
  //  memset( ( ByteIt* )textFrame, 'a', 3 );
  //}
  //else
  //{
  //  textFrame.Clear();
  //}
  //return;


  // 4) Prediction test ( 35 / 27 / 7 patterns discovered expected with with delay 10 )
  //
  if( tick % 2 == 0 )
  {
    memset( ( ByteIt* )textFrame, 'd', tick % 8 );
  }
  else if( tick % 3 == 0 )
  {
    memset( ( ByteIt* )textFrame, 150, tick % 7 );
  }
  return;

  // 5) Tree-shape non-char test ( 35 / 35 / 25 patterns discovered expected, 30 char types, 5 content)
  //textFrame.Clear();
  //memset( ( ByteIt* )textFrame, 150, tick % 7 );
  //return;

  // 6) Random test (nothing expected, check logs carefully! ;-)
  //memset( ( ByteIt* )textFrame, rand() % 200, rand() % 20 );
  //return;

  // 7) Mirror test  ( 35 / 35 / 30 patterns discovered expected with 0 delay )
  //if( mirrorFrame.Size() != textFrame.Size())
  //{
  //  textFrame.Clear();
  //}
  //else
  //{
  //  if( !textFrame.Empty() )
  //  {
  //    mirrorFrame = textFrame;
  //  }
  //  else
  //  {
  //    textFrame = mirrorFrame;
  //  }
  //}
  //return;

  int& a = *(( int* ) callMaiTrixBackArguments );

  //DLog::msg( DLog::LevelInfo, "Running MaiTrix , test a = %d", a );
}



void MaiTrixUnitTestCallbackAfter_Debug( void* callMaiTrixBackArguments )
{
  // do nothing in debug cycle
}




void MaiTrixUnitTestCallback( PeriodIt tick, int rf, IFrame& textFrame, IFrame& audioFrame, IFrame& videoFrame, void* callMaiTrixBackArguments )
{
  DLog::msg( DLog::LevelVerbose, "Entering MaiTrix callback" );

  MaiTrix& matrix = *(( MaiTrix* ) callMaiTrixBackArguments );

  DLog::msg( DLog::LevelVerbose, "MaiTrix name = '%s', size %d x %d x %d", matrix.Name(), matrix.Size().X, matrix.Size().Y, matrix.Size().Z );
  
  string messageIn;
  string messageOut;

  int gf = 1;

  // *********************** HERE *********************** 
  //
  // Note: "Do not create more then needed" (c) Okkam
  //
  // In general we don't have to clear those streams, it's just now we're testing
  // Okkama rules - do not create more cells then needed, all cells are likely
  // to be aligned in a "row", "thread" and we suppres interdirectional connections
  // to see with analyzer some pattern in the output of the one channel - text.
  //
  // 1) Eventually we need to find good maiTrix (with good patterns for all RFs)
  // 2) Then we can mix all channels and see... what?
  //
  audioFrame.Clear();
  videoFrame.Clear();

  //DebugBytesToText(( ByteIt* )textFrame, textFrame.Size(), messageIn );
  //DLog::msg( DLog::LevelInfo, "In tick %3d TEXT input  '%s' of data with size '%d'", tick, messageIn.c_str(), textFrame.Size());

  matrix.Launch( MaiTrix::GF2GF( gf ), rf, (ByteIt*)textFrame, (ByteIt*)audioFrame, (ByteIt*)videoFrame, probe );

  //DebugBytesToText(( ByteIt* )textFrame, textFrame.Size(), messageOut );
  //DLog::msg( DLog::LevelInfo, "In tick %3d TEXT output '%s' of data with size '%d'", tick, messageOut.c_str(), textFrame.Size());
}


void MaiTrixUnitTestCallbackAfter( void* callMaiTrixBackArguments )
{
  DLog::msg( DLog::LevelVerbose, "Entering MaiTrix after callback" );

  MaiTrix& matrix = *(( MaiTrix* ) callMaiTrixBackArguments );

  matrix.Reboot();
}


void OutputStatistic( MaiTrixUnitTest::HitsStatistic& statistic, int unitTestSeriesRuns )
{
  if( unitTestSeriesRuns <= 0 )
  {
    DLog::msg( DLog::LevelInfo, "Total runs should be positive" );
    return;
  }

  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "===========> Results for %d unit test series runs ===========>", unitTestSeriesRuns );

  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "Total      runs = %3.1f", ( double )statistic.runs           / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "Complete   runs = %3.1f", ( double )statistic.completeRuns   / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "Prediction runs = %3.1f", ( double )statistic.predictionRuns / unitTestSeriesRuns );  
  DLog::msg( DLog::LevelInfo, "NonTrivial runs = %3.1f", ( double )statistic.nontrivialRuns / unitTestSeriesRuns );    
  DLog::msg( DLog::LevelInfo, "" );

  DLog::msg( DLog::LevelInfo, "Identity Level 0 [ Content   ], period hits = %3.1f, count hits = %3.1f", ( double )statistic.periodHits[ Its::Content   ] / unitTestSeriesRuns, ( double )statistic.countHits[ Its::Content   ] / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "Identity Level 1 [ Width     ], period hits = %3.1f, count hits = %3.1f", ( double )statistic.periodHits[ Its::Width     ] / unitTestSeriesRuns, ( double )statistic.countHits[ Its::Width     ] / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "Identity Level 2 [ CharTypes ], period hits = %3.1f, count hits = %3.1f", ( double )statistic.periodHits[ Its::CharTypes ] / unitTestSeriesRuns, ( double )statistic.countHits[ Its::CharTypes ] / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "Identity Level 3 [ Present   ], period hits = %3.1f, count hits = %3.1f", ( double )statistic.periodHits[ Its::Present   ] / unitTestSeriesRuns, ( double )statistic.countHits[ Its::Present   ] / unitTestSeriesRuns );
  DLog::msg( DLog::LevelInfo, "" );
  DLog::msg( DLog::LevelInfo, "<=========== Results for %d unit test series runs <===========", unitTestSeriesRuns );
}



int _tmain( int argc, _TCHAR* argv[] )
{
  const char* maiTrixPath     = ".\\dma\\";
  const char* maiTrixDebPath  = ".\\deb\\";

  //
  // compare to MaiTrixTest.exe 50 7 200 37 45 5 1 17199
  //
  PeriodIt    optimalDuration    = 300; //  See Long runs tests result
  PeriodIt    optimalSeriesRuns  = 50;  //  See Long series tests result

  const char* matrix1Name        = "First";
  const char* matrix2Name        = "Second";
  SizeIt      matrixSizeXY       = 50;
  SizeIt      matrixSizeZ        = 7;
  PeriodIt    maxDuration        = optimalDuration; // 400?  Optimum test!
  int         unitTestSeriesRuns = optimalSeriesRuns;
  bool        useRandomRf        = true;
  MaiTrix::MaiTrixOptions maiTrixOptions = MaiTrix::CacheLayers;

  // Set 0 to find "prediction runs" if any and so all runs would be complete by reaching ticksLimit
  //
  // as 'completeness' depends on delay, check TextPatternAnalyzer::PatternSearchComplete() for details
  //
  PeriodIt    delay         = 0;       //results should be:
  //int         rf            = 111;   // bad,      0-0    / 0-0    (Content/Width)
  //int         rf            = 7193;  // good,     8-8    / 2-2    (Content/Present)
  //int         rf            = 17199; // good,     10-10  / 0-0    (Content/Width)
  //int         rf            = 31997; // good,     20-12  / 0-15   (Content/Width)
  //int         rf            = 10715; // perfect,  25-25  / 0-0    (Content/Width)
  //int         rf            = 11401; // perfect,  25-24  / 0-0    (Content/Width)
  //int         rf            = 12510; // perfect,  25-25  / 0-0    (Content/Width)
  //int         rf            = 13510; // perfect,  23-23  / 2-2    (Content/Width)
  int         rf;

  //
  // Usage: MaiTrixUnitTest.exe [ unitTestSeriesRuns = 5 ], [ Rf = random ] [ maxDuration = 100 ]
  //
  unitTestSeriesRuns = argc > 1 ? atoi( argv[ 1 ] ) : optimalSeriesRuns;
  rf                 = argc > 2 ? atoi( argv[ 2 ] ) : 0;
  maxDuration        = argc > 3 ? atoi( argv[ 3 ] ) : optimalDuration;

  if( argc == 1 )
  {
    cout << "Usage: MaiTrixUnitTest.exe [ unitTestSeriesRuns = 5 ], [ Rf = random ] [ maxDuration = 100 ]" << endl << endl;
  }

  if( rf == 0 )
  {
    cout << "Running MaiTrix unit test, series runs = " << unitTestSeriesRuns << ", Rf = random, maxDuration = " << maxDuration << endl << endl;
    useRandomRf = true;
  }
  else
  {
    cout << "Running MaiTrix unit test, series runs = " << unitTestSeriesRuns << ", Rf = " << rf << ", maxDuration = " << maxDuration << endl << endl;
    useRandomRf = false;
  }

  string logFilePath( maiTrixDebPath );

  logFilePath += "MaiTrixUnitTest.log";
  
  DLog::init( logFilePath.c_str(), DLog::LevelDebug );
  
  DLog::msg( DLog::LevelInfo, "Running MaiTrix Unit Tests" );
  DLog::msg( DLog::LevelInfo, "Unit Tests conditions: series runs = %d, Rf = %d, run duration = %d ticks", unitTestSeriesRuns, rf, maxDuration );
  DLog::msg( DLog::LevelInfo, "" );

  try
  {
    // Observe at least 3 layers! otherwise text will no cover all the side
    // and we will miss some output as casted to the other types
    //
    // Note it!
    //
    SizeIt textCapacity = ( matrixSizeXY * 4 ) * 3; // 3 layers

    MaiTrix  matrixFirst   ( matrix1Name, matrixSizeXY, matrixSizeXY, matrixSizeZ, textCapacity, 10, 10, maiTrixPath, maiTrixOptions );
    MaiTrix  matrixSecond  ( matrix2Name, matrixSizeXY, matrixSizeXY, matrixSizeZ, textCapacity, 10, 10, maiTrixPath, maiTrixOptions );
    
    MaiTrix&    matrixTest      = matrixFirst;//matrixSecond;
    CapacityIt  maiTrixCapacity = matrixTest.Capacity();
    MaiTrixUnitTest::HitsStatistic statisticSummary;

    // 5 traits, 7 patterns
    //
    //unitTest.Run( 2, 2, maiTrixPath, maiTrixDebPath, rf );
    srand( GetTickCount() );

    clock_t startTime = clock();

    for( int loopRf = 0; loopRf < unitTestSeriesRuns; loopRf++ )
    {
      MaiTrixUnitTest unitTest( maiTrixCapacity, 
                                maxDuration, 
                                delay, 
                                MaiTrixUnitTestCallback, 
                                MaiTrixUnitTestCallbackAfter, 
                                &matrixTest );

      if( useRandomRf )
      {
        rf = rand();
      }

      cout << "Rf = " << rf << endl;

      DLog::msg( DLog::LevelInfo, "************************************************************" );
      DLog::msg( DLog::LevelInfo, "For MaiTrix '%s' unit test series %d begins for Rf %d", matrixTest.Name(), loopRf, rf );
      DLog::msg( DLog::LevelInfo, "************************************************************" );

      MaiTrixUnitTest::HitsStatistic& statistic = unitTest.Run( 5, 7, maiTrixPath, maiTrixDebPath, rf );

      statisticSummary += statistic;
    }
 
    clock_t finishTime = clock();
  
    DLog::msg( DLog::LevelInfo, "" );
    DLog::msg( DLog::LevelInfo, "MaiTrix '%s' unit test run time: %5.1f sec", matrixTest.Name(), ((( double )( finishTime - startTime)) / CLOCKS_PER_SEC ));

    OutputStatistic( statisticSummary, unitTestSeriesRuns );
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelError, "MaiTrix unit test error: %s", x.what() );
    cout << "MaiTrix unit test failed with error" << x.what() << endl;
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "MaiTrix unit test general error" );
    cout << "MaiTrix unit test failed with general error" << endl;
  }

  DLog::msg( DLog::LevelInfo, "MaiTrix Unit Tests complete" );
  cout << "MaiTrix unit test complete" << endl;

   return 0;
}
/*******************************************************************************
 FILE         :   Guide.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Integration point for everything

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/26/2011

 LAST UPDATE  :   09/26/2011
*******************************************************************************/


#include "stdafx.h"

#include "ISaver.h"
#include "IGuidex.h"
#include "Guide.h"
#include "Internals.h"
#include "DLog.h"

// Data feeders
#include "Feeders\IFeederText.h"
#include "Feeders\IFeederImage.h"
#include "Feeders\IFeederTextTest.h"
#include "Feeders\IFeederTextABC.h"

#include <boost/format.hpp>

using namespace Its;
using namespace DCommon;
using boost::format;
using boost::io::group;


Guide* Guide::instance = 0;


Guide::Guide( PeriodIt _statisticPeriod, PeriodIt _showMaiTrixPeriod ):
  statisticPeriod   ( _statisticPeriod ),
  showMaiTrixPeriod ( _showMaiTrixPeriod ),
  seriesStartTime   ( 0 ),
  seriesStartCycle  ( 0 ),
  correctionsCounter( 0 ),
  effectiveInPeriod ( 0 ),
  callbackCounter   ( 0 )
{
  if( Guide::instance == 0 )
  {
    instance = this;
  }
}



Guide::~Guide()
{
  if( !IsThisSingleton() )
  {
    return;
  }

  try
  {
    for( IGuide::FeedersList_t::iterator feederLoop = feeders.begin(); feederLoop != feeders.end(); feederLoop++ )
    {
      delete *feederLoop;
    }

    feeders.clear();
  }
  catch( ... )
  {
    ShowMessage( "General cleanup error" );
  }

  instance = 0;
}


void Guide::ShowMaiTrix( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath )
{
  if( !IsThisSingleton() )
  {
    instance->ShowMaiTrix( maiTrix, maiTrixPath, maiTrixDebPath );
    return;
  }

  // Flush caches, etc... first to show correct current data
  maiTrix.Commit(); 
  IGuideX::TakeSnapshot( maiTrix.Name(), maiTrixPath, maiTrixDebPath );
}


void Guide::LoadStatistics( MaiTrix& maiTrix, const char* maiTrixPath, CubeStat& maiTrixStat )
{
  if( !IsThisSingleton() )
  {
    instance->LoadStatistics( maiTrix, maiTrixPath, maiTrixStat );
    return;
  }

  MaiTrixSnapshot snapshot( maiTrix.Name(), maiTrixPath );

  // Flush caches, etc... first to show correct current data
  maiTrix.Commit();
  snapshot.CollectStatistics( &maiTrixStat );

  SizeItl ioCells = maiTrixStat.cellsIOa + maiTrixStat.cellsIOv + maiTrixStat.cellsIOt;
    DLog::msg( DLog::LevelInfo, "Loading statistics in period %d, good-bad-dead-sin-no-io: %d-%d-%d-%d-%d-%d",
    callbackCounter, maiTrixStat.cellsGood, maiTrixStat.cellsBad, maiTrixStat.cellsDead, 
    maiTrixStat.cellsSin, maiTrixStat.cellsNo, ioCells );

  // Do basic validation
  if( maiTrixStat.cellsDead > 0 )
  {
    throw IGuideException( DException::Error, "Too many dead cells: %d", maiTrixStat.cellsDead );
  }

  if( maiTrix.Size().X * maiTrix.Size().Y * maiTrix.Size().Z != maiTrixStat.CellsTotal())
  {
    throw IGuideException( "Statistical maiTrix size validation error" );
  }
  
  if( maiTrix.Size().X * maiTrix.Size().Y != ioCells )
  {
    throw IGuideException( "Statistical maiTrix I/O size validation error" );
  }
}


void Guide::CompleteFeedbackCallback( IGuide::GuideFeedback_t& feedback )
{
  // commit and snapshot
  DLog::msg( DLog::LevelInfo, "Maitrix %s is being finalized", feedback.maiTrix->Name());
  ShowMaiTrix( *feedback.maiTrix, feedback.maiTrixPath, feedback.maiTrixDebPath );
}


void Guide::FeedbackCallbackToGuide( IGuide::GuideFeedback_t& feedback )
{
  CubeStat  maiTrixStat;
  float     seriesAverageTime;
  clock_t   statisticsStart;

  if( !IsThisSingleton() )
  {
    instance->FeedbackCallbackToGuide( feedback );
    return;
  }

  // Cycle doesn't reflect current statistic as:
  //
  // 1) we can load existing maiTrix  
  // 2) callback is not invoked during delay
  //
  callbackCounter++; 

  if( feedback.gf < 0 )
  {
    correctionsCounter++;
  }

  if( seriesStartTime == 0 )
  {
    seriesStartTime   = clock();
    seriesStartCycle  = feedback.maiTrix->Cycle();
    seriesAverageTime = 0;
  }
  else
  {
    PeriodIt cyclesPassed = feedback.maiTrix->Cycle() - seriesStartCycle;

    if( cyclesPassed == 0 )
    {
      throw IGuideException( "Cycles calculation error" );
    }

    seriesAverageTime = (( float )( clock() - seriesStartTime ) / CLOCKS_PER_SEC ) / cyclesPassed;
  }

  statisticsStart = clock();

  int averageCorrections = ( int ) (((( float ) correctionsCounter ) / callbackCounter ) * 100.0f );

  if( effectiveInPeriod == 0 )
  {
    effectiveInPeriod = feedback.lessonRepetitions * feedback.lessonTotalLength;
  }
  else
  {
    effectiveInPeriod--;
  }
  
  BasicInfo basicInfo;

  basicInfo.sins                = feedback.sins;
  basicInfo.tick                = feedback.tick;
  basicInfo.maiTrixSize         = feedback.maiTrix->Size();
  basicInfo.maiTrixCapacity     = feedback.maiTrix->Capacity();
  basicInfo.cycle               = feedback.maiTrix->Cycle();
  basicInfo.averageCorrections  = averageCorrections;
  basicInfo.seriesAverageTime   = seriesAverageTime;
  basicInfo.pqEstimate          = feedback.pqEstimate;
  basicInfo.lessonLevel         = feedback.lessonLevel;
  basicInfo.lessonTopic         = feedback.lessonTopic;
  basicInfo.lessonTotalLength   = feedback.lessonTotalLength;
  basicInfo.lessonRepetition    = feedback.lessonRepetition;
  basicInfo.lessonRepetitions   = feedback.lessonRepetitions;
  basicInfo.effectiveInPeriod   = effectiveInPeriod;
  basicInfo.baseMessage         = feedback.baseMessage;
  basicInfo.message             = feedback.message;

  if( callbackCounter % statisticPeriod == 0 || feedback.takeSnapshot )
  {
    LoadStatistics( *feedback.maiTrix, feedback.maiTrixPath, maiTrixStat );
    ShowStatistics( maiTrixStat );
  }

  if( callbackCounter % showMaiTrixPeriod == 0 || feedback.takeSnapshot )
  {
    ShowMaiTrix( *feedback.maiTrix, feedback.maiTrixPath, feedback.maiTrixDebPath );
  }

  basicInfo.statisticsTime = ((( float )clock() - ( float )statisticsStart) / CLOCKS_PER_SEC );

  ShowBasicInfo( basicInfo );
}



// Do not do anything by default
//
// Those methods are not virtual in case we want to run Guide quietly
//
void Guide::ShowBasicInfo( BasicInfo& basicInfo )
{
}


void Guide::ShowStatistics( CubeStat& maiTrixStat )
{
}


void Guide::ShowAndAdjust( AdjustmentInfo& adjustmentInfo )
{
}


void Guide::ShowMessage( const char* message )
{
}


void Guide::CallbackToGuide( int pq, int vq, const char *message, IGuide::GuideWay_t& way )
{
  if( !IsThisSingleton() )
  {
    instance->CallbackToGuide( pq, vq, message, way );
    return;
  }

  AdjustmentInfo adjustmentInfo;

  adjustmentInfo.pq                 = pq;
  adjustmentInfo.vq                 = vq;
  adjustmentInfo.message            = message;
  adjustmentInfo.delay              = way.delay;
  adjustmentInfo.repetitions        = way.repetitions;
  adjustmentInfo.action             = IGuide::None; 
  adjustmentInfo.probe              = way.probe;
  adjustmentInfo.statisticPeriod    = statisticPeriod;
  adjustmentInfo.showMaiTrixPeriod  = showMaiTrixPeriod;

  ShowAndAdjust( adjustmentInfo );

  way.delay         = adjustmentInfo.delay;
  way.repetitions   = adjustmentInfo.repetitions;
  way.action        = adjustmentInfo.action;
  way.probe         = adjustmentInfo.probe;

  statisticPeriod   = adjustmentInfo.statisticPeriod;
  showMaiTrixPeriod = adjustmentInfo.showMaiTrixPeriod;

  seriesStartTime   = 0;
  effectiveInPeriod = 0;
}


void Guide::OneCompleteFeedbackCallback( IGuide::GuideFeedback_t& feedback )
{
  if( instance )
  {
    instance->CompleteFeedbackCallback( feedback );
  }
}


void Guide::OneFeedbackCallbackToGuide( IGuide::GuideFeedback_t& feedback )
{
  if( instance ) // doesn't make a lot of sence but ... ;-)
  {
    instance->FeedbackCallbackToGuide( feedback );
  }
}


void Guide::OneCallbackToGuide( int pq, int vq, const char *message, IGuide::GuideWay_t& way )
{
  if( instance )
  {
    instance->CallbackToGuide( pq, vq, message, way );
  }
}


void Guide::LogMessage( const char *message, exception* x )
{
  string fullMessage( message );
    
  if( x != 0 )
  {
    fullMessage += x->what();
  }

  ShowMessage( fullMessage.c_str() );

  DLog::msg( DLog::LevelInfo, "%s", fullMessage.c_str());
}


void Guide::Its()
{
  __try
  {
    It();
  }
  __except( EXCEPTION_EXECUTE_HANDLER )
  {
    char buffer[ 1024 ];
    sprintf_s( buffer, sizeof( buffer ), "Structured exception: %d, system is not stable.", GetExceptionCode());
    cout << buffer;
  }
}


void Guide::It()
{
  if( !IsThisSingleton() )
  {
    instance->Its();
    return;
  }

  IGuideX guide;

  try
  {

    feeders.push_back( new IFeederTextTest( IFeederTextTest::BaseFeed ));
    // feeders.push_back( new IFeederTextABC( 'A', 'C' ));
    // feeders.push_back( new IFeederImage( ".\\feed\\image\\source\\png", 1, ".png" ));

    guide.Teach( feeders, OneCallbackToGuide, OneFeedbackCallbackToGuide );
    guide.Complete( OneCompleteFeedbackCallback );

    LogMessage( "Teaching complete", 0 );
  }
  catch( CubeException& x )
  {
    string fullMessage = str( boost::format("Cuda error: %s / %3d") % x.what() % ( short )x.GetCudaError());
    LogMessage( fullMessage.c_str(), 0 );
    guide.TakeSnapshot();
  }
  catch( BasicException& x )
  {
    LogMessage( "Basic Its error: ", &x );
    guide.TakeSnapshot();
  }
  catch( exception& x )
  {
    LogMessage( "Error: ", &x );
    guide.TakeSnapshot();
  }
  catch( ... )
  {
    LogMessage( "General error", 0 );
    guide.TakeSnapshot();
  }
}



/*******************************************************************************
 FILE         :   GuideMultiConsoleThreads.cpp 

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Internal threads for guide with multiconsole output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/31/2012

 LAST UPDATE  :   01/31/2012
*******************************************************************************/


#include "stdafx.h"

#include "Init.h"
#include "IGuide.h"
#include "GuideMultiConsoleThreads.h"
#include "GuideMultiConsole.h"
#include "DLog.h"

#include <iomanip>

#include <boost/format.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>


using namespace Its;
using namespace DCommon;
using namespace std;

using boost::format;
using boost::io::group;
using namespace boost::posix_time;
using namespace boost::gregorian;


DCriticalSection InputThread::section;

const int ScreenPosCurrent  = 20;
const int ScreenPosAdjusted = 40;


InputThread::InputThread( GuideMultiConsole& _guide ):
  DThread(),
  guide( _guide ),
  maiTrixSizeZ( 0 )
{
  adjustedInfo.delay              = INITIAL_DELAY;
  adjustedInfo.repetitions        = INITIAL_REPETITIONS;
  adjustedInfo.action             = IGuide::None;
  adjustedInfo.statisticPeriod    = INITIAL_PERIOD_STATISTIC;
  adjustedInfo.showMaiTrixPeriod  = INITIAL_PERIOD_SHOWMAITRIX;
}


void InputThread::run()
{
  __try
  {
    runThread();
  }
  __except( EXCEPTION_EXECUTE_HANDLER )
  {
    char buffer[ 1024 ];
    sprintf_s( buffer, sizeof( buffer ), "Structured exception: %d, system is not stable.", GetExceptionCode());
    guide.mconsole << guide.colorInfo << guide.Position( 0, 0 ) << buffer;
    adjustedInfo.action = IGuide::Pause;
  }
}


void InputThread::runThread()
{
  // text and pos are read-only so no access collisions
  MultiCon& mconsole = guide.mconsole;

  try
  {
    mconsole << guide.pageControl << guide.colorText;

    //mconsole << guide.Position(  0,  2 ) << "PQ             :";
    //mconsole << guide.Position(  0,  3 ) << "VQ             :";
    mconsole << guide.Position(  0,  4 ) << "Message        :";

    mconsole << guide.Position( ScreenPosCurrent, 6 ) << "Current      /      Adjusted";
  
    mconsole << guide.Position(  0,  8 ) << "Action         :";
    mconsole << guide.Position(  0,  9 ) << "Stat. Period   :";
    mconsole << guide.Position(  0, 10 ) << "Snapshots      :";
    mconsole << guide.Position(  0, 11 ) << "Delay          :";
    mconsole << guide.Position(  0, 12 ) << "Repetitions    :";
    // mconsole << guide.Position(  0, 13 ) << "Probe (divert) :";
    // mconsole << guide.Position(  0, 14 ) << "Probe (mix)    :";
    // mconsole << guide.Position(  0, 15 ) << "Probe (create) :";

    IGuide::GuideAction_t action;
    multichoice   selector( guide.colorControl, 4, "None", "Pause", "Reboot", "Void" );
    int           fieldNumber = 0;
    FieldFocus    focus; // what's selected next?
    const int     fieldOne = 0;

    while( !shouldExit() )
    {
      ShowAdjustment( adjustedInfo, false );
      switch( fieldNumber )
      {
        case fieldOne:
          {
            guide.mconsole >> guide.Position( pos( ScreenPosAdjusted, 8 ) ) >> selector;

            focus = selector.Focus();

            TextToAction( selector.Choice(), action );

            DCriticalSection::Lock locker( section );

            adjustedInfo.action = action;
          }
          break;

        case fieldOne +  1: focus = ReadNumber( pos( ScreenPosAdjusted,  9 ), adjustedInfo.statisticPeriod   , 5, 1, 1000 ); break;
        case fieldOne +  2: focus = ReadNumber( pos( ScreenPosAdjusted, 10 ), adjustedInfo.showMaiTrixPeriod , 5, 1, 1000 ); break;
        case fieldOne +  3: focus = ReadNumber( pos( ScreenPosAdjusted, 11 ), adjustedInfo.delay             , 5, 1, 1000 ); break;
        case fieldOne +  4: focus = ReadNumber( pos( ScreenPosAdjusted, 12 ), adjustedInfo.repetitions       , 5, 1, 9999 ); break;
        /*
        case fieldOne +  5: focus = ReadNumber( pos( ScreenPosAdjusted    , 13 ), adjustedInfo.probe.divertLow   , 3, MinByteValue                 , adjustedInfo.probe.divertHigh ); break;
        case fieldOne +  6: focus = ReadNumber( pos( ScreenPosAdjusted + 6, 13 ), adjustedInfo.probe.divertHigh  , 3, adjustedInfo.probe.divertLow , MaxByteValue                  ); break;
        case fieldOne +  7: focus = ReadNumber( pos( ScreenPosAdjusted    , 14 ), adjustedInfo.probe.mixLow      , 3, MinByteValue                 , adjustedInfo.probe.mixHigh    ); break;
        case fieldOne +  8: focus = ReadNumber( pos( ScreenPosAdjusted + 6, 14 ), adjustedInfo.probe.mixHigh     , 3, adjustedInfo.probe.mixLow    , MaxByteValue                  ); break;
        case fieldOne +  9: focus = ReadNumber( pos( ScreenPosAdjusted    , 15 ), adjustedInfo.probe.createLow   , 3, 1                            , adjustedInfo.probe.createHigh ); break;
        case fieldOne + 10: focus = ReadNumber( pos( ScreenPosAdjusted + 6, 15 ), adjustedInfo.probe.createHigh  , 3, adjustedInfo.probe.createLow , 7                             ); break;
        */
      }

      fieldNumber = focus == FieldNext ? fieldNumber + 1 : fieldNumber - 1;
      int lastFieldNumber = fieldOne + 4;

      if( fieldNumber > lastFieldNumber )
      {
        fieldNumber = 0;
      }
      else if( fieldNumber < 0 )
      {
        fieldNumber = lastFieldNumber;
      }
    }
  }
  catch( BasicException& x )
  {
    mconsole << guide.colorInfo << guide.Position( 0, 0 ) << x.what();
    adjustedInfo.action = IGuide::Pause;
  }
  catch( ... )
  {
    mconsole << guide.colorInfo << guide.Position( 0, 0 ) << "Input thread failed for unknown reason. System is not stable.";
    adjustedInfo.action = IGuide::Pause;
  }
}


// Can be called from another thread
void InputThread::Update( Guide::AdjustmentInfo& currentInfo )
{
  DCriticalSection::Lock locker( section );

  // While probe in general supposed to changed in this thread, but
  // we change some runtime parameters, like probe.sizeZ in IGuide::Lesson
  // when maiTrix is launched. And those 'volatile' variables are being passed
  // with currentInfo so we want to preserve it as this may be needed to
  // calculate statistics.
  maiTrixSizeZ = currentInfo.probe.sizeZ;
  
  currentInfo.delay             = adjustedInfo.delay;
  currentInfo.repetitions       = adjustedInfo.repetitions;
  currentInfo.action            = adjustedInfo.action;
  currentInfo.probe             = adjustedInfo.probe;
  currentInfo.statisticPeriod   = adjustedInfo.statisticPeriod;
  currentInfo.showMaiTrixPeriod = adjustedInfo.showMaiTrixPeriod;

  ShowAdjustment( currentInfo,  true  );
  ShowAdjustment( adjustedInfo, false );
}


// Can be called from another thread
void InputThread::ShowAdjustment( Guide::AdjustmentInfo& info, bool current )
{
  MultiCon& mconsole  = guide.mconsole;
  int       column    = current ? ScreenPosCurrent : ScreenPosAdjusted;

  mconsole << guide.pageControl << guide.colorData;

  if( current )
  {
    string message = info.message;

    message.resize( 20, ' ' );

    //mconsole << guide.Position(  column,  2 ) << str( boost::format( "%3d" ) % info.pq ); 
    //mconsole << guide.Position(  column,  3 ) << str( boost::format( "%3d" ) % info.vq );
    mconsole << guide.Position(  column,  4 ) << message;
  }

  mconsole << guide.Position(  column,  8 ) << ActionToText( info.action );
  mconsole << guide.Position(  column,  9 ) << str( boost::format( "%-2d   - %-5d ticks" ) % info.statisticPeriod   % ( info.statisticPeriod   * maiTrixSizeZ ));
  mconsole << guide.Position(  column, 10 ) << str( boost::format( "%-2d   - %-5d ticks" ) % info.showMaiTrixPeriod % ( info.showMaiTrixPeriod * maiTrixSizeZ ));
  mconsole << guide.Position(  column, 11 ) << str( boost::format( "%-5d"                ) % info.delay );
  mconsole << guide.Position(  column, 12 ) << str( boost::format( "%-5d"                ) % info.repetitions );
  /*
  mconsole << guide.Position(  column, 13 ) << str( boost::format( "%-3d / %-3d" ) % ( int ) info.probe.divertLow % ( int ) info.probe.divertHigh );
  mconsole << guide.Position(  column, 14 ) << str( boost::format( "%-3d / %-3d" ) % ( int ) info.probe.mixLow    % ( int ) info.probe.mixHigh    );
  mconsole << guide.Position(  column, 15 ) << str( boost::format( "%-3d / %-3d" ) % ( int ) info.probe.createLow % ( int ) info.probe.createHigh );
  */

  guide.mconsole << guide.colorOutline << guide.clockPosition << guide.TimeStamp() << " Effective Time" << guide.colorData;
}


string InputThread::ActionToText( IGuide::GuideAction_t action )
{
  string actionText = "Unknown";

  switch( action )
  {
    case IGuide::None   : actionText = "None"   ; break;
    case IGuide::Pause  : actionText = "Pause"  ; break;
    case IGuide::Reboot : actionText = "Reboot" ; break;
    case IGuide::Void   : actionText = "Void"   ; break;
  }

  return actionText;
}


void InputThread::TextToAction( string actionText, IGuide::GuideAction_t& action )
{
  if( actionText == "None" )
  {
    action = IGuide::None; 
  }
  else if( actionText == "Pause" )
  {
    action = IGuide::Pause; 
  }
  else if( actionText == "Reboot" )
  {
    action = IGuide::Reboot; 
  }
  else if( actionText == "Void" )
  {
    action = IGuide::Void; 
  }
  else
  {
    throw InputThreadLeaveException( "Internal error. Invalid action Text value." ); 
  }
}


void InputThread::ReadText( pos position, string& text )
{
  string value = text;

  guide.mconsole >> guide.Position( position ) >> guide.colorControl >> value;

  if( shouldExit() )
  {
    throw InputThreadLeaveException(); 
  }

  guide.mconsole << guide.Position( position ) << guide.colorData << value;

  DCriticalSection::Lock locker( section );

  text = value;
}


template<typename T> 
FieldFocus InputThread::ReadNumber( pos position, T& number, int fieldSize, long lowerBound, long upperBound )
{
  consolenumber fieldNumber( number, fieldSize, lowerBound, upperBound, guide.colorControl );

  guide.mconsole >> guide.Position( position ) >> fieldNumber;

  if( shouldExit() )
  {
    throw InputThreadLeaveException(); 
  }

  DCriticalSection::Lock locker( section );

  number = fieldNumber.Value();

  return fieldNumber.Focus();
}



TimeTrackerThread::TimeTrackerThread( GuideMultiConsole& _guide ):
  DThread(),
  guide( _guide )
{
}


void TimeTrackerThread::run()
{
  // text and pos are read-only so no access collisions
  MultiCon& mconsole    = guide.mconsole;
  clockPosition         = pos( 2, mconsole.Height() - 1 );
  pos uptimePosition    = clockPosition + pos( 40, 0 );

  ptime         systemStarted( second_clock::local_time() );
  format        periodFormat( "%7d days %02d hours %02d min Uptime" ); 
  string        periodFormated;
  time_duration uptime;
  ULONG         updays;
 
  try
  {
    mconsole << page( guide.pageMessage ) << guide.colorOutline;
    
    while( !shouldExit() )
    {
      mconsole << clockPosition << guide.TimeStamp() << " Local Time";
      
      ptime       currentTime ( second_clock::local_time() );
      time_period periodTime  ( systemStarted, currentTime );
      date_period periodDate  ( systemStarted.date(), currentTime.date() );

      uptime = periodTime.length();
      updays = periodDate.length().days();
      
      periodFormated = str( periodFormat % updays % uptime.hours() % uptime.minutes() );

      mconsole << uptimePosition << periodFormated;

      Sleep( 500 );
    }
  }
  catch( CubeException& x )
  {
    mconsole << guide.colorInfo << guide.Position( 0, 0 ) << x.what() << " / " << ( short )x.GetCudaError();
  }
  catch( BasicException& x )
  {
    mconsole << guide.colorInfo << guide.Position( 0, 0 ) << x.what();
  }
  catch (...)
  {
    mconsole << guide.colorInfo << guide.Position( 0, 0 ) << "Clock thread failed for unknown reason. System unstable.";
  }
}

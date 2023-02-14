/*******************************************************************************
 FILE         :   GuideMultiConsole.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Guide with multiconsole output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/19/2012

 LAST UPDATE  :   01/19/2012
*******************************************************************************/


#include "stdafx.h"

#include "IGuide.h"
#include "GuideMultiConsole.h"
#include "DLog.h"

#include <iomanip>
#include <boost/format.hpp>


using namespace Its;
using namespace DCommon;
using namespace std;

using boost::format;
using boost::io::group;



GuideMultiConsole::GuideMultiConsole():
  Base( INITIAL_PERIOD_STATISTIC, INITIAL_PERIOD_SHOWMAITRIX ),
  mconsole    ( 4, "Its"          ),
  colorText   ( color::white      ),
  colorData   ( color::lightcyan  ),
  colorControl( color::lightgray  ),
  colorInfo   ( color::lightgreen ),
  colorOutline( color::black, color::lightgray ),
  input       ( *this ), 
  timeTracker ( *this ),
  pageBasic   ( 0     ),
  pageStat    ( 1     ),
  pageControl ( 2     ),
  pageMessage ( 3     ),
  topLeft     ( 3, 3  ),
  bottomRight ( mconsole.Width() - 3, mconsole.Height() - 3 )
{
  if( IsThisSingleton() )
  {
    int       screenWidth  = mconsole.Width();
    int       screenHeight = mconsole.Height();
    VersionIt version      ( MaiTrix::Version() );
    format    headerFormat ( "DMA %1d.%1d.%1d %s page %1d of %1d " ); // Fixed 22 symbol format  

    clockPosition = pos( 2, mconsole.Height() - 1 );

    for( int pageNumber = 0; pageNumber < mconsole.Pages(); pageNumber++ ) 
    {
      string footer = string( screenWidth, ' ' );
      string header = str( headerFormat % version.Major % version.Minor % version.Build % 
                           group( showpos, setw( screenWidth - 23 ), " " ) % ( pageNumber + 1 ) % mconsole.Pages() );
      
      mconsole << page( pageNumber ) << pos( 0, 0                ) << colorOutline << header;
      mconsole << page( pageNumber ) << pos( 0, screenHeight - 1 ) << colorOutline << footer;
    }
    
    input.start();
    timeTracker.start();
    
    ShowMessage( "Do Guide..." );
  }
}


GuideMultiConsole::~GuideMultiConsole()
{
  if( IsThisSingleton() )
  {
    timeTracker.stop();
    input.stop(); 

    ShowMessage( "End." );
  }
}    


void GuideMultiConsole::ShowMaiTrix( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath )
{
  Base::ShowMaiTrix( maiTrix, maiTrixPath, maiTrixDebPath );

  mconsole << pageBasic << colorOutline << clockPosition << TimeStamp() << " Snapshot Taken To " << maiTrixDebPath;
}


void GuideMultiConsole::ShowBasicInfo( BasicInfo& basicInfo )
{
  SizeIt cyclesPerMin = 60.0 / basicInfo.seriesAverageTime;
  //
  // Note multipage output
  //
  mconsole << page( pageMessage );
  size_t text_size = ( mconsole.Width() * mconsole.Height()) / 4;

  ShowText( 0, 0,  "Base Message : ", basicInfo.baseMessage.substr( 0, text_size ));
  ShowText( 0, 7,  "Message      : ", basicInfo.message.substr( 0, text_size ));
  ShowText( 0, 14, "Response     : ", basicInfo.sins.response.substr( 0, text_size ));

  mconsole << pageBasic << colorText << Position( 0, 2 ) << "Basic info:";

  ShowText( 0, 4, "Size / Capacity                      : ", str( boost::format( "%d x %d x %d / %dT x %dA x %dV") %  
                                                                 basicInfo.maiTrixSize.X % 
                                                                 basicInfo.maiTrixSize.Y % 
                                                                 basicInfo.maiTrixSize.Z % 
                                                                 basicInfo.maiTrixCapacity.sizeText % 
                                                                 basicInfo.maiTrixCapacity.sizeAudio % 
                                                                 basicInfo.maiTrixCapacity.sizeVideo ));

  ShowText( 0,  5, "Repetition                          : ", str( boost::format( "%3d of %3d            " ) % basicInfo.lessonRepetition % basicInfo.lessonRepetitions ));
  //ShowText( 0,  6, "Sins, total-cardinals-top-expected  : ", str( boost::format( "%3d - %3d - %3d - %3d " ) % basicInfo.sins.total % basicInfo.sins.cardinals % basicInfo.sins.top % basicInfo.sins.expected ));
  ShowText( 0,  7, "Stability                           : ", str( boost::format( "%3d %%                " ) % basicInfo.sins.stability     ));
  //ShowText( 0,  6, "Corrections                         : ", str( boost::format( "%3d %%"          ) % basicInfo.averageCorrections ));
  //ShowText( 0,  7, "PQ                                  : ", str( boost::format( "%3d %%"          ) % basicInfo.pqEstimate         ));

  ShowText( 0,  8, "LsLevel                             : ", str( boost::format( "%3d of 10             " ) % basicInfo.lessonLevel        ));
  ShowText( 0,  9, "LsLength                            : ", str( boost::format( "%3d ticks total       " ) % basicInfo.lessonTotalLength  ));
  ShowText( 0, 10, "LsTopic                             : ", str( boost::format( "  %-20.20s            " ) % basicInfo.lessonTopic        ));
  
  //ShowText( 0, 11, "Cycle duration                      : ", str( boost::format( "%0#3.2d sec "    ) % basicInfo.seriesAverageTime  ));
  ShowText( 0, 11, "Cycles / min                        : ", str( boost::format( "%3d                   " ) % cyclesPerMin                 ));
  ShowText( 0, 12, "Statistics duration                 : ", str( boost::format( "%3.2f sec             " ) % basicInfo.statisticsTime     ));

  pos cycleCounterPosition      = clockPosition + pos( 52, 0 );
  pos effectiveInPeriodPosition = clockPosition + pos( 34, 0 );

  mconsole << colorOutline << cycleCounterPosition << str( boost::format( "Cycle %-5d" ) % basicInfo.cycle );
  mconsole << pageControl  << colorOutline << effectiveInPeriodPosition << str( boost::format( ", updated in %3d cycle(s)" ) % basicInfo.effectiveInPeriod );
}


void GuideMultiConsole::ShowStatistics( CubeStat& maiTrixStat )
{
  mconsole << pageStat << colorText << Position( 0, 0 ) << "Statistics collected:";

  ShowText( 0, 2,  "No       cells       : ", str( boost::format( "%3d %%, %6d"    ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsNo   ) % maiTrixStat.cellsNo   ));
  ShowText( 0, 3,  "Sin      cells       : ", str( boost::format( "%3d %%, %6d"    ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsSin  ) % maiTrixStat.cellsSin  ));
  ShowText( 0, 4,  "Good     cells       : ", str( boost::format( "%3d %%, %6d"    ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsGood ) % maiTrixStat.cellsGood ));
  ShowText( 0, 5,  "Bad      cells       : ", str( boost::format( "%3d %%, %6d"    ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsBad  ) % maiTrixStat.cellsBad  ));
  ShowText( 0, 6,  "Dead     cells       : ", str( boost::format( "%3d %%, %6d"    ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsBad  ) % maiTrixStat.cellsDead ));

  ShowText( 0, 8,  "Text     cells       : ", str( boost::format( "%3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOt ) % maiTrixStat.cellsIOt ));
  ShowText( 0, 9,  "Audio    cells       : ", str( boost::format( "%3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOa ) % maiTrixStat.cellsIOa ));
  ShowText( 0, 10, "Video    cells       : ", str( boost::format( "%3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOv ) % maiTrixStat.cellsIOv ));

  ShowText( 0, 12, "Cells total          : ", str( boost::format( "%-9d"           ) % maiTrixStat.CellsTotal()));
  ShowText( 0, 13, "Cells with data      : ", str( boost::format( "%3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsWithData            )));
  //ShowText( 0, 13, "Cells with no data   : ", str( boost::format( "%3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsWithNoData   )));
  //ShowText( 0, 14, "Cells with no signal : ", str( boost::format( "%3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsWithNoSignal )));
  //ShowText( 0, 15, "Cells above median   : ", str( boost::format( "%3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsAboveMedian  )));
  //ShowText( 0, 16, "Average cells level  : ", str( boost::format( "%3d"            ) % ( int ) maiTrixStat.levelAverage ));

  mconsole << pageStat << colorOutline << clockPosition << TimeStamp() << " Statistics Updated";
}


void GuideMultiConsole::ShowAndAdjust( AdjustmentInfo& currentInfo )
{
  ShowMessage( "Complete." );

  input.Update( currentInfo );

  switch( currentInfo.action )
  {
    case IGuide::None   : ShowMessage( "Do Guide... "  ); break;
    case IGuide::Pause  : ShowMessage( "Pausing...  "  ); break;
    case IGuide::Reboot : ShowMessage( "Rebooting..."  ); break;
    case IGuide::Void   : ShowMessage( "Voiding...  "  ); break;
  }
}


void GuideMultiConsole::ShowText( SHORT x, SHORT y, string label, string data )
{
  mconsole << Position( x, y ) << colorText << label << colorData << data;
}


void GuideMultiConsole::ShowMessage( const char* _message )
{
  int    viewWidth = pos::RelativeViewWidth( topLeft, bottomRight );
  string message( _message );

  message.resize( viewWidth, ' ' );

  mconsole << pageBasic << colorInfo << Position( 0, 0 ) << message;
}


pos GuideMultiConsole::Position( SHORT x, SHORT y )
{
  return Position( pos( x, y ) );
}


pos GuideMultiConsole::Position( pos p )
{
  // Boundary check and reposition relatively to topLeft
  pos relativePosition = topLeft + p;

  relativePosition.Bound( topLeft, bottomRight );

  return relativePosition;
}


string GuideMultiConsole::TimeToString( tm& time )
{
  // like strtime() runtime
  format timestampFormat( "%02d:%02d:%02d %02d/%02d/%4d" ); 
  string timeFormatted = str( timestampFormat % time.tm_hour % time.tm_min % time.tm_sec % ( time.tm_mon + 1 ) % time.tm_mday % ( time.tm_year + 1900 ));

  return timeFormatted;
}


string GuideMultiConsole::TimeStamp()
{
  time_t now = time( &now );
  tm*  local = localtime( &now );   

  return TimeToString( *local );
}

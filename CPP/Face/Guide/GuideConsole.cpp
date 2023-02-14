/*******************************************************************************
 FILE         :   GuideConsole.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Guide with console output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/18/2012

 LAST UPDATE  :   01/18/2012
*******************************************************************************/


#include "stdafx.h"

#include <iostream>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "IGuide.h"
#include "GuideConsole.h"
#include "DLog.h"



using namespace Its;
using namespace DCommon;

using boost::format;



GuideConsole::GuideConsole():
  Base()
{
  if( IsThisSingleton() )
  {
    cout << endl << "Do Guide..." << endl;
  }
}


GuideConsole::~GuideConsole()
{
  if( IsThisSingleton() )
  {
    cout << "End.";
  }
}    


void GuideConsole::ShowMaiTrix( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath )
{
  Base::ShowMaiTrix( maiTrix, maiTrixPath, maiTrixDebPath );

  cout << endl << "Snapshot mirrored, check " << maiTrixDebPath << endl;
}


void GuideConsole::ShowBasicInfo( BasicInfo& basicInfo )
{
  cout <<  endl;
  cout <<  boost::format( "Base Message                        [  %s  ]"             ) % basicInfo.baseMessage << endl;
  cout <<  boost::format( "Message                             [  %s  ]"             ) % basicInfo.message << endl;
  cout <<  boost::format( "Response                            [  %s  ]"             ) % basicInfo.sins.response << endl;
  cout <<  endl;
  cout <<  boost::format( "Size / Capacity                     : %d x %d x %d / %dT x %dA x %dV") % basicInfo.maiTrixSize.X  % basicInfo.maiTrixSize.Y % basicInfo.maiTrixSize.Z % 
                                                                                     basicInfo.maiTrixCapacity.sizeText % basicInfo.maiTrixCapacity.sizeAudio % basicInfo.maiTrixCapacity.sizeVideo << endl;
  cout <<  boost::format( "Stability                           : %3d %%"             ) % basicInfo.sins.stability << endl;

  cout <<  boost::format( "Repetition                          : %d in cycle %d"     ) % basicInfo.lessonRepetition % basicInfo.cycle << endl;
  cout <<  boost::format( "Cycle Length                        : %3.2d sec, average" ) % basicInfo.seriesAverageTime << endl;
  cout <<  boost::format( "Corrections                         : %3d %%"             ) % basicInfo.averageCorrections << endl;
  cout <<  boost::format( "PQ                                  : %d"                 ) % basicInfo.pqEstimate << endl;
  cout <<  boost::format( "LsLevel                             : %d of 10 "          ) % basicInfo.lessonLevel << endl;
  cout <<  boost::format( "LsLength                            : %d ticks total"     ) % basicInfo.lessonTotalLength << endl;
  cout <<  boost::format( "LsTopic                             [  %s  ]"             ) % basicInfo.lessonTopic << endl;
  cout <<  boost::format( "Statistics time                     : %3.2d "             ) % basicInfo.statisticsTime << endl;
}


void GuideConsole::ShowStatistics( CubeStat& maiTrixStat )
{
  SizeItl cellsTotal = maiTrixStat.CellsTotal();

  cout << endl << "Statistics collected:" << endl << endl;

  cout <<  boost::format( "No       cells           : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsNo   ) << endl;
  cout <<  boost::format( "Sin      cells           : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsSin  ) << endl;
  cout <<  boost::format( "Good     cells           : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsGood ) << endl;
  cout <<  boost::format( "Bad      cells           : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsBad  ) << endl;
  cout <<  boost::format( "Dead     cells           : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsDead ) << endl;
  cout <<  endl;
  cout <<  boost::format( "T    cells           : %3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOt ) % maiTrixStat.cellsIOt << endl;
  cout <<  boost::format( "A    cells           : %3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOa ) % maiTrixStat.cellsIOa << endl;
  cout <<  boost::format( "V    cells           : %3d %%, 2 x %d" ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsIOv ) % maiTrixStat.cellsIOv << endl;
  cout <<  endl;
  cout <<  boost::format( "Cells total          : %d"             ) % cellsTotal << endl;
  cout <<  endl;                                               
  cout <<  boost::format( "Cells with data      : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.cellsWithData   ) << endl;
  //cout <<  boost::format( "Cells with no data   : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsWithNoData   ) << endl;
  //cout <<  boost::format( "Cells with no signal : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsWithNoSignal ) << endl;
  //cout <<  boost::format( "Cells above median   : %3d %%"         ) % maiTrixStat.PercentageTotal( maiTrixStat.levelCellsAboveMedian  ) << endl;
  //cout <<  boost::format( "Average cells level  : %d"             ) % ( int ) maiTrixStat.levelAverage   << endl;
  cout << endl;
}


void GuideConsole::ShowAndAdjust( AdjustmentInfo& adjustmentInfo )
{
  cout << endl << endl << "Complete." << endl << endl;
  cout <<  boost::format( "PQ / VQ              : %d / %d"        ) % adjustmentInfo.pq % adjustmentInfo.vq << endl;
  cout <<  boost::format( "Delay / Repetitions  : %d / %d"        ) % adjustmentInfo.delay % adjustmentInfo.repetitions << endl;
  cout <<  boost::format( "Message              [  %s  ]"         ) % adjustmentInfo.message << endl;
  cout <<  boost::format( "Statistic period     [  %d  ]"         ) % adjustmentInfo.statisticPeriod << endl;
  cout <<  boost::format( "Snapshot period      [  %d  ]"         ) % adjustmentInfo.showMaiTrixPeriod << endl;

  cout << endl;
  cout << "Press 'p' to pause, 'r' to reboot, 'v' to void" << endl;
  cout << "Press 'c' to change delay & repetitions" << endl;

  adjustmentInfo.action = IGuide::None;
  string input;

  cin >> input;

  if( input == "p" )
  {
    cout << endl << "Pausing..." << endl; 
    adjustmentInfo.action = IGuide::Pause; 
  }
  else if( input == "r" )
  {
    cout << endl << "Rebooting..." << endl; 
    adjustmentInfo.action = IGuide::Reboot; 
  }
  else if( input == "v" )
  {
    cout << endl << "Voiding..." << endl; 
    adjustmentInfo.action = IGuide::Void; 
  }
  else if( input == "c" )
  {
    cout << endl << "Current delay: " << adjustmentInfo.delay << endl;
    cout << "Updated delay: "; 
    cin >> input;
    
    adjustmentInfo.delay = boost::lexical_cast< PeriodIt >( input );

    cout << endl << "Current repetitions: " << adjustmentInfo.repetitions << endl;
    cout << "Updated repetitions: " ; 
    cin >> input;
    
    adjustmentInfo.repetitions = boost::lexical_cast< PeriodIt >( input );
  }

}


void GuideConsole::ShowMessage( const char* message )
{
  cout << message << endl;
}


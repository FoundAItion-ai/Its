/*******************************************************************************
 FILE         :   IFeederCommon.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Common functions for all feeders (debug helpers, etc...)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   10/26/2011

 LAST UPDATE  :   10/26/2011
*******************************************************************************/

#include "stdafx.h"

#include "IFeederCommon.h"


using namespace Its;
using namespace DCommon;


DCriticalSection IFeederCommon::section;



IFeederCommon::~IFeederCommon()
{
  DLog::msg( DLog::LevelDebug, "Cleaning common feeder" );
}



void IFeederCommon::DebugInfo( const char* frameId, PeriodIt tick, IFrameSet& frameSet )
{
  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelVerbose, "" );
  DLog::msg( DLog::LevelVerbose, "Response content of the feeeder '%s' in tick '%d' and frame Id '%s' from MaiTrix:", Name(), tick, frameId );
  DLog::msg( DLog::LevelVerbose, "" );

  frameSet.DebugInfo();
}


void IFeederCommon::SetDimensions( const SizeIt2D & _dimensions )
{
  dimensions = _dimensions;
}




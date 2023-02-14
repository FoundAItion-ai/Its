/*******************************************************************************
 FILE         :   IFeederCommon.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Common functions for all feeders (debug helpers, etc...)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   10/26/2011

 LAST UPDATE  :   10/26/2011
*******************************************************************************/


#ifndef __IFEEDERCOMMON_H
#define __IFEEDERCOMMON_H


#include "IFeeder.h"


using namespace std;


namespace Its
{


class IFeederCommon : public virtual IFeeder
{
protected:
  virtual ~IFeederCommon();

  void DebugInfo( const char* frameId, PeriodIt tick, IFrameSet& frameSet );
  void SetDimensions( const SizeIt2D & dimensions ) override;

  static DCriticalSection section;
  SizeIt2D dimensions;
};



}; // namespace Its


#endif // __IFEEDERCOMMON_H

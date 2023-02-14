/*******************************************************************************
 FILE         :   GuideConsole.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Guide with console output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/18/2012

 LAST UPDATE  :   01/18/2012
*******************************************************************************/


#ifndef __GUIDECONSOLE_H
#define __GUIDECONSOLE_H


#include "Guide.h"


namespace Its
{


class GuideConsole: public Guide
{
public:
  typedef Guide Base;

  GuideConsole();
  virtual ~GuideConsole();

protected:
  virtual void ShowMaiTrix    ( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath );

  virtual void ShowMessage    ( const char* message );
  virtual void ShowStatistics ( CubeStat&       maiTrixStat     );
  virtual void ShowBasicInfo  ( BasicInfo&      basicInfo       );
  virtual void ShowAndAdjust  ( AdjustmentInfo& adjustmentInfo  );
};




}; // namespace Its


#endif // __GUIDECONSOLE_H

/*******************************************************************************
 FILE         :   GuideMultiConsole.h 

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Guide with multiconsole output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/19/2012

 LAST UPDATE  :   01/19/2012
*******************************************************************************/


#ifndef __GUIDEMULTICONSOLE_H
#define __GUIDEMULTICONSOLE_H


#include "Guide.h"
#include "View/MultiCon.h"
#include "GuideMultiConsoleThreads.h"


namespace Its
{

class InputThread;

class GuideMultiConsole: public Guide
{
public:
  typedef Guide Base;

  GuideMultiConsole();
  virtual ~GuideMultiConsole();

protected:
  friend InputThread;
  friend TimeTrackerThread;

  virtual void ShowMaiTrix    ( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath );

  virtual void ShowStatistics ( CubeStat&       maiTrixStat     );
  virtual void ShowBasicInfo  ( BasicInfo&      basicInfo       );
  virtual void ShowAndAdjust  ( AdjustmentInfo& adjustmentInfo  );

  virtual void ShowText( SHORT labelX, SHORT labelY, string label, string data );
  virtual void ShowMessage( const char* message );
  
  virtual pos  Position( SHORT x, SHORT y ); // Boundary check and reposition relatively to topLeft
  virtual pos  Position( pos p ); // -- " -- " -- 

  string TimeToString( tm& localTime );
  string TimeStamp();

private:
  TimeTrackerThread timeTracker;
  InputThread       input;
  MultiCon          mconsole;
  
  pos         topLeft;      // top left and bottom right corners of active console
  pos         bottomRight;
  pos         clockPosition;
  
  color       colorText;    // Combine into theme?
  color       colorData;    // statistic's data
  color       colorControl;
  color       colorInfo;    // system message
  color       colorOutline;
  
  page        pageBasic;
  page        pageStat;
  page        pageControl;
  page        pageMessage;
};




}; // namespace Its


#endif // __GUIDEMULTICONSOLE_H

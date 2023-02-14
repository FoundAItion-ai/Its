/*******************************************************************************
 FILE         :   GuideMultiConsoleThreads.h 

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Internal threads for guide with multiconsole output/control

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/31/2012

 LAST UPDATE  :   01/31/2012
*******************************************************************************/


#ifndef __GUIDEMULTICONSOLETHREADS_H
#define __GUIDEMULTICONSOLETHREADS_H


#include "Guide.h"
#include "View/MultiCon.h"


namespace Its
{


class GuideMultiConsole;


class InputThreadLeaveException: public IGuideException
{
public:
  InputThreadLeaveException(): 
    IGuideException( "Request to leave." ) {}

  InputThreadLeaveException( const char* message ): 
    IGuideException( message ) {}
};


class InputThread: public DThread
{
public:
  InputThread( GuideMultiConsole& _guide );

  // Called from another thread
  void Update( Guide::AdjustmentInfo& currentInfo );

protected:
  virtual void run();
  virtual void runThread();

  void    ShowAdjustment( Guide::AdjustmentInfo& adjustment, bool current );
  
  string  ActionToText( IGuide::GuideAction_t action );
  void    TextToAction( string actionText, IGuide::GuideAction_t& action );
  
  // Thread safe read text/number with highlighting and check for leave request
  void    ReadText( pos position, string& text );
  
  template<typename T> 
  FieldFocus ReadNumber( pos position, T& number, int fieldSize, long lowerBound, long upperBound );
   
private:
  static DCriticalSection section;
  Guide::AdjustmentInfo   adjustedInfo;
  GuideMultiConsole&      guide;
  SizeIt                  maiTrixSizeZ;
};


class TimeTrackerThread: public DThread
{
public:
  TimeTrackerThread( GuideMultiConsole& _guide );

protected:
  virtual void run();
  
  pos                clockPosition;
  GuideMultiConsole& guide;  
};

}; // namespace Its


#endif // __GUIDEMULTICONSOLETHREADS_H

/*******************************************************************************
 FILE         :   DLog.h

 COPYRIGHT    :   DMAlex, 2008

 DESCRIPTION  :   Log file

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/05/2008

 LAST UPDATE  :   09/05/2008
*******************************************************************************/


#ifndef __DLOG_H
#define __DLOG_H

#include <memory>
#include "DLogC.h"


namespace DCommon
{

class DCriticalSection;


class ITS_SPEC DLog
{
public:
  static const int LevelVerbose  = DLog_LevelDebug + 0;
  static const int LevelDebug    = DLog_LevelDebug + 1;
  static const int LevelInfo     = DLog_LevelDebug + 2;
  static const int LevelWarning  = DLog_LevelDebug + 3;
  static const int LevelError    = DLog_LevelDebug + 4;
  static const int LevelNoLog    = DLog_LevelDebug + 5;

  static void init( const char *logFilename, int minLevel );
  static void msg( int level, const char *format, ... );
  static int  level();
  
protected:
  static void msgFmt( int level, const char *format, va_list args );

  friend void DCommon::DLog_msg( int level, const char *format, ... );

private:
  static std::auto_ptr<std::ostream> streamLog;
  static DCriticalSection cSection; 
  static int minLevel;
};

}; // namespace DCommon

#endif // __DLOG_H

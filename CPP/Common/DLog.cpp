/*******************************************************************************
 FILE         :   DLog.h

 COPYRIGHT    :   DMAlex, 2008

 DESCRIPTION  :   Log file

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/05/2008

 LAST UPDATE  :   09/05/2008
*******************************************************************************/


#include "stdafx.h"

#include <fstream>
#include <time.h>
#include <stdarg.h>
#include <assert.h>



using namespace std;

namespace DCommon
{


ITS_SPEC auto_ptr<ostream> DLog::streamLog;
ITS_SPEC DCriticalSection  DLog::cSection; 
ITS_SPEC int DLog::minLevel = DLog::LevelError;



void DLog::init( const char *logFilename, int _minLevel )
{
  if( _minLevel == DLog::LevelNoLog ) return;
  
  DCriticalSection::Lock cSection( cSection );
  streamLog = auto_ptr<ostream>( new ofstream( logFilename, ios_base::out | ios_base::trunc ));
  
  minLevel = _minLevel;
  msg( 0, "Log file initialized." );
}


void DLog::msgFmt( int level, const char *format, va_list args )
{
  DCriticalSection::Lock cSection( cSection );
  int  nBuf;
  char szBuffer  [ 1024 ];
  char timestamp [ 1024 ];

  nBuf = _vsnprintf( szBuffer, sizeof( szBuffer ), format, args );

  time_t t        = time( 0 );
  struct tm * ts  = localtime( &t );
  DWORD  threadID = GetCurrentThreadId();
  
  sprintf( timestamp, "%02d.%02d.%4d %02d:%02d:%02d  [ %2d, %5d ] ",  ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900,
                                                                      ts->tm_hour, ts->tm_min, ts->tm_sec, level, threadID );
  szBuffer  [ 1024 - 1 ] = 0;
  timestamp [ 1024 - 1 ] = 0;

  // Output truncated 
  if( nBuf == -1 )
  {
    *streamLog.get() << timestamp << "  Next line has been truncated, check formatting:" << endl << std::flush;
  }

  *streamLog.get() << timestamp << "  " << szBuffer << endl << std::flush;
}


void DLog::msg( int level, const char *format, ... )
{
  if( level < minLevel ) return;
    
  va_list args;
  va_start(args, format);

  msgFmt( level, format, args );

  va_end(args);
}
  


int DLog::level() 
{ 
  return minLevel; 
}


// just a C-style call to DLog::msg()
void DLog_msg( int level, const char *format, ... )
{
  if( level < DCommon::DLog::minLevel ) return;
    
  va_list args;
  va_start(args, format);

  DLog::msgFmt( level, format, args );

  va_end(args);
}


}; // namespace DCommon
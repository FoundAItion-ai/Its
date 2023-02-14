/*******************************************************************************
 FILE         :   DException.h 

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Base formatted exception

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   10/06/2011

 LAST UPDATE  :   10/06/2011
*******************************************************************************/


#ifndef __DEXCEPTION_H
#define __DEXCEPTION_H

#include <string>
#include <exception>
#include <assert.h>
#include <stdarg.h>


using namespace std;



namespace DCommon
{



#define DEXCEPTION_CONSTRUCTOR0( classname, baseclassname )                 \
  classname( const char* message ):                                         \
    baseclassname( message )                                                \
  {                                                                         \
    SetLevel( Error );                                                      \
  }                                                                        
                                                            

#define DEXCEPTION_CONSTRUCTOR0_EX( classname, baseclassname, extra )       \
  classname( const char* message ):                                         \
    baseclassname( message )                                                \
  {                                                                         \
    SetLevel( Error );                                                      \
    extra;                                                                  \
  }                                                         
                                                            

// Level passed second to prevent "ambiguous call to overloaded function"
//
#define DEXCEPTION_CONSTRUCTOR1( classname, baseclassname )                 \
  classname( const char* message, int _level ):                             \
    baseclassname( message )                                                \
  {                                                                         \
    SetLevel( _level );                                                     \
  }                                                         
                                                            

#define DEXCEPTION_CONSTRUCTOR1_EX( classname, baseclassname, extra )       \
  classname( const char* message, int _level ):                             \
    baseclassname( message )                                                \
  {                                                                         \
    SetLevel( _level );                                                     \
    extra;                                                                  \
  }                                                         
                                                            

// Return format parameter in comma operator as can not return va_start() result 
// on 64 bit platform but we need some dummy result, will be discarded anyway
//
#define DEXCEPTION_CONSTRUCTOR2( classname, baseclassname )                 \
  classname( int _level, const char *format, ... ):                         \
    baseclassname( formattedMessage( _level, 0, 0, format,                  \
                 ( va_start( formatArguments, format ), ( void* )format ))) \
  {                                                                         \
    va_end( formatArguments );                                              \
    SetLevel( _level );                                                     \
  }


#define DEXCEPTION_CONSTRUCTOR2_EX( classname, baseclassname, extra )       \
  classname( int _level, const char *format, ... ):                         \
    baseclassname( formattedMessage( _level, 0, 0, format,                  \
                 ( va_start( formatArguments, format ), ( void* )format ))) \
  {                                                                         \
    va_end( formatArguments );                                              \
    SetLevel( _level );                                                     \
    extra;                                                                  \
  }


#define DEXCEPTION_CONSTRUCTOR3( classname, baseclassname )                 \
  classname( int _level, const char *fileName, int line,                    \
             const char *format, ... ):                                     \
    baseclassname( formattedMessage( _level, fileName, line, format,        \
                 ( va_start( formatArguments, format ), ( void* )format ))) \
  {                                                                         \
    va_end( formatArguments );                                              \
    SetLevel( _level );                                                     \
  }


#define DEXCEPTION_CONSTRUCTOR3_EX( classname, baseclassname, extra )       \
  classname( int _level, const char *fileName, int line,                    \
             const char *format, ... ):                                     \
    baseclassname( formattedMessage( _level, fileName, line, format,        \
                 ( va_start( formatArguments, format ), ( void* )format ))) \
  {                                                                         \
    va_end( formatArguments );                                              \
    SetLevel( _level );                                                     \
    extra;                                                                  \
  }



class ITS_SPEC DException: public std::exception
{
public:
  DEXCEPTION_CONSTRUCTOR0( DException, exception );
  DEXCEPTION_CONSTRUCTOR1( DException, exception );
  DEXCEPTION_CONSTRUCTOR2( DException, exception );
  DEXCEPTION_CONSTRUCTOR3( DException, exception );

  static const int Warning        = 0;
  static const int Error          = 1;
  static const int CriticalError  = 2;

  int Level() { return level; }

  // Translates system errors from GetLastError()
  // into meaningful messages
  static string TranslateSystemMessage( DWORD errorCode ); 

protected:
  va_list formatArguments; // careful, protected member

  void SetLevel( int _level ) { level = _level; }

  const char* formattedMessage( int level, const char *fileName, int line, const char *format, void* dummy );


private:
  static const int bufferSize = 1024;
  char   formatBuffer[ bufferSize ];
  int    level;
};




}; // namespace DCommon


#endif // __DEXCEPTION_H

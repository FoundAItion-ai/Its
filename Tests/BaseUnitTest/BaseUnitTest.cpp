/*******************************************************************************
 FILE         :   BaseUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - base unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   04/17/2014
*******************************************************************************/


#include "stdafx.h"
#include <stdarg.h>
#include <string>

#include "BaseUnitTest.h"
#include "DCommonIncl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;
using namespace DCommon;


// not thread-safe funcions
namespace
{

const int buffest_size = 1024;

void WriteMessage( const char *message )
{
  Logger::WriteMessage(( string( message ) + "\n" ).c_str() );
  DLog::msg( DLog::LevelInfo, "UnitTest: %s", message );
}

void WriteMessage( const wchar_t *message )
{
  Logger::WriteMessage(( wstring( message ) + L"\n" ).c_str() );
  // 8 Jul 2014 - DMA - TODO: add DLog::msg() for wide chars?
}

void WriteMessage( const char *format, va_list args )
{
  char buffer[ buffest_size ];
  
  if( _vsnprintf_s( buffer, sizeof( buffer ), _TRUNCATE, format, args ) == -1 )
  {
    Logger::WriteMessage( "Next message is truncated\n" );
  }

  WriteMessage( buffer );
}

void WriteMessage( const wchar_t *format, va_list args )
{
  wchar_t buffer[ buffest_size ];
  
  if( _vsnwprintf_s( buffer, sizeof( buffer ), _TRUNCATE, format, args ) == -1 )
  {
    Logger::WriteMessage( L"Next message is truncated\n" );
  }

  WriteMessage( buffer );
}

}


namespace BaseUnitTest
{

BEGIN_TEST_MODULE_ATTRIBUTE()
  TEST_MODULE_ATTRIBUTE( L"Date", L"08/07/2013" )
  TEST_MODULE_ATTRIBUTE( L"Name", L"Base unit tests" )
END_TEST_MODULE_ATTRIBUTE()

TEST_MODULE_INITIALIZE(ModuleInitialize)
{
  ::WriteMessage( "Base unit tests module initialize" );
  DLog::init( "MaiTrix_UnitTest.log", DLog::LevelDebug );
}

TEST_MODULE_CLEANUP(ModuleCleanup)
{
  ::WriteMessage( "Base unit tests module cleanup" );
}


BaseTest::BaseTest() 
{
}

// static 
void WriteMessage( const char *message )
{
  ::WriteMessage( message );
}

// static 
void WriteMessage( const wchar_t *message )
{
  ::WriteMessage( message );
}

// static 
void BaseTest::WriteMessage( const char *format, ... )
{
  if( string( format ).find( "%" ) == string::npos )
  {
    ::WriteMessage( format );
  }
  else
  {
    va_list args;
    va_start( args, format );

    ::WriteMessage( format, args );

    va_end( args );  
  }
}

// static 
void BaseTest::WriteMessage( const wchar_t *format, ... )
{
  if( wstring( format ).find( L"%" ) == string::npos )
  {
    ::WriteMessage( format );
  }
  else
  {
    va_list args;
    va_start( args, format );

    ::WriteMessage( format, args );

    va_end( args );  
  }
}

} // BaseUnitTest
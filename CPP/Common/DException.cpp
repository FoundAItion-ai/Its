/*******************************************************************************
 FILE         :   DException.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Base formatted exception

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   10/06/2011

 LAST UPDATE  :   10/06/2011
*******************************************************************************/


#include "stdafx.h"
#include "DException.h"

#include <assert.h>
#include <errno.h>


using namespace std;


namespace DCommon
{



const char* DException::formattedMessage( int level, const char *fileName, int line, const char *format, void* dummy )
{
  string  formattedString;
  int     writtenChars;

  writtenChars = _vsnprintf_s( formatBuffer, bufferSize, _TRUNCATE, format, formatArguments );
  assert( bufferSize != -1 ); // Output truncated or other error?

  formattedString = formatBuffer;

  if( fileName != 0 )
  {
    writtenChars = _snprintf_s( formatBuffer, bufferSize, _TRUNCATE, ", severity level %d, file '%s' in line %i", level, fileName, line );
    assert( bufferSize != -1 ); // Output truncated or other error?
  }
  else
  {
    writtenChars = _snprintf_s( formatBuffer, bufferSize, _TRUNCATE, ", severity level %d", level );
    assert( bufferSize != -1 ); // Output truncated or other error?
  }

  formattedString.append( formatBuffer );

  errno_t copyError = strncpy_s( formatBuffer, bufferSize, formattedString.c_str(), _TRUNCATE );
  assert( copyError == 0 || copyError == STRUNCATE ); // truncation allowed here

  return formatBuffer;
}



string DException::TranslateSystemMessage( DWORD errorCode )
{
  char   buffer[ bufferSize ];
  DWORD  strLen = 0;
  string message;

  if(( strLen = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0, buffer, bufferSize, NULL )) > 2 )
  {
    buffer[ strLen - 2 ] = 0; // remove CR/LF
    
    message = buffer;
  }
  else
  {
    message = _ltoa( errorCode, buffer, 10 );
  }
  
  return message;
}


}; // namespace DCommon


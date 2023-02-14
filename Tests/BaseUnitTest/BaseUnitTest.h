/*******************************************************************************
 FILE         :   BaseUnitTest.h

 COPYRIGHT    :   DMAlex, 2014

 DESCRIPTION  :   Base unit test class

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   06/12/2014

 LAST UPDATE  :   06/12/2014
*******************************************************************************/


#ifndef __BASEUNITTEST_H
#define __BASEUNITTEST_H

#include <time.h>

namespace BaseUnitTest
{
  class BaseTest
  {
  public:
    BaseTest();

    static void WriteMessage( const wchar_t *format, ... );
    static void WriteMessage( const char *format, ... );

  protected:
    // Single inputs with MaiTrix::ReadResponse() logic may only produce response in this range
    // While in reality all 255 ascii chars are probable.
    static const unsigned char responseCharLow   = 0x30; // '0';
    static const unsigned char responseCharHigh  = responseCharLow + 0x80;
    static const unsigned char responseCharSpace = ' ';
  };

}; // namespace Its


#endif // __BASEUNITTEST_H

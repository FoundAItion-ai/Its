/*******************************************************************************
 FILE         :   Its.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Entry point

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/09/2012

 LAST UPDATE  :   01/09/2012
*******************************************************************************/


#include "stdafx.h"
#include <iostream>

#include "GuideConsole.h"
#include "GuideMultiConsole.h"


using namespace Its;


int _tmain( int argc, _TCHAR* argv[] )
{
  try
  {
    GuideMultiConsole guide;
    guide.Its();
  }
  catch( exception& x )
  {
    cout << "Failed: " << x.what() << endl;
  }
  catch( ... )
  {
    cout << "Failed" << endl;
  }

  DWORD lastError = GetLastError();
  if( lastError != ERROR_SUCCESS )
  {
    cout << DException::TranslateSystemMessage( lastError ) << endl;
  }
  return 0;
}


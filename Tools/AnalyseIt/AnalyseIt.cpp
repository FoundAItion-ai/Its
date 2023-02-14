/*******************************************************************************
FILE         :   AnalyseIt.cpp 

COPYRIGHT    :   DMAlex, 2014

DESCRIPTION  :   Its - tool for Maitrix analysis

PROGRAMMED BY:   Alex Fedosov

CREATION DATE:   04/28/2014

LAST UPDATE  :   04/28/2014
*******************************************************************************/

#include "stdafx.h"
#include <iostream>

#pragma warning(disable : 4996)

#include "Internals.h"

using namespace std;
using namespace Its;


int _tmain( int argc, _TCHAR* argv[] )
{
  wstring maiTrixNameWide( L"Its" );
  wstring maiTrixPathWide( L".\\dma" );

  if( argc == 1 )
  {
    cout << "Usage: AnalyseIt.exe [maiTrixName] [maiTrixPath]" << endl;
    cout << "Usage: AnalyseIt.exe Its .\\dma" << endl;
  }

  if( argc > 1 )
  {
    maiTrixNameWide = argv[ 1 ];
  }

  if( argc > 2 )
  {
    maiTrixPathWide = argv[ 2 ];
  }

  string maiTrixName( maiTrixNameWide.begin(), maiTrixNameWide.end() );
  string maiTrixPath( maiTrixPathWide.begin(), maiTrixPathWide.end() );
  string snapshotPath( ".\\deb" );
  bool autoCorrectionEnabled = true;

  string logFilePath( snapshotPath );
  logFilePath += "\\AnalyseIt.log";

  DLog::init( logFilePath.c_str(), DLog::LevelVerbose );

  try
  {
    MaiTrixSnapshot snapshot( maiTrixName.c_str(), maiTrixPath.c_str(), autoCorrectionEnabled );
    snapshot.Take( snapshotPath.c_str() );  
  }
  catch( exception& x )
  {
    cout << "Error:" << x.what() << endl;
    DLog::msg( DLog::LevelError, "Error: %s", x.what() );
  }
  catch( ... )
  {
    cout << "General error" << endl;
    DLog::msg( DLog::LevelError, "General error" );
  }
 

  return 0;
}


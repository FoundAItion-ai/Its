/*******************************************************************************
FILE         :   IGuidex.h

COPYRIGHT    :   DMAlex, 2018

DESCRIPTION  :   Integration point for everything, extended

PROGRAMMED BY:   Alex Fedosov

CREATION DATE:   08/29/2018

LAST UPDATE  :   08/29/2018
*******************************************************************************/


#include "stdafx.h"
#include "IGuidex.h"
#include "Internals.h"

using namespace std;
using namespace Its;

void IGuideX::TakeSnapshot()
{
  if( oneMaiTrix.get())
  {
    oneMaiTrix->Commit();
  }
  TakeSnapshot( MaiTrixName, MaiTrixPath, MaiTrixDebPath );
}


// static
void IGuideX::TakeSnapshot( const char* maiTrixName, const char* maiTrixPath, const char* snapshotPath )
{
  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelInfo, "Taking snapshot of maiTrix %s", maiTrixName );
  MaiTrixSnapshot snapshot( maiTrixName, maiTrixPath );
  snapshot.Take( snapshotPath );
}


IGuideX::IGuideX(): IGuide()
{  
}


IGuideX::~IGuideX()
{  
}


/*******************************************************************************
 FILE         :   BasicCubeEngine.h 

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Basic Cube engine for desktop 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/02/2011

 LAST UPDATE  :   08/02/2011
*******************************************************************************/


#ifndef __BASICCUBEENGINE_H
#define __BASICCUBEENGINE_H


// No namespaces, C-style
// Could be used from C#/Java, etc
//

#include "BasicCubeDefines.h"

extern "C" 
{
  enum BasicResponseCode
  {
    BASICCUBE_CODE_OK,
    BASICCUBE_CODE_NOMEMORY,
    BASICCUBE_CODE_ERROR
  };

  int BASICCUBE_SPEC BasicCubeSet( int sizeX, int sizeY );

  int BASICCUBE_SPEC BasicCubeLaunch( void* data, void* dataU, void* dataD, void* dataOut, int gf, int rf, void* probe );
}


#endif // __BASICCUBEENGINE_H

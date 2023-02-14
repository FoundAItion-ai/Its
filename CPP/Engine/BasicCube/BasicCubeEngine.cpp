/*******************************************************************************
 FILE         :   CudaCube.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Loading and executing Cubes on GPU/Cuda (C-Style)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   07/17/2011
*******************************************************************************/

#include "stdafx.h"

#include <exception>
#include <memory>
#include <list>

#include "BasicCube.h"
#include "BasicCubeEngine.h"
#include "BasicCubeManager.h"


using namespace Its;
using namespace std;


#define CUBE_PROLOG  int cubeResponseCode;                  \
  try                                                       \
  {

    
#define CUBE_EPILOG  cubeResponseCode = BASICCUBE_CODE_OK;  \
  }                                                         \
  catch( bad_alloc& )                                       \
  {                                                         \
    cubeResponseCode = BASICCUBE_CODE_NOMEMORY;             \
  }                                                         \
  catch( ... )                                              \
  {                                                         \
    cubeResponseCode = BASICCUBE_CODE_ERROR;                \
  }                                                         \
  return cubeResponseCode;




auto_ptr< BasicCubeManager > OneBasicCubeManager;



int BasicCubeSet( int sizeX, int sizeY )
{
  CUBE_PROLOG;

  OneBasicCubeManager = auto_ptr< BasicCubeManager > ( new BasicCubeManager( sizeX, sizeY ));

  CUBE_EPILOG;
}



int BasicCubeLaunch( void* data, void* dataU, void* dataD, void* dataOut, int gf, int rf, void* probe )
{
  CUBE_PROLOG;
  
  if( OneBasicCubeManager.get())
  {
    OneBasicCubeManager->Launch( (Cell*)data, (Cell*)dataU, (Cell*)dataD, (Cell*)dataOut, gf, rf, (Probe*)probe );
  }

  CUBE_EPILOG;
}


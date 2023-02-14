/*******************************************************************************
 FILE         :   CudaCube.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Loading and executing Cubes on GPU/Cuda (C-Style)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   07/17/2011
*******************************************************************************/

#include "stdafx.h"

#include "memory.h"

#include "CudaCube.h"
#include "CubeManager.h"


using namespace Its;
using namespace std;


#define CUBE_PROLOG  int cubeResponseCode;              \
  try                                                   \
  {

    
#define CUBE_EPILOG  cubeResponseCode = CUBE_CODE_OK;   \
  }                                                     \
  catch( CubeException& e )                             \
  {                                                     \
    if( e.GetCudaError() == CUDA_ERROR_OUT_OF_MEMORY )  \
      cubeResponseCode = CUBE_CODE_NOMEMORY;            \
    else                                                \
      cubeResponseCode = CUBE_CODE_CUDA_ERROR;          \
  }                                                     \
  catch( ... )                                          \
  {                                                     \
    cubeResponseCode = CUBE_CODE_ERROR;                 \
  }                                                     \
  return cubeResponseCode;



auto_ptr< CubeManager > OneCubeManager;


int InitKernel( int blockSizeX, int blockSizeY, int blockSizeZ )
{
  CUBE_PROLOG;
  
  OneCubeManager = auto_ptr< CubeManager > ( new CubeManager( blockSizeX, blockSizeY, blockSizeZ ));
  
  CUBE_EPILOG;
}


int CloseKernel()
{
  CUBE_PROLOG;
  
  delete OneCubeManager.release();
  
  CUBE_EPILOG;
}


int GetDeviceCount( int &deviceCount )
{
  CUBE_PROLOG;
  
  deviceCount = OneCubeManager->GetDeviceCount();
  
  CUBE_EPILOG;
}


int GetDeviceInfo( int deviceOrdinal,  int& majorVersion,     int& minorVersion, 
                   size_t& memorySize, int& sharedMemorySize, int& multiProcessorCount )
{
  CUBE_PROLOG;
  
  OneCubeManager->GetDeviceInfo( deviceOrdinal, majorVersion,     minorVersion, 
                                 memorySize,    sharedMemorySize, multiProcessorCount );
  
  CUBE_EPILOG;
}


int CubeSet( int deviceOrdinal, int sizeX, int sizeY ) // recreate context and reallocate memory
{
  CUBE_PROLOG;
  
  OneCubeManager->CubeSet( deviceOrdinal, sizeX, sizeY );

  CUBE_EPILOG;
}


int CubeLaunch( int deviceOrdinal, void* data, void* dataU, void* dataD, void* dataOut, int gf, int rf, void* probe )
{
  CUBE_PROLOG;
  
  OneCubeManager->CubeLaunch( deviceOrdinal, (Cell*)data, (Cell*)dataU, (Cell*)dataD, (Cell*)dataOut, gf, rf, (Probe*)probe );
  
  CUBE_EPILOG;
}


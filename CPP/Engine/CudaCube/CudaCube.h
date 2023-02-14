/*******************************************************************************
 FILE         :   CudaCube.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Loading and executing Cubes on GPU/Cuda (C-Style)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   07/17/2011
*******************************************************************************/


#ifndef __CUDACUBE_H
#define __CUDACUBE_H

#include "CudaCubeDefines.h"


// No namespaces, C-style
// Could be used from C#/Java, etc
//


extern "C" 
{
  enum ResponseCode
  {
    CUBE_CODE_OK,
    CUBE_CODE_NOMEMORY,
    CUBE_CODE_CUDA_ERROR,
    CUBE_CODE_ERROR
  };

  int  CUBE_SPEC InitKernel( int blockSizeX, int blockSizeY, int blockSizeZ );
  int  CUBE_SPEC CloseKernel();

  int  CUBE_SPEC GetDeviceCount( int &deviceCount );
  int  CUBE_SPEC GetDeviceInfo ( int deviceOrdinal,  int& majorVersion,     int& minorVersion, 
                                 size_t& memorySize, int& sharedMemorySize, int& multiProcessorCount );

  int  CUBE_SPEC CubeSet    ( int deviceOrdinal, int sizeX,  int sizeY ); // recreate context and reallocate memory
  int  CUBE_SPEC CubeLaunch ( int deviceOrdinal, void* data, void* dataU, void* dataD, void* dataOut, int gf, int rf, void* probe );
}


#endif // __CUDACUBE_H

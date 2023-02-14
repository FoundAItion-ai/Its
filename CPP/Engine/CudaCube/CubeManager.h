/*******************************************************************************
 FILE         :   CubeManager.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Loading and executing Cubes on GPU/Cuda 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   07/17/2011
*******************************************************************************/


#ifndef __CUBEMANAGER_H
#define __CUBEMANAGER_H

#include <map>
#include <cuda.h>

#include "DCommonIncl.h"
#include "Cube.h"
#include "CudaCubeDefines.h"
#include "CubeException.h"
#include "CUDALoader.h"


namespace Its
{

using namespace DCommon;

#define NOCUDADEVICE  ( -1 )

// 16 Jul 2013 - DMA - Removing spared 2x space
//
// #define CubeRequiredSharedMemory ( 2 * ( HyperCubeSize * HyperCubeSize * 3 * sizeof( Cell )))
// #define CubeRequiredSharedMemory ( HyperCubeSize * HyperCubeSize * 3 * sizeof( Cell ))
//
// 10 Sep 2013 - DMA - Do not use shared memory anymore
// 
#define CubeRequiredSharedMemory 0


struct Probe;


class CUBE_SPEC CubeManager
{
public:
  CubeManager( int blockSizeX, int blockSizeY, int blockSizeZ );
  ~CubeManager();

  int  GetDeviceCount();
  void GetDeviceInfo( int deviceOrdinal,  int& majorVersion,     int& minorVersion, 
                      size_t& memorySize, int& sharedMemorySize, int& multiProcessorCount );

  void CubeSet    ( int deviceOrdinal, int sizeX,  int sizeY ); // recreate context and reallocate memory
  void CubeLaunch ( int deviceOrdinal, Cell* data, Cell* dataU, Cell* dataD, Cell* dataOut, int gf, int rf, Probe* probe );
 
protected:
  bool AllocateMemoryCheck( CUdeviceptr& ptr, size_t size );
  void DeAllocateMemoryCheck( CUdeviceptr ptr );

private:
  struct CubeContext
  {
    CUdevice    cuDevice;
    CUcontext   cuContext;
    CUmodule    cuModule;
    CUfunction  cuFunction;
    CUdeviceptr cuDataIn;
    CUdeviceptr cuDataInU;
    CUdeviceptr cuDataInD;
    CUdeviceptr cuDataOut;
    CUdeviceptr cuProbe;
    int         sizeX;
    int         sizeY;
  };

  // device ordinal -> context
  typedef std::map< int, CubeContext* > CubeContextMap;


  static DCriticalSection section;
  static CubeManager*     instance;
  CubeContextMap          contexts;

  CUDALoader              cudaLoader;
  int                     blockSizeX;
  int                     blockSizeY;
  int                     blockSizeZ;
};


}; // namespace Its

#endif // __CUBEMANAGER_H

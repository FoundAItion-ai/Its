/*******************************************************************************
 FILE         :   CubeKernel.cu 

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Kernel (GPU) for Cube 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/22/2011

 LAST UPDATE  :   07/22/2011
*******************************************************************************/



#include <cuda.h>         

#include "Cube.h"
#include "BasicCube.h"

 
using namespace Its;

__device__  SizeIt GetIndexX()
{
  return blockIdx.x * blockDim.x + threadIdx.x; 
}

__device__  SizeIt GetIndexY()
{
  return blockIdx.y * blockDim.y + threadIdx.y; 
}


extern "C" __global__ void CubeEntry( int gf, int random,
                                      void* dataIn,  void* dataInU, void* dataInD, 
                                      void* dataOut, int sizeX, int sizeY, Probe* probe ) 
{
  PointIt size      ( sizeX       , sizeY       );
  PointIt index     ( GetIndexX() , GetIndexY() );
  PointIt position  ( threadIdx.x , threadIdx.y );

  BasicCube cube( dataIn, dataInU, dataInD,                    
                  size,   index,   position
                  );


  __syncthreads();

  cube.It( dataOut, gf, random, probe );
}


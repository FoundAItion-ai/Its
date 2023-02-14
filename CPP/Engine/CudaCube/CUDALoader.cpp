/*******************************************************************************
 FILE         :   CUDALoader.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Loading Cuda functionl dynamically

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   03/23/2012

 LAST UPDATE  :   03/23/2012
*******************************************************************************/


#include "stdafx.h"

#include "CubeException.h"
#include "CUDALoader.h"


using namespace Its;


CUDALoader::CUDALoader()
{
  if( !LoadCudaLibrary())
  {
    throw CubeException( DException::Error, "Can not load CUDA library, system level error [ %s ]", DException::TranslateSystemMessage( GetLastError() ).c_str());  
  }
}



bool CUDALoader::LoadCudaLibrary()
{
  bool cudaLoaded = false;
  
  // 26 Mar 2012 - DMA - TODO:
  // loading NVidia 64 bit dll is not tested yet (same name of the DLL?)
  //
  // http://developer.download.nvidia.com/compute/cuda/4_1/rel/drivers/devdriver_4.1_winvista-win7_64_286.19_general.exe
  //
  HMODULE cudaModule = LoadLibrary( "nvcuda.dll" );

  if( cudaModule )
  {
    cudaLoaded = true;

    cudaLoaded = cudaLoaded && (( pcuCtxCreate                = ( FUNC_cuCtxCreate                ) GetProcAddress( cudaModule, "cuCtxCreate"               )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuCtxDestroy               = ( FUNC_cuCtxDestroy               ) GetProcAddress( cudaModule, "cuCtxDestroy"              )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuCtxSetCurrent            = ( FUNC_cuCtxSetCurrent            ) GetProcAddress( cudaModule, "cuCtxSetCurrent"           )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuCtxSynchronize           = ( FUNC_cuCtxSynchronize           ) GetProcAddress( cudaModule, "cuCtxSynchronize"          )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceComputeCapability  = ( FUNC_cuDeviceComputeCapability  ) GetProcAddress( cudaModule, "cuDeviceComputeCapability" )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceGet                = ( FUNC_cuDeviceGet                ) GetProcAddress( cudaModule, "cuDeviceGet"               )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceGetAttribute       = ( FUNC_cuDeviceGetAttribute       ) GetProcAddress( cudaModule, "cuDeviceGetAttribute"      )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceGetCount           = ( FUNC_cuDeviceGetCount           ) GetProcAddress( cudaModule, "cuDeviceGetCount"          )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceGetName            = ( FUNC_cuDeviceGetName            ) GetProcAddress( cudaModule, "cuDeviceGetName"           )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuDeviceTotalMem           = ( FUNC_cuDeviceTotalMem           ) GetProcAddress( cudaModule, "cuDeviceTotalMem"          )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuFuncSetBlockShape        = ( FUNC_cuFuncSetBlockShape        ) GetProcAddress( cudaModule, "cuFuncSetBlockShape"       )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuFuncSetSharedSize        = ( FUNC_cuFuncSetSharedSize        ) GetProcAddress( cudaModule, "cuFuncSetSharedSize"       )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuInit                     = ( FUNC_cuInit                     ) GetProcAddress( cudaModule, "cuInit"                    )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuLaunchGrid               = ( FUNC_cuLaunchGrid               ) GetProcAddress( cudaModule, "cuLaunchGrid"              )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuMemAlloc                 = ( FUNC_cuMemAlloc                 ) GetProcAddress( cudaModule, "cuMemAlloc"                )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuMemcpyDtoH               = ( FUNC_cuMemcpyDtoH               ) GetProcAddress( cudaModule, "cuMemcpyDtoH"              )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuMemcpyHtoD               = ( FUNC_cuMemcpyHtoD               ) GetProcAddress( cudaModule, "cuMemcpyHtoD"              )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuMemFree                  = ( FUNC_cuMemFree                  ) GetProcAddress( cudaModule, "cuMemFree"                 )) != NULL );
    cudaLoaded = cudaLoaded && (( pcuModuleGetFunction        = ( FUNC_cuModuleGetFunction        ) GetProcAddress( cudaModule, "cuModuleGetFunction"       )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuModuleLoad               = ( FUNC_cuModuleLoad               ) GetProcAddress( cudaModule, "cuModuleLoad"              )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuParamSeti                = ( FUNC_cuParamSeti                ) GetProcAddress( cudaModule, "cuParamSeti"               )) != NULL ); 
    cudaLoaded = cudaLoaded && (( pcuParamSetSize             = ( FUNC_cuParamSetSize             ) GetProcAddress( cudaModule, "cuParamSetSize"            )) != NULL ); 
  }
  
  return cudaLoaded;
}




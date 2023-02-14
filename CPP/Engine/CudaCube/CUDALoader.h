/*******************************************************************************
 FILE         :   CUDALoader.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Loading Cuda functionl dynamically

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   03/23/2012

 LAST UPDATE  :   03/23/2012
*******************************************************************************/


#ifndef __CUDALOADER_H
#define __CUDALOADER_H

#include <cuda.h>

namespace Its
{

class CUDALoader
{
public:
  CUDALoader();

  typedef CUresult ( CUDAAPI *FUNC_cuCtxCreate                )( CUcontext *pctx, unsigned int flags, CUdevice dev );
  typedef CUresult ( CUDAAPI *FUNC_cuCtxDestroy               )( CUcontext ctx );
  typedef CUresult ( CUDAAPI *FUNC_cuCtxSetCurrent            )( CUcontext ctx );
  typedef CUresult ( CUDAAPI *FUNC_cuCtxSynchronize           )( void );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceComputeCapability  )( int *major, int *minor, CUdevice dev );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceGet                )( CUdevice *device, int ordinal );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceGetAttribute       )( int *pi, CUdevice_attribute attrib, CUdevice dev );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceGetCount           )( int *count );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceGetName            )( char *name, int len, CUdevice dev );
  typedef CUresult ( CUDAAPI *FUNC_cuDeviceTotalMem           )( size_t *bytes, CUdevice dev );
  typedef CUresult ( CUDAAPI *FUNC_cuFuncSetBlockShape        )( CUfunction hfunc, int x, int y, int z );
  typedef CUresult ( CUDAAPI *FUNC_cuFuncSetSharedSize        )( CUfunction hfunc, unsigned int bytes );
  typedef CUresult ( CUDAAPI *FUNC_cuInit                     )( unsigned int Flags );
  typedef CUresult ( CUDAAPI *FUNC_cuLaunchGrid               )( CUfunction f, int grid_width, int grid_height );
  typedef CUresult ( CUDAAPI *FUNC_cuMemAlloc                 )( CUdeviceptr *dptr, size_t bytesize );
  typedef CUresult ( CUDAAPI *FUNC_cuMemcpyDtoH               )( void *dstHost, CUdeviceptr srcDevice, size_t ByteCount );
  typedef CUresult ( CUDAAPI *FUNC_cuMemcpyHtoD               )( CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount );
  typedef CUresult ( CUDAAPI *FUNC_cuMemFree                  )( CUdeviceptr dptr );
  typedef CUresult ( CUDAAPI *FUNC_cuModuleGetFunction        )( CUfunction *hfunc, CUmodule hmod, const char *name );
  typedef CUresult ( CUDAAPI *FUNC_cuModuleLoad               )( CUmodule *module, const char *fname );
  typedef CUresult ( CUDAAPI *FUNC_cuParamSeti                )( CUfunction hfunc, int offset, unsigned int value );
  typedef CUresult ( CUDAAPI *FUNC_cuParamSetSize             )( CUfunction hfunc, unsigned int numbytes );

  FUNC_cuCtxCreate                pcuCtxCreate;
  FUNC_cuCtxDestroy               pcuCtxDestroy;
  FUNC_cuCtxSetCurrent            pcuCtxSetCurrent;
  FUNC_cuCtxSynchronize           pcuCtxSynchronize;
  FUNC_cuDeviceComputeCapability  pcuDeviceComputeCapability;
  FUNC_cuDeviceGet                pcuDeviceGet;
  FUNC_cuDeviceGetAttribute       pcuDeviceGetAttribute;
  FUNC_cuDeviceGetCount           pcuDeviceGetCount;
  FUNC_cuDeviceGetName            pcuDeviceGetName;
  FUNC_cuDeviceTotalMem           pcuDeviceTotalMem;
  FUNC_cuFuncSetBlockShape        pcuFuncSetBlockShape;
  FUNC_cuFuncSetSharedSize        pcuFuncSetSharedSize;
  FUNC_cuInit                     pcuInit;
  FUNC_cuLaunchGrid               pcuLaunchGrid;
  FUNC_cuMemAlloc                 pcuMemAlloc;
  FUNC_cuMemcpyDtoH               pcuMemcpyDtoH;
  FUNC_cuMemcpyHtoD               pcuMemcpyHtoD;
  FUNC_cuMemFree                  pcuMemFree;
  FUNC_cuModuleGetFunction        pcuModuleGetFunction;
  FUNC_cuModuleLoad               pcuModuleLoad;
  FUNC_cuParamSeti                pcuParamSeti;
  FUNC_cuParamSetSize             pcuParamSetSize;

protected:  
  bool LoadCudaLibrary();

  HMODULE cudaModule;
};


}; // namespace Its

#endif // __CUDALOADER_H

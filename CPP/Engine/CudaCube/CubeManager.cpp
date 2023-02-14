/*******************************************************************************
 FILE         :   CubeManager.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Loading and executing Cubes on GPU/Cuda 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   07/17/2011
*******************************************************************************/

#include "stdafx.h"

#include "BasicCube.h"
#include "CubeManager.h"


using namespace Its;
using namespace std;



// 15 Mar 2012 - DMA - use different kernels to avoid error #300, CUDA_ERROR_INVALID_SOURCE
//                     on devices with compute capability 1.1 and 2.0 
//
// Note: 10 - virtual capability, 11 - real
//
// 17 Jul 2015 - DMA - Compute capability 1.0 and 1.1 are obsolete and not supported in CUDA SDK 7.0
// 27 Apr 2017 - DMA - Compute capability 2.0 is obsolete and not supported in CUDA SDK 8.0
// 29 Aug 2018 - DMA - Compute capability 3.0 is obsolete removed
//
// Modules listed in try order
const char *CubeModules[] = { ".\\CubeKernel.cubin.arch_50", ".\\CubeKernel.cubin.arch_35" };
const char *CubeEntryFunctionName = "CubeEntry";


CubeManager*      CubeManager::instance = NULL;
DCriticalSection  CubeManager::section;


CubeManager::CubeManager( int _blockSizeX, int _blockSizeY, int _blockSizeZ )
{
  if( instance == NULL )
  {
    CU_SAFE_CALL_IT( cudaLoader.pcuInit( 0 ));

    blockSizeX = _blockSizeX;
    blockSizeY = _blockSizeY;
    blockSizeZ = _blockSizeZ;
    
    instance   = this;
  }
}


CubeManager::~CubeManager()
{
  DCriticalSection::Lock locker( section );

  if( instance == this )
  {
    for( CubeContextMap::const_iterator loopContext  = instance->contexts.begin(); 
                                        loopContext != instance->contexts.end(); 
                                        loopContext++ )
    {
      CubeContext& context = *loopContext->second;
    
      // do not throw exceptions
      cudaLoader.pcuMemFree   ( context.cuDataIn   );    
      cudaLoader.pcuMemFree   ( context.cuDataInU  );    
      cudaLoader.pcuMemFree   ( context.cuDataInD  );    
      cudaLoader.pcuMemFree   ( context.cuDataOut  );    
      cudaLoader.pcuMemFree   ( context.cuProbe    );
      cudaLoader.pcuCtxDestroy( context.cuContext  );
    }
    
    instance->contexts.clear();
  }
}


int CubeManager::GetDeviceCount()
{
  int deviceCount = 0;
  
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetCount( &deviceCount ));
  
  return deviceCount;
}


void CubeManager::GetDeviceInfo( int deviceOrdinal,  int& majorVersion,     int& minorVersion, 
                                 size_t& memorySize, int& sharedMemorySize, int& multiProcessorCount )
{
  CUdevice device;
  char deviceName[ 1024 ];
  int  canMapHostMemory = 0;

  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGet                ( &device, deviceOrdinal ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceComputeCapability  ( &majorVersion, &minorVersion, device ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceTotalMem           ( &memorySize, device ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetName            ( deviceName, sizeof( deviceName ), device ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetAttribute       ( &multiProcessorCount , CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT         , device ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetAttribute       ( &canMapHostMemory    , CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY          , device ));
  CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetAttribute       ( &sharedMemorySize    , CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK  , device ));
}                                


// reset context and reallocate memory
void CubeManager::CubeSet( int deviceOrdinal, int sizeX, int sizeY )
{
  DCriticalSection::Lock locker( section );

  CubeContextMap::const_iterator mappedContext = instance->contexts.find( deviceOrdinal );
  CubeContext* context = NULL;
  CUresult     errorCode;

  try
  {
    if( mappedContext != instance->contexts.end())
    {
      context = mappedContext->second;

      CU_SAFE_CALL_IT( cudaLoader.pcuCtxSetCurrent( context->cuContext ));
      
      CU_SAFE_CALL_IT( cudaLoader.pcuMemFree( context->cuDataIn  ));
      CU_SAFE_CALL_IT( cudaLoader.pcuMemFree( context->cuDataInU ));
      CU_SAFE_CALL_IT( cudaLoader.pcuMemFree( context->cuDataInD ));
      CU_SAFE_CALL_IT( cudaLoader.pcuMemFree( context->cuDataOut ));
      CU_SAFE_CALL_IT( cudaLoader.pcuMemFree( context->cuProbe   ));
    }
    else
    {
      context = new CubeContext();

      memset( context, 0, sizeof( context ));
      context->sizeX = sizeX;
      context->sizeY = sizeY;

      CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGet ( &context->cuDevice,   deviceOrdinal ));
      CU_SAFE_CALL_IT( cudaLoader.pcuCtxCreate ( &context->cuContext,  0, context->cuDevice ));

      // Try top CUDA compute capability first
      for( int moduleNumber = 0; moduleNumber < ARRAYSIZE( CubeModules ); moduleNumber++ )
      {
        string moduleName = CubeModules[ moduleNumber ];
        if(( errorCode = cudaLoader.pcuModuleLoad( &context->cuModule, moduleName.c_str())) == CUDA_SUCCESS )
        {
          DLog::msg( DLog::LevelInfo, "Cube kernel module '%s' loaded", moduleName.c_str());
          break;
        }

        if( errorCode == CUDA_ERROR_FILE_NOT_FOUND )
        {
          DLog::msg( DLog::LevelWarning, "Cube kernel module '%s' not found", moduleName.c_str());
          continue;
        }

        DLog::msg( DLog::LevelWarning, "Can not load Cube kernel module '%s', error %d", moduleName.c_str(), errorCode );
      }

      if( errorCode != CUDA_SUCCESS )
      {
        throw CubeException( DException::Error, "Can not load Cube kernel", errorCode );
      }

      CU_SAFE_CALL_IT( cudaLoader.pcuModuleGetFunction( &context->cuFunction, context->cuModule, CubeEntryFunctionName ));
    }
    
    size_t memoryRequired   = BasicCube::CubeMemorySize( sizeX, sizeY );
    size_t memoryAvailable  = 0;
    int    sharedMemorySize = 0;
     
    CU_SAFE_CALL_IT( cudaLoader.pcuDeviceTotalMem     ( &memoryAvailable,  context->cuDevice ));
    CU_SAFE_CALL_IT( cudaLoader.pcuDeviceGetAttribute ( &sharedMemorySize, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK, context->cuDevice ));

    if( CubeRequiredSharedMemory > sharedMemorySize )
    {
      throw CubeException( DException::Error, "Not enough shared memory on the device, required %d bytes, present %d bytes", CubeRequiredSharedMemory, sharedMemorySize );  
    }

    //
    // Need 4 layers, 3 in, 1 out
    //
    // 80% onboard RAM usage limit
    //
    if(  memoryRequired * 4 > memoryAvailable * 0.8 ||
        !AllocateMemoryCheck( context->cuDataIn   , memoryRequired ) || 
        !AllocateMemoryCheck( context->cuDataInU  , memoryRequired ) || 
        !AllocateMemoryCheck( context->cuDataInD  , memoryRequired ) || 
        !AllocateMemoryCheck( context->cuDataOut  , memoryRequired ) ||
        !AllocateMemoryCheck( context->cuProbe    , sizeof( Probe )))
    {
      throw CubeException( "Not enough memory on the device", CUDA_ERROR_OUT_OF_MEMORY );
    }
  }
  catch( ... )
  {
    if( context != NULL )
    {
      if( context->cuContext != NULL )
      {
        cudaLoader.pcuCtxDestroy( context->cuContext );
      }
   
      DeAllocateMemoryCheck( context->cuDataIn  );
      DeAllocateMemoryCheck( context->cuDataInU );
      DeAllocateMemoryCheck( context->cuDataInD );
      DeAllocateMemoryCheck( context->cuDataOut );
      DeAllocateMemoryCheck( context->cuProbe   );
    }
    
    delete context;
    throw;
  }

  instance->contexts[ deviceOrdinal ] = context;
}



void CubeManager::CubeLaunch( int deviceOrdinal, Cell* data, Cell* dataU, Cell* dataD, Cell* dataOut, int gf, int rf, Probe* probe )
{
  DCriticalSection::Lock locker( section );

  CubeContextMap::const_iterator mappedContext = instance->contexts.find( deviceOrdinal );
  CubeContext* context = NULL;

  if( !*probe )
  {
    throw CubeException( DException::Error, "Can not create Cuda Cube, probe is invalid" );
  }

  if( mappedContext != instance->contexts.end())
  {
    context = mappedContext->second;
  }
  else
  {
    throw CubeException( "Cube has not been set.", CUDA_SUCCESS );
  }

  size_t  cubeMemorySize  = BasicCube::CubeMemorySize( context->sizeX, context->sizeY );
  int     offset          = 0;
  int     gridSizeX       = ( context->sizeX / instance->blockSizeX ) + 1;
  int     gridSizeY       = ( context->sizeY / instance->blockSizeY ) + 1;  
  
  CU_SAFE_CALL_IT( cudaLoader.pcuCtxSetCurrent  ( context->cuContext )); // I believe it's still needed
  CU_SAFE_CALL_IT( cudaLoader.pcuMemcpyHtoD     ( context->cuDataIn  , data,  cubeMemorySize ));
  CU_SAFE_CALL_IT( cudaLoader.pcuMemcpyHtoD     ( context->cuDataInU , dataU, cubeMemorySize ));
  CU_SAFE_CALL_IT( cudaLoader.pcuMemcpyHtoD     ( context->cuDataInD , dataD, cubeMemorySize ));
  CU_SAFE_CALL_IT( cudaLoader.pcuMemcpyHtoD     ( context->cuProbe   , probe, sizeof( Probe )));
  
  CU_SAFE_CALL_IT( cudaLoader.pcuFuncSetBlockShape( context->cuFunction,  instance->blockSizeX, 
                                                                          instance->blockSizeY, 
                                                                          instance->blockSizeZ ));
                                          
  // 17 Apr 2012 - DMA - Shared memory as an external array
  //
  // Should we set shared memory to HyperCubeSize * HyperCubeSize * 3 * sizeof ( Cell ) at least?
  // Get twice as much just in case!
  //
  // And why the function is deprecated?
  // 
  // ~2700 bytes 
  //
  // what is the default value?
  // CU_SAFE_CALL_IT( cudaLoader.pcuFuncSetSharedSize( context->cuFunction, 0 ));

  CU_SAFE_CALL_IT( cudaLoader.pcuFuncSetSharedSize( context->cuFunction, CubeRequiredSharedMemory ));

  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction,      0, gf                 ));  offset += sizeof( gf                 );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, rf                 ));  offset += sizeof( rf                 );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->cuDataIn  ));  offset += sizeof( context->cuDataIn  );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->cuDataInU ));  offset += sizeof( context->cuDataInU );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->cuDataInD ));  offset += sizeof( context->cuDataInD );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->cuDataOut ));  offset += sizeof( context->cuDataOut );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->sizeX     ));  offset += sizeof( context->sizeX     );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->sizeY     ));  offset += sizeof( context->sizeY     );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSeti     ( context->cuFunction, offset, context->cuProbe   ));  offset += sizeof( context->cuProbe   );
  CU_SAFE_CALL_IT( cudaLoader.pcuParamSetSize  ( context->cuFunction, offset ));

  // Launch the Cube grid
  CU_SAFE_CALL_IT( cudaLoader.pcuLaunchGrid( context->cuFunction, gridSizeX, gridSizeY ));
  CU_SAFE_CALL_IT( cudaLoader.pcuCtxSynchronize() );

  CU_SAFE_CALL_IT( cudaLoader.pcuMemcpyDtoH( (void *)dataOut, context->cuDataOut, cubeMemorySize )); 
}
 

bool CubeManager::AllocateMemoryCheck( CUdeviceptr& ptr, size_t size )
{
  return cudaLoader.pcuMemAlloc( &ptr, size ) == CUDA_SUCCESS;
}


void CubeManager::DeAllocateMemoryCheck( CUdeviceptr ptr )
{
  if( ptr != NULL )
  {
    cudaLoader.pcuMemFree( ptr );
  }
}





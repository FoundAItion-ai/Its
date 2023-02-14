/*******************************************************************************
 FILE         :   CubeException.h 

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Errors in Cuda Cube module

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   03/23/2012

 LAST UPDATE  :   03/23/2012
*******************************************************************************/


#ifndef __CUBEEXCEPTION_H
#define __CUBEEXCEPTION_H

#include <cuda.h>

#include "CudaCubeDefines.h"
#include "BasicException.h"


namespace Its
{

#  define CU_SAFE_CALL_NO_SYNC_IT( call ) {                     \
    CUresult errorCode = call;                                  \
    if( CUDA_SUCCESS != errorCode ) {                           \
        throw CubeException( "Cuda driver error", errorCode );  \
    }}


#  define CU_SAFE_CALL_IT( call )       CU_SAFE_CALL_NO_SYNC_IT   ( call ); 



class CUBE_SPEC CubeException : public EngineException
{
public:
  DEXCEPTION_CONSTRUCTOR0_EX( CubeException, EngineException, errorCode = CUDA_ERROR_UNKNOWN );
  DEXCEPTION_CONSTRUCTOR1_EX( CubeException, EngineException, errorCode = CUDA_ERROR_UNKNOWN );
  DEXCEPTION_CONSTRUCTOR2_EX( CubeException, EngineException, errorCode = CUDA_ERROR_UNKNOWN );
  DEXCEPTION_CONSTRUCTOR3_EX( CubeException, EngineException, errorCode = CUDA_ERROR_UNKNOWN );

  CubeException( const char* message, CUresult _errorCode ):
        EngineException( message ),
        errorCode( _errorCode )
    {
    }
    
    CUresult GetCudaError() { return errorCode; }
    
private:
  CUresult errorCode;
};



}; // namespace Its

#endif // __CUBEEXCEPTION_H

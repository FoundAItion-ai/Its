// CudaCellTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>
#include <time.h>
#include <memory>
#include <windows.h>

#include "DCommonIncl.h"
#include "Cube.h"
#include "CudaCube.h"
#include "BasicCube.h"
#include "BasicCubeTypes.h"
#include "BasicCubeEngine.h"
#include "CubeManager.h"
#include "BasicCubeManager.h"


using namespace Its;
using namespace std;
using namespace DCommon;


#define USE_CUBEMANAGER


#define CUDA_CUBE_SAFE_CALL( call ) {              \
  int cubeRsponseCode = call;                      \
  CheckCudaCubeStatus( cubeRsponseCode );          \
  }

#define BASIC_CUBE_SAFE_CALL( call ) {             \
  int cubeRsponseCode = call;                      \
  CheckBasicCubeStatus( cubeRsponseCode );         \
  }




void CheckBasicCubeStatus( int cubeResponseCode )
{
  switch( cubeResponseCode )
  {
    case BASICCUBE_CODE_NOMEMORY:
      cout << "Not enough memory on the host." << endl;
      break;

    case BASICCUBE_CODE_ERROR:
      cout << "Basic Cube error." << endl;
      break;
  }
  
  if( cubeResponseCode != BASICCUBE_CODE_OK )
  {
    exit( cubeResponseCode );
  }
}



void CheckCudaCubeStatus( int cudaCubeResponseCode )
{
  switch( cudaCubeResponseCode )
  {
    case CUBE_CODE_NOMEMORY:
      cout << "Not enough memory on GPU." << endl;
      break;

    case CUBE_CODE_CUDA_ERROR:
      cout << "Low level GPU error." << endl;
      break;

    case CUBE_CODE_ERROR:
      cout << "Cuda Cube error." << endl;
      break;
  }
  
  if( cudaCubeResponseCode != CUBE_CODE_OK )
  {
    exit( cudaCubeResponseCode );
  }
}


int PrepareCudaCube( CubeManager* cubeManager, Cell* cells, int matrixSizeX, int matrixSizeY )
{
  int deviceCount, deviceOrdinal = -1;
  size_t memorySize = 0;


#ifdef USE_CUBEMANAGER
  if( cubeManager == 0 )
  {
    cout << "CubeManager was not created." << endl;
    return deviceOrdinal;
  }

  deviceCount = cubeManager->GetDeviceCount();

#else
  CUDA_CUBE_SAFE_CALL( GetDeviceCount( deviceCount ));
#endif

  cout << "DeviceInfo -> GPU Device(s) info:" << endl;
  cout << "DeviceInfo -> There are " << deviceCount << " device(s) supporting CUDA." << endl;

  for( int loopDevice = 0; loopDevice < deviceCount; loopDevice++ )
  {
    int majorVersion, minorVersion, multiProcessorCount, sharedMemorySize;
                                 
  #ifdef USE_CUBEMANAGER
    cubeManager->GetDeviceInfo( loopDevice,  majorVersion,      minorVersion, 
                                        memorySize,  sharedMemorySize,  multiProcessorCount );
  #else
    CUDA_CUBE_SAFE_CALL( GetDeviceInfo( loopDevice,  majorVersion,      minorVersion, 
                                        memorySize,  sharedMemorySize,  multiProcessorCount ));
  #endif

    cout << "DeviceInfo -> Device #" << loopDevice << " supports " 
         << majorVersion << "." << minorVersion << " CUDA version" << endl;
         
    cout << "DeviceInfo -> Device #" << loopDevice << " has " << multiProcessorCount 
         << " multiprocessors and " << memorySize / (1024*1024) 
         << " Mb global memory onboard with " << sharedMemorySize / ( 1024 ) 
         << " Kb shared memory" << endl;
         
    if( majorVersion >= 1 )
    {
      deviceOrdinal = loopDevice;
      break;
    }
  }

#ifdef USE_CUBEMANAGER
  cubeManager->CubeSet( deviceOrdinal, matrixSizeX, matrixSizeY );
#else
  CUDA_CUBE_SAFE_CALL( CubeSet( deviceOrdinal, matrixSizeX, matrixSizeY ));
#endif

  return deviceOrdinal;
}



bool CompareResult( Cell* matrixBasic, Cell* matrixCuda, int matrixSizeX, int matrixSizeY )
{
  bool resultEqual = true;
  
  for( int loopCell = 0; loopCell < matrixSizeX * matrixSizeY; loopCell++ )
  {
    Cell& cellBasic = matrixBasic [ loopCell ];
    Cell& cellCuda  = matrixCuda  [ loopCell ];

    if( !( cellBasic == cellCuda ))
    {
      resultEqual = false;
      break;
    }
  }
  
  return resultEqual;
}


void PopulateMatrix( Cell* cells, int matrixSizeX, int matrixSizeY, bool noInput )
{
  for( int loopCell = 0; loopCell < matrixSizeX * matrixSizeY; loopCell++ )
  {
    Cell& cell = cells[ loopCell ];

    for( int loopDirection = 0; loopDirection < DIRECTIONS; loopDirection++ )
    {
      if( loopCell < matrixSizeX )
      {
        cell.type   = T_CELL;
        cell.data   = noInput ? NOSIGNAL : loopCell;
        cell.uplink = INITIAL_UPLINK; //( 0x1 << 4 ); // ( 0x1 << 1 ) | 
      }
      else
      {
        cell.type   = NO_CELL;
        cell.data   = 0;
        cell.uplink = 0; 
      }
    }
  }
}

 
void ShowMatrix( Cell* cells, int sourceCellSize )
{
  if( sourceCellSize > 10 )
  {
    return;
  }

  for( int loopY = 0; loopY < sourceCellSize; loopY++ )
  {
    //cout << "Cell [" << loopY << "] = "; 

    for( int loopX = 0; loopX < sourceCellSize; loopX++ )
    {
      Cell& cell = cells[ loopY * sourceCellSize + loopX ];

      //cout << (int)cell.left << "; "; 
      //cout << (int)cell.right << "; "; 
      cout << (int)cell.type << ","            
           << (int)cell.data << "; "; 
    }
    
    cout << endl;
  }

  cout << endl;
}


int CudaMain( int argc, _TCHAR* argv[] )
{
  // U and D won't change
  CellArray                   matrixBasic, matrixCuda, matrixU, matrixD;
  auto_ptr< CubeManager >     cubeManager;
  auto_ptr< BasicCubeManager> basicCubeManager;
  const char*                 maiTrixDebPath = ".\\deb\\";
  clock_t                     startTime, finishTime;

  int     sourceMatrixSize  = 10;// 700;
  bool    cudaGPUPresent    = true;
  bool    noInput           = true;
  int     gf                = 1;
  int     rf                = 1;
  int     cycles            = 30;

  Probe probe;

  srand( GetTickCount() );
  rf = rand();

  string logFilePath( maiTrixDebPath );

  logFilePath += "Cube.log";

  DLog::init( logFilePath.c_str(), DLog::LevelDebug );
  DLog::msg( DLog::LevelDebug, "Running Cube tests:" );

  if( argc > 1 )
  {
    sourceMatrixSize = atoi( argv[ 1 ] );
  }

  if( argc > 2 )
  {
    gf = atoi( argv[ 2 ] );
  }

  if( gf == 0 ) // test purpose, static maiTrix
  {
    rf = 0;
  }

  if( InitKernel( CubeSize, CubeSize, DIRECTIONS ) != CUBE_CODE_OK )
  {
    cudaGPUPresent = false;
    cout << "Nvidia GPU is not found, use CPU." << endl << endl;
  }
  else
  {
    matrixCuda = CellArray( sourceMatrixSize * sourceMatrixSize );
    PopulateMatrix( matrixCuda.data(), sourceMatrixSize, sourceMatrixSize, noInput );

  #ifdef USE_CUBEMANAGER
    cubeManager = auto_ptr< CubeManager > ( new CubeManager( CubeSize, CubeSize, DIRECTIONS ));
  #endif
  }

  matrixBasic = CellArray( sourceMatrixSize * sourceMatrixSize );
  matrixU     = CellArray( sourceMatrixSize * sourceMatrixSize );
  matrixD     = CellArray( sourceMatrixSize * sourceMatrixSize );
 
  cout << "Matrix size of " << sourceMatrixSize << " x " << sourceMatrixSize 
       << ", memory allocated, Mb: " 
       << BasicCube::CubeMemorySize( sourceMatrixSize, sourceMatrixSize ) * 4.0 / (1024 * 1024) << endl << endl;
  
  PopulateMatrix( matrixBasic.data() , sourceMatrixSize, sourceMatrixSize, noInput );
  PopulateMatrix( matrixU.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
  PopulateMatrix( matrixD.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
  
  ShowMatrix( matrixBasic.data(), sourceMatrixSize );

  if( cudaGPUPresent )
  {
    int deviceOrdinal = PrepareCudaCube( cubeManager.get(), matrixCuda.data(), sourceMatrixSize, sourceMatrixSize );

    cout << endl << "Launching grid" << endl;
    startTime = clock();

    for( int loopCycle = 0; loopCycle < cycles; loopCycle++ )
    {
    #ifdef USE_CUBEMANAGER
      cubeManager->CubeLaunch( deviceOrdinal, 
                               matrixCuda.data(), matrixU.data(), matrixD.data(),
                               matrixCuda.data(), 
                               gf, rf, &probe );
    #else
      CUDA_CUBE_SAFE_CALL( CubeLaunch( deviceOrdinal, 
                                       matrixCuda.data(), matrixU.data(), matrixD.data(),
                                       matrixCuda.data(), 
                                       gf, rf, &probe ));
    #endif
    
      ShowMatrix( matrixCuda.data(), sourceMatrixSize );

      PopulateMatrix( matrixCuda.data()  , sourceMatrixSize, sourceMatrixSize, noInput );
      PopulateMatrix( matrixU.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
      PopulateMatrix( matrixD.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
    }

    finishTime = clock();
    cout << "GPU Time: " << finishTime - startTime << endl << endl;
    ShowMatrix( matrixCuda.data(), sourceMatrixSize );
  }

#ifdef USE_CUBEMANAGER
  basicCubeManager = auto_ptr< BasicCubeManager > ( new BasicCubeManager( sourceMatrixSize, sourceMatrixSize ));
#else
  BASIC_CUBE_SAFE_CALL( BasicCubeSet( sourceMatrixSize, sourceMatrixSize ));
#endif

  cout << "Launching Basic Cube" << endl;
  startTime = clock();

  for( int loopCycle = 0; loopCycle < cycles; loopCycle++ )
  {
  #ifdef USE_CUBEMANAGER
    basicCubeManager->Launch( matrixBasic.data(), matrixU.data(), matrixD.data(), 
                              matrixBasic.data(),
                              gf, rf, &probe );
  #else
    BASIC_CUBE_SAFE_CALL( BasicCubeLaunch( matrixBasic.data(), matrixU.data(), matrixD.data(), 
                                           matrixBasic.data(),
                                           gf, rf, &probe ));
  #endif

    ShowMatrix( matrixBasic.data(), sourceMatrixSize );

    PopulateMatrix( matrixBasic.data() , sourceMatrixSize, sourceMatrixSize, noInput );
    PopulateMatrix( matrixU.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
    PopulateMatrix( matrixD.data()     , sourceMatrixSize, sourceMatrixSize, noInput );
  }

  finishTime = clock();
  cout << "CPU Time: " << finishTime - startTime << endl << endl;
  startTime = clock();
  
  ShowMatrix( matrixBasic.data(), sourceMatrixSize );

  //BASIC_CUBE_SAFE_CALL( BasicCubeLaunch( matrixBasic.data(), matrixU.data(), matrixD.data(), gf ));
  //ShowMatrix( matrixBasic.data(), sourceMatrixSize );

  //BASIC_CUBE_SAFE_CALL( BasicCubeLaunch( matrixBasic.data(), matrixU.data(), matrixD.data(), gf ));
  //ShowMatrix( matrixBasic.data(), sourceMatrixSize );

  //BASIC_CUBE_SAFE_CALL( BasicCubeLaunch( matrixBasic.data(), matrixU.data(), matrixD.data(), gf ));
  //ShowMatrix( matrixBasic.data(), sourceMatrixSize );

  if( cudaGPUPresent )
  {
    if( CompareResult( matrixBasic.data(), matrixCuda.data(), sourceMatrixSize, sourceMatrixSize ))
    {
      cout << "Results are identical."<< endl;
    }
    else
    {
      cout << "Results are NOT identical."<< endl;
    }
  }
  else
  {
    if( CompareResult( matrixBasic.data(), matrixU.data(), sourceMatrixSize, sourceMatrixSize ))
    {
      cout << "Results are identical."<< endl;
    }
    else
    {
      cout << "Results are NOT identical."<< endl;
    }
  }

#ifdef USE_CUBEMANAGER
  // do nothing, will be release in auto_ptr destructor
#else
  if( cudaGPUPresent )
  {
    CUDA_CUBE_SAFE_CALL( CloseKernel());
  }
#endif

	return 0;
}



int _tmain( int argc, _TCHAR* argv[] )
{
  try
  {
    return CudaMain( argc, argv );
  }
  catch( DException& x )
  {
    cout << "Error: " << x.what() << endl;
  }
  catch( exception& x )
  {
    cout << "General error: " << x.what() << endl;
  }
  catch( ... )
  {
    cout << "General unknown error." << endl;
  }
}



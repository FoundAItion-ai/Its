/*******************************************************************************
 FILE         :   BasicCubeManager.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Basic Cube manager for desktop 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/09/2011

 LAST UPDATE  :   08/09/2011
*******************************************************************************/

#include "stdafx.h"

#include <memory>
#include <list>
#include <windows.h>

#include "BasicException.h"
#include "BasicCubeManager.h"


using namespace Its;
using namespace std;
using namespace DCommon;



//
// Not a singleton as CudaCube as we're not limited with hardware interface, only memory and
// may have a lot of the class instances.
//
// TODO: use multiple threads for faster processing.
//
//
// TODO: in theory we can use CubeManager to process different layers with the same 
// size, so we might want to make cubeThread static and share it
//
BasicCubeManager::BasicCubeManager( int _sizeX, int _sizeY ):
    sizeX( _sizeX ),
    sizeY( _sizeY )
{
  if( sizeX < CubeSize || sizeY < CubeSize )
  {
    throw BasicCubeException( DException::Error, "Can not create Basic Cube, size %d x %d is too small", _sizeX, _sizeY );
  }

  for( int cellIndexY = 0; cellIndexY < sizeY; cellIndexY++ )
  {
    for( int cellIndexX = 0; cellIndexX < sizeX; cellIndexX++ )
    {
      PointIt size      ( sizeX, sizeY );
      PointIt position  ( cellIndexX % CubeSize, cellIndexY % CubeSize );
      PointIt index     ( cellIndexX, cellIndexY );

      BasicCube* bcube = new BasicCube( NULL, NULL, NULL, 
                                        size, index, position );

      cubeThread.push_back( bcube );
    }
  }

}


BasicCubeManager::~BasicCubeManager()
{
  for( CubeThread_t::const_iterator cube = cubeThread.begin(); cube != cubeThread.end(); cube++ )
  {
    delete *cube;
  }
}


void BasicCubeManager::Launch( Cell* data, Cell* dataU, Cell* dataD, Cell* dataOut, int gf, int rf, Probe* probe )
{
  if( !*probe )
  {
    throw BasicCubeException( DException::Error, "Can not create Basic Cube, probe is invalid" );
  }
  
  for( CubeThread_t::const_iterator cube = cubeThread.begin(); cube != cubeThread.end(); cube++ )
  {
    (*cube)->Init( data, dataU, dataD );
    (*cube)->It( dataOut, gf, rf, probe );
  }
}


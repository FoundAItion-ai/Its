/*******************************************************************************
 FILE         :   BasicCubeManager.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Basic Cube manager for desktop 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/09/2011

 LAST UPDATE  :   08/09/2011
*******************************************************************************/


#ifndef __BASICCUBEMANAGER_H
#define __BASICCUBEMANAGER_H

#include <memory>
#include <list>

#include "BasicCube.h"
#include "BasicCubeTypes.h"
#include "BasicCubeDefines.h"


using namespace std;


namespace Its
{


// Not really needed, just to reduce annoying warnings without pragmas ;-)
//template class BASICCUBE_SPEC auto_ptr< Cell >;
//template class BASICCUBE_SPEC std::allocator< BasicCube* >;
//template class BASICCUBE_SPEC list< BasicCube*, std::allocator< BasicCube* >>;


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


class BASICCUBE_SPEC BasicCubeManager
{
public:
  BasicCubeManager( int _sizeX, int _sizeY );
  ~BasicCubeManager();

  void Launch( Cell* data, Cell* dataU, Cell* dataD, Cell* dataOut, int gf, int rf, Probe* probe );

private:
  typedef list< BasicCube* > CubeThread_t;

  int sizeX;
  int sizeY;
  
  CubeThread_t  cubeThread;
};


}; // namespace Its

#endif // __BASICCUBEMANAGER_H

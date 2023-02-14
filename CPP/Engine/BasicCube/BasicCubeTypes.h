/*******************************************************************************
 FILE         :   BasicCubeTypes.h

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   Basic implementation types 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   02/22/2013

 LAST UPDATE  :   02/22/2013
*******************************************************************************/

#ifndef __BASICCUBETYPES_H
#define __BASICCUBETYPES_H

#include <vector>
#include "Cube.h"
#include "CubeUtils.h"

using namespace std;


namespace Its
{

typedef vector<ByteIt>  ByteArray;
typedef vector<Cell>    CellArray;

// Linear
typedef ByteArray ByteArray1D;

// Flat
struct ByteArray2D
{
  ByteArray2D( ByteArray::size_type capacity, SizeIt2D _size ) :
    byteArray( capacity ),
    size( _size )
  {
    memset( byteArray.data(),  NOSIGNAL, byteArray.size() );
  }

  ByteArray byteArray;
  SizeIt2D  size;
};

};


#endif // __BASICCUBETYPES_H

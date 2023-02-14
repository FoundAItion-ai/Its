/*******************************************************************************
 FILE         :   Celllayer.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Layer of Cells, extracted from MaiTrix.h to avoid circular reference and C4150 and not releasing ISaver with auto_ptr<>

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   03/06/2012

 LAST UPDATE  :   03/06/2012
*******************************************************************************/


#ifndef __CELLLAYER_H
#define __CELLLAYER_H

#include <memory>

#include "DCommonIncl.h"
#include "BasicCube.h"
#include "BasicCubeTypes.h"
#include "BasicException.h"
#include "ITypes.h"
#include "MaiTrixDefines.h"


using namespace std;
using namespace DCommon;

namespace Its
{


// Not really needed, just to reduce annoying warnings without pragmas ;-)
//
// 06 Mar 2012 - DMA - "annoying warnings" (C4150 below) leads to 3 days bug fix as auto_ptr<ISaver> was not cleared in Release 
//                      build configuration only, debug works fine!
//
//template class auto_ptr< Cell >;


/*

Will cause this warning as at the moment ISaver is not defined to prevent circular reference:

>c:\development\tools\microsoft visual studio 10.0\vc\include\memory(931): warning C4150: deletion of pointer to incomplete type 'Its::ISaver'; no destructor called
>          c:\work\test\its\source\cpp\engine\maitrix\maitrix.h(39) : see declaration of 'Its::ISaver'
>          c:\development\tools\microsoft visual studio 10.0\vc\include\memory(930) : while compiling class template member function 'std::auto_ptr<_Ty>::~auto_ptr(void)'
>          with
>          [
>              _Ty=Its::ISaver

*/


enum Layer_t
{
  textlayer,  
  audiolayer,
  videolayer,
  emptylayer
};



#define THROWMAITRIXERROR( format, argument1 )                          \
        throw MaiTrixException( DException::Error, __FILE__, __LINE__,  \
                                format, argument1 );

#define THROWMAITRIXERROR2( format, argument1, argument2 )              \
        throw MaiTrixException( DException::Error, __FILE__, __LINE__,  \
                                format, argument1, argument2 );

#define THROWMAITRIXERROR3( format, argument1, argument2, argument3 )   \
        throw MaiTrixException( DException::Error, __FILE__, __LINE__,  \
                                format, argument1, argument2, argument3 );



class MAITRIX_SPEC MaiTrixException: public EngineException
{
public:
  DEXCEPTION_CONSTRUCTOR0( MaiTrixException, EngineException );
  DEXCEPTION_CONSTRUCTOR1( MaiTrixException, EngineException );
  DEXCEPTION_CONSTRUCTOR2( MaiTrixException, EngineException );
  DEXCEPTION_CONSTRUCTOR3( MaiTrixException, EngineException );
};



//
// 2D layer 
//
struct MAITRIX_SPEC Celllayer
{
  Celllayer( const char* _maiTrixName, SizeIt2D _size, SizeIt _positionZ, PeriodIt _cycle ):
    type        ( Its::emptylayer   ),
    maiTrixName ( _maiTrixName      ),
    size        ( _size             ),
    positionZ   ( _positionZ        ),
    cycle       ( _cycle            ),
    cells       ( _size.X * _size.Y )
  {
  }

  Celllayer():
    type        ( Its::emptylayer ),
    maiTrixName ( ""              ),
    size        ( 0, 0            ),
    positionZ   ( 0               ),
    cycle       ( 0               ),
    cells       ( 0               )
  {
  }

  void Clear()
  {
    cells.clear();
  }

  bool Empty()
  {
    return cells.size() == 0;
  }

  operator Cell* ()
  { 
    return cells.data(); 
  }
  
  void operator = ( Celllayer& that )
  {
    if( this->size != that.size || !cells.data() )
    {
      // reallocate storage for full copy
      cells = CellArray( that.size.X * that.size.Y );
    }

    this->size        = that.size;
    this->positionZ   = that.positionZ;
    this->cycle       = that.cycle;
    this->type        = that.type;
    this->maiTrixName = that.maiTrixName;

    // 22 Jan 2013 - DMA - Shallow copy replaced with full
    //
    // Shallow copy - pointer to body only
    // this->cells.reset( that.cells.release() );

    // Full copy
    if( that.cells.data() )
    {
      memcpy( this->cells.data(), that.cells.data(), BasicCube::CubeMemorySize( size.X, size.Y ));
    }
  }

  string FileNameWithPath( const char* folderPath, const char* fileExt )
  {
    if( maiTrixName.empty() )
    {
      throw MaiTrixException( "Can not compose full path, MaiTrix name should not be empty" );
    }

    string fileName = folderPath;
    char positionZString[ 33 ]; // "stores the result (up to 33 bytes) in str." (MSDN)

    _itoa_s( positionZ, positionZString, sizeof( positionZString ), 10 );
  
    fileName += maiTrixName;
    fileName += "_";
    fileName += positionZString;
    fileName += fileExt;

    return fileName;
  }


  Layer_t   type;      // layer's IO type
  string    maiTrixName;
  SizeIt2D  size;      // X : Y
  SizeIt    positionZ; // on Z axis in 3D-representation of layers in the maiTrix
  PeriodIt  cycle;     // time
  
private:  
  CellArray cells;
};



}; // namespace Its


#endif // __CELLLAYER_H

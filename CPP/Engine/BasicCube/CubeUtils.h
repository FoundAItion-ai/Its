/*******************************************************************************
 FILE         :   CubeUtils.h

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   Cube - Utility classes

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/26/2013

 LAST UPDATE  :   07/14/2015
*******************************************************************************/

#ifndef __CUBE_UTILS_H
#define __CUBE_UTILS_H

#if defined(__CUDACC_RTC__)
#include <common_functions.h>
#elif !defined(NO_CUBE_ASSERTS)
#include <assert.h>
#endif

#include "Init.h"
#include "Cube.h"
#include "BasicCubeDefines.h"

namespace BaseUnitTest {
  class BitArrayUnitTest;
}

#if defined(NO_CUBE_ASSERTS)
#define CU_ASSERT( expression )
#else
#define CU_ASSERT( expression ) assert( expression );
#endif

namespace Its
{

class ReflectionArray;


struct BASICCUBE_SPEC SizeIt2D
{
  CUDA_DECL SizeIt2D( const SizeIt2D& copy ) :
    X( copy.X ),
    Y( copy.Y )
  {}

  CUDA_DECL SizeIt2D( const SizeIt _X, const SizeIt _Y ) :
    X( _X ),
    Y( _Y )
  {}

  CUDA_DECL SizeIt2D() :
    X( 0 ),
    Y( 0 )
  {
  }

  CUDA_DECL void operator  = ( const SizeIt2D& that ) { this->X = that.X;
                                                        this->Y = that.Y; }

  CUDA_DECL bool operator == ( const SizeIt2D& that ) { return this->X == that.X && 
                                                               this->Y == that.Y; }

  CUDA_DECL bool operator != ( const SizeIt2D& that ) { return this->X != that.X || 
                                                               this->Y != that.Y; }

  SizeIt X;
  SizeIt Y;
};



struct BASICCUBE_SPEC SizeIt3D
{
  CUDA_DECL SizeIt3D( const SizeIt3D& copy ) :
    X( copy.X ),
    Y( copy.Y ),
    Z( copy.Z )
  {}

  CUDA_DECL SizeIt3D( const SizeIt _X, const SizeIt _Y, const SizeIt _Z ) :
    X( _X ),
    Y( _Y ),
    Z( _Z )
  {}

  CUDA_DECL SizeIt3D() :
    X( 0 ),
    Y( 0 ),
    Z( 0 )
  {
  }

  CUDA_DECL void operator  = ( const SizeIt3D& that ) { this->X = that.X;
                                                        this->Y = that.Y;
                                                        this->Z = that.Z; }

  CUDA_DECL bool operator == ( const SizeIt3D& that ) { return this->X == that.X && 
                                                               this->Y == that.Y &&
                                                               this->Z == that.Z; }

  CUDA_DECL bool operator != ( const SizeIt3D& that ) { return this->X != that.X || 
                                                               this->Y != that.Y ||
                                                               this->Z != that.Z; }

  CUDA_DECL SizeItl Volume() { return X * Y * Z; }

  SizeIt X;
  SizeIt Y;
  SizeIt Z;
};



struct BASICCUBE_SPEC PointIt
{
  CUDA_DECL PointIt():
    X( 0 ),
    Y( 0 )
  {
  }

  CUDA_DECL PointIt( const PosIt _X, const PosIt _Y ):
    X( _X ),
    Y( _Y )
  {
  }

  CUDA_DECL PointIt( const PointIt& point ):
    X( point.X ),
    Y( point.Y )
  {
  }

  CUDA_DECL bool LeftCubeSide   () { return X == 0;            }
  CUDA_DECL bool RightCubeSide  () { return X == CubeSize - 1; }
  CUDA_DECL bool UpperCubeSide  () { return Y == 0;            }
  CUDA_DECL bool BottomCubeSide () { return Y == CubeSize - 1; }

  CUDA_DECL bool LeftSide       ( const SizeIt2D& size ) { return X == 0;          }
  CUDA_DECL bool RightSide      ( const SizeIt2D& size ) { return X == size.X - 1; }
  CUDA_DECL bool UpperSide      ( const SizeIt2D& size ) { return Y == 0;          }
  CUDA_DECL bool BottomSide     ( const SizeIt2D& size ) { return Y == size.Y - 1; }
  CUDA_DECL bool Side           ( const SizeIt2D& size ) 
  { 
    return LeftSide( size ) || RightSide( size ) || UpperSide( size ) || BottomSide( size );
  }

  // [leftBottom, rightUp)
  CUDA_DECL bool Inside( const PointIt& leftBottom, const PointIt& rightUp ) const
  {
    return leftBottom.X <= this->X && leftBottom.Y <= this->Y && this->X < rightUp.X && this->Y < rightUp.Y;
  }

  CUDA_DECL PointIt operator +  ( const PointIt& that ) { return PointIt( this->X + that.X, this->Y + that.Y ); }
  CUDA_DECL PointIt operator -  ( const PointIt& that ) { return PointIt( this->X - that.X, this->Y - that.Y ); }

  CUDA_DECL bool operator == ( const PointIt& that ) { return this->X == that.X && 
                                                              this->Y == that.Y; }

  CUDA_DECL bool operator != ( const PointIt& that ) { return this->X != that.X || 
                                                              this->Y != that.Y; }

  PosIt X;
  PosIt Y;
};



typedef PointIt PointIt2D;




#define Zero2D          PointIt( 0, 0 )
#define One2D           PointIt( 1, 1 )
#define CubeSize2D      PointIt( CubeSize      , CubeSize      )
#define HyperCubeSize2D PointIt( HyperCubeSize , HyperCubeSize )



// Up or downlinks wrapper, coordinates:
//
// ( -1, -1 ) ( -1, 0 ), ( -1, 1 )
// (  0, -1 ) (  0, 0 ), (  0, 1 )
// (  1, -1 ) (  1, 0 ), (  1, 1 )
//
//   or in shifts:
//
//   0       1       2
//   7               3
//   6       5       4
//
//   and native, multilink sample = 0x86:
//
//   0       1       1
//   1               0
//   0       0       0
//
//   native mirrored, sample = 0x68:
//
//   0       0       0
//   0               1
//   1       1       0
//
//
// The class is immutable
class Link
{
public:
  explicit CUDA_DECL Link() :
    link( 0 )
  {
  }

  // Default copy contructor/assignment should be good.
  // Maybe we should try reference & for performance reason?
  explicit CUDA_DECL Link( ByteIt _link ) :
    link( _link )
  {
  }

  // Create link from single point
  CUDA_DECL static bool FromPoint( const PointIt & fromPoint, Link * toLink )
  {
    if( !toLink || !IsValidPoint( fromPoint ))
    {
      return false;
    }

    return FromShift( PointToShift( fromPoint ), toLink );
  }

  // Create single link from shift
  CUDA_DECL static bool FromShift( ByteIt fromShift, Link * toLink )
  {
    if( !toLink || fromShift >= 8 )
    {
      return false;
    }

    *toLink = Link( 0x1 << fromShift );
    return true;
  }

  CUDA_DECL bool ToPoint( PointIt * toPoint ) const
  {
    ByteIt shift;
    if( !toPoint || !ToShift( &shift )) 
    {
      return false;
    }

    *toPoint = PointIt( ShiftToPointX( shift ), ShiftToPointY( shift ));
    return true;
  }

  CUDA_DECL bool ToShift( ByteIt * toShift ) const
  {
    if( toShift ) 
    {
      for( int loopShift = 0; loopShift < 8; loopShift++ )
      {
        if( link == ( 0x1 << loopShift ))
        {
          *toShift = loopShift;
          return true;
        }
      }
    }

    return false;
  }

  CUDA_DECL bool HasPoint( const PointIt & what ) const
  {
    return HasShift( PointToShift( what ));
  }
  
  CUDA_DECL bool HasShift( ByteIt what ) const
  {
    if( what >= 8 )
    {
      return false;
    }
    return ( link & ( 0x1 << what )) == ( 0x1 << what );
  }
  
  CUDA_DECL bool HasLink( const Link & what ) const
  {
    return ( link & what.link ) == what.link;
  }
  
  // Raw link
  CUDA_DECL bool HasLink( ByteIt what ) const
  {
    return ( link & what ) == what;
  }
  
  CUDA_DECL SizeIt Count() const
  {
    if( link == INITIAL_UPLINK )
    {
      return 0;
    }

    SizeIt count = 0;
    for( int loopShift = 0; loopShift < 8; loopShift++ )
    {
      if( HasShift( loopShift ))
      {
        count++;
      }
    }
    return count;
  }

  CUDA_DECL bool IsSingle() const
  {
    return Count() == 1;
  }

  CUDA_DECL bool IsEmpty() const
  {
    return link == 0;
  }

  CUDA_DECL Link Mirror() const
  {
    // Not rotation, mirroring!
    // 0110 0001
    // 0001 0110
    return Link( RawMirror() );
  }

  // Reduce multi link into a pair based on symmetry
  CUDA_DECL bool Reduce( Link * link1, Link * link2 ) const
  {
    if( !link1 || !link2 )
    {
      return false;
    }

    if( IsEmpty())
    {
      *link1 = Link( 0 );
      *link2 = Link( 0 );
      return true;
    }

    ByteIt mirror = RawMirror();
    ByteIt result = link & ~mirror;  // Clear symmetrical links
    PosIt  dx = 0;
    PosIt  dy = 0;
    PosIt  dx1, dx2;
    PosIt  dy1, dy2;

    if( result == 0 )
    {
      *link1 = Link( 0 );
      *link2 = Link( 0 );
      return true;
    }

    if( Link( result ).IsSingle())
    {
      *link1 = Link( result );
      *link2 = Link( result );
      return true;
    }

    // Now asymmetrical links:
    //
    // 1 1 1              0 1 0
    // 0 0 0  ->          0 0 0
    // 0 0 0              0 0 0
    //
    // 0 1 0              1 0 0
    // 1 0 0  ->          0 0 0
    // 0 0 0              0 0 0
    //
    // 0 1 1              0 1 0
    // 1 0 0  ->          0 0 0
    // 0 0 0              0 0 0
    //
    // 0 1 1              0 1 0     0 0 1
    // 0 0 0  ->          0 0 0  &  0 0 0
    // 0 0 0              0 0 0     0 0 0
    //
    // 1 1 1     1 2 0    1 0 0     0 1 0
    // 1 0 0  -> 0 0 0 -> 0 0 0  &  0 0 0
    // 0 0 0     0 0 0    0 0 0     0 0 0
    //                    toLink    alternative
    //
    // and although toLink & alternative vectors length is not the same
    // we consider them equal so they may be distributed with 50/50% probability 
    for( int loopShift = 0; loopShift < 8; loopShift++ )
    {
      ByteIt point = 0x1 << loopShift;
      if(( result & point ) == point )
      {
        dx += ShiftToPointX( loopShift );
        dy += ShiftToPointY( loopShift );
      }
    }

    // There may be a case when all asymmetrical links neutralize each other
    //
    // 0 1 0              0 0 0
    // 1 0 0  ->          0 0 0
    // 0 0 1              0 0 0
    if( dx == 0 && dy == 0  )
    {
      *link1 = Link( 0 );
      *link2 = Link( 0 );
      return true;
    }

    CU_ASSERT( dx != 0 || dy != 0 );

    if( dx == 0 )
    {
      dx1 = dx2 = 0;
      dy1 = dy2 = dy < 0 ? -1 : 1;
    }
    else if( dy == 0 )
    {
      dx1 = dx2 = dx < 0 ? -1 : 1;
      dy1 = dy2 = 0;
    }
    else if( dx * dx == dy * dy )
    {
      dx1 = dx2 = dx < 0 ? -1 : 1;
      dy1 = dy2 = dy < 0 ? -1 : 1;
    }
    else
    {
      dx1 = dx < 0 ? -1 : 1;
      dy1 = dy < 0 ? -1 : 1;
      dx2 = dx * dx == 1 ? 0 : dx1;
      dy2 = dy * dy == 1 ? 0 : dy1;
    }

    return FromPoint( PointIt( dx1, dy1 ), link1 ) && FromPoint( PointIt( dx2, dy2 ), link2 );
  }

  CUDA_DECL Link Rotate( bool left )
  {
    if( left )
    {
      return Link(( link << 1 ) | ( link >> 7 ));
    }
    else
    {
      return Link(( link >> 1 ) | ( link << 7 ));
    }
  }


  CUDA_DECL ByteIt Raw() const
  {
    return link;
  }

  // TODO: Should be friends
  CUDA_DECL bool operator == ( const Link& that ) const { return this->link == that.link; }
  CUDA_DECL bool operator != ( const Link& that ) const { return this->link != that.link; }

  CUDA_DECL Link   operator +  ( const Link& that ) { return Link( this->link | that.link ); }
  CUDA_DECL Link & operator += ( const Link& that ) { this->link |= that.link; return *this; }

private:
  CUDA_DECL ByteIt RawMirror() const
  {
    // Not rotation, mirroring!
    // 0110 0001
    // 0001 0110
    return ( link >> 4 ) | ( link << 4 );
  }


  CUDA_DECL static bool IsValidPoint( const PointIt & point )
  {
    // Inside[leftBottom, rightUp)
    return point.Inside( PointIt( -1, -1 ), PointIt( 2, 2 ));
  }


  CUDA_DECL static PosIt ShiftToPointX( ByteIt shift )
  {
    PosIt shiftAsPointX[ 8 ] = { -1, -1, -1, 0, 1, 1, 1, 0 };
    return shiftAsPointX[ shift ];
  }

  CUDA_DECL static PosIt ShiftToPointY( ByteIt shift )
  {
    PosIt shiftAsPointY[ 8 ] = { -1, 0, 1, 1, 1, 0, -1, -1 };
    return shiftAsPointY[ shift ];
  }

  CUDA_DECL static ByteIt PointToShift( const PointIt & point )
  {
    /*
    1) Standard downlink bitorder = ( i + 1 ) * 3 + j + 1

    -1 -1 = 0
    -1  0 = 1
    -1  1 = 2
     0 -1 = 3
     ...
     1  1 = 8
        
     View:

     0  1  2
     3  4  5
     6  7  8

    Downlink reversed = ( -i + 1 ) * 3 - j + 1 = Uplink

    2) Suggested circular downlink bitorder (easy to rotate and make spirals)

     View:

     0  1  2
     7  8  3
     6  5  4
     
    */

    ByteIt shiftOrderUp[ 3 ][ 3 ] = 
      { 
        { 0, 1, 2 },
        { 7, 8, 3 },
        { 6, 5, 4 }
      };
    
    return shiftOrderUp[ point.X + 1 ][ point.Y + 1 ];
  }

  ByteIt link;
};


class MemoryArray
{
public:
  // [0] lesson memory, [1] cycle memory
  enum {
    ArraySize = 2
  };
  typedef ByteIt Array[ ArraySize ];

  CUDA_DECL MemoryArray( Array& other_array_data, int _index = 0 ) :
    array( other_array_data ),
    index( _index )
  {
  }

  CUDA_DECL void Clear()
  {
    array[ index ] = 0;
  }

  CUDA_DECL void ClearAll()
  {
    for( int loopByte = 0; loopByte < ArraySize; loopByte++ )
    {
      array[ loopByte ] = 0;
    }
  }

  CUDA_DECL bool IsEmpty()
  {
    // Technically we need to check Link.IsEmpty what is less efficient.
    return array[ index ] == 0;
  }

  CUDA_DECL Link Get()
  {
    return Link( array[ index ]);
  }

  CUDA_DECL void Set( const Link & link )
  {
    CU_ASSERT( !link.IsEmpty() );
    array[ index ] = link.Raw();
  }

private:
  Array& array;
  int    index;
};


template<typename T, int size>
class TriBitsArray
{
private:
  enum {
    ArraySize = ( size * 3 ) / 8,
    FlagsSize = ( size / 8 ),
  };

  typedef T Array[ ArraySize ]; // 3-bits snips
  typedef T Flags[ FlagsSize ]; // Flag if snip is set

public:
  enum {
    ArrayFlagsSize = ArraySize + FlagsSize
  };

  typedef T ArrayFlags[ ArrayFlagsSize ]; // Combined

  // Note that other_array_data is not being cleared/owned.
  CUDA_DECL TriBitsArray( ArrayFlags& other_array_data ):
    array(( Array& )( other_array_data )),
    flags(( Flags& )(*( &other_array_data[ 0 ] + ( ArraySize * sizeof( T )))))
  {
    Snips();
  }

  CUDA_DECL void Clear() 
  {
    for( int loopArray = 0; loopArray < ArraySize; loopArray++ )
    {
      array[ loopArray ] = 0;
    }
    for( int loopFlags = 0; loopFlags < FlagsSize; loopFlags++ )
    {
      flags[ loopFlags ] = 0;
    }
  }  

  CUDA_DECL int Snips() 
  {
    static_assert( size % 8 == 0, "Unaligned TriBitsArray size"  );
    static_assert( size <= 256,   "TriBitsArray size is too big" );
    return size;
  }

  // Assume one uplink
  CUDA_DECL bool GetLink( const Link & downlink, Link * uplink )
  {
    // CRT asserts are fine on Cuda, CUDA_ERROR_INVALID_IMAGE is expected.
    CU_ASSERT( !downlink.IsEmpty() );
    CU_ASSERT( uplink != NULL );

    int ordinal = downlink.Raw();
    if( !GetFlag( ordinal )) 
    {
      return false;
    }

    return Link::FromShift( Get( ordinal ), uplink );
  }

  CUDA_DECL void ClearLink( const Link & downlink )
  {
    CU_ASSERT( !downlink.IsEmpty() );
    int ordinal = downlink.Raw();
    SetFlag( ordinal, false );
    Set( ordinal, 0 ); // Not needed, may be removed due to performance reasons
  }

  CUDA_DECL bool SetLink( const Link & downlink, const Link& uplink )
  {
    CU_ASSERT( !downlink.IsEmpty() );
    CU_ASSERT( !uplink.IsEmpty() );
    ByteIt shift; // Assume one uplink

    if( !uplink.ToShift( &shift )) 
    {
      return false;
    }

    int ordinal = downlink.Raw();
    SetFlag( ordinal, true );
    Set( ordinal, shift );
    return true;
  }

private:
  CUDA_DECL void ValidateRange( int ordinal, SizeIt pos, SizeIt size )
  {
    CU_ASSERT( ordinal >= 0 );
    CU_ASSERT( ordinal < Snips() );
    CU_ASSERT( pos > 0 || pos == 0 ); // Not >=
    CU_ASSERT( pos < size );
  }

  CUDA_DECL bool GetFlag( int ordinal )
  {
    SizeIt pos   = ordinal / 8;
    SizeIt shift = ordinal % 8;

    ValidateRange( ordinal, pos, FlagsSize );

    ByteIt flag = flags[ pos ];
    return ( flag & ( 0x1 << shift )) == ( 0x1 << shift );
  }

  CUDA_DECL void SetFlag( int ordinal, bool enable )
  {
    SizeIt pos   = ordinal / 8;
    SizeIt shift = ordinal % 8;

    ValidateRange( ordinal, pos, FlagsSize );

    if( enable ) 
    {
      flags[ pos ] |= ( 0x1 << shift );
    }
    else 
    {
      flags[ pos ] &= ~( 0x1 << shift );
    }
  }

  CUDA_DECL ByteIt Get( int ordinal )
  {
    SizeIt pos    = ( ordinal * 3 ) / 8;
    SizeIt shift  = ( ordinal * 3 ) % 8;
    ByteIt result = 0;

    ValidateRange( ordinal, pos, ArraySize );
    ByteIt first_byte = array[ pos ];

    // 1) Aligned tribit
    // 012345670123456701234567
    // xxx
    //
    // 2) Unaligned tribit
    // 012345670123456701234567
    //    xxx
    if( shift <= 5 ) 
    {
      ByteIt mask = 0x07 << ( 5 - shift );
      result = ( first_byte & mask ) >> ( 5 - shift );
    } 
    else
    {
      // 3) Unaligned split tribit
      // 012345670123456701234567
      //       xxx
      CU_ASSERT( pos + 1 < ArraySize );
      ByteIt second_byte = array[ pos + 1 ];
      ByteIt first_mask, second_mask;

      if( shift == 6 ) 
      {
        first_mask  = 0x03;
        second_mask = 0x01 << 7;
        result = (( first_byte  & first_mask  ) << 1 ) | 
                 (( second_byte & second_mask ) >> 7 );
      }
      else
      {
        first_mask  = 0x1;
        second_mask = 0x3 << 6;
        result = (( first_byte  & first_mask  ) << 2 ) | 
                 (( second_byte & second_mask ) >> 6 );
      }
    }

    CU_ASSERT( result <= 0x7 );
    return result;
  }

  CUDA_DECL void Set( int ordinal, ByteIt bits )
  {
    CU_ASSERT( bits > 0 || bits == 0 ); // Not >=
    CU_ASSERT( bits <= 0x7 );

    SizeIt pos   = ( ordinal * 3 ) / 8;
    SizeIt shift = ( ordinal * 3 ) % 8;

    ValidateRange( ordinal, pos, ArraySize );
    ByteIt & first_byte = array[ pos ];

    // 1) Aligned tribit
    // 012345670123456701234567
    // xxx
    //
    // 2) Unaligned tribit
    // 012345670123456701234567
    //    xxx
    if( shift <= 5 ) 
    {
      first_byte &= ~( 0x07 << ( 5 - shift ));
      first_byte +=  ( bits << ( 5 - shift ));
    }
    else
    {
      // 3) Unaligned split tribit
      // 012345670123456701234567
      //       xxx
      CU_ASSERT( pos + 1 < ArraySize );
      ByteIt & second_byte = array[ pos + 1 ];
      
      if( shift == 6 )
      {
        first_byte  &= 0xFC; // 11111100
        first_byte  += ( bits & 0x06 ) >> 1;
        second_byte &= 0x7F; // 01111111
        second_byte += ( bits & 0x01 ) << 7;
      }
      else
      {
        first_byte  &= 0xFE; // 11111110
        first_byte  += ( bits & 0x04 ) >> 2;
        second_byte &= 0x3F; // 00111111
        second_byte += ( bits & 0x03 ) << 6;
      }
    }
  }

  friend class BaseUnitTest::BitArrayUnitTest;
  friend class ReflectionArray;

  Array& array; // byte -> tri-bit value mapping, like 201 -> 2, 17 -> 2, 10 -> 3, 15 -> 5
  Flags& flags; // shows if snips is set, like 0110001001001
};


#define TRI_BITS_ARRAY(size) \
  typedef TriBitsArray<ByteIt, size>  TriBits##size; \
  template class __declspec(dllexport) TriBitsArray<ByteIt, size>;

TRI_BITS_ARRAY(256); // => TriBits256

typedef TriBits256 ShapeArray;


class ReflectionArray
{
public:
  enum { ArraySize = 8 };
  typedef ByteIt Array[ ArraySize ];

  CUDA_DECL ReflectionArray( Array& other_array_data ) :
    array( other_array_data ) 
  {
  }

  CUDA_DECL void Clear()
  {
    for( int loopByte = 0; loopByte < ArraySize; loopByte++ )
    {
      array[ loopByte ] = 0;
    }
  }

  CUDA_DECL bool Get( const Link & uplink, Link * downlink )
  {
    CU_ASSERT( uplink.IsSingle() );
    CU_ASSERT( downlink != NULL );
    ByteIt pos;

    if( !uplink.ToShift( &pos ) || pos >= ArraySize || !downlink )
    {
      return false;
    }

    *downlink = Link( array[ pos ] );
    return true;
  }

  CUDA_DECL bool Set( const Link & uplink, const Link & downlink )
  {
    CU_ASSERT( uplink.IsSingle() );
    CU_ASSERT( !downlink.IsEmpty() );
    ByteIt pos;

    if( !uplink.ToShift( &pos ) || pos >= ArraySize )
    {
      return false;
    }

    array[ pos ] |= downlink.Raw();
    return true;
  }

  // Find downlink in reflections, starting with current uplink
  CUDA_DECL bool Find( const Link & downlink, Link * uplink, bool * found )
  {
    CU_ASSERT( uplink != NULL && found != NULL && !downlink.IsEmpty() );
    ByteIt pos;

    if( uplink == NULL || found == NULL || downlink.IsEmpty() )
    {
      return false;
    }

    if( uplink->Count() == 0 )
    {
      // This should not really happen (there are reflections but no uplink),
      // but we can search no matter what.
      pos = 0;
    }
    else if( !uplink->ToShift( &pos ) || pos >= ArraySize )
      {
        return false;
      }

    *found = false;
    for( int loopByte = 0; loopByte < ArraySize; loopByte++ )
    {
      Link reflected_link( array[ pos ]);
      if( reflected_link.HasLink( downlink ))
      {
        if( !Link::FromShift( pos, uplink ))
        {
          return false;
        }
        *found = true;
        break;
      }

      if( ++pos >= ArraySize )
      {
        pos = 0;
      }
    }

    return true;
  }

  // Recalculate reflection
  CUDA_DECL bool Reset( ShapeArray * shape )
  {
    if( !shape || shape->Snips() != ( 1 << ArraySize ))
    {
      return false;
    }

    Clear();

    for( int loopShape = 0; loopShape < shape->Snips(); loopShape++ )
    {
      if( shape->GetFlag( loopShape ))
      {
        ByteIt shift = shape->Get( loopShape );
        array[ shift ] |= loopShape; // no boundary check needed
      }
    }

    return true;
  }

private:
  Array& array;
};


}; // namespace Its


#endif // __CUBE_UTILS_H
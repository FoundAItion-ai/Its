/*******************************************************************************
 FILE         :   BasicCube.h

 COPYRIGHT    :   DMAlex, 2011-2015

 DESCRIPTION  :   Basic Cube shared between Cuda and desktop

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/02/2011

 LAST UPDATE  :   09/23/2015
*******************************************************************************/

#ifndef __BASICCUBE_H
#define __BASICCUBE_H


#include "stdafx.h"
#include <math.h>
#include <stdlib.h>

#include "Init.h"
#include "Cube.h"
#include "BasicCubeDefines.h"

#include <exception>
#include "CubeUtils.h"
#include "tinymt64.h"

namespace Its
{

#define ARRAY_CELL( arrayname, sizeX, posX, posY ) \
    ((( Cell* )arrayname)[ (( posY ) * sizeX ) + ( posX ) ])

#define ARRAY_2D_CELL( arrayname, size, pos ) \
    ((( Cell* )arrayname)[ (( pos.Y ) * size.X ) + ( pos.X ) ])


#define COPY_DIRECTION_IN  0
#define COPY_DIRECTION_OUT 1

#define ISSIGNAL( signal ) ( signal > NOSIGNAL )
#define ISPASSINGSIGNAL( mask ) (( mask | cell.data ) == mask )
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))


inline bool BASICCUBE_SPEC CUDA_DECL operator == ( const Cell& a, const Cell& b )
{
  if( a.type       != b.type   ||
      a.data       != b.data   ||
      a.updata     != b.updata ||
      a.uplink     != b.uplink ||
      a.reflection != b.reflection ||
      a.deviation  != b.deviation )
  {
    return false;
  }

  for( SizeIt loopShape = 0; loopShape < ARRAY_SIZE( a.shape ); loopShape++ )
  {
    if( a.shape[ loopShape ] != b.shape[ loopShape ])
    {
      return false;
    }
  }

  for( SizeIt loopReflection = 0; loopReflection < ARRAY_SIZE( a.reflections ); loopReflection++ )
  {
    if( a.reflections[ loopReflection ] != b.reflections[ loopReflection ])
    {
      return false;
    }
  }

  for( SizeIt loopMemory = 0; loopMemory < ARRAY_SIZE( a.memory ); loopMemory++ )
  {
    if( a.memory[ loopMemory ] != b.memory[ loopMemory ])
    {
      return false;
    }
  }

  return true;
}


inline bool BASICCUBE_SPEC CUDA_DECL GetRandomLink( uint64_t random, Link * link )
{
    uint64_t choice = random % 0xFFFFFFFF;
    ByteIt shift = 0;

    if( choice > 0 && choice <= 0x1FFFFFFF )
    {
      shift = 0;
    }
    else if( choice > 0x1FFFFFFF && choice <= 0x3FFFFFFE )
    {
      shift = 1;
    }
    else if( choice > 0x3FFFFFFE && choice <= 0x5FFFFFFD )
    {
      shift = 2;
    }
    else if( choice > 0x5FFFFFFD && choice <= 0x7FFFFFFC )
    {
      shift = 3;
    }
    else if( choice > 0x7FFFFFFC && choice <= 0x9FFFFFFB )
    {
      shift = 4;
    }
    else if( choice > 0x9FFFFFFB && choice <= 0xBFFFFFFA )
    {
      shift = 5;
    }
    else if( choice > 0xBFFFFFFA && choice <= 0xDFFFFFF9 )
    {
      shift = 6;
    }
    else if( choice > 0xDFFFFFF9 && choice <= 0xFFFFFFFF )
    {
      shift = 7;
    }

    return Link::FromShift( shift, link );
}


// Adjustment range
// 
// when low is 1 and high is 255 there's no adjustments
struct BASICCUBE_SPEC Probe
{
  ByteIt   divertLow;
  ByteIt   divertHigh;
  ByteIt   mixLow;
  ByteIt   mixHigh;
  ByteIt   createLow;
  ByteIt   createHigh;

  // 25 Sep 2012 - DMA - TODO: move to main parameters if it works ok?
  PeriodIt cycle; // to support long term cyclic processes

  // 3 Feb 2017 - DMA - to support variable gravity force
  SizeIt posZ;
  SizeIt sizeZ;

  // 2 Mar 2017 - DMA - gravity mode for testing
  enum GravityMode
  {
    ConstantGravity,
    VariableGravity,
    NoGravity
  };

  GravityMode gravityMode;

  // Exclusive modes, flags are not needed
  enum TestMode
  {
    Disabled,
    DeadCells,
    InclusiveLinks
  };

  TestMode testMode;  // RT unit testing

  Probe():
    createLow   ( INITIAL_PROBE_CREATE_LOW  ),
    createHigh  ( INITIAL_PROBE_CREATE_HIGH ),
    divertLow   ( INITIAL_PROBE_DIVERT_LOW  ),
    divertHigh  ( INITIAL_PROBE_DIVERT_HIGH ),
    mixLow      ( INITIAL_PROBE_MIX_LOW     ),
    mixHigh     ( INITIAL_PROBE_MIX_HIGH    ),
    cycle       ( 0 ),
    posZ        ( 0 ),
    sizeZ       ( 0 ),
    gravityMode ( VariableGravity ),
    testMode    ( Disabled )
  {
  }

  // Do use default copy constructors

  CUDA_DECL bool operator  !() { return this->divertLow  > this->divertHigh  ||
                                        this->mixLow     > this->mixHigh     ||
                                        this->createLow  > this->createHigh; }


  CUDA_DECL void operator  = ( const Probe& that ) { this->divertLow   = that.divertLow;
                                                     this->divertHigh  = that.divertHigh;
                                                     this->mixLow      = that.mixLow;
                                                     this->mixHigh     = that.mixHigh;
                                                     this->createLow   = that.createLow;
                                                     this->createHigh  = that.createHigh;
                                                     this->cycle       = that.cycle;
                                                     this->posZ        = that.posZ;
                                                     this->sizeZ       = that.sizeZ;
                                                     this->gravityMode = that.gravityMode;
                                                     this->testMode    = that.testMode; }

};



//
// Cover "edges" on the sides
//
class BASICCUBE_SPEC BasicCube
{
public:
  //
  // size     - matrix layer size
  // index    - matrix layer index    (0 - size)
  // position - cell position in cube (0 - CubeSize)
  //
  CUDA_DECL BasicCube( void*                 _dataIn,  
                       void*                 _dataInU,  
                       void*                 _dataInD,                         
                       PointIt               _size,    
                       PointIt               _index,   
                       PointIt               _position ):
    size     ( _size        ),
    index    ( _index       ), 
    position ( _position    ),
    dataIn   ( 0 ),
    dataInU  ( 0 ),
    dataInD  ( 0 ),
    probe    ( 0 )
  {
    Init( _dataIn, _dataInU, _dataInD );
  }
    

  CUDA_DECL void Init( void* _dataIn, void* _dataInU, void* _dataInD )
  {
    if( _dataIn && _dataInU && _dataInD )
    {
      dataIn  = _dataIn;
      dataInU = _dataInU;
      dataInD = _dataInD;
    }
  }

  CUDA_DECL void SetProbe( Probe* _probe )
  {
    probe = _probe;
  }
   
  CUDA_DECL void It( void* dataOut, int gf, int random, Probe* _probe )
  {
    if( index.Inside( Zero2D, size ))
    {
      PointIt cellPos( position + One2D ); // in hypercube
   
      Cell&  cellIn  = GetHyperCube( cellPos ); 
      Cell&  cellOut = ARRAY_2D_CELL( dataOut, size, index );

      memcpy( &cellOut, (const void*)&cellIn, sizeof( Cell ));

      // Do bounds check or at least low < high check?
      //
      // 10 Apr 2012 - DMA - Yes, a must! Bounds check on upper level.
      // Looks like division by zero error muted on Cuda GPU.
      //
      SetProbe( _probe );
      BuildAndProcessPlane( gf, random, cellIn, cellOut, cellPos );
    }
  }


  CUDA_DECL static SizeItl CubeMemorySize( SizeIt sizeX, SizeIt sizeY ) 
  { 
    return sizeX * sizeY * sizeof( Cell ); 
  }

  // Choose direction based on:
  //
  // 1) gravity
  // 2) spatial symmetry (active downlinks)
  // 3) temporal symmetry (all downlinks)
  // 4) Z-axis would affect gravity/symmetry mixing proportion
  //
  // These would divert main gravity vector as a sum of vectors
  //
  CUDA_DECL bool NewLink( ByteIt cellType, int random, const Link & downlink, const Link & downSignal, const Link & lastDownlink, Link * link )
  {
    // 21 Oct 2016 - DMA - At the end we don't care about convergence, only about diversity
    if( cellType == S_CELL )
    {
      return GetLinearRandomLink( random, link );
    }

    int randomX = ( random & 0x0000FFFF );
    int randomY = ( random & 0xFFFF0000 ) >> 16;
    Link gravityLink;

    // Gravity
    if( !link || !GravityLink( cellType, random, randomX, randomY, &gravityLink ))
    {
      return false;
    }

    // Mixing in spatial symmetry, 50/50 with gravity
    Link link1, link2;
    if( !downSignal.Reduce( &link1, &link2 ))
    {
      return false;
    }

    *link = ( link1 + link2 ).Mirror();
    *link += gravityLink;

    if( !link->Reduce( &link1, &link2 ))
    {
      return false;
    }

    *link = randomX % 2 == 0 ? link1 : link2;

    PointIt point;
    if( !link->IsEmpty() && !link->ToPoint( &point ))
    {
      return false;
    }

    // Hit center?
    if( point.X == 0 && point.Y == 0 )
    {
      if( !GetLinearRandomLink( random, link ) || !link->ToPoint( &point ))
      {
        return false;
      }
    }

    CU_ASSERT( point.X != 0 || point.Y != 0 );

    // Border control:
    //
    // 1) On the border
    if( index.X == 0 )
    {
      point.X = -1;
    }

    if( index.X == size.X - 1 )
    {
      point.X = 1;
    }

    if( index.Y == 0 )
    {
      point.Y = -1;
    }

    if( index.Y == size.Y - 1 )
    {
      point.Y = 1;
    }
    
    // 2) Close to the border
    if( index.X == 1 && point.X > 0 || index.X == size.X - 2 && point.X < 0 )
    {
      point.X = -point.X;
    }
   
    if( index.Y == 1 && point.Y > 0 || index.Y == size.Y - 2 && point.Y < 0 )
    {
      point.Y = -point.Y;
    }

    return Link::FromPoint( PointIt( -point.X, -point.Y ), link );
  }

  // 1-100
  CUDA_DECL static int GravityForce( Probe* _probe )
  {
    int force = ( _probe->posZ * 100 ) / _probe->sizeZ;
    force *= force;
    force /= 100;
    CU_ASSERT( force >= 0 && force <= 100 );
    return force;
  }

  // Accesssors
  PointIt Position() { return position; }
  PointIt Index()    { return index;    }
  PointIt Size()     { return size;     }

protected:
  CUDA_DECL void BuildAndProcessPlane( int gf, int random, Cell& cellIn, Cell& cellOut, const PointIt& at )
  {
    ByteIt reflectedMode = NOSIGNAL;
    ByteIt mode = NOSIGNAL;
    Link   downlink;
    Link   downSignal;
    Link   reflection;

    BuildPlane( at, &downlink, &downSignal, mode, reflectedMode, &reflection );
    ProcessPlane( gf, random, cellOut, downlink, downSignal, reflection, mode, reflectedMode, at );
  }

  CUDA_DECL uint64_t GetLinearRandom( int random, PosIt positionLimit )
  {
    using namespace DCommon;

    // 11 Apr 2014 - DMA - Do not use just random as we need dimensional volatility
    tinymt64_t tinymt;
    tinymt.mat1 = index.X;
    tinymt.mat2 = index.Y;
    tinymt.tmat = positionLimit;

    // Tiny Mersenne Twister pseudo random
    tinymt64_init( &tinymt, random );

    return tinymt64_generate_uint64( &tinymt );
  }

  CUDA_DECL bool GetLinearRandomLink( int random, Link * link )
  {
    uint64_t linearRandom = GetLinearRandom( random, size.X * size.Y );
    return GetRandomLink( linearRandom, link );
  }

  CUDA_DECL int G( int random, PosIt position, PosIt positionLimit )
  {
    uint64_t choice = GetLinearRandom( random, positionLimit ) % 0xFFFFFFFF;

    // Default probability spread 80 / 10 / 10
    //
    // 1 Nov 2016 - DMA - dynamic settings with probe disabled due to potential performance impact
    // and they are not really usefull anyway.
    //
    // int probabilityIn   = probe ? probe->divertLow  : INITIAL_PROBE_DIVERT_LOW;
    // int probabilityStay = probe ? probe->divertHigh : INITIAL_PROBE_DIVERT_HIGH;
    uint64_t probabilityStay = 0xFFFFFFFF - 0x19999999;
    uint64_t probabilityIn   = 0xFFFFFFFF - 0x19999999 - 0x19999999;

    return choice < probabilityIn ? -1 : ( choice > probabilityStay ? 0 : 1 );
  }

  CUDA_DECL bool GravityLink( ByteIt cellType, int random, int randomX, int randomY, Link * link )
  {
    if( probe )
    {
      if( probe->gravityMode == Probe::NoGravity )
      {
        *link = Link();
        return true;
      }

      if( probe->gravityMode == Probe::VariableGravity )
      {
        // GravityForce, 0-100 %
        if( random > ( INT_MAX / 100 ) * GravityForce( probe ))
        {
          *link = Link();
          return true;
        }
      }
    }

    // 20 Mar 2014 - DMA - Fixed due to the issue with interdirectional dependency.
    // assume 0 < random < UINT_MAX = 0xFFFFFFFF
    PosIt  endX   = ( size.X / 2 ) - index.X;
    PosIt  endY   = ( size.Y / 2 ) - index.Y;
    PosIt  deltaX = endX > 0 ? G( randomX, index.X, size.X ) : -G( randomX, index.X, size.X );
    PosIt  deltaY = endY > 0 ? G( randomY, index.Y, size.Y ) : -G( randomY, index.Y, size.Y );

    if( deltaX == 0 && deltaY == 0 )
    {
      *link = Link();
      return true;
    }

    return Link::FromPoint( PointIt( deltaX, deltaY ), link );
  }

  /*
  
    31 Jan 2013 - DMA - The idea:

    1) Each cell reflects its input(s) back

    2) Each cell can reconnect if it doesn't like its own reflection or 
    global feedback (below)

    3) G force tries to keep all cells together and merge them into 
    singular cells

    4) Global feedback tells if the whole cells community, maiTrix is stable 
    and reached singularity

    5) On every input maiTrix reacts (with reflection) till singularity is reached
  

    29 Aug 2013 - DMA - Memoirs. When maiTrix is stable we can memorize 
    color and shape:

    6) Shape is a map of stable input directions (all input is the same, 
    different directions) into a single 'good' direction, 256 -> 8.

    7) Color is a map of input levels (not directions, just different values, like
    we have 3 X and 2 Y => XY) into 'bad directions', 256 -> 8 for 8 colors.

    We either memorize shape or (exclusive) color.
  

    10 Jul 2014 - DMA - Reflections.

    There should be reflections from every single level to the very top (input).
    They should be almost identical from close levels and may be more distorted as
    we go deeper. However if maiTrix is perfect (absolutely stable) all reflections
    should be identical.

    27 May 2015 - DMA - Axial reflections.

    1) Remove color as cell's attribute, replace it with reshape. 
    Use color to indicate changing nodes.
    2) We should send reflections in all connected directions for the same
    target vector, for example:
    
    -We have 6 connected links 00011111;
    -If shape 00010011 is compressed => 01000000 and reflected back as 00010011 
    then if another shape 00000100 is also compressed => into the same 01000000
    we will reflect it as mixed 00000100 + 00010011 = 00010111

    They should adhere to "second reflection" rule - while first reflection may
    or may not be equal to input but second reflection should be very close
    to the first one as our understanding of the situation should not be 
    changing that fast;

  */  
  CUDA_DECL void ProcessPlane( int gf, int random, Cell& cellOut, const Link & downlink, const Link & downSignal,
    const Link & reflection, ByteIt mode, ByteIt reflectedMode, const PointIt& at )
  {
    ByteIt cellType = cellOut.type;
    // NO_CELL   - no passing signals in this run
    // GOOD_CELL - passing signals in this run
    // BAD_CELL  - passing signals and downlinks > 1?
    // DEAD_CELL - internal processing error
    //
    // TODO: rename BAD_CELL -> NEW_CELL
    //
    // Zero overhead.
    MemoryArray     memoryLesson ( cellOut.memory, /*index=*/ 0 );
    MemoryArray     memoryCycle  ( cellOut.memory, /*index=*/ 1 );
    Link            lastDownlink ( memoryLesson.Get()  ); // Set during the lesson
    ReflectionArray reflections  ( cellOut.reflections ); // uplink -> reflectedMode
    ShapeArray      shape        ( cellOut.shape       ); // uplink -> shape

    // Positive feedback
    if( gf > 0 )
    {
      ClearCell( cellOut );
      return;
    }

    // Negative feedback?
    if( gf < 0 )
    {
      if( !lastDownlink.IsEmpty())
      {
        if( cellType == BAD_CELL )
        {
          // Clear wrong link and recalculate reflections
          shape.ClearLink( lastDownlink );
          reflections.Reset( &shape );  // Can probably set DEAD_CELL in case of error, but seems too much
        }
        else
        {
          // Clear wrong deviation????????????
          cellOut.deviation = INITIAL_DEVIATION;
        }
      }

      ClearCell( cellOut );
      return;
    }

    // Neutral feedback, normal flow
    if( IO_CELL( cellType ))
    {
      Link ioUplink( cellOut.uplink );
      cellOut.updata      = reflectedMode;
      cellOut.reflection |= reflectedMode;  // Not really needed, just for better visibility in image dumps
      
      if( ISSIGNAL( cellOut.data ) && !ioUplink.IsEmpty())
      {
        if( !shape.SetLink( ioUplink, ioUplink ) || !reflections.Set( ioUplink, ioUplink ))
        {
          cellOut.type = DEAD_CELL;
        }
      }
      return;
    }

    if( cellType == DEAD_CELL || !ISSIGNAL( mode ) || downlink.IsEmpty())
    {
      cellOut.data       = NOSIGNAL;
      cellOut.updata     = reflectedMode;
      cellOut.reflection = reflection.Raw();

      memoryCycle.Clear();
      CU_ASSERT( cellType != S_CELL || ( cellType == S_CELL && reflectedMode == NOSIGNAL && reflection.IsEmpty()));
      return;
    }

    if( cellType == S_CELL )
    {
      // 26 Jul 2017 - DMA - Change response range from 8 to 256 (based on down signal).
      // This is a bit different from how it was set before, not accounting for temporal symmetry,
      // for all downlinks, only active downlinks as "uplink" (read: "data") is completely unique
      // in this context.
      //
      // 29 Aug 2017 - DMA - Change response range 256 to 64 to reduce directional variaty
      cellOut.data       = downlink.Raw() % 64 + 1;
      cellOut.updata     = SIGNAL;
      cellOut.uplink     = INITIAL_UPLINK;
      cellOut.reflection = downlink.Raw();
      CU_ASSERT( reflection.IsEmpty());      
      return;
    }

    Link      uplink( cellOut.uplink );
    ByteIt    newType       = GOOD_CELL;
    bool      linkFound     = false;
    bool      result        = shape.GetLink( downlink, &uplink );
    uint64_t  linearRandom  = GetLinearRandom( random, size.X * size.Y );
    bool      linkInclusive = linearRandom % 2 == 0;

    if( probe && probe->testMode == Probe::InclusiveLinks )
    {
      // Unit testing only, make LabyrinthTwoReflectedDots test stable
      linkInclusive = true;
    }

    if( !result )
    {
      // Never tried that direction?
      //
      // Check if downlink is inclusive, from one of already reflected directions
      // starting with the current one
      if( reflections.Find( downlink, &uplink, &linkFound ))
      {
        // Follow G and asymmetry for new, non inclusive, links.
        //
        // 6 Sep 2017 - DMA - Switch to random inclusiveness model - if link was part of the reflection
        // before use it on par with new direction for flexibility.
        if(( linkFound && linkInclusive ) || NewLink( cellType, random, downlink, downSignal, lastDownlink, &uplink ))
        {
          newType = BAD_CELL;
          result  = shape.SetLink( downlink, uplink ) && reflections.Set( uplink, downlink );
        }
      }
    }

    // Mix reflections sent from above with our own
    Link reflectedUplink;
    result = result && reflections.Get( uplink, &reflectedUplink );
    CU_ASSERT( !reflectedUplink.IsEmpty());

    if( result && probe && probe->testMode == Probe::DeadCells )
    {
      // Unit testing only, make some cells noticeable
      result = linearRandom % 5 == 0 ? false : true;
    }

    if( !result )
    {
      newType = DEAD_CELL;
    }

    // Temporal symmetry, deviation.
    if( memoryCycle.Get() == downlink )
    {
      CU_ASSERT( newType != BAD_CELL );
      cellOut.deviation = random % 2 == 0; // TODO - need better, full range, logic
      uplink = uplink.Rotate( cellOut.deviation );
    }

    // Case when we see positive reflectedMode signal here is inclusive
    // as we're reflecting updata anyway.
    cellOut.type       = newType;
    cellOut.data       = SIGNAL;
    cellOut.updata     = SIGNAL;
    cellOut.uplink     = uplink.Raw();
    cellOut.reflection = ( reflectedUplink + reflection ).Raw();

    memoryCycle.Set( downlink );
    memoryLesson.Set( downlink );
  }


  CUDA_DECL void ClearCell( Cell& cellOut )  
  {
    MemoryArray memory( cellOut.memory );
    memory.ClearAll();

    cellOut.data       = INITIAL_DATA;
    cellOut.updata     = INITIAL_DATA;
    cellOut.reflection = INITIAL_REFLECTION;

    if( cellOut.type == GOOD_CELL || cellOut.type == BAD_CELL )
    {
      cellOut.type   = NO_CELL;
      cellOut.uplink = INITIAL_UPLINK;
    }
  }


  // 5 x 5
  CUDA_DECL Cell& GetHyperCube( const PointIt& at, const PointIt& offset = Zero2D ) 
  {
    PointIt dataPos( index + offset ); // in matrix  
    return ARRAY_2D_CELL( dataIn, size, dataPos );
  }
  

  // 5 x 5 x 3
  CUDA_DECL Cell& GetHyperCube3D( const PointIt& at, PosIt offsetX, PosIt offsetY, PosIt offsetZ ) 
  {
    PointIt offset( offsetX, offsetY );
    PointIt dataPos( index + offset ); // in matrix  

    if( offsetZ > 0 )
    {
      return ARRAY_2D_CELL( dataInU, size, dataPos );
    }
    else if( offsetZ < 0 )
    {
      return ARRAY_2D_CELL( dataInD, size, dataPos );
    }

    return ARRAY_2D_CELL( dataIn, size, dataPos );
  }
 

  CUDA_DECL void BuildPlane( const PointIt& at, Link * downLink, Link * downSignal, ByteIt& mode, ByteIt& reflectedMode, Link * reflection )
  {
    Cell& cell( GetHyperCube( at ));
    ReflectionArray reflections( cell.reflections );
    Link reflectedLink;

    mode = reflectedMode = NOSIGNAL;
    *downLink   = Link(); // incoming links with signal
    *reflection = Link(); // actual reflection with signal
    *downSignal = Link(); // all down links with signal, including not connected

    for( PosIt offsetX = -1; offsetX <= 1; offsetX++ )
    {
      for( PosIt offsetY = -1; offsetY <= 1; offsetY++ )
      {
        // abs position in 2D matrix projection
        PointIt dataPos( index.X + offsetX, index.Y + offsetY );
        Link oneLink;

        // Check if within the matrix boundaries
        if( !dataPos.Inside( Zero2D, size ))
        {
          continue;
        }
        
        // mirrored
        if( !Link::FromPoint( PointIt( -offsetX, -offsetY ), &oneLink ))
        {
          continue;
        }

        // For top and bottom layers we read 'voided' stub layers next to them, 
        // containing nothing, so mode & reflectedMode will be 0.
        Cell& downCell ( GetHyperCube3D( at, offsetX, offsetY, -1 ));
        Cell& upCell   ( GetHyperCube3D( at, offsetX, offsetY,  1 ));        

        if( Link( downCell.uplink ) == oneLink && ISSIGNAL( downCell.data ))
        {
          *downLink += oneLink.Mirror();
          mode = SIGNAL;
        }

        if( ISSIGNAL( downCell.data ))
        {
          *downSignal += oneLink.Mirror();
        }

        // There is diamond-shape reflection problem:
        //
        //            (1)
        //             |
        //            (1)
        //           /   \
        //         (1)    (1)   (0)
        //          a\   /b     /c
        //            (1) -----/
        //            /  \
        //          (1)  (0)
        //
        // when we should not calculate reflections based on uplink only (upCell.uplink, marked 'a')
        // but on both uplink and ALL active upper reflections 'a' & 'b'.
        // Below we calculate upper reflections, reflection from uplink may not be determined
        // here as uplink is not known yet and will be assigned in ProcessPlane later.
        if( cell.type == S_CELL || !ISSIGNAL( upCell.updata ) || !Link( upCell.reflection ).HasLink( oneLink ))
        {
          continue;
        }

        if( IO_CELL( cell.type ))
        {
          reflectedMode = SIGNAL;
          continue;
        }
        
        if( reflections.Get( oneLink.Mirror(), &reflectedLink ) && !reflectedLink.IsEmpty())
        {
          *reflection  += reflectedLink;
          reflectedMode = SIGNAL;
          continue;
        } 

        // There should be no such situation as we have reflection but never been uplinked
        // to that direction.
        //
        // 9 May 2018 - DMA - for temporal symmetry uplink shift there may be such situation.
        // CU_ASSERT( false );
      }
    }
  }


  CUDA_DECL int GetNormalizedRandom( int random, int range_min, int range_max ) 
  {
    float normalizedRandomFloat = ((( float )random / ( RAND_MAX + 1 )) * ( range_max - range_min ) + range_min );
    int   normalizedRandomInt;

    //
    // 10 Feb 2012 - DMA - Doesn't help much
    //
    //#ifdef OPTIMIZE_IT
    //normalizedRandomInt = (( normalizedRandomFloat - ( int )normalizedRandomFloat ) == 0 ? 
    //                       ( int ) normalizedRandomFloat : ( int ) normalizedRandomFloat + 1 );
    //#else
      #if defined(__CUDA_ARCH__)
        normalizedRandomInt = float2int( normalizedRandomFloat, cudaRoundNearest );
      #else
        normalizedRandomInt = ( int )ceil( normalizedRandomFloat );
      #endif
    //#endif

    return normalizedRandomInt;
  }


  CUDA_DECL int GetLinearizedRandom( int random, int range_min, int range_max ) 
  {
    // 11 Apr 2012 - DMA - There may be division by zero error below
    // so check range, effectively - probe, validity on upper level due to performance 
    // penalty here.
    //
    int linearIndex     = index.Y * size.X + index.X;
    int linearizedRange = linearIndex % ( range_max - range_min + 1 );
    int normalRandom    = GetNormalizedRandom( random, range_min, range_min + linearizedRange );

    return normalRandom;
  }


  // Used internally, do not export
  CUDA_DECL inline int BitCount( ByteIt c )
  { 
    // Tom St Denis 
    unsigned char s;

    s  = (  c & 0x11 );
    s += (( c >> 1 ) & 0x11 );
    s += (( c >> 2 ) & 0x11 );
    s += (( c >> 3 ) & 0x11 );

    return ( s & 15 ) + ( s >> 4 );
  }

private:
  void*    dataIn;
  void*    dataInU;
  void*    dataInD;
  Probe*   probe;

  PointIt  position; // cell position in cube (0 - CubeSize)
  PointIt  index;    // matrix layer index    (0 - size)
  PointIt  size;     // matrix layer size
};


// Cube statistics
//
struct BASICCUBE_SPEC CubeStat
{
  CubeStat()
  {
    cellsSin  = 0;
    cellsNo   = 0;
    cellsGood = 0;
    cellsBad  = 0;
    cellsDead = 0;
    cellsIOa  = 0;
    cellsIOv  = 0;
    cellsIOt  = 0;
  
    levelAverage           = 0;
    levelCellsWithNoData   = 0;
    levelCellsWithNoSignal = 0;
    levelCellsAboveMedian  = 0;

    cellsWithData          = 0;
    cellsInputWithData     = 0;
  }
  
  // Cell counters
  SizeItl cellsSin;
  SizeItl cellsNo;
  SizeItl cellsGood;
  SizeItl cellsBad;
  SizeItl cellsDead;

  SizeItl cellsIOa;
  SizeItl cellsIOv;
  SizeItl cellsIOt;

  // Levels
  ByteIt  levelAverage;
  SizeItl levelCellsWithNoData;   // data == INITIAL_DATA
  SizeItl levelCellsWithNoSignal; // data == NOSIGNAL
  SizeItl levelCellsAboveMedian;  // data > 127 = ( max - min / 2 )  
  
  SizeItl cellsWithData;          // All cells with data > NOSIGNAL
  SizeItl cellsInputWithData;     // All input cells with data > NOSIGNAL
  
  // Calculated fields
  SizeItl CellsTotal() 
  {
    return cellsSin  + cellsNo   + cellsGood  + cellsBad +
           cellsIOa  + cellsIOv  + cellsIOt   + cellsDead;
  }

  SizeItl CellsTotalSignificant() 
  {
    return CellsTotal() - cellsNo;
  }

  SizeItl CellsTotalInput() 
  {
    return cellsIOa + cellsIOv + cellsIOt;
  }

  int PercentageSignificant( SizeItl value ) 
  {
    return Percentage( value, CellsTotalSignificant() );
  }

  int PercentageTotal( SizeItl value ) 
  {
    return Percentage( value, CellsTotal() );
  }

  int PercentageTotalInput( SizeItl value ) 
  {
    return Percentage( value, CellsTotalInput() );
  }

  int PercentageMeaningfulInput( SizeItl value ) 
  {
    return Percentage( value, cellsInputWithData );
  }

  int Percentage( SizeItl value, SizeItl total ) 
  {
    int percentage = 0;

    if( total != 0 )
    {
      percentage = ( int )(( value * 100.0 ) / ( float ) total );
    }

    return percentage;
  }

  CUDA_DECL CubeStat operator + ( const CubeStat& that ) 
  { 
    CubeStat result = *this;
    
    result.cellsNo                += that.cellsNo;
    result.cellsGood              += that.cellsGood;
    result.cellsBad               += that.cellsBad;
    result.cellsDead              += that.cellsDead;
    result.cellsSin               += that.cellsSin;
    result.cellsIOa               += that.cellsIOa;
    result.cellsIOv               += that.cellsIOv;
    result.cellsIOt               += that.cellsIOt;
          
    // Do not add average due to out-of-range error.
    //              
    result.levelCellsWithNoData   += that.levelCellsWithNoData;
    result.levelCellsWithNoSignal += that.levelCellsWithNoSignal;
    result.levelCellsAboveMedian  += that.levelCellsAboveMedian;

    result.cellsInputWithData     += that.cellsInputWithData;
    result.cellsWithData          += that.cellsWithData;

    return result; 
  }

  CUDA_DECL void operator = ( const CubeStat& that ) 
  { 
    this->cellsNo                = that.cellsNo;
    this->cellsGood              = that.cellsGood;
    this->cellsBad               = that.cellsBad;
    this->cellsDead              = that.cellsDead;
    this->cellsSin               = that.cellsSin;
    this->cellsIOa               = that.cellsIOa;
    this->cellsIOv               = that.cellsIOv;
    this->cellsIOt               = that.cellsIOt;
    
    this->levelAverage           = that.levelAverage;
    this->levelCellsWithNoData   = that.levelCellsWithNoData;
    this->levelCellsWithNoSignal = that.levelCellsWithNoSignal;
    this->levelCellsAboveMedian  = that.levelCellsAboveMedian;

    this->cellsInputWithData     = that.cellsInputWithData;
    this->cellsWithData          = that.cellsWithData;
  }
};


}; // namespace Its

#endif // __BASICCUBE_H

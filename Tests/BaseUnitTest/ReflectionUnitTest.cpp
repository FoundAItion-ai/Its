/*******************************************************************************
 FILE         :   ReflectionUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - reflection unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   04/17/2014
*******************************************************************************/


#include "stdafx.h"
#include <functional>

#include "BaseUnitTest.h"
#include "MaiTrix.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


namespace BaseUnitTest
{		
  class BasicCubeReflection : public BasicCube
  {
  public:
    typedef ByteIt ByteCube[ CubeSize ][ CubeSize ];
    typedef BasicCube Base;

    BasicCubeReflection( void*     _dataIn,  
                         void*     _dataInU,  
                         void*     _dataInD,                         
                         void*     _dataOut,
                         PointIt   _size,
                         PointIt   _index,   
                         PointIt   _position,
                         ByteCube& _uplinks,
                         ByteCube& _data,
                         Probe*    _probe ) :
    BasicCube( _dataIn, _dataInU, _dataInD, _size, _index, _position ),
    uplinks( _uplinks ),
    data( _data ),
    dataIn( _dataIn ),
    dataOut( _dataOut )
    {
      SetProbe( _probe );
    }

  public:

    void ItTest( Cell& _cell, bool copyShape = false )
    {
      int gf = GF::Base;
      int random = 91234567;
      SizeIt2D  layer_size( CubeSize, CubeSize );
      PointIt2D position( 1, 1 );
      Cell& cellIn  = ARRAY_2D_CELL( dataIn,  layer_size, position );
      Cell& cellOut = ARRAY_2D_CELL( dataOut, layer_size, position );

      // Preserve state of memory in between calls.
      memcpy( &cellIn.memory, &_cell.memory, sizeof( _cell.memory ));

      if( copyShape )
      {
        // For temporal symmetry test we need to preserve state
        memcpy( &cellIn.shape,  &_cell.shape,  sizeof( _cell.shape  ));
        memcpy( &cellIn.reflections,  &_cell.reflections,  sizeof( _cell.reflections  ));
      }

      It( dataOut, gf, random, NULL );
      memcpy( &_cell, &cellOut, sizeof( _cell ));
    }

  private:
    ByteCube& uplinks;
    ByteCube& data;
    void*     dataIn;
    void*     dataOut;
  };


  TEST_CLASS(ReflectionUnitTest), BaseTest
  {
  public:
    typedef BasicCubeReflection::ByteCube ByteCube;
    typedef function<void ( BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data )> 
      BasicCubeTestCallback;
   
    TEST_METHOD(Mirroring)
    {
      WriteMessage( "Mirroring" );

      ByteCube data0        = { { 0, 0, 0 }, { 0, 0, 0 }, { 0,    0, 0    } }; // D level
      ByteCube data1        = { { 1, 0, 0 }, { 0, 0, 0 }, { 0,    0, 0    } }; // D level
      ByteCube data7        = { { 7, 7, 7 }, { 7, 7, 7 }, { 7,    7, 7    } }; // D level
      ByteCube reflections1 = { { 1, 1, 1 }, { 1, 1, 1 }, { 1,    1, 1    } }; // main level
      ByteCube reflections2 = { { 1, 1, 1 }, { 1, 1, 1 }, { 1,    1, 0xFF } }; // main level
      ByteCube reflections3 = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 0xFF, 1    } }; // main level
      ByteCube reflections0 = { { 0, 0, 0 }, { 0, 0, 0 }, { 0,    0, 0    } }; // main level

      // 1) Uplink is not changed when there's no input
      for( ByteIt shift = 0; shift < 8; shift++ )
      {
        ByteIt uplink = ( 0x01 << shift ); // main level

        // Only uplink may be changed
        ValidateReflection( data0, reflections1, GOOD_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            Assert::IsTrue( cellOut.type == GOOD_CELL );
            Assert::IsTrue( cellOut.data == NOSIGNAL );
            Assert::IsTrue( cellOut.updata == NOSIGNAL );
            Assert::IsTrue( cellOut.uplink != 0 );
            Assert::IsTrue( cellOut.uplink == uplink );
            Assert::IsTrue( cellOut.reflection == 0 );  // don't expect reflections from above, only from ourself
          }});

        // Only uplink may be changed for IOCELL
        ValidateReflection( data0, reflections1, A_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            Assert::IsTrue( cellOut.type == A_CELL );
            Assert::IsTrue( cellOut.data == NOSIGNAL );
            Assert::IsTrue( cellOut.updata == NOSIGNAL );
            Assert::IsTrue( cellOut.uplink != 0 );
            Assert::IsTrue( cellOut.uplink == uplink );
            Assert::IsTrue( cellOut.reflection == 0 );  // don't expect reflections from above, only from ourself
          }});
      }

      // As we generate new uplink based on random (rf) and it's fixed in ItTest(),
      // so our uplink is also constant.
      static ByteIt fixedUplink = 4;

      // 2) New uplink is generated (fixed, see above)
      ValidateReflection( data7, reflections1, NO_CELL, 0, [](
        BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
        {
          Cell cellOut = { 0 };
          basic_cube.ItTest( cellOut );
          Assert::IsTrue( cellOut.type == BAD_CELL );
          Assert::IsTrue( cellOut.data == SIGNAL );
          Assert::IsTrue( cellOut.updata == SIGNAL );
          Assert::IsTrue( cellOut.uplink == fixedUplink );
          Assert::IsTrue( cellOut.reflection == 0xFF );
        }});

      for( ByteIt shift = 0; shift < 8; shift++ )
      {
        ByteIt uplink = ( 0x01 << shift ); // main level

        // 2) New uplink is generated no matter where it was pointing before
        ValidateReflection( data7, reflections1, NO_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            Assert::IsTrue( cellOut.type == BAD_CELL );
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.uplink == fixedUplink );
            Assert::IsTrue( cellOut.reflection == 0xFF );
          }});

        // 3) Exisitng uplink is found
        // Note that while it depends on random but with random = 0
        // linearRandom % 2 == 0 and we don't generate new uplink.
        ValidateReflection( data7, reflections2, NO_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            
            Assert::IsTrue( cellOut.type == BAD_CELL );
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.reflection == 0xFF );

            // reflection exists for downlink from the direction (0xFF), so uplink found and set to 0x01 << 0
            Assert::IsFalse( fixedUplink   == ( 0x01 << 0 ));
            Assert::IsTrue( cellOut.uplink == ( 0x01 << 0 ));
          }});

        // 4) Another exisitng uplink is found
        ValidateReflection( data7, reflections3, NO_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            Assert::IsTrue( cellOut.type == BAD_CELL ); 
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.reflection == 0xFF );

            // another reflection exists for downlink from the direction (0xFF), so uplink found (mirror to 0x01 << 0)
            Assert::IsFalse( fixedUplink   == ( 0x01 << 1 ));
            Assert::IsTrue( cellOut.uplink == ( 0x01 << 1 ));
          }});

        // 5) Temporal symmetry
        ValidateReflection( data1, reflections0, NO_CELL, uplink, [](
          BasicCubeReflection& basic_cube, ByteIt uplink, ByteCube& uplinks, ByteCube& data ) { 
          {
            Cell cellOut = { 0 };
            basic_cube.ItTest( cellOut );
            Assert::IsTrue( cellOut.type == BAD_CELL );
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.uplink == 8 );
            Assert::IsTrue( cellOut.reflection == 1 );

            basic_cube.ItTest( cellOut, /*copyShape=*/true );
            ByteIt uplink = cellOut.uplink;
            Assert::IsTrue( cellOut.type == GOOD_CELL );
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.uplink != 8 );
            Assert::IsTrue( cellOut.reflection == 1 );

            basic_cube.ItTest( cellOut, /*copyShape=*/true );
            Assert::IsTrue( cellOut.type == GOOD_CELL );
            Assert::IsTrue( cellOut.data == SIGNAL );
            Assert::IsTrue( cellOut.updata == SIGNAL );
            Assert::IsTrue( cellOut.uplink == uplink );
            Assert::IsTrue( cellOut.reflection == 1 );
            }});
      }
    }

  private:
    void ValidateReflection( ByteCube& data, ByteCube& reflections, ByteIt type, ByteIt uplink, BasicCubeTestCallback cube_callback )
    {
      SizeIt2D  layer_size( CubeSize, CubeSize );
      Celllayer layerU   ( "UnitTest", layer_size, 2, 1 );
      Celllayer layer    ( "UnitTest", layer_size, 1, 1 );
      Celllayer layerD   ( "UnitTest", layer_size, 0, 1 );
      Celllayer layerOut ( "UnitTest", layer_size, 1, 1 );

      for( PosIt loopPositionX = 0; loopPositionX < CubeSize; loopPositionX++ )
      {
        for( PosIt loopPositionY = 0; loopPositionY < CubeSize; loopPositionY++ )
        {
          PointIt2D position( loopPositionX, loopPositionY );
          Cell& cellU   = ARRAY_2D_CELL( layerU,   layerU.size, position );
          Cell& cell    = ARRAY_2D_CELL( layer,    layer.size,  position );
          Cell& cellD   = ARRAY_2D_CELL( layerD,   layerD.size, position );
          Cell& cellOut = ARRAY_2D_CELL( layerOut, layerD.size, position );

          InitCell( &cellU   );
          InitCell( &cell    );
          InitCell( &cellD   );
          InitCell( &cellOut );

          Link oneLink;
          if( Link::FromPoint( PointIt( -loopPositionX + 1, -loopPositionY + 1 ), &oneLink ))
          {
            cellD.data = data[ loopPositionX ][ loopPositionY ];
            cellD.uplink = oneLink.Raw();
          }
          else
          {
            cell.uplink = uplink;
            cell.type   = type;
          }

          cellU.updata = NOSIGNAL;
          cellU.reflection = 0xFF;
        }
      }

      for( PosIt loopPositionX = 0; loopPositionX < CubeSize; loopPositionX++ )
      {
        for( PosIt loopPositionY = 0; loopPositionY < CubeSize; loopPositionY++ )
        {
          Cell& cell = ARRAY_2D_CELL( layer, layer.size, PointIt2D( 1, 1 ));

          Link oneLink;
          if( Link::FromPoint( PointIt( -loopPositionX + 1, -loopPositionY + 1 ), &oneLink ))
          {
            ByteIt shift;
            Assert::IsTrue( oneLink.ToShift( &shift ));
            cell.reflections[ shift ] = reflections[ loopPositionX ][ loopPositionY ];
          }
        }
      }

      PointIt position( 0, 0 );
      PointIt at( position + One2D ); // in hypercube
      ByteCube uplinks = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; // dummy
      Probe default_probe;

      BasicCubeReflection basic_cube( (Cell*)layer, (Cell*)layerU, (Cell*)layerD, (Cell*)layerOut,
        PointIt( 3, 3 ), PointIt( 1, 1 ), position, uplinks, data, &default_probe );

      cube_callback( basic_cube, uplink, uplinks, data );
    }


    void InitCell( Cell * cell )
    {
      cell->type       = INITIAL_TYPE;
      cell->data       = INITIAL_DATA;
      cell->updata     = INITIAL_DATA;
      cell->uplink     = INITIAL_UPLINK;
      cell->reflection = INITIAL_REFLECTION;

      memset( &cell->shape,       INITIAL_DATA, sizeof( cell->shape       ));
      memset( &cell->reflections, INITIAL_DATA, sizeof( cell->reflections ));
      memset( &cell->memory,      INITIAL_DATA, sizeof( cell->memory      ));
    }
  };
}
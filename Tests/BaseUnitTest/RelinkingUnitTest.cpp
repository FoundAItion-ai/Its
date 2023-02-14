/*******************************************************************************
 FILE         :   RelinkingUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - relinking unit tests

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
  class BasicCubeRelinking : public BasicCube
  {
  public:
    typedef ByteIt ByteCube[ CubeSize ][ CubeSize ];
    typedef BasicCube Base;

    BasicCubeRelinking( void*     _dataIn,
                        void*     _dataInU,  
                        void*     _dataInD,                         
                        PointIt   _size,    
                        PointIt   _index,   
                        PointIt   _position ):
    BasicCube( _dataIn, _dataInU, _dataInD, _size, _index, _position )
    {}

    void Relink( Cell& cell, int random )
    {
      Link link;
      Link downlink( 0 );     // no spatial asymmetry, gravity only
      Link downSignal( 0 );   // -- " -- " --
      Link lastDownlink( 0 ); // for temporal symmetry
      Assert::IsTrue( Base::NewLink( cell.type, random, downlink, downSignal, lastDownlink, &link ));
      cell.uplink = link.Raw();
    }
  };


  TEST_CLASS(RelinkingUnitTest), BaseTest
  {
  public:
    typedef BasicCubeRelinking::ByteCube ByteCube;
    typedef function<void(BasicCubeRelinking& basic_cube, ByteCube& uplinks, ByteCube& data)>
      BasicCubeTestCallback;
   
    typedef int expected_t[3][3];

    void swap_two( expected_t& expected, expected_t& edge_expected, 
      PosIt fromX, PosIt fromY, PosIt toX, PosIt toY )
    {
      swap( expected[ fromX ][ fromY ], expected[ toX ][ toY ] );
      swap( edge_expected[ fromX ][ fromY ], edge_expected[ toX ][ toY ] );
    }
    
    
    TEST_METHOD(Relinking)
    {
      WriteMessage( "Relinking" );

      PointIt size( 50, 50 );
      PointIt index_left_top( 10, 10 );
      PointIt index_left_top_edge( 1, 1 );
      PointIt index_right_top( 40, 10 );
      PointIt index_right_top_edge( 48, 1 );
      PointIt index_left_bottom( 10, 40 );
      PointIt index_left_bottom_edge( 1, 48 );      
      PointIt index_right_bottom( 40, 40 );
      PointIt index_right_bottom_edge( 48, 48 );

      int expectedIn   = INITIAL_PROBE_DIVERT_LOW;        // 80%  from BasicCube.G()
      int expectedStay = 100 - INITIAL_PROBE_DIVERT_HIGH; // 10%
      int expectedOut  = 100 - expectedIn - expectedStay; // 10%

      // Should be for left top:
      //
      // 1   1   8
      // 1   0   8
      // 8   8  65
      // _________
      // total: 65 + ( 4 * 8 ) + ( 3 * 1 ) = 100 %
      //
      int expected   [ 3 ][ 3 ] = 
      { { expectedOut  * expectedOut / 100, expectedOut  * expectedStay / 100, expectedOut  * expectedIn / 100 },
        { expectedStay * expectedOut / 100,                                 0, expectedStay * expectedIn / 100 },
        { expectedIn   * expectedOut / 100, expectedIn   * expectedStay / 100, ( expectedIn   * expectedIn / 100 ) + ( expectedOut  * expectedOut / 100 ) } };

      // Avoiding edges:
      //
      // 0   0   0
      // 0   0   9
      // 0   9  82
      //
      // TODO: we might want to test 4 more edges in the middle of the sides, not corners.
      //
      int edge_expected[ 3 ][ 3 ] =
      { { 0, 0, 0  },
        { 0, 0, expected[ 1 ][ 0 ] + expected[ 1 ][ 2 ] },
        { 0, expected[ 0 ][ 1 ] + expected[ 2 ][ 1 ],  expected[ 2 ][ 2 ] + expected[ 0 ][ 0 ] + expected[ 0 ][ 2 ] + expected[ 2 ][ 0 ]} };

      WriteMessage( "Left top" );
      RelinkingPointTest( size, index_left_top, expected, PointIt( 1, 1 ));

      WriteMessage( "Left top edge" ); 
      RelinkingPointTest( size, index_left_top_edge, edge_expected, PointIt( 1, 1 ));

      // 8   1   1
      // 8   0   1
      // 65  8   8
      swap_two( expected, edge_expected, 0, 0, 0, 2 );
      swap_two( expected, edge_expected, 1, 0, 1, 2 );
      swap_two( expected, edge_expected, 2, 0, 2, 2 );

      WriteMessage( "Left bottom" );
      RelinkingPointTest( size, index_left_bottom, expected, PointIt( 1, -1 ));

      WriteMessage( "Left bottom edge" );      
      RelinkingPointTest( size, index_left_bottom_edge, edge_expected, PointIt( 1, -1 ));

      // 8   8  65
      // 1   0   8
      // 1   1   8
      swap_two( expected, edge_expected, 0, 1, 1, 0 );
      swap_two( expected, edge_expected, 0, 2, 2, 0 );
      swap_two( expected, edge_expected, 1, 2, 2, 1 );

      WriteMessage( "Right top" );
      RelinkingPointTest( size, index_right_top, expected, PointIt( -1, 1 ));

      WriteMessage( "Right top edge" );
      RelinkingPointTest( size, index_right_top_edge, edge_expected, PointIt( -1, 1 ));

      // 65  8   8
      // 8   0   1
      // 8   1   1
      swap_two( expected, edge_expected, 0, 0, 0, 2 );
      swap_two( expected, edge_expected, 1, 0, 1, 2 );
      swap_two( expected, edge_expected, 2, 0, 2, 2 );

      WriteMessage( "Right bottom" );
      RelinkingPointTest( size, index_right_bottom, expected, PointIt( -1, -1 ));

      WriteMessage( "Right bottom edge" );
      RelinkingPointTest( size, index_right_bottom_edge, edge_expected, PointIt( -1, -1 ));
    }

    TEST_METHOD(RelinkingNewLink) 
    {
      PointIt   size( 50, 50 );
      SizeIt2D  layer_size( size.X, size.Y );
      Celllayer layer( "UnitTest", layer_size, 1, 1 );
      PointIt   index, position;
      BasicCubeRelinking cube( (Cell*)layer, (Cell*)layer, (Cell*)layer, size, index, position );
      Cell   cell       = { 0 };
      ByteIt zeroLink   = 0;
      int    iterations = 1000000;

      WriteMessage( "RelinkingNewLink" );

      for( int iteration = 0; iteration < iterations; iteration++ )
      {
        unsigned int rf = 0;
        errno_t result = rand_s( &rf );
        assert( result == 0 );

        cell.type   = NO_CELL;
        cell.uplink = 0;
        cube.Relink( cell, rf );
        Assert::AreNotEqual( zeroLink, cell.uplink );
      }
    }

    TEST_METHOD(RelinkingSins)
    {
      WriteMessage( "RelinkingSins" );

      PointIt size( 50, 50 );
      PointIt index( 25, 25 );

      // Should be for left top:
      //
      // 12  12  12
      // 12   0  12
      // 12  12  12
      // _________
      // total: 12 * 8 = 96 %
      //
      int expectedOne = 100 / 8 + 1;
      int expected[ 3 ][ 3 ] = 
      { { expectedOne, expectedOne, expectedOne },
        { expectedOne,           0, expectedOne },
        { expectedOne, expectedOne, expectedOne } };

      RelinkingPointTest( size, index, expected, PointIt( 1, 1 ), S_CELL );
    }

    TEST_METHOD(RelinkingGravityForce)
    {
      Probe probe;
      probe.sizeZ = 200;

      //                                      1, 1/2, 1/3,  1/4, 1/5,  1/6
      //                                      1, 0.5, 0.3, 0.25, 0.2, 0.16
      const static int forceChartAxis[] = {   1,   2,   3,    4,   5,    6 };
      const static int forceChart[]     = { 100,  25,   9,    6,   4,    3 };
      int forceAtMedian = 0;

      for( SizeIt pos = 1; pos <= probe.sizeZ; pos++ )
      {
        probe.posZ = pos;
        int force = BasicCube::GravityForce( &probe );
        Assert::IsTrue( force >= 0   );
        Assert::IsTrue( force <= 100 );

        if( pos == 1 )
        {
          Assert::IsTrue( force == 0 );
          continue;
        }

        if( pos == probe.sizeZ - ( probe.sizeZ / 2 ))
        {
          forceAtMedian = force;
        }

        if( pos == probe.sizeZ - ( probe.sizeZ / 4 ))
        {
          Assert::IsTrue( force > forceAtMedian * 2 );
        }


        for( int axis = 0; axis < ARRAYSIZE( forceChartAxis ); axis++ )
        {
          if( pos == probe.sizeZ / forceChartAxis[ axis ])
          {
            WriteMessage( "In %d pos out of %d size, relative (1 / %d), expect %d, observe %d", pos, probe.sizeZ, forceChartAxis[ axis ], forceChart[ axis ], force );
            Assert::IsTrue( force >= forceChart[ axis ] - 1 );
            Assert::IsTrue( force <= forceChart[ axis ] + 1 );
          }
        }
      }
    }

  private:
    void RelinkingPointTest( PointIt size, PointIt index, expected_t& expected, PointIt target, ByteIt cellType = NO_CELL )
    {
      SizeIt2D  layer_size( size.X, size.Y );
      Celllayer layer( "UnitTest", layer_size, 1, 1 );

      for( SizeIt loopPositionY = 0; loopPositionY < layer.size.Y; loopPositionY++ )
      {
        for( SizeIt loopPositionX = 0; loopPositionX < layer.size.X; loopPositionX++ )
        {
          PointIt2D position( loopPositionX, loopPositionY );
          Cell& cell  = ARRAY_2D_CELL( layer,  layer.size,  position );
          PointIt point( loopPositionX, loopPositionY );

          ZeroMemory( &cell, sizeof( cell ));

          if( point.Side( layer_size ))
          {
            cell.type = S_CELL;
          }
          else
          {
            cell.type = GOOD_CELL; // can be anything, but S_CELL
          }
        }
      }

      PointIt  position;
      BasicCubeRelinking cube( (Cell*)layer, (Cell*)layer, (Cell*)layer, size, index, position );
      Cell cell  = { 0 };
      int probability[ 3 ][ 3 ] = { 0 };
      int iterations = 1000000;

      for( int iteration = 0; iteration < iterations; iteration++ )
      {
        bool assigned = false;
        unsigned int rf = 0;
        errno_t result = rand_s( &rf );
        assert( result == 0 );

        cell.type   = cellType;
        cell.uplink = 0;
        cube.Relink( cell, rf );

        Assert::IsTrue( cell.uplink != 0 );

        for( int deltaX = -1; deltaX <= 1; deltaX++ )
          for( int deltaY = -1; deltaY <= 1; deltaY++ )
          {
            Link link;
            if( !Link::FromPoint( PointIt( deltaX, deltaY ), &link ))
            {
              continue;
            }

            if( cell.uplink == link.Raw())
            {
              probability[ deltaX + 1 ][ deltaY + 1 ]++;
              assigned = true;
            }
          }

        Assert::IsTrue( assigned );
      }

      int probability_total = 0;
      for( int deltaX = -1; deltaX <= 1; deltaX++ )
      {
        for( int deltaY = -1; deltaY <= 1; deltaY++ )
        {
          int probability_percent = (int) ceil( (float)probability[ deltaX + 1 ][ deltaY + 1 ] / ( iterations / 100.00f ));
          probability_total += probability[ deltaX + 1 ][ deltaY + 1 ];
          probability[ deltaX + 1 ][ deltaY + 1 ] = probability_percent;
        }
      }

      WriteMessage( "Expected:" );
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", expected[0][0], expected[0][1], expected[0][2]);
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", expected[1][0], expected[1][1], expected[1][2]);
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", expected[2][0], expected[2][1], expected[2][2]);

      WriteMessage( "Actual:" );
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", probability[0][0], probability[0][1], probability[0][2]);
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", probability[1][0], probability[1][1], probability[1][2]);
      WriteMessage("[ %3d ] [ %3d ] [ %3d ]", probability[2][0], probability[2][1], probability[2][2]);

      for( int deltaX = -1; deltaX <= 1; deltaX++ )
      {
        for( int deltaY = -1; deltaY <= 1; deltaY++ )
        {
          int probability_percent = probability [ deltaX + 1 ][ deltaY + 1 ];
          int expected_percent    = expected    [ deltaX + 1 ][ deltaY + 1 ];

          // Set some error margin
          // 15 Nov - DMA - TODO: should be 0 error margin for edges
          if( deltaX == target.X && deltaY == target.Y )
          {
            Assert::IsTrue( expected_percent - 1 <= probability_percent );
          }
          else
          {
            // TODO - change probability_percent calculations? median?
            Assert::IsTrue( expected_percent - 1 <= probability_percent &&
                            probability_percent  <= expected_percent + 3 );
          }
        }
      }

      Assert::IsTrue( probability_total == iterations );
    }
  };
}
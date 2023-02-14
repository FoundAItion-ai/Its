/*******************************************************************************
 FILE         :   BuildPlaneUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - build plane unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   08/12/2016
*******************************************************************************/


#include "stdafx.h"
#include <functional>

#include "BaseUnitTest.h"
#include "MaiTrix.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


// 8 Dec 2015 - DMA - why iterations? BuildPlane is determinated, removed.
// Lambda function should match BuildPlaneUnitTest::BasicCubeTestCallback type.
#define BASIC_VALIDATOR  \
  for( int iteration = 1; iteration <= 1; iteration++ ) \
    { \
    ByteCube uplinks      = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; \
    ByteCube data         = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; \
    ByteCube updata       = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; \
    ByteCube reflections  = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; \
    ValidateBasicCube( uplinks, data, updata, reflections, \
      []( BasicCubeBuildPlane& basic_cube, ByteCube& uplinks, ByteCube& data, ByteCube& updata, ByteCube& reflections ) { \
          ByteIt  downLink, downSignal, mode, reflectedMode;  \
          SizeIt  downlinks = 0;          \
          PointIt position( 0, 0 );       \
          PointIt at( position + One2D ); \
          Link    reflection;             \
          downLink = mode = 0;      

#define BASIC_VALIDATOR_END }});


namespace BaseUnitTest
{		
  class BasicCubeBuildPlane : public BasicCube
  {
  public:
    typedef ByteIt ByteCube[ CubeSize ][ CubeSize ];
    typedef BasicCube Base;

    BasicCubeBuildPlane( void*     _dataIn,
                         void*     _dataInU,  
                         void*     _dataInD,                         
                         PointIt   _size,    
                         PointIt   _index,   
                         PointIt   _position,
                         ByteCube& _uplinks,
                         ByteCube& _data,
                         ByteCube& _updata,
                         ByteCube& _reflections ) :
    BasicCube       ( _dataIn, _dataInU, _dataInD, _size, _index, _position ),
    uplinks         ( _uplinks ),
    data            ( _data ),
    updata          ( _updata ),
    reflections     ( _reflections ),
    centralCellType ( GOOD_CELL )
    {}

  public:
    enum
    {
      magicReflectionOne   = 0x09, // 00001001
      magicReflectionTwo   = 0x07, // 00000111
      magicReflectionOther = 0x80  // 10000000
    };

    SizeIt BuildPlaneTest( const PointIt& at, ByteIt& downSignal, ByteIt& downLink, ByteIt& mode, ByteIt& reflectedMode, Link& reflection )
    {
      for( PosIt loopPositionY = 0; loopPositionY < Size().Y; loopPositionY++ )
      {
        for( PosIt loopPositionX = 0; loopPositionX < Size().X; loopPositionX++ )
        {
          Cell& downCell  = GetHyperCube3D( at, loopPositionX - 1, loopPositionY - 1, -1 );
          Cell& cell      = GetHyperCube3D( at, loopPositionX - 1, loopPositionY - 1,  0 );
          Cell& upCell    = GetHyperCube3D( at, loopPositionX - 1, loopPositionY - 1,  1 );

          ZeroMemory( &downCell , sizeof( Cell ));
          ZeroMemory( &cell     , sizeof( Cell ));
          ZeroMemory( &upCell   , sizeof( Cell ));

          downCell.uplink   = uplinks     [ loopPositionX ][ loopPositionY ];
          downCell.data     = data        [ loopPositionX ][ loopPositionY ];

          upCell.updata     = updata      [ loopPositionX ][ loopPositionY ];
          upCell.reflection = reflections [ loopPositionX ][ loopPositionY ];
        }
      }
      
      Cell& cell = GetHyperCube( at );

      // case with S_CELL handled and asserted in ProcessPlane()
      if( centralCellType == S_CELL  || IO_CELL( centralCellType ))
      {
        SetSingularCentralCell( cell, centralCellType );
      }
      else
      {
        // Set them all, otherwise BuildPlane() below will assert
        // as we assume if reflection from the cell above does exist
        // so there should be our reflection pointing to that cell
        memset( cell.reflections, magicReflectionOther, sizeof( cell.reflections ));

        cell.type = GOOD_CELL;
        cell.reflections[ 0 ] = magicReflectionOne; // 1001
        cell.reflections[ 1 ] = magicReflectionTwo; // 0111
      }

      Link link;
      Link signal;

      BuildPlane( at, &link, &signal, mode, reflectedMode, &reflection );

      downLink   = link.Raw();
      downSignal = signal.Raw();
      return link.Count();
    }

    void SetCentralCellType( ByteIt type )
    {
      centralCellType = type;
    }

    void SetSingularCentralCell( Cell& cell, ByteIt type )
    {
      // Same as MaiTrix::InitCell
      cell.type       = type;
      cell.data       = INITIAL_DATA;
      cell.updata     = INITIAL_DATA;
      cell.uplink     = INITIAL_UPLINK;
      cell.reflection = INITIAL_REFLECTION;

      memset( &cell.shape,       INITIAL_DATA, sizeof( cell.shape ));
      memset( &cell.reflections, INITIAL_DATA, sizeof( cell.reflections ));
      memset( &cell.memory,      INITIAL_DATA, sizeof( cell.memory ));
    }

    static Link CalculateExpectedDownSignal( ByteCube& data )
    {
      Link expectedDownSignal;
      for( ByteIt shift = 0; shift < 8; shift++ )
      {
        Link oneLink;
        PointIt onePoint;
        Assert::IsTrue( Link::FromShift( shift, &oneLink ));
        Assert::IsTrue( oneLink.ToPoint( &onePoint ));
        if( data[ onePoint.X + 1 ][ onePoint.Y + 1 ])
        {
          expectedDownSignal += oneLink;
        }
      }

      return expectedDownSignal;
    }

  private:
    ByteCube& uplinks;     // downcell
    ByteCube& data;        // downcell
    ByteCube& updata;      // upcell
    ByteCube& reflections; // upcell
    ByteIt    centralCellType;
  };


  TEST_CLASS(BuildPlaneUnitTest), BaseTest
  {
  public:
    typedef BasicCubeBuildPlane::ByteCube ByteCube;
    typedef function<void ( BasicCubeBuildPlane& basic_cube, ByteCube& uplinks, ByteCube& data,
                            ByteCube& updata, ByteCube& reflections )> 
      BasicCubeTestCallback;

    //    { 4, 5, 6 } 
    //    { 3, 8, 7 } 
    //    { 2, 1, 0 }  
    //
    TEST_METHOD(BuildPlaneNoDowncellLinksNoSignal)
    {
      WriteMessage( "BuildPlaneNoDowncellLinksNoSignal" );

      BASIC_VALIDATOR {
        downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

        Assert::IsTrue( downlinks     == 0 );
        Assert::IsTrue( downSignal    == Link().Raw());
        Assert::IsTrue( downLink      == Link().Raw());
        Assert::IsTrue( mode          == NOSIGNAL );
        Assert::IsTrue( reflectedMode == NOSIGNAL );
        Assert::IsTrue( reflection.IsEmpty());
      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneOneDowncellLinkNoSignal)
    {
      WriteMessage( "BuildPlaneOneDowncellLinkNoSignal" );

      BASIC_VALIDATOR {
        for( ByteIt shift = 0; shift < 8; shift++ )
        {
          ClearCube( uplinks );

          // selector = 1, 2, 4, 8, 16...
          ByteIt selector = ( 0x1 << shift );
          ByteIt uplinks_total = EnableSelectedUplinks( uplinks, selector );
          Assert::IsTrue( uplinks_total == 1 );

          downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

          Assert::IsTrue( downlinks     == 0 );
          Assert::IsTrue( downSignal    == Link().Raw());
          Assert::IsTrue( downLink      == Link().Raw());
          Assert::IsTrue( mode          == NOSIGNAL );
          Assert::IsTrue( reflectedMode == NOSIGNAL );
          Assert::IsTrue( reflection.IsEmpty());
        }
      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneOneDowncellLink)
    {
      WriteMessage( "BuildPlaneOneDowncellLink" );

      BASIC_VALIDATOR {
        for( ByteIt shift = 0; shift < 8; shift++ )
        {
          ClearCube( uplinks );

          ByteIt selector = ( 0x1 << shift );  // selector = uplink
          ByteIt uplinks_total = EnableSelectedUplinks( uplinks, selector );
          Assert::IsTrue( uplinks_total == 1 );

          SetRandomData( data );
          SetSelectedData( data, SIGNAL, selector );

          Link expectedDownSignal = BasicCubeBuildPlane::CalculateExpectedDownSignal( data );
          downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

          // downlink = mirrored uplink
          Assert::IsTrue( downSignal    == expectedDownSignal.Raw());
          Assert::IsTrue( downLink      == Link( selector ).Mirror().Raw());
          Assert::IsTrue( downlinks     == 1 );
          Assert::IsTrue( mode          == SIGNAL   );
          Assert::IsTrue( reflectedMode == NOSIGNAL );
          Assert::IsTrue( reflection.IsEmpty());
        }
      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneManyDowncellLinks)
    {
      WriteMessage( "BuildPlaneManyDowncellLinks" );

      BASIC_VALIDATOR {
        for( int selector = 1; selector <= 255; selector++ )
        {
          ClearCube( uplinks );
          SetRandomData( data );
          SetSelectedData( data, SIGNAL, selector );
          Link expectedDownSignal = BasicCubeBuildPlane::CalculateExpectedDownSignal( data );

          // selector = 1, 2, 3, 4, 5, 6...
          ByteIt uplinks_total = EnableSelectedUplinks( uplinks, selector );
          downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );
          
          Assert::IsTrue( downSignal    == expectedDownSignal.Raw());
          Assert::IsTrue( downLink      == Link( selector ).Mirror().Raw());
          Assert::IsTrue( downlinks     == uplinks_total );
          Assert::IsTrue( mode          == SIGNAL );
          Assert::IsTrue( reflectedMode == NOSIGNAL );
          Assert::IsTrue( reflection.IsEmpty());
        }
      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneManyDowncellLinksSimple)
    {
      WriteMessage( "BuildPlaneManyDowncellLinksSimple" );

      BASIC_VALIDATOR {
        uplinks[ 0 ][ 0 ] = 0x01 << 4;
        uplinks[ 1 ][ 2 ] = 0x01 << 7;
        uplinks[ 2 ][ 2 ] = 0x01 << 0;

        SetRandomData( data );

        data   [ 0 ][ 0 ] = SIGNAL;
        data   [ 1 ][ 2 ] = SIGNAL;
        data   [ 2 ][ 2 ] = NOSIGNAL;

        Link expectedDownSignal = BasicCubeBuildPlane::CalculateExpectedDownSignal( data );
        downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

        Assert::IsTrue( downSignal    == expectedDownSignal.Raw());
        Assert::IsTrue( downlinks     == 2 );
        Assert::IsTrue( downLink      == Link(( 0x01 << 4 ) | ( 0x01 << 7 )).Mirror().Raw());
        Assert::IsTrue( mode          == SIGNAL );
        Assert::IsTrue( reflectedMode == NOSIGNAL );
        Assert::IsTrue( reflection.IsEmpty());
      BASIC_VALIDATOR_END }
    }

    // As mode and reflectedMode/reflection calculations are independent
    // we can test them separately, test cases:
    //
    // 1) upcellUplink = INITIAL_UPLINK (should never happen, there's an assert)
    //    upcellUplink = 1 for all other tests
    // 2) reflection is ok, no updata
    // 3) no reflection, updata is ok
    // 4) reflection is in the wrong direction, updata is ok
    // 5) reflections from different directions, updata is ok
    // 6) diamond-shape reflection - BuildPlaneUpcellLinksMultiDirectional
    // 7) top level S_CELL reflections (uplink based)
    TEST_METHOD(BuildPlaneUpcellLinksBasic)
    {
      WriteMessage( "BuildPlaneUpcellLinksBasic" );

      BASIC_VALIDATOR {
        Link nonZeroLink;
        Assert::IsTrue( Link::FromPoint( PointIt( -1, -1 ), &nonZeroLink ));
        Assert::IsFalse( nonZeroLink.IsEmpty());
        ByteIt allDirections = 0xFF; // raw link

        for( int loopX = 0; loopX <= 2; loopX++ )
        {
          for( int loopY = 0; loopY <= 2; loopY++ )
          {
            WriteMessage( "X: %d, Y: %d", loopX, loopY );
            memset( updata,      0x0, sizeof( ByteCube ));
            memset( reflections, 0x0, sizeof( ByteCube ));

            WriteMessage( "Reflect to all directions" );
            updata      [ loopX ][ loopY ] = SIGNAL;
            reflections [ loopX ][ loopY ] = allDirections;  

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks  == 0 );
            Assert::IsTrue( downSignal == Link().Raw());
            Assert::IsTrue( downLink   == Link().Raw());
            Assert::IsTrue( mode       == NOSIGNAL );

            // We don't receive signals from zenith, byte = 8 bits, not 9 :-)
            // As it doesn't improve signal passing much anyway.
            if( loopX == 1 && loopY == 1 ) 
            {
              Assert::IsTrue( reflectedMode == NOSIGNAL );
              Assert::IsTrue( reflection.IsEmpty());
            }
            else 
            {
              Assert::IsTrue( reflectedMode == SIGNAL );
              Assert::IsFalse( reflection.IsEmpty());

              // We are reflecting in three magic directions
              if( loopX == 0 && loopY == 0 ) 
              {
                Assert::IsTrue( reflection == Link( BasicCubeBuildPlane::magicReflectionOne ));
              }
              else if( loopX == 0 && loopY == 1 )
              {
                Assert::IsTrue( reflection == Link( BasicCubeBuildPlane::magicReflectionTwo ));
              }
              else
              {
                Assert::IsTrue( reflection == Link( BasicCubeBuildPlane::magicReflectionOther ));
              }
            }

            WriteMessage( "No signal from above => no reflections" );
            updata      [ loopX ][ loopY ] = NOSIGNAL;
            reflections [ loopX ][ loopY ] = allDirections;  

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == NOSIGNAL );
            Assert::IsTrue( reflection.IsEmpty());

            WriteMessage( "Updata everywhere, no reflections" );
            SetSelectedData( updata, SIGNAL, allDirections );
            reflections [ loopX ][ loopY ] = INITIAL_REFLECTION;

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == NOSIGNAL );
            Assert::IsTrue( reflection.IsEmpty());
          }
        }

      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneUpcellLinksDirectional)
    {
      // Direction dependent cases
      WriteMessage( "BuildPlaneUpcellLinksDirectional" );

      BASIC_VALIDATOR {
        Link nonZeroLink;
        Assert::IsTrue( Link::FromPoint( PointIt( -1, -1 ), &nonZeroLink ));

        for( int loopX = 0; loopX <= 2; loopX++ )
        {
          for( int loopY = 0; loopY <= 2; loopY++ )
          {
            Link   direction( 0xFF ); // all directions
            Link   expectedReflection;
            Link   expectedReflectionMirrored;
            ByteIt expectedReflectedMode;
            
            if( !Link::FromPoint( PointIt( loopX - 1, loopY - 1 ), &direction ))
            {
              // We don't receive signals from zenith, but should not crash.
              // And in this case are reflecting into all directions as
              // direction is unassigned and keep 0xFF.
              expectedReflectedMode      = NOSIGNAL;
              expectedReflection         = Link();
              expectedReflectionMirrored = Link();
              Assert::IsTrue( direction.Raw() == 0xFF );
              Assert::IsTrue( loopX = 1 && loopY == 1 );
            }
            else 
            {
              expectedReflectedMode = SIGNAL;
              Assert::IsTrue( direction.Raw() != 0xFF );
            
              // We are reflecting in three magic directions
              if( loopX == 0 && loopY == 0 ) 
              {
                expectedReflection = Link( BasicCubeBuildPlane::magicReflectionOne );
              }
              else if( loopX == 0 && loopY == 1 )
              {
                expectedReflection = Link( BasicCubeBuildPlane::magicReflectionTwo );
              }
              else
              {
                expectedReflection = Link( BasicCubeBuildPlane::magicReflectionOther );
              }

              if( loopX == 2 && loopY == 2 ) 
              {
                expectedReflectionMirrored = Link( BasicCubeBuildPlane::magicReflectionOne );
              }
              else if( loopX == 2 && loopY == 1 )
              {
                expectedReflectionMirrored = Link( BasicCubeBuildPlane::magicReflectionTwo );
              }
              else
              {
                expectedReflectionMirrored = Link( BasicCubeBuildPlane::magicReflectionOther );
              }
            }

            WriteMessage( "X: %d, Y: %d", loopX, loopY );
            memset( updata,      0x0, sizeof( ByteCube ));
            memset( reflections, 0x0, sizeof( ByteCube ));

            WriteMessage( "Reflect to a single and correct direction" );
            updata      [ loopX ][ loopY ] = SIGNAL;
            reflections [ loopX ][ loopY ] = direction.Mirror().Raw();

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == expectedReflectedMode );
            Assert::IsTrue( reflection    == expectedReflection );
            
            WriteMessage( "Reflect to a another single and correct direction (mirrored)" );
            updata      [ loopX ][ loopY ] = NOSIGNAL;
            reflections [ loopX ][ loopY ] = INITIAL_REFLECTION;
            updata      [ -( loopX - 1 ) + 1 ][ -( loopY - 1 ) + 1 ] = SIGNAL;
            reflections [ -( loopX - 1 ) + 1 ][ -( loopY - 1 ) + 1 ] = direction.Raw();

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == expectedReflectedMode );
            Assert::IsTrue( reflection    == expectedReflectionMirrored );
            
            WriteMessage( "Two correct reflections (direct and mirrored)" );
            updata      [ loopX ][ loopY ] = SIGNAL;
            reflections [ loopX ][ loopY ] = direction.Mirror().Raw();

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == expectedReflectedMode );
            Assert::IsTrue( reflection    == expectedReflection + expectedReflectionMirrored );
            
            memset( updata,      0x0, sizeof( ByteCube ));
            memset( reflections, 0x0, sizeof( ByteCube ));

            WriteMessage( "Reflect to a single, incorrect direction" );
            updata      [ loopX ][ loopY ] = SIGNAL;
            reflections [ loopX ][ loopY ] = direction.Raw();

            downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

            Assert::IsTrue( downlinks     == 0 );
            Assert::IsTrue( downSignal    == Link().Raw());
            Assert::IsTrue( downLink      == Link().Raw());
            Assert::IsTrue( mode          == NOSIGNAL );
            Assert::IsTrue( reflectedMode == NOSIGNAL );
            Assert::IsTrue( reflection.IsEmpty());
            
            WriteMessage( "Reflect to a multiple, incorrect directions" );
            for( int loopReflection = 0; loopReflection < 0xFF; loopReflection++ )
            {
              if( loopReflection & direction.Mirror().Raw()) // this one is correct
              {
                continue;
              }

              updata      [ loopX ][ loopY ] = SIGNAL;
              reflections [ loopX ][ loopY ] = loopReflection;

              downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

              Assert::IsTrue( downlinks     == 0 );
              Assert::IsTrue( downSignal    == Link().Raw());
              Assert::IsTrue( downLink      == Link().Raw());
              Assert::IsTrue( mode          == NOSIGNAL );
              Assert::IsTrue( reflectedMode == NOSIGNAL );
              Assert::IsTrue( reflection.IsEmpty());
            }
            
            WriteMessage( "Reflect to a multiple incorrect and one correct directions" );
            for( int loopReflection = 0; loopReflection < 0xFF; loopReflection++ )
            {
              if( loopReflection & direction.Mirror().Raw()) // this one is correct
              {
                updata      [ loopX ][ loopY ] = SIGNAL;
                reflections [ loopX ][ loopY ] = loopReflection;

                downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

                Assert::IsTrue( downlinks     == 0 );
                Assert::IsTrue( downSignal    == Link().Raw());
                Assert::IsTrue( downLink      == Link().Raw());
                Assert::IsTrue( mode          == NOSIGNAL );
                Assert::IsTrue( reflectedMode == expectedReflectedMode );
                Assert::IsTrue( reflection    == expectedReflection );
              }
            }
          }
        }

      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneUpcellLinksMultiDirectional)
    {
      WriteMessage( "BuildPlaneUpcellLinksMultiDirectional" );

      BASIC_VALIDATOR {
        Link nonZeroLink;
        Assert::IsTrue( Link::FromPoint( PointIt( -1, -1 ), &nonZeroLink ));

        // Choosing cells with reflection
        for( int loopSelector = 1; loopSelector <= 0xFF; loopSelector++ )
        {
          memset( updata,      0x0, sizeof( ByteCube ));
          memset( reflections, 0x0, sizeof( ByteCube ));
          int selected = 0;
          Link expectedReflection1;
          Link expectedReflection2;
          Link expectedReflection3;

          for( int loopX = 0; loopX <= 2; loopX++ )
          {
            for( int loopY = 0; loopY <= 2; loopY++ )
            {
              Link direction;
              if( !Link::FromPoint( PointIt( loopX - 1, loopY - 1 ), &direction ))
              {
                continue;
              }

              ByteIt rawDirection = direction.Mirror().Raw();
              if((( rawDirection & loopSelector ) != rawDirection ))
              {
                continue;
              }

              updata      [ loopX ][ loopY ] = SIGNAL;
              reflections [ loopX ][ loopY ] = rawDirection;
              selected++;

              if( loopX == 0 && loopY == 0 ) 
              {
                expectedReflection1 = Link( BasicCubeBuildPlane::magicReflectionOne );
              }
              else if( loopX == 0 && loopY == 1 )
              {
                expectedReflection2 = Link( BasicCubeBuildPlane::magicReflectionTwo );
              }
              else
              {
                expectedReflection3 = Link( BasicCubeBuildPlane::magicReflectionOther );
              }
            }
          }

          WriteMessage( "Selector: %d, selected: %d", loopSelector, selected );
          downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

          Assert::IsTrue( downlinks     == 0 );
          Assert::IsTrue( downSignal    == Link().Raw());
          Assert::IsTrue( downLink      == Link().Raw());
          Assert::IsTrue( mode          == NOSIGNAL );
          Assert::IsTrue( reflectedMode == SIGNAL );
          Assert::IsTrue( reflection    == expectedReflection1 + expectedReflection2 + expectedReflection3 );
        }

      BASIC_VALIDATOR_END }
    }

    TEST_METHOD(BuildPlaneUpcellSpecialCells)
    {
      WriteMessage( "BuildPlaneUpcellSpecialCells" );

      BASIC_VALIDATOR {
        memset( updata, SIGNAL, sizeof( ByteCube ));
        memset( reflections, 0xFF, sizeof( ByteCube )); // 0xFF - all directions

        // 1) Singular
        //
        // The only case when we can not calculate reflection/reflectedMode when building
        // the plane as there are no uplinked cells/upsignals so we use uplink direction to get
        // calculate cell.reflection based on cell.reflections in ProcessPlane() later on.
        basic_cube.SetCentralCellType( S_CELL );
        downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

        Assert::IsTrue( downlinks     == 0 );
        Assert::IsTrue( downSignal    == Link().Raw());
        Assert::IsTrue( downLink      == Link().Raw());
        Assert::IsTrue( mode          == NOSIGNAL );
        Assert::IsTrue( reflectedMode == NOSIGNAL );
        Assert::IsTrue( reflection.IsEmpty());

        // 2) IO
        //
        // reflection is not needed for IO
        basic_cube.SetCentralCellType( V_CELL );
        downlinks = basic_cube.BuildPlaneTest( at, downSignal, downLink, mode, reflectedMode, reflection );

        Assert::IsTrue( downlinks     == 0 );
        Assert::IsTrue( downSignal    == Link().Raw());
        Assert::IsTrue( downLink      == Link().Raw());
        Assert::IsTrue( mode          == NOSIGNAL );
        Assert::IsTrue( reflectedMode == SIGNAL );
        Assert::IsTrue( reflection.IsEmpty());

        BASIC_VALIDATOR_END }
    }

  private:
    static ByteIt RandomByte()
    {
      int selector = rand() % 2;
      return selector == 0 ? NOSIGNAL : SIGNAL;
    }

    static void SetRandomData( ByteCube& data )
    {
      srand( GetTickCount());

      for( SizeIt loopPositionY = 0; loopPositionY < CubeSize; loopPositionY++ )
      {
        for( SizeIt loopPositionX = 0; loopPositionX < CubeSize; loopPositionX++ )
        {
          data[ loopPositionY ][ loopPositionX ] = RandomByte();
        }
      }
    }

    static ByteIt EnableSelectedUplinks( ByteCube& uplinks, ByteIt selector )
    {
      ByteIt counter = 0;

      if( selector & ( 0x01 << 0 )) { uplinks[ 2 ][ 2 ] = 0x01 << 0; counter++; }
      if( selector & ( 0x01 << 1 )) { uplinks[ 2 ][ 1 ] = 0x01 << 1; counter++; }
      if( selector & ( 0x01 << 2 )) { uplinks[ 2 ][ 0 ] = 0x01 << 2; counter++; }
      if( selector & ( 0x01 << 3 )) { uplinks[ 1 ][ 0 ] = 0x01 << 3; counter++; }
      if( selector & ( 0x01 << 4 )) { uplinks[ 0 ][ 0 ] = 0x01 << 4; counter++; }
      if( selector & ( 0x01 << 5 )) { uplinks[ 0 ][ 1 ] = 0x01 << 5; counter++; }
      if( selector & ( 0x01 << 6 )) { uplinks[ 0 ][ 2 ] = 0x01 << 6; counter++; }
      if( selector & ( 0x01 << 7 )) { uplinks[ 1 ][ 2 ] = 0x01 << 7; counter++; }

      return counter;
    }

    static void SetSelectedData( ByteCube& data, ByteIt value, ByteIt selector )
    {
      if( selector & ( 0x01 << 0 )) { data[ 2 ][ 2 ] = value; }
      if( selector & ( 0x01 << 1 )) { data[ 2 ][ 1 ] = value; }
      if( selector & ( 0x01 << 2 )) { data[ 2 ][ 0 ] = value; }
      if( selector & ( 0x01 << 3 )) { data[ 1 ][ 0 ] = value; }
      if( selector & ( 0x01 << 4 )) { data[ 0 ][ 0 ] = value; }
      if( selector & ( 0x01 << 5 )) { data[ 0 ][ 1 ] = value; }
      if( selector & ( 0x01 << 6 )) { data[ 0 ][ 2 ] = value; }
      if( selector & ( 0x01 << 7 )) { data[ 1 ][ 2 ] = value; }
    }

    static void ClearCube( ByteCube& cube )
    {
      for( SizeIt loopPositionY = 0; loopPositionY < CubeSize; loopPositionY++ )
      {
        for( SizeIt loopPositionX = 0; loopPositionX < CubeSize; loopPositionX++ )
        {
          cube[ loopPositionY ][ loopPositionX ] = 0;
        }
      }
    }

    void ValidateBasicCube( ByteCube& uplinks, ByteCube& data, ByteCube& updata,
                            ByteCube& reflections, BasicCubeTestCallback cube_callback )
    {
      SizeIt2D  layer_size( CubeSize, CubeSize );
      Celllayer layerU ( "UnitTest", layer_size, 2, 1 );
      Celllayer layer  ( "UnitTest", layer_size, 1, 1 );
      Celllayer layerD ( "UnitTest", layer_size, 0, 1 );

      for( SizeIt loopPositionY = 0; loopPositionY < layerD.size.Y; loopPositionY++ )
      {
        for( SizeIt loopPositionX = 0; loopPositionX < layerD.size.X; loopPositionX++ )
        {
          PointIt2D position( loopPositionX, loopPositionY );
          Cell& cellU = ARRAY_2D_CELL( layerU, layerU.size, position );
          Cell& cell  = ARRAY_2D_CELL( layer,  layer.size,  position );
          Cell& cellD = ARRAY_2D_CELL( layerD, layerD.size, position );

          InitCell( &cellU );
          InitCell( &cell  );
          InitCell( &cellD );
        }
      }

      PointIt position( 0, 0 );
      PointIt at( position + One2D ); // in hypercube

      BasicCubeBuildPlane basic_cube( (Cell*)layer, (Cell*)layerU, (Cell*)layerD, 
        PointIt( 3, 3 ), PointIt( 1, 1 ), position, uplinks, data,
        updata, reflections );

      cube_callback( basic_cube, uplinks, data, updata, reflections );
    }

    // Should match MaiTrix::ClearLayer
    void InitCell( Cell * cell )
    {
      cell->type       = INITIAL_TYPE;
      cell->data       = INITIAL_DATA;
      cell->updata     = INITIAL_DATA;
      cell->uplink     = INITIAL_UPLINK;
      cell->reflection = INITIAL_REFLECTION;

      memset( &cell->shape,       INITIAL_DATA, sizeof( cell->shape ));
      memset( &cell->reflections, INITIAL_DATA, sizeof( cell->reflections ));
      memset( &cell->memory,      INITIAL_DATA, sizeof( cell->memory ));
    }
  };
}
/*******************************************************************************
 FILE         :   SinsTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - sins unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   04/17/2014
*******************************************************************************/


#include "stdafx.h"

#include "BaseUnitTest.h"
#include "MaiTrix.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


namespace BaseUnitTest
{		
  class MaiTrixTest : public MaiTrix
  {
  public:
    typedef MaiTrix Base;

    MaiTrixTest( SizeIt sizeX, SizeIt sizeY, SizeIt sizeZ ) :
      Base( sizeX, sizeY, sizeZ ),
      estimatedStability( 95 )
    {}

    virtual void EstimateStability( int & stability, SizeIt & topRed ) override
    {
      stability = estimatedStability;
    }

    void LastJudgementTest()
    {
      int stability = 99;
      string response;

      // Case 1: no sins
      Sins sins = LastJudgement( response, stability );
    
      Assert::IsTrue( sins.response == "" );
      Assert::IsTrue( sins.complete );
      Assert::IsTrue( sins.stability == 95 );

      this->estimatedStability = 85; // below the cut
      sins = LastJudgement( "abc", stability );
    
      Assert::IsTrue( sins.response == "abc" );
      Assert::IsFalse( sins.complete );
      Assert::IsTrue( sins.stability == 85 );

      // TODO: Add more tests for sleeping?
    }

    void CountMetricsTest()
    {
      Celllayer bottom_layer( "UnitTest", SizeIt2D( 0, 0 ), 1, 0 );
      CubeStat  maiTrixStat;
      SizeItl   inputStableCells = 0;
      SizeItl   totalStableCells = 0;

      // Case 1: bottom layer, empty
      CountMetrics( bottom_layer, maiTrixStat, inputStableCells, totalStableCells );      

      Assert::IsTrue( inputStableCells == 0 );
      Assert::IsTrue( totalStableCells == 0 );
      Assert::IsTrue( maiTrixStat.cellsGood == 0 );
      Assert::IsTrue( maiTrixStat.cellsBad == 0 );
      Assert::IsTrue( maiTrixStat.cellsInputWithData == 0 );
      Assert::IsTrue( maiTrixStat.cellsSin == 0 );
      Assert::IsTrue( maiTrixStat.CellsTotal() == 0 );

      // Case 2: cover bottom layer Z = 1
      Celllayer middle_layer( "UnitTest", SizeIt2D( 4, 4 ), 1, 0 );

      //
      //  0  1  2  3
      //  4  5  6  7
      //  8  9 10 11
      // 12 13 14 15
      //
      for( int loopCell = 0; loopCell < 16; loopCell++ )
      {
        Cell& cell = *(( Cell* )middle_layer + loopCell );

        switch( loopCell )
        {
          case 5: 
            cell.type   = GOOD_CELL;
            cell.data   = 'g';
            cell.updata = 'g';
            break;
          case 6:
            cell.type   = GOOD_CELL;
            cell.data   = 'g';
            cell.updata = 'g';
            break;
          case 9:
          case 10:
            cell.type   = BAD_CELL;
            cell.data   = 'b';
            cell.updata = 'c';
            break;
          default:
            cell.type   = S_CELL;
            cell.data   = loopCell == 1 ? 's' : NOSIGNAL;
            cell.updata = NOSIGNAL;
            break;
        }
      }

      CountMetrics( middle_layer, maiTrixStat, inputStableCells, totalStableCells );      

      Assert::IsTrue( inputStableCells == 2 );
      Assert::IsTrue( totalStableCells == 2 );
      Assert::IsTrue( maiTrixStat.cellsGood == 2 );
      Assert::IsTrue( maiTrixStat.cellsBad == 2 );
      Assert::IsTrue( maiTrixStat.cellsInputWithData == 0 );
      Assert::IsTrue( maiTrixStat.cellsSin == 12 );
      Assert::IsTrue( maiTrixStat.CellsTotal() == 16 );
    }

    int estimatedStability;
  };
  

  // 28 Jul 2017 - DMA - TODO:
  // Maybe we need unit test for sins.response range as we stopped using
  // uplink to produce response (1-8) and switched to downlink with much wider
  // range 1-256
  TEST_CLASS(SinsUnitTest), BaseTest
  {
  public:
    TEST_METHOD(Sins)
    {
      WriteMessage( "Sins" );

      MaiTrixTest maiTrixTest( 4, 4, 0 );

      WriteMessage( "LastJudgementTest" );
      maiTrixTest.LastJudgementTest();

      WriteMessage( "CountMetricsTest" );
      maiTrixTest.CountMetricsTest();
    }
  };
}
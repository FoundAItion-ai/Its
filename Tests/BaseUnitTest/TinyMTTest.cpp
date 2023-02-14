/*******************************************************************************
 FILE         :   TinyMTTest.cpp 

 COPYRIGHT    :   DMAlex, 2014

 DESCRIPTION  :   MaiTrix - TinyMT 3rd party lib unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   06/30/2014

 LAST UPDATE  :   06/30/2014
*******************************************************************************/


#include "stdafx.h"

#include <limits>
#include "BaseUnitTest.h"
#include "CppUnitTest.h"
#include "tinymt64.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace DCommon;


// based on tinymt/check64.c by Mutsuo Saito/Makoto Matsumoto
namespace BaseUnitTest
{		
  TEST_CLASS(TinyMTUnitTest), BaseTest
  {
  public:    
    TEST_METHOD(GenerateRandomness)
    {
      WriteMessage( "GenerateRandomness" );

      const uint64_t random_first = 15503804787016557143;
      tinymt64_t tinymt;
      tinymt.mat1 = 0xfa051f40;
      tinymt.mat2 = 0xffd0fff4;
      tinymt.tmat = 0x58d02ffeffbfffbc;
      int seed = 1;
      uint64_t random;
      uint64_t expected[10][3] { 
        { random_first,         17280942441431881838, 2177846447079362065  },
        { 10087979609567186558, 8925138365609588954,  13030236470185662861 },
        { 4821755207395923002,  11414418928600017220, 18168456707151075513 },
        { 1749899882787913913,  2383809859898491614,  4819668342796295952  },
        { 11996915412652201592, 11312565842793520524, 995000466268691999   },
        { 6363016470553061398,  7460106683467501926,  981478760989475592   },
        { 11852898451934348777, 5976355772385089998,  16662491692959689977 },
        { 4997134580858653476,  11142084553658001518, 12405136656253403414 },
        { 10700258834832712655, 13440132573874649640, 15190104899818839732 },
        { 14179849157427519166, 10328306841423370385, 9266343271776906817  }
      };


      tinymt64_init( &tinymt, seed );

      for( int i = 0; i < 10; i++ ) 
      {
        for( int j = 0; j < 3; j++ ) 
        {
          uint64_t random = tinymt64_generate_uint64( &tinymt );
          WriteMessage( "Random: %I64u", random );
          Assert::IsTrue( random > 0 );
          Assert::IsTrue( random == expected[i][j] );
        }
      }

      WriteMessage( "TinyMT reseed unit tests" );
      tinymt64_t tinymt_reseed;

      memcpy( &tinymt_reseed, &tinymt, sizeof( tinymt_reseed ));
      tinymt64_init( &tinymt_reseed, seed );
      random = tinymt64_generate_uint64( &tinymt_reseed );
      WriteMessage( "Random first: %I64u", random );
      Assert::IsTrue( random == random_first );

      memcpy( &tinymt_reseed, &tinymt, sizeof( tinymt_reseed ));
      tinymt64_init( &tinymt_reseed, seed + 1 );
      uint64_t random_seed = tinymt64_generate_uint64( &tinymt_reseed );
      WriteMessage( "Random seed + 1: %I64u", random_seed );
      Assert::IsFalse( random_seed == random_first );

      memcpy( &tinymt_reseed, &tinymt, sizeof( tinymt_reseed ));
      tinymt_reseed.mat1++;
      tinymt64_init( &tinymt_reseed, seed );
      uint64_t random_mat1 = tinymt64_generate_uint64( &tinymt_reseed );
      WriteMessage( "Random mat1 + 1: %I64u", random_mat1 );
      Assert::IsFalse( random_mat1 == random_first );
      Assert::IsFalse( random_mat1 == random_seed );

      memcpy( &tinymt_reseed, &tinymt, sizeof( tinymt_reseed ));
      tinymt_reseed.mat2++;
      tinymt64_init( &tinymt_reseed, seed );
      uint64_t random_mat2 = tinymt64_generate_uint64( &tinymt_reseed );
      WriteMessage( "Random mat2 + 1: %I64u", random_mat2 );
      Assert::IsFalse( random_mat2 == random_first );
      Assert::IsFalse( random_mat2 == random_seed );
      Assert::IsFalse( random_mat2 == random_mat1 );

      memcpy( &tinymt_reseed, &tinymt, sizeof( tinymt_reseed ));
      tinymt_reseed.tmat++;
      tinymt64_init( &tinymt_reseed, seed );
      uint64_t random_tmat = tinymt64_generate_uint64( &tinymt_reseed );
      WriteMessage( "Random tmat + 1: %I64u", random_tmat );
      Assert::IsFalse( random_tmat == random_first );
      Assert::IsFalse( random_tmat == random_seed );
      Assert::IsFalse( random_tmat == random_mat1 );
      Assert::IsFalse( random_tmat == random_mat2 );
    }

    TEST_METHOD(GenerateRandomnessEvenly)
    {
      WriteMessage( "GenerateRandomnessEvenly" );

      int dispersion[ 100 ] = { 0 };
      int cycles = 1000;

      for( int loopCycle = 1; loopCycle <= cycles; loopCycle++ )
      {
        for( int loopCycleX = 1; loopCycleX <= 10; loopCycleX++ )
        {
          for( int loopCycleY = 1; loopCycleY <= 10; loopCycleY++ )
          {
            tinymt64_t tinymt;
            tinymt.mat1 = loopCycleX;
            tinymt.mat2 = loopCycleY;
            tinymt.tmat = 10;

            unsigned int rf = 0;
            Assert::IsTrue( rand_s( &rf ) == 0 );
            tinymt64_init( &tinymt, rf );

            uint64_t random_mt = tinymt64_generate_uint64( &tinymt ) % 100;
            Assert::IsTrue( random_mt >= 0 );
            Assert::IsTrue( random_mt < 100 );

            dispersion[ random_mt ]++;
          }
        }
      }

      for( int pos = 0; pos < 100; pos++ )
      {
        WriteMessage( "Dispersion %d : %d", pos, dispersion[ pos ] );
        Assert::IsTrue( dispersion[ pos ] > cycles - 200 );
        Assert::IsTrue( dispersion[ pos ] < cycles + 200 );
      }
    }

    TEST_METHOD(GenerateRandomnessIndependently)
    {
      WriteMessage( "GenerateRandomnessIndependently" );

      int dispersion[ 100 ] = { 0 };
      int dispersionAlt[ 100 ] = { 0 };
      int cycles = 100000;
      uint64_t randomMax = 0;

      for( int loopCycle = 0; loopCycle < cycles; loopCycle++ )
      {
        tinymt64_t tinymt;
        tinymt.mat1 = 5;
        tinymt.mat2 = 5;
        tinymt.tmat = 10;

        unsigned int rf = 0;
        Assert::IsTrue( rand_s( &rf ) == 0 );
        tinymt64_init( &tinymt, rf );

        uint64_t random1 = tinymt64_generate_uint64( &tinymt );
        
        tinymt.mat1 = 8;
        tinymt.mat2 = 8;

        uint64_t random2 = tinymt64_generate_uint64( &tinymt );
        int deviation = ( random1 - random2 ) % 100;
        int deviationAlt = (( random1 % 0xFFFFFFFF ) - ( random2 % 0xFFFFFFFF )) % 100;

        dispersion    [ deviation    ]++;
        dispersionAlt [ deviationAlt ]++;

        if( random1 > randomMax )
        {
          randomMax = random1;
        }
      }

      int average = ( 100000 / 100 );

      for( int pos = 0; pos < 100; pos++ )
      {
        WriteMessage( "Dispersion %d : %d", pos, dispersion[ pos ] );
        Assert::IsTrue( dispersion[ pos ] > average - ( average / 5 ));
        Assert::IsTrue( dispersion[ pos ] < average + ( average / 5 ));
      }

      for( int pos = 0; pos < 100; pos++ )
      {
        WriteMessage( "Dispersion alt %d : %d", pos, dispersionAlt[ pos ] );
        Assert::IsTrue( dispersionAlt[ pos ] > average - ( average / 5 ));
        Assert::IsTrue( dispersionAlt[ pos ] < average + ( average / 5 ));
      }

      uint64_t maxValue = std::numeric_limits<uint64_t>::max();
      WriteMessage( "Max: %I64u out of %I64u", randomMax, maxValue );
      Assert::IsTrue( randomMax > maxValue - ( maxValue / 5 ));
      Assert::IsTrue( randomMax < maxValue );
    }

    TEST_METHOD(GenerateRandomnessExternally)
    {
      WriteMessage( "GenerateRandomnessExternally" );
    
      int cycles = 100000;
      unsigned int randomMax = 0;

      for( int loopCycle = 0; loopCycle < cycles; loopCycle++ )
      {
        unsigned int rf = 0;
        errno_t result = rand_s( &rf );
        assert( result == 0 );
 
        if( rf > randomMax )
        {
          randomMax = rf;
        }
      }

      WriteMessage( "Max rf: %I32u out of %I32u", randomMax, UINT_MAX );
      Assert::IsTrue( randomMax > UINT_MAX - ( UINT_MAX / 5 ));
      Assert::IsTrue( randomMax < UINT_MAX );
    }
  };
}
/*******************************************************************************
 FILE         :   BitArrayTest.cpp 

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - bit array unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   04/17/2014
*******************************************************************************/


#include "stdafx.h"

#include "BaseUnitTest.h"
#include "CubeUtils.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;

TRI_BITS_ARRAY(64)
TRI_BITS_ARRAY(80)

// Must fail in compile time:
// TRI_BITS_ARRAY(65)
// TRI_BITS_ARRAY(87)
// TRI_BITS_ARRAY(264)


namespace BaseUnitTest
{		
  TEST_CLASS(BitArrayUnitTest), BaseTest
  {
  public:    
    TEST_METHOD(BitArray)
    {
      WriteMessage("BitArray unit tests");

      // TRI_BITS_ARRAY(256) is already declared in CubeUtils.h
      BitArrayGeneralTest<TriBits64>();
      BitArrayGeneralTest<TriBits80>();
      BitArrayGeneralTest<TriBits256>();

      BitArrayUplinkTest<TriBits64>();
      BitArrayUplinkTest<TriBits80>();
      BitArrayUplinkTest<TriBits256>();
    }
    
  private:
    template <typename T>
    void BitArrayGeneralTest()
    {
      T::ArrayFlags arrayExternal;
      T      bits( arrayExternal );
      int    snips = bits.Snips();
      ByteIt expected;

      bits.Clear();
      WriteMessage( "BitArrayTest %d", snips );

      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        expected = 0;
        Assert::AreEqual( expected, bits.Get( loopSnip ));
      }

      WriteMessage( "Get/Set one place, all values" );
      for( ByteIt loopBitsValue = 0; loopBitsValue <= 0x07; loopBitsValue++ )
      {
        bits.Set( 11, loopBitsValue );
        Assert::AreEqual( loopBitsValue, bits.Get( 11 ));
      }

      WriteMessage( "Get/Set many places, one value" );
      expected = 0x05;
      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        bits.Set( loopSnip, expected );
        Assert::AreEqual( expected, bits.Get( loopSnip ));
      }

      WriteMessage( "Get/Set many places, many values" );
      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        for( ByteIt loopBitsValue = 0; loopBitsValue <= 0x07; loopBitsValue++ )
        {
          expected = loopBitsValue;
          bits.Set( loopSnip, expected );
          Assert::AreEqual( expected, bits.Get( loopSnip ));
        }
      }

      WriteMessage( "Externally cleared BitArray" );
      T bitsExternal( arrayExternal );
      ByteIt expectedExternal;
      
      // Cleared externally, all 0
      for( int loopByte = 0; loopByte < T::ArraySize; loopByte++ )
      {
        arrayExternal[ loopByte ] = 0;
      }

      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        expectedExternal = 0;
        Assert::AreEqual( expectedExternal, bitsExternal.Get( loopSnip ));
      }

      // Get/Set
      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        for( ByteIt loopBitsValue = 0; loopBitsValue <= 0x07; loopBitsValue++ )
        {
          expectedExternal = loopBitsValue;
          bitsExternal.Set( loopSnip, expectedExternal );
          Assert::AreEqual( expectedExternal, bitsExternal.Get( loopSnip ));
        }
      }

      // All 1
      for( int loopSnip = 0; loopSnip < snips; loopSnip++ )
      {
        bitsExternal.Set( loopSnip, 0x07 ); // 111
      }
    
      expectedExternal = 0xFF; // 11111111

      for( int loopByte = 0; loopByte < T::ArraySize; loopByte++ )
      {
        Assert::AreEqual( expectedExternal, arrayExternal[ loopByte ]);
      }

      // Flags
      WriteMessage( "Flags" );
      for( int loopFlag = 0; loopFlag < snips; loopFlag++ )
      {
        Assert::IsFalse( bitsExternal.GetFlag( loopFlag ));
      }
        
      for( int loopFlag = 0; loopFlag < snips; loopFlag++ )
      {
        bitsExternal.SetFlag( loopFlag, true );
        Assert::IsTrue( bitsExternal.GetFlag( loopFlag ));
        bitsExternal.SetFlag( loopFlag, false );
        Assert::IsFalse( bitsExternal.GetFlag( loopFlag ));
        bitsExternal.SetFlag( loopFlag, true );
      }

      expectedExternal = 0xFF; // 11111111, both flags and snips
      for( int loopByte = 0; loopByte < T::ArrayFlagsSize; loopByte++ )
      {
        Assert::AreEqual( expectedExternal, arrayExternal[ loopByte ]);
      }

      // Clearing
      WriteMessage( "Clear" );
      bits.Clear();
      expectedExternal = 0x0; // both flags and snips
      for( int loopByte = 0; loopByte < T::ArrayFlagsSize; loopByte++ )
      {
        Assert::AreEqual( expectedExternal, arrayExternal[ loopByte ]);
      }
    }

    template <typename T>
    ByteIt GetUplink( T * bits, int shape )
    {
      Link downlink( shape );
      Link uplink;
      Assert::IsTrue( bits->GetLink( downlink, &uplink ));
      return uplink.Raw();
    }

    template <typename T>
    void BitArrayUplinkTest()
    {
      T::ArrayFlags arrayExternal;
      T      bits( arrayExternal );
      int    snips    = bits.Snips();
      ByteIt expected = 0;

      WriteMessage( "BitArrayUplinkTest %d", snips );
      bits.Clear();

      // No response after clearance
      // For Link(0) test must crash but there's no death tests in this framework.
      for( int loopSnip = 1; loopSnip < snips; loopSnip++ )
      {
        expected = 0;
        Link downlink( loopSnip );
        Link uplink;
        Assert::IsFalse( bits.GetLink( downlink, &uplink ));
        Assert::AreEqual( expected, uplink.Raw() );
      }

      for( int loopByte = 0; loopByte < T::ArraySize; loopByte++ )
      {
        // 0x3 = 000 000 110 000 000 0
        arrayExternal[ loopByte ] = loopByte == 0 ? 0x3 : 0; 
      }

      for( int loopByte = T::ArraySize; loopByte < T::ArrayFlagsSize; loopByte++ )
      {
        // All flags are enabled
        arrayExternal[ loopByte ] = 0xFF;
      }

      for( int loopSnip = 1; loopSnip < snips; loopSnip++ )
      {       
        if( loopSnip != 2 )
        {
          expected = 1;
          Assert::AreEqual( expected, GetUplink( &bits, loopSnip ));
        }
        else
        {
          expected = ( 1 << 0x6 ); // 0x6 = 110 snip #3
          Assert::AreEqual( expected, GetUplink( &bits, loopSnip ));
        }
      }

      for( int loopSnip = 1; loopSnip < snips; loopSnip++ )
      {
        BitArrayUplinkValidation( &bits, loopSnip, 0x01, 0x01 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x02, 0x02 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x04, 0x04 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x08, 0x08 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x10, 0x10 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x20, 0x20 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x40, 0x40 );
        BitArrayUplinkValidation( &bits, loopSnip, 0x80, 0x80 );
        BitArrayUplinkIncorrect ( &bits, loopSnip, 0x03 );
        BitArrayUplinkIncorrect ( &bits, loopSnip, 0x05 );
        BitArrayUplinkIncorrect ( &bits, loopSnip, 0x12 );
      }

      WriteMessage( "ClearLink" );
      bits.Clear();
      for( int loopSnip = 1; loopSnip < snips; loopSnip++ )
      {
        expected = 0;
        Link downlink( loopSnip );
        Link uplink;
        Assert::IsFalse( bits.GetLink( downlink, &uplink ));
        Assert::AreEqual( expected, uplink.Raw() );

        expected = 0x20;
        Link uplink20( expected );
        Assert::IsTrue( bits.SetLink( downlink, uplink20 ));
        Assert::IsTrue( bits.GetLink( downlink, &uplink ));
        Assert::IsTrue( bits.GetFlag( loopSnip ));
        Assert::AreEqual( expected, uplink.Raw() );
        Assert::AreEqual( expected, ( ByteIt )( 0x1 << bits.Get( loopSnip )));

        bits.ClearLink( downlink );
        Assert::IsFalse( bits.GetLink( downlink, &uplink ));
        Assert::IsFalse( bits.GetFlag( loopSnip ));
        // note that we don't change uplink if we can not find it
        Assert::AreEqual( expected, uplink.Raw() );
      }
    }
 
    template <typename T>
    void BitArrayUplinkValidation( T * bits, int snip, ByteIt value, ByteIt expected )
    {
      Link downlink( snip );
      Link uplink( value );
      Assert::IsTrue( bits->SetLink( downlink,  uplink ));
      Assert::AreEqual( expected, GetUplink( bits, snip ));
    }

    template <typename T>
    void BitArrayUplinkIncorrect( T * bits, int snip, ByteIt value )
    {
      Link downlink( snip );
      Link uplink( value );
      Assert::IsFalse( bits->SetLink( downlink,  uplink ));
    }
  };
}
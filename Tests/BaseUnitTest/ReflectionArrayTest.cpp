/*******************************************************************************
 FILE         :   ReflectionArrayTest.cpp

 COPYRIGHT    :   DMAlex, 2016

 DESCRIPTION  :   MaiTrix - ReflectionArray class unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/11/2016

 LAST UPDATE  :   01/11/2016
*******************************************************************************/

#include "stdafx.h"

#include "BaseUnitTest.h"
#include "CubeUtils.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


namespace BaseUnitTest
{		
  TEST_CLASS(ReflectionArrayUnitTest), BaseTest
  {
  public:    
    TEST_METHOD(ReflectionArrayBasics)
    {
      WriteMessage( "ReflectionArray basics unit tests" );
      ReflectionArray::Array reflection = { 0 };
      ReflectionArray reflectionArray( reflection );
      Link linkZero;

      // there should be no such thing as Link 0, linkZero???????????????????????

      for( int loopShift = 0; loopShift <= 0x07; loopShift++ )  
      {
        // Get/Set
        Link uplink( loopShift ); // single link
        Link downlink;
        Assert::IsTrue( Link::FromShift( loopShift, &uplink ));

        // Initial content
        // No death tests in CppUnitTestFramework!
        // Assert::IsFalse( reflectionArray.Get( uplink, NULL ));
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == linkZero );

        // Set one downlink
        Assert::IsTrue( reflectionArray.Set( uplink, Link( 1 )));
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == Link( 1 ));

        // Two downlinks (double get/set)
        Assert::IsTrue( reflectionArray.Set( uplink, Link( 4 )));
        Assert::IsTrue( reflectionArray.Set( uplink, Link( 4 ))); // can only try the same value
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == Link( 5 )); // both reflected, 1 & 4
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == Link( 5 )); // should not change

        // Clearing
        reflectionArray.Clear();
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == linkZero );

        // Litter to see if it interferes with other reflections
        Assert::IsTrue( reflectionArray.Set( uplink, Link( 7 )));
        Assert::IsTrue( reflectionArray.Get( uplink, &downlink ));
        Assert::IsTrue( downlink == Link( 7 ));
     }
    }

    TEST_METHOD(ReflectionArrayFind)
    {
      WriteMessage( "ReflectionArray find unit tests" );
      ReflectionArray::Array reflection = { 0 };
      ReflectionArray reflectionArray( reflection );
      bool found;

      // Empty search
      for( int loopShift = 0; loopShift <= 0x07; loopShift++ )  
      {
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link uplink, startLink;
          Assert::IsTrue( Link::FromShift( loopShift, &uplink ));
          startLink = uplink;
          Assert::IsTrue( reflectionArray.Find( Link( loopDownlink ), &uplink, &found ));
          Assert::IsFalse( found );
          Assert::IsTrue( startLink == uplink );
        }
      }

      //            (2)
      //            /
      //  (1) --> (.)              00000001 -> 00000110
      //            \
      //            (4)
      //
      Assert::IsTrue( reflectionArray.Set( Link( 1 ), Link( 2 )));
      Assert::IsTrue( reflectionArray.Set( Link( 1 ), Link( 4 )));

      // Unlikely case with no uplink as we have uplink but do not pass it into the search
      for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
      {
        Link downlink( loopDownlink );
        Link uplink;
        if( downlink == Link( 2 ) || downlink == Link( 4 ) || downlink == Link( 2 ) + Link( 4 ))
        {
          Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
          Assert::IsTrue( found );
          Assert::IsTrue( uplink == Link( 1 ));
        }
        else
        {
          uplink = Link( 8 );
          Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
          Assert::IsFalse( found );
          Assert::IsTrue( uplink == Link( 8 ));
        }
      }

      // Normal case, valid uplink
      for( int loopShift = 0; loopShift <= 0x07; loopShift++ )  
      {
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link downlink( loopDownlink );
          Link uplink;

          if( downlink == Link( 2 ) || downlink == Link( 4 ) || downlink == Link( 2 ) + Link( 4 ))
          {
            // Starting with any uplink...
            Assert::IsTrue( Link::FromShift( loopShift, &uplink ));
            //WriteMessage( "Looking for %d downlink starting from %d uplink", downlink.Raw(), uplink.Raw());
            Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
            Assert::IsTrue( found );
            // ...we should find the correct one
            Assert::IsTrue( uplink == Link( 1 ));
          }
          else
          {
            // uplink is not changed if nothing is found
            uplink = Link( 8 );
            Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
            Assert::IsFalse( found );
            Assert::IsTrue( uplink == Link( 8 ));
          }
        }
      }

      // Adding more reflections:
      //
      //            (16)
      //            /
      //  (4) --> (.)                00000100 -> 00110000
      //            \
      //            (32)
      //
      // with a single operation:
      Assert::IsTrue( reflectionArray.Set( Link( 4 ), Link( 48 )));

      for( int loopShift = 0; loopShift <= 0x07; loopShift++ )  
      {
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link downlink( loopDownlink );
          Link uplink;
          Assert::IsTrue( Link::FromShift( loopShift, &uplink ));
          Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));

          // Old one is still there
          if( downlink == Link( 2 ) || downlink == Link( 4 ) || downlink == Link( 2 ) + Link( 4 ))
          {
            Assert::IsTrue( found );
            Assert::IsTrue( uplink == Link( 1 ));
          }
          // and new one
          else if( downlink == Link( 16 ) || downlink == Link( 32 ) || downlink == Link( 16 ) + Link( 32 ))
          {
            Assert::IsTrue( found );
            Assert::IsTrue( uplink == Link( 4 ));
          }
          else
          {
            uplink = Link( 8 );
            Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
            Assert::IsFalse( found );
            Assert::IsTrue( uplink == Link( 8 ));
          }
        }
      }

      // Cross-reflections:
      //
      //            (2)
      //            /
      //  (1) --> (.) -- (4)             
      //            \
      //            (16)                 00000001 -> 00010110
      //
      //            (32)                 00000100 -> 00110000
      //            /
      //  (4) --> (.)                    
      //            \
      //            (16)
      //
      Assert::IsTrue( reflectionArray.Set( Link( 1 ), Link( 16 )));

      for( int loopShift = 0; loopShift <= 0x07; loopShift++ )  
      {
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link downlink( loopDownlink );
          Link uplink, startLink;
          bool found;

          Assert::IsTrue( Link::FromShift( loopShift, &uplink ));
          startLink = uplink;
          Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));

          // Old one is growing
          if( downlink == Link( 2 ) || downlink == Link( 4 ) || downlink == Link( 2 ) + Link( 4 ) ||
              downlink == Link( 2 ) + Link( 16 ) || downlink == Link( 4 ) + Link( 16 ) ||
              downlink == Link( 2 ) + Link( 4 ) + Link( 16 ))
          {
            Assert::IsTrue( found );
            Assert::IsTrue( uplink == Link( 1 ));
          }
          // Second new one is shrinking
          else if( downlink == Link( 32 ) || downlink == Link( 16 ) + Link( 32 ))
          {
            Assert::IsTrue( found );
            Assert::IsTrue( uplink == Link( 4 ));
          }
          // cross link belongs to both, result is dependent on initial uplink and
          // on search order - from left to right
          else if( downlink == Link( 16 ))
          {
            Assert::IsTrue( found );
            if( startLink == Link( 2 ) || startLink == Link( 3 ) || startLink == Link( 4 ))
            {
              Assert::IsTrue( uplink == Link( 4 ));
            }
            else
            {
              Assert::IsTrue( uplink == Link( 1 ));
            }
          }
          else
          {
            uplink = Link( 8 );
            Assert::IsTrue( reflectionArray.Find( downlink, &uplink, &found ));
            if( found )
            {
              WriteMessage( "Found %d uplink for %d downlink starting from %d", uplink.Raw(), downlink.Raw(), startLink.Raw());
            }

            Assert::IsFalse( found );
            Assert::IsTrue( uplink == Link( 8 ));
          }
        }
      }
    }

    TEST_METHOD(ReflectionArrayReset)
    {
      WriteMessage( "ReflectionArray reset unit tests" );
      ReflectionArray::Array reflection = { 0 };
      ReflectionArray reflectionArray( reflection );
      ShapeArray::ArrayFlags arrayShape;
      ShapeArray shape( arrayShape );

      WriteMessage( "Shape must be present" );
      Assert::IsFalse( reflectionArray.Reset( NULL ));

      WriteMessage( "Empty shape => no reflections" );
      shape.Clear();
      reflectionArray.Clear();
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      for( int loopReflection = 0; loopReflection < ReflectionArray::ArraySize; loopReflection++ )
      {
        Assert::IsTrue( reflection[ loopReflection ] == 0 );
      }

      WriteMessage( "Single link is reflected after reset" );
      for( int loopUplinkShift = 0; loopUplinkShift < 8; loopUplinkShift++ )
      {
        Link uplink;
        Assert::IsTrue( Link::FromShift( loopUplinkShift, &uplink ));
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link downlink( loopDownlink );
          Link testlink;

          shape.Clear();
          reflectionArray.Clear();

          Assert::IsTrue( shape.SetLink( downlink, uplink ));
          Assert::IsTrue( reflectionArray.Get( uplink, &testlink ));
          Assert::IsTrue( testlink == Link());
          Assert::IsTrue( reflectionArray.Reset( &shape ));
          Assert::IsTrue( reflectionArray.Get( uplink, &testlink ));
          Assert::IsTrue( testlink == downlink );

          // all other uplinks are not reflected
          for( int loopOtherUplinkShift = 0; loopOtherUplinkShift < 8; loopOtherUplinkShift++ )
          {
            Link anotherUplink;
            Assert::IsTrue( Link::FromShift( loopOtherUplinkShift, &anotherUplink ));

            if( uplink != anotherUplink )
            {
              Assert::IsTrue( reflectionArray.Get( anotherUplink, &testlink ));
              Assert::IsTrue( testlink == Link());
            }
          }
        }
      }

      WriteMessage( "Cleared link is not reflected after reset" );
      
      for( int loopUplinkShift = 0; loopUplinkShift < 8; loopUplinkShift++ )
      {
        Link uplink;
        Assert::IsTrue( Link::FromShift( loopUplinkShift, &uplink ));
        for( int loopDownlink = 1; loopDownlink <= 0xFF; loopDownlink++ )
        {
          Link downlink( loopDownlink );
          Link testlink;

          shape.Clear();
          reflectionArray.Clear();

          Assert::IsTrue( shape.SetLink( downlink, uplink ));
          Assert::IsTrue( shape.SetLink( Link( 1 ), uplink ));
          Assert::IsTrue( shape.SetLink( Link( 12 ), uplink ));
          Assert::IsTrue( reflectionArray.Reset( &shape ));
          Assert::IsTrue( reflectionArray.Get( uplink, &testlink ));
          Assert::IsTrue( testlink.HasLink( downlink ));
          Assert::IsTrue( testlink.HasLink( Link( 1 )));
          Assert::IsTrue( testlink.HasLink( Link( 12 )));
          
          shape.ClearLink( Link( 12 ));
          Assert::IsTrue( reflectionArray.Reset( &shape ));
          Assert::IsTrue( reflectionArray.Get( uplink, &testlink ));

          if( downlink != Link( 12 ))
          {
            Assert::IsTrue( testlink.HasLink( downlink ));
          }
          Assert::IsTrue( testlink.HasLink( Link( 1 )));
          // Clearing 12 won't help if another mapping has it
          if( downlink != Link( 12 ) && downlink.HasLink( 12 ))
          {
            Assert::IsTrue( testlink.HasLink( Link( 12 )));
          }
          else
          {
            Assert::IsFalse( testlink.HasLink( Link( 12 )));
          }
        }
      }
    }

    TEST_METHOD(ReflectionArrayResetRedirection)
    {
      WriteMessage( "ReflectionArray reset redirection unit tests" );
      ReflectionArray::Array reflection = { 0 };
      ReflectionArray reflectionArray( reflection );
      ShapeArray::ArrayFlags arrayShape;
      ShapeArray shape( arrayShape );

      Link linkOne( 0x1 );
      Link linkTwo( 0x2 );
      Link linkAll( 0xFF );
      Link reflected;

      // 1 -> 1, 2 -> 1
      shape.Clear();
      Assert::IsTrue( shape.SetLink( linkOne, linkOne ));
      Assert::IsTrue( shape.SetLink( linkTwo, linkOne ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      ExpectedReflection( &reflectionArray, linkOne, linkOne + linkTwo );
      ExpectedReflection( &reflectionArray, linkTwo, Link());
      
      Assert::IsTrue( shape.SetLink( linkOne, linkTwo ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      ExpectedReflection( &reflectionArray, linkOne, linkTwo );
      ExpectedReflection( &reflectionArray, linkTwo, linkOne );

      // FF -> 1
      shape.Clear();
      Assert::IsTrue( shape.SetLink( linkAll, linkOne ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      ExpectedReflection( &reflectionArray, linkOne, linkAll );
      ExpectedReflection( &reflectionArray, linkTwo, Link());

      Assert::IsTrue( shape.SetLink( linkAll, linkTwo ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      ExpectedReflection( &reflectionArray, linkOne, Link());
      ExpectedReflection( &reflectionArray, linkTwo, linkAll );

      // 1 -> 1, 2 -> 1, 1 & 2 -> 1
      shape.Clear();
      Assert::IsTrue( shape.SetLink( linkOne, linkOne ));
      Assert::IsTrue( shape.SetLink( linkTwo, linkOne ));
      Assert::IsTrue( shape.SetLink( linkOne + linkTwo, linkOne ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      ExpectedReflection( &reflectionArray, linkOne, linkOne + linkTwo );
      ExpectedReflection( &reflectionArray, linkTwo, Link());

      Assert::IsTrue( shape.SetLink( linkOne, linkTwo ));
      Assert::IsTrue( reflectionArray.Reset( &shape ));
      // Note that reflection is the same even though 1 is redirected
      ExpectedReflection( &reflectionArray, linkOne, linkOne + linkTwo );
      ExpectedReflection( &reflectionArray, linkTwo, linkOne );
    }

    TEST_METHOD(ReflectionArrayResetRedirectionAdvanced)
    {
      WriteMessage( "ReflectionArray reset redirection advanced unit tests" );
      ReflectionArray::Array reflection = { 0 };
      ReflectionArray reflectionArray( reflection );
      ShapeArray::ArrayFlags arrayShape;
      ShapeArray shape( arrayShape );

      shape.Clear();
      for( int loopUplinkShift = 0; loopUplinkShift < 8; loopUplinkShift++ )
      {
        Link uplink;
        Link expectedDownlink;
        Link reflectedLink;
        Assert::IsTrue( Link::FromShift( loopUplinkShift, &uplink ));

        // Going down makes sense otherwise we won't notice any changes
        // as 0xFF includes all reflections so we need to switch it first
        for( int loopDownlink = 0xFF; loopDownlink >= 1; loopDownlink-- )
        {
          Link uplinkCheck;
          expectedDownlink = expectedDownlink + Link( loopDownlink );

          Assert::IsTrue( shape.SetLink( Link( loopDownlink ), uplink ));
          Assert::IsTrue( shape.GetLink( Link( loopDownlink ), &uplinkCheck ));
          Assert::IsTrue( uplink == uplinkCheck );
          Assert::IsTrue( reflectionArray.Reset( &shape ));
          Assert::IsTrue( reflectionArray.Get( uplink, &reflectedLink ));
          Assert::IsTrue( reflectedLink == expectedDownlink );

          ByteIt expectedAnotherReflection = 0;
          for( int loopAnotherDownlink = 0; loopAnotherDownlink < loopDownlink; loopAnotherDownlink++ )
          {
            expectedAnotherReflection |= loopAnotherDownlink;
          }

          for( int loopOtherUplinkShift = 0; loopOtherUplinkShift < 8; loopOtherUplinkShift++ )
          {
            Link anotherUplink;
            Link anotherReflectedlink;
            Assert::IsTrue( Link::FromShift( loopOtherUplinkShift, &anotherUplink ));
            Assert::IsTrue( reflectionArray.Get( anotherUplink, &anotherReflectedlink ));

            if( loopOtherUplinkShift == loopUplinkShift - 1 ) 
            {
              Assert::IsTrue( anotherReflectedlink.Raw() == expectedAnotherReflection );
            }
            else if ( loopOtherUplinkShift != loopUplinkShift )
            {
              // We're switching from loopUplinkShift - 1 to loopUplinkShift uplink and all the rest
              // supposed to be already cleared or not yet set
              Assert::IsTrue( anotherReflectedlink == Link());
            }
          }
        }
        Assert::IsTrue( reflectedLink.Raw() == 0xFF );
      }
    }

private:
    void ExpectedReflection( ReflectionArray * reflectionArray, const Link & uplink, const Link & expectedLink ) 
    {
      Link reflected;
      Assert::IsTrue( reflectionArray && reflectionArray->Get( uplink, &reflected ));
      if( reflected != expectedLink )
      {
        WriteMessage( "ExpectedReflection: %d is expected, %d is received", expectedLink, reflected );
        Assert::IsTrue( false );
      }
    }
  };
}
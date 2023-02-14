/*******************************************************************************
 FILE         :   LinkTest.cpp

 COPYRIGHT    :   DMAlex, 2015

 DESCRIPTION  :   MaiTrix - Link class unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/24/2015

 LAST UPDATE  :   08/24/2015
*******************************************************************************/


#include "stdafx.h"

#include "BaseUnitTest.h"
#include "CubeUtils.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


namespace BaseUnitTest
{		
  TEST_CLASS(LinkUnitTest), BaseTest
  {
  public:    
    TEST_METHOD(LinkSingleDirection)
    {
      WriteMessage( "SingleLink unit tests" );
      PointIt point;
      ByteIt expected = 0;
      SizeIt size     = 0;
      ByteIt shift    = 0;

      WriteMessage( "Zero link" );
      Link zeroLink( 0 );
      Assert::IsTrue( zeroLink == Link());
      Assert::IsTrue( zeroLink.IsEmpty());
      Assert::IsFalse( zeroLink.ToShift( &shift ));
      Assert::IsFalse( zeroLink.ToPoint( &point ));
      Assert::IsFalse( zeroLink.HasShift( 0x0 ));
      Assert::IsFalse( zeroLink.HasShift( 0x01 ));
      Assert::AreEqual( size, zeroLink.Count());
      Assert::IsFalse( zeroLink.IsSingle());
      Link zeroLinkMirror = zeroLink.Mirror();
      Assert::AreEqual( expected, zeroLinkMirror.Raw());
      Assert::AreEqual( expected, zeroLink.Raw());

      WriteMessage( "Single link" );
      Link oneLink( 0x01 );
      Assert::IsFalse( oneLink.IsEmpty());
      Assert::IsFalse( oneLink == Link());
      Assert::IsTrue( oneLink.ToShift( &shift ));
      expected = 0;
      Assert::AreEqual( expected, shift );
      Assert::IsTrue( oneLink.ToPoint( &point ));
      Assert::IsTrue( point == PointIt( -1, -1 ));
      Assert::IsTrue(  oneLink.HasShift( 0x0 ));
      Assert::IsFalse( oneLink.HasShift( 0x01 ));
      Assert::IsFalse( oneLink.HasShift( 0x02 ));
      Assert::IsFalse( oneLink.HasShift( 0x03 ));
      Assert::IsFalse( oneLink.HasLink(  0x02 ));
      Assert::IsFalse( oneLink.HasLink(  0x12 ));
      Assert::IsTrue(  oneLink.HasLink(  0x01 ));
      Assert::IsTrue(  oneLink.HasLink( oneLink ));
      Assert::IsTrue(  oneLink.HasPoint( PointIt( -1, -1 )));
      Assert::IsFalse( oneLink.HasPoint( PointIt(  1, -1 )));
      Assert::IsFalse( oneLink.HasPoint( PointIt( -1,  1 )));
      Assert::IsFalse( oneLink.HasPoint( PointIt(  2,  1 )));
      Assert::IsFalse( oneLink.HasPoint( PointIt(  1, -4 )));
      size = 1;
      Assert::AreEqual( size, oneLink.Count());
      Assert::IsTrue( oneLink.IsSingle());
      Link oneLinkMirror = oneLink.Mirror();
      expected = 0x10;
      Assert::AreEqual( expected, oneLinkMirror.Raw());
      expected = 0x01;
      Assert::AreEqual( expected, oneLink.Raw());

      WriteMessage( "Conversion from" );
      Link converted;
      Assert::IsTrue( Link::FromShift( 0x0, &converted ));
      Assert::IsTrue( oneLink == converted );
      Assert::IsTrue( Link::FromPoint( PointIt( -1, -1 ), &converted ));
      Assert::IsTrue( oneLink == converted );
      // Point(0, 0) is unused
      Assert::IsFalse( Link::FromPoint( PointIt( 0, 0 ), &converted ));

      WriteMessage("Advanced single link");
      for( ByteIt loopShift = 1; loopShift < 8; loopShift++ )  
      {
        Link link( 0x01 << loopShift );

        Assert::IsFalse( link.IsEmpty());
        Assert::IsFalse( link == Link());
        Assert::IsTrue( link != Link());
        Assert::IsTrue( link.ToShift( &shift ));
        Assert::AreEqual( loopShift, shift );
        Assert::IsTrue( link.ToPoint( &point ));
        Assert::IsTrue( point != PointIt( -1, -1 ));
        Assert::IsTrue( point != PointIt(  0,  0 ));
        Link toLink; // back conversion
        Assert::IsTrue( Link::FromPoint( point, &toLink ));
        Assert::IsTrue( link == toLink );

        WriteMessage( "Link: %d, (%d, %d)", link.Raw(), point.X, point.Y );

        Assert::IsFalse( link.HasShift( 0x0 ));
        Assert::IsTrue(  link.HasShift( loopShift ));
        Assert::IsFalse( link.HasLink( 0x22 ));
        Assert::IsTrue(  link.HasLink( link ));
        Assert::IsFalse( link.HasLink( oneLink ));
        Assert::IsTrue(  link.HasLink( 0x01 << loopShift ));
        Assert::IsFalse( link.HasLink(( 0x01 << loopShift ) + 1 ));
        Assert::IsTrue( link.HasLink( link.Raw()));
        
        for( ByteIt pointLoop = 1; pointLoop < 9; pointLoop++ ) 
        {
          PointIt checkPoint(( pointLoop % 3 ) - 1, ( pointLoop / 3 ) - 1 );
          if( checkPoint == point )
          {
            Assert::IsTrue( link.HasPoint( point ));
          }
          else 
          {
            Assert::IsFalse( link.HasPoint( checkPoint ));
          }
        }

        size = 1;
        Assert::AreEqual( size, link.Count());
        Assert::IsTrue( link.IsSingle());

        Link linkMirror = link.Mirror();
        Assert::IsFalse( linkMirror.IsEmpty());
        Assert::IsFalse( linkMirror == Link());
        Assert::IsTrue(  linkMirror != Link());
        Assert::IsTrue(  link == link );
        Assert::IsFalse( link != link );
        Assert::IsTrue(  linkMirror == linkMirror );
        Assert::IsTrue(  linkMirror != oneLinkMirror );
        Assert::IsFalse( linkMirror == oneLinkMirror );
        Assert::IsTrue(  Link::FromPoint( PointIt( -point.X, -point.Y ), &converted ));
        Assert::IsTrue(  linkMirror == converted );
        Assert::IsTrue(  linkMirror.ToPoint( &point ));
        Assert::IsTrue(  point != PointIt(  0,  0 ));
        WriteMessage( "Mirror Link: %d, (%d, %d)", linkMirror.Raw(), point.X, point.Y );

        expected = 0x01 << loopShift;
        Assert::AreEqual( expected, link.Raw());
      }
    }
    
    TEST_METHOD(LinkMultipleDirections)
    {
      WriteMessage("MultiLink unit tests");
      PointIt point;
      ByteIt expected = 0;
      SizeIt size     = 0;
      ByteIt shift    = 0;

      WriteMessage( "Two links" );
      Link oneLink( 0x01 );
      Link secondLink( 0x02 );
      Link anotherLink( 0x07 );
      Link twoLinks( 0x03 );
      // From... functions can not create multi-links, and To... functions always fail
      Assert::IsFalse( twoLinks.ToShift( &shift ));
      Assert::IsFalse( twoLinks.ToPoint( &point ));

      // shift
      Assert::IsTrue(  twoLinks.HasShift( 0x00 ));
      Assert::IsTrue(  twoLinks.HasShift( 0x01 )); 
      Assert::IsFalse( twoLinks.HasShift( 0x02 ));
      Assert::IsFalse( twoLinks.HasShift( 0x03 ));
      Assert::IsFalse( twoLinks.HasShift( 0x04 ));
      // raw link
      Assert::IsTrue(  twoLinks.HasLink( 0x01 ));
      Assert::IsTrue(  twoLinks.HasLink( 0x02 ));
      Assert::IsTrue(  twoLinks.HasLink( 0x03 ));
      Assert::IsFalse( twoLinks.HasLink( 0x04 ));
      // link
      Assert::IsTrue(  twoLinks.HasLink( oneLink ));
      Assert::IsTrue(  twoLinks.HasLink( secondLink ));
      Assert::IsTrue(  twoLinks.HasLink( twoLinks ));
      Assert::IsFalse( twoLinks.HasLink( anotherLink ));
      Assert::IsTrue(  oneLink + secondLink == twoLinks );
      // point
      Assert::IsTrue(  twoLinks.HasPoint( PointIt( -1, -1 )));
      Assert::IsTrue(  twoLinks.HasPoint( PointIt( -1,  0 )));
      Assert::IsFalse( twoLinks.HasPoint( PointIt(  1,  0 )));

      size = 2;
      Assert::AreEqual( size, twoLinks.Count());
      size = 3;
      Assert::AreEqual( size, anotherLink.Count());
      Assert::IsTrue( oneLink.IsSingle());
      Assert::IsTrue( secondLink.IsSingle());
      Assert::IsFalse( twoLinks.IsSingle());
      Assert::IsFalse( anotherLink.IsSingle());
      Link twoLinksMirror = twoLinks.Mirror();
      Assert::IsTrue(  oneLink.Mirror() + secondLink.Mirror() == twoLinksMirror );
      // Shift 0,1 => 4,5
      Assert::IsTrue( twoLinksMirror.HasShift( 0x04 ));
      Assert::IsTrue( twoLinksMirror.HasShift( 0x05 ));
      Assert::IsTrue( twoLinksMirror.HasLink( 0x30 ));
      Assert::IsTrue( twoLinksMirror.Raw() == 0x30 );

      WriteMessage( "Three links" );
      int counter = 0;
      for( ByteIt loopShift1 = 0; loopShift1 < 8; loopShift1++ )  
      {
        Link link1( 0x01 << loopShift1 );
        for( ByteIt loopShift2 = 0; loopShift2 < 8; loopShift2++ )  
        {
          Link link2( 0x01 << loopShift2 );
          if( loopShift1 == loopShift2 )
          {
            continue;
          }
          for( ByteIt loopShift3 = 0; loopShift3 < 8; loopShift3++ )  
          {
            Link link3( 0x01 << loopShift3 );
            if( loopShift3 == loopShift1 || loopShift3 == loopShift2 )
            {
              continue;
            }
            Link threeLink = link1 + link2 + link3;

            WriteMessage( "link # %d", ++counter );
            WriteMessage( "%d  %d  %d", threeLink.HasShift( 0 ), threeLink.HasShift( 1 ), threeLink.HasShift( 2 ));
            WriteMessage( "%d     %d",  threeLink.HasShift( 7 ),                          threeLink.HasShift( 3 ));
            WriteMessage( "%d  %d  %d", threeLink.HasShift( 6 ), threeLink.HasShift( 5 ), threeLink.HasShift( 4 ));
            WriteMessage( "" );

            for( ByteIt loopShift = 0; loopShift < 8; loopShift++ )
            {
              if( loopShift != loopShift1 && loopShift != loopShift2 && loopShift != loopShift3 )
              {
                Assert::IsFalse(  threeLink.HasShift( loopShift ));
                
                Link converted;
                Assert::IsTrue(  Link::FromShift( loopShift, &converted ));
                Assert::IsTrue(  converted.ToPoint( &point ));
                Assert::IsFalse( threeLink.HasPoint( point ));
                Assert::IsTrue(  point != PointIt(  0,  0 ));
              }
            }

            Assert::IsTrue(  threeLink.HasShift( loopShift1 ));
            Assert::IsTrue(  threeLink.HasShift( loopShift2 ));
            Assert::IsTrue(  threeLink.HasShift( loopShift3 ));
            Assert::IsTrue(  threeLink.HasLink( link1 ));
            Assert::IsTrue(  threeLink.HasLink( link2 ));
            Assert::IsTrue(  threeLink.HasLink( link3 ));
            Assert::IsFalse( threeLink.HasLink( 0x1B ));
            Assert::IsFalse( threeLink.HasLink( 0x39 ));
            Assert::IsFalse( threeLink.HasLink( 0x3B ));
            Assert::IsFalse( threeLink.HasLink( 0xEE ));

            Assert::IsTrue(  link1.ToPoint( &point ));
            Assert::IsTrue(  threeLink.HasPoint( point ));
            Assert::IsTrue(  link2.ToPoint( &point ));
            Assert::IsTrue(  threeLink.HasPoint( point ));
            Assert::IsTrue(  link3.ToPoint( &point ));
            Assert::IsTrue(  threeLink.HasPoint( point ));
            Assert::IsFalse( threeLink.ToShift( &shift ));
            Assert::IsFalse( threeLink.ToPoint( &point ));

            size = 3;
            Link threeLinkMirrored = threeLink.Mirror();
            Assert::AreEqual( size, threeLink.Count());
            Assert::AreEqual( size, threeLinkMirrored.Count());
            Assert::IsFalse( threeLink.IsSingle());
            Assert::IsFalse( threeLinkMirrored.IsSingle());
            Link threeLinkDoubleMirrored = threeLinkMirrored.Mirror();
            Assert::IsTrue( threeLink == threeLinkDoubleMirrored );
            Assert::AreEqual( threeLink.Raw(), threeLinkDoubleMirrored.Raw());

            Assert::IsFalse( link1 == Link());
            Assert::IsFalse( link2 == Link());
            Assert::IsFalse( link3 == Link());
            Assert::IsFalse( link1.IsEmpty());
            Assert::IsFalse( link2.IsEmpty());
            Assert::IsFalse( link3.IsEmpty());
            Assert::IsFalse( threeLink.IsEmpty());
            Assert::IsFalse( threeLinkMirrored.IsEmpty());
          }
        }
      }

      WriteMessage( "Eight links" );
      Link eightLinks( 0xFF );

      for( ByteIt loopShift = 0; loopShift < 8; loopShift++ )  
      {
        Assert::IsTrue( eightLinks.HasShift( loopShift ));
      }

      for( int linkLoop = 0; linkLoop <= 0xFF; linkLoop++ )  
      {
        Assert::IsTrue( eightLinks.HasLink( linkLoop ));
      }

      Assert::IsTrue( eightLinks.HasLink( eightLinks ));
      Assert::IsFalse( eightLinks.ToShift( &shift ));
      Assert::IsFalse( eightLinks.ToPoint( &point ));

      size = 8;
      Link eightLinkMirrored = eightLinks.Mirror();
      Assert::AreEqual( size, eightLinks.Count());
      Assert::AreEqual( size, eightLinkMirrored.Count());
      Assert::IsFalse( eightLinks.IsSingle());
      Assert::IsFalse( eightLinkMirrored.IsSingle());
      Assert::IsFalse( eightLinks.IsEmpty());
      Assert::IsFalse( eightLinkMirrored.IsEmpty());

      Assert::AreEqual( eightLinkMirrored.Raw(), eightLinkMirrored.Raw());
    }

    TEST_METHOD(LinkReduction)
    {
      WriteMessage( "LinkReduction" );
      Link link1, link2;

      WriteMessage( "Zero link" );
      Link zeroLink( 0 );
      Assert::IsTrue( zeroLink.Reduce( &link1, &link2 ));
      Assert::IsTrue( link1.IsEmpty());
      Assert::IsTrue( link2.IsEmpty());

      WriteMessage( "Symmetrical links" );
      for( int linkLoop = 1; linkLoop <= 0xFF; linkLoop++ )  
      {
        Link link( linkLoop );
        Link mirrored_link = link.Mirror();
        Link symmetrical_link = link + mirrored_link;
        Assert::IsTrue( symmetrical_link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1.IsEmpty());
        Assert::IsTrue( link2.IsEmpty());
      }
    
      WriteMessage( "Asymmetrical 1 link(s)" );
      for( ByteIt loopShift = 0; loopShift < 8; loopShift++ )  
      {
        Link link( 0x01 << loopShift );
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == link );
        Assert::IsTrue( link2 == link );
      }

      // with some extra symmetrical links
      for( ByteIt loopShift1 = 0; loopShift1 < 8; loopShift1++ )  
      {
        for( ByteIt loopShift2 = 0; loopShift2 < 8; loopShift2++ )  
        {
          Link link( 0x01 << loopShift1 );
          Link symmetrical( 0x01 << loopShift2 );

          if( link == symmetrical || link == symmetrical.Mirror())
          {
            continue;
          }

          Link combined = link + symmetrical + symmetrical.Mirror();

          Assert::IsTrue( combined.Reduce( &link1, &link2 ));
          Assert::IsTrue( link1 == link );
          Assert::IsTrue( link2 == link );
        }
      }

      WriteMessage( "Asymmetrical 2 link(s)" );
      for( ByteIt loopShift1 = 0; loopShift1 < 8; loopShift1++ )  
      {
        for( ByteIt loopShift2 = 0; loopShift2 < 8; loopShift2++ )  
        {
          if( loopShift1 == loopShift2 )
          {
            continue;
          }
          Link firstLink  ( 0x01 << loopShift1 );
          Link secondLink ( 0x01 << loopShift2 );

          if( firstLink == secondLink.Mirror())
          {
            continue;
          }

          Link link = firstLink + secondLink;
          PosIt distance = abs( loopShift1 - loopShift2 );

          Assert::IsTrue( link.Reduce( &link1, &link2 ));

          if( distance == 1 || distance == 7 )
          {
            Assert::IsTrue( link1 == firstLink || link1 == secondLink );
            Assert::IsTrue( link2 == firstLink || link2 == secondLink );
            Assert::IsTrue( link1 != link2 );
          }
          else if( distance == 2 || distance == 6 )
          {
            Link middle_link;
            if( distance == 6 )
            {
              if( loopShift1 == 0 || loopShift2 == 0 )
              {
                middle_link = Link( 0x01 << 7 );
              }
              else if( loopShift1 == 1 || loopShift2 == 1 )
              {
                middle_link = Link( 0x01 << 0 );
              }
              else
              {
                Assert::IsTrue( false );
              }
            }
            else
            {
              middle_link = Link( 0x01 << (( loopShift1 + loopShift2 ) / 2 ));
            }

            Assert::IsTrue( link1 == middle_link );
            Assert::IsTrue( link2 == middle_link );
          }
          else
          {
            Assert::IsTrue( link1 != firstLink && link1 != secondLink );
            Assert::IsTrue( link2 != firstLink && link2 != secondLink );
            Assert::IsTrue( link1 == link2 );
          }
        }
      }

      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 4 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 3 ));
        Assert::IsTrue( link1 == link2 );
      }
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 6 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 7 ));
        Assert::IsTrue( link1 == link2 );
      }

      WriteMessage( "Asymmetrical 3 link(s)" );
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 3 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 2 ));
        Assert::IsTrue( link2 == Link( 0x01 << 2 ));
      }
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 4 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 2 ));
        Assert::IsTrue( link2 == Link( 0x01 << 3 ));
      }
      {
        Link link(( 0x01 << 5 ) | ( 0x01 << 6 ) | ( 0x01 << 7 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 6 ));
        Assert::IsTrue( link2 == Link( 0x01 << 6 ));
      }
      {
        Link link(( 0x01 << 5 ) | ( 0x01 << 6 ) | ( 0x01 << 0 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 6 ));
        Assert::IsTrue( link2 == Link( 0x01 << 7 ));
      }

      WriteMessage( "Asymmetrical 4 link(s)" );
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 3 ) | ( 0x01 << 4 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 2 ));
        Assert::IsTrue( link2 == Link( 0x01 << 3 ));
      }
      {
        Link link(( 0x01 << 0 ) | ( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 7 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 0 ));
        Assert::IsTrue( link2 == Link( 0x01 << 1 ));
      }

      WriteMessage( "Asymmetrical with mixed symmetrical link" );
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 6 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 1 ));
        Assert::IsTrue( link2 == Link( 0x01 << 1 ));
      }
      {
        Link link(( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 3 ) | ( 0x01 << 6 ) | ( 0x01 << 7 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 1 ));
        Assert::IsTrue( link2 == Link( 0x01 << 1 ));
      }
      {
        Link link(( 0x01 << 0 ) | ( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 3 ) | ( 0x01 << 4 ) | ( 0x01 << 6 ) | ( 0x01 << 7 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 1 ));
        Assert::IsTrue( link2 == Link( 0x01 << 1 ));
      }
      {
        Link link(( 0x01 << 0 ) | ( 0x01 << 1 ) | ( 0x01 << 2 ) | ( 0x01 << 6 ));
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1 == Link( 0x01 << 0 ));
        Assert::IsTrue( link2 == Link( 0x01 << 1 ));
      }

      // 6 Feb 2016 - DMA - Run-time error
      // Case when asymmetrical links neutralize each other.
      {
        Link link( 74 );
        Assert::IsTrue( link.Reduce( &link1, &link2 ));
        Assert::IsTrue( link1.IsEmpty());
        Assert::IsTrue( link2.IsEmpty());
      }
    }
  };
}
/*******************************************************************************
 FILE         :   IFeederTextTest.cpp

 COPYRIGHT    :   DMAlex, 2011-2017

 DESCRIPTION  :   Simple data provider for testing

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   06/29/2017
*******************************************************************************/

#include "stdafx.h"
#include "IFeederTextTest.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/xpressive/xpressive.hpp>


using namespace Its;
using namespace DCommon;

using namespace boost::xpressive;
using boost::format;

extern const char* tagListSeparator;

const char * One    = "1";
const char * Two    = "2";
const char * Three  = "3";


IFeederTextTest::IFeederTextTest( TestFeederType _feedType ) :
  feedType(_feedType)
{
  DLog::msg( DLog::LevelDebug, "Loading '%s' feeder", Name() );
}



IFeederTextTest::~IFeederTextTest()
{
  DLog::msg( DLog::LevelDebug, "Cleaning '%s' feeder", Name() );
}


// well, for text data types it's kind of "made up" value, sort of "one word per cycle",
// 100 seems to be enough for one looooooooooooooooooooong word ;-)
//
// 24 Aug 2017 - DMA - All data types are equivalent now so we can consume the whole
// "bottom layer" for better training.
CapacityIt IFeederTextTest::Capacity()
{
  return CapacityIt( dimensions.X * dimensions.Y, 0, 0 ); 
}


StringList_t IFeederTextTest::Tags()
{
  StringList_t tagList;
  
  // Start with numbers
  switch( feedType )
  {
    case NoFeed:
      tagList.push_back( "no" );
      break;
    case BaseFeed:
      tagList.push_back( One );
      tagList.push_back( Two );
      tagList.push_back( Three );
      break;
    case SimpleFeed:
    case RussianFeed:
      assert(false); // Not supported
      break;
  }

  tagList.sort( Its::StringOrder );
  return tagList;
}



// List of all unique frame Ids with this tag(s) / complex topic
StringList_t IFeederTextTest::Frames( const char* _tagLine, LevelIt level )
{
  typedef boost::tokenizer< boost::char_separator<char> > tokenizer;
  boost::char_separator<char> tagSeparator( tagListSeparator );
  
  string       tagLine ( _tagLine );
  tokenizer    tokens  ( tagLine, tagSeparator );
  StringList_t frameIds;
  format       frameIdFormat( "TextTest_%s_%d" );

  if( tagLine.find( "no" ) != string::npos )
  {
    frameIds.push_back( "TextTest_0_0" );
  }

  for( tokenizer::iterator tokenLoop = tokens.begin(); tokenLoop != tokens.end(); ++tokenLoop )
  {
    if( *tokenLoop == One || *tokenLoop == Two || *tokenLoop == Three )
    {
      for( int frameLoop = 1; frameLoop < 100; frameLoop++ )
      {
        frameIds.push_back( str( frameIdFormat % *tokenLoop % frameLoop ) );
      }
    }
  }

  frameIds.sort   ( Its::StringOrder );
  frameIds.unique ( Its::StringEqual );

  return frameIds;
}


// How long is the frame?
PeriodIt IFeederTextTest::TicksInFrame( const char* frameId )
{
  return 1;
}


void IFeederTextTest::LoadFrame( const char* _frameId, PeriodIt tick, IFrameSet& frameSet )
{
  if( frameSet.Capacity() != Capacity())
  {
    throw IFeederException( DException::Error, "Text test feeder can not load frame, inconsistent capacity %dT x %dA x %dV",
                            frameSet.Capacity().sizeText, frameSet.Capacity().sizeAudio, frameSet.Capacity().sizeVideo );
  }

  memset( frameSet.text(), NOSIGNAL, frameSet.Capacity().sizeText );
  string  frameId( _frameId );

  if( tick != 0 || frameId == "TextTest_0_0" )
  {
    DLog::msg( DLog::LevelVerbose, "No frame '%s' in '%d' tick in '%s' feeder", _frameId, tick, Name() );
    return;
  }

  sregex  frameIdRegex = sregex::compile( "^([A-Za-z]+)_(\\d+)_(\\d+)$" );
  smatch  what;
  LevelIt level;
  string  prefix;
  string  tag;
  int id;

  if( !regex_match( frameId, what, frameIdRegex ) || what.size() != 4 )
  {
    throw IFeederException( DException::Error, "String '%s' is not recognized as frame Id for feeder '%s'", _frameId, Name() );
  }
  
  prefix = what[ 1 ];
  tag    = what[ 2 ];
  id     = boost::lexical_cast< int > ( what[ 3 ] );

  if( prefix != "TextTest" || ( tag != One && tag != Two && tag != Three ))
  {
    throw IFeederException( DException::Error, "String '%s' has no valid tag %s or prefix %s in feeder '%s'", _frameId, prefix.c_str(), tag.c_str(), Name() );
  }

  // We can not produce random frames as they should be consistent, same id => same frame,
  // but we want to distribute input evenly.
  SizeIt maxSize = frameSet.Capacity().sizeText;
  SizeIt pos;

  if( tag == One )
  {
    pos = ( 7 * id ) % maxSize;
    *( frameSet.text() + pos ) = One[ 0 ];
  }
  else
  {
    // We will try to separate two "streams/channels" initially to make it easier to learn.
    // Hopefully by the time different type of inputs ("1" / "22" / "333") converge
    // we have established root for each one so new inputs may be adopted faster.
    if( tag == Two ) 
    {
      pos = maxSize - (( 11 * id ) % maxSize ) - 1;
    } 
    else
    {
      pos = ( 17 * id ) % maxSize;
    }

    // Avoid right edge as we don't want to separate these dots
    if( pos % dimensions.X == dimensions.X - 2 )
    {
      pos -= 1;
      DLog::msg( DLog::LevelInfo, "Text Test Feeder shifting one pos '%d', max '%d'", pos, maxSize );
    }

    if( pos % dimensions.X == dimensions.X - 1 )
    {
      pos -= 2;
      DLog::msg( DLog::LevelInfo, "Text Test Feeder shifting two pos '%d', max '%d'", pos, maxSize );
    }

    if( pos >= maxSize - 2 )
    {
      pos = maxSize - 3;
      DLog::msg( DLog::LevelInfo, "Text Test Feeder setting pos '%d', max '%d'", pos, maxSize );
    }

    if( tag == Two )
    {
      *( frameSet.text() + pos + 0 ) = Two[ 0 ];
      *( frameSet.text() + pos + 1 ) = Two[ 0 ];
    }
    else
    {
      *( frameSet.text() + pos + 0 ) = Three[ 0 ];
      *( frameSet.text() + pos + 1 ) = Three[ 0 ];
      *( frameSet.text() + pos + 2 ) = Three[ 0 ];
    }
  }

  DLog::msg( DLog::LevelVerbose, "Loading frame '%s' in '%d' tick in '%s' feeder", _frameId, tick, Name() );
  DebugInfo( _frameId, tick, frameSet );
}


void IFeederTextTest::SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet )
{
  if( frameSet.Capacity().sizeAudio > 0 ||
      frameSet.Capacity().sizeVideo > 0 )
  {
    throw IFeederException( DException::Error, "Feeder '%s' can only save text", Name() );
  }

  DebugInfo( frameId, tick, frameSet );
}




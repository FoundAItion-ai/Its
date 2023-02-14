/*******************************************************************************
 FILE         :   IFeederTextABC.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Text alphabet feeder

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/29/2011

 LAST UPDATE  :   09/29/2011
*******************************************************************************/

#include "stdafx.h"

#include "IFeederTextABC.h"

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/date_time/time_facet.hpp>

using namespace Its;
using namespace DCommon;

using namespace boost::xpressive;
using namespace boost::algorithm;

using boost::format;


const char* tagListSeparator = " ";


IFeederTextABC::IFeederTextABC( ByteIt from_char, ByteIt to_char )
{
  DLog::msg( DLog::LevelDebug, "Loading '%s' feeder", Name() );

  tagList.clear();
  
  for( ByteIt loopSymbol = from_char; loopSymbol <= to_char; loopSymbol++ )
  {
    string symbolTag = boost::lexical_cast< string >( loopSymbol );

    tagList.push_back( symbolTag );
  }
}


IFeederTextABC::~IFeederTextABC()
{
  DLog::msg( DLog::LevelDebug, "Cleaning '%s' feeder", Name() );
}


StringList_t IFeederTextABC::Tags()
{
  return tagList;
}



// List of all unique frame Ids with this tag(s)
StringList_t IFeederTextABC::Frames( const char* _tagLine, LevelIt level )
{
  typedef boost::tokenizer< boost::char_separator<char> > tokenizer;

  boost::char_separator<char> tagSeparator( tagListSeparator );
  
  string       tagLine ( _tagLine );
  tokenizer    tokens  ( tagLine, tagSeparator );
  StringList_t frameIds;
  format       frameIdFormat( "%s_%d" );
    
  for( tokenizer::iterator tokenLoop = tokens.begin(); tokenLoop != tokens.end(); ++tokenLoop )
  {
    if( find( tagList.begin(), tagList.end(), *tokenLoop ) != tagList.end() )
    {
      frameIds.push_back( str( frameIdFormat % *tokenLoop % level ) );
    }
  }

  return frameIds;
}



void IFeederTextABC::LoadFrame( const char* _frameId, PeriodIt tick, IFrameSet& frameSet )
{
  if( frameSet.Capacity() != Capacity())
  {
    throw IFeederException( DException::Error, "Text ABC feeder can not load frame, inconsistent capacity %dT x %dA x %dV",
                            frameSet.Capacity().sizeText, frameSet.Capacity().sizeAudio, frameSet.Capacity().sizeVideo );
  }
  
  memset( frameSet.text(), NOSIGNAL, frameSet.Capacity().sizeText );

  if( tick > TicksInFrame( _frameId ) || tick % DEFAULT_TICKS_IN_FRAME > DEFAULT_EMPTY_TICKS_IN_FRAME )
  {
    DLog::msg( DLog::LevelVerbose, "No frame '%s' in '%d' tick in '%s' feeder", _frameId, tick, Name() );
    return;
  }

  sregex  frameIdRegex = sregex::compile( "^([A-Z])_(\\d+)$" );
  smatch  what;
  LevelIt level;
  string  frameId( _frameId );
  string  tag;

  if( !regex_match( frameId, what, frameIdRegex ) || what.size() != 3 )
  {
    throw IFeederException( DException::Error, "String '%s' is not recognized as frame Id for feeder '%s'", _frameId, Name() );
  }
  
  tag   = what[ 1 ];
  level = boost::lexical_cast< LevelIt > ( what[ 2 ] );
  level = min< LevelIt >( level, Capacity().sizeText );

  if( find( tagList.begin(), tagList.end(), tag ) == tagList.end() ||
      tag.length() != 1 )
  {
    throw IFeederException( DException::Error, "String '%s' has no valid tag in feeder '%s'", _frameId, Name() );
  }

  // Do lowercase output for testing purpose or just like it  ;-)
  string lowerCaseTag = tag;
  to_lower( lowerCaseTag );

  // on low levels, it's just one letter, then may be more
  //for( LevelIt loopLevel = 0; loopLevel < level; loopLevel++ )
  for( LevelIt loopLevel = 0; loopLevel < 1; loopLevel++ )
  {
    *( frameSet.text() + loopLevel ) = ( ByteIt ) ( lowerCaseTag[ 0 ] );
  }

  DLog::msg( DLog::LevelVerbose, "Loading frame '%s' in '%d' tick in '%s' feeder", _frameId, tick, Name() );
  DebugInfo( _frameId, tick, frameSet );
}


void IFeederTextABC::SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet )
{
  if( frameSet.Capacity().sizeAudio > 0 ||
      frameSet.Capacity().sizeVideo > 0 )
  {
    throw IFeederException( DException::Error, "Feeder '%s' can only save text", Name() );
  }

  DebugInfo( frameId, tick, frameSet );
}




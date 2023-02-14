/*******************************************************************************
 FILE         :   IFeeder.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Data mining/collection/providers

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/30/2011

 LAST UPDATE  :   09/30/2011
*******************************************************************************/


#include "stdafx.h"
#include "IFeeder.h"


using namespace Its;
using namespace DCommon;


DCriticalSection IFrameSet::section;


IFrameSet::IFrameSet( CapacityIt _capacity, LevelIt _level, Interaction_t _type ):
  textFrame  ( _capacity.sizeText  ),
  audioFrame ( _capacity.sizeAudio ),
  videoFrame ( _capacity.sizeVideo ),
  level      ( _level ),
  type       ( _type  )
{      
  // do check type and size?
  // or we can have say, audio | text frame set with text set size 0?
}



void IFrameSet::Clear()
{
  Clear( type );
}


void IFrameSet::Clear( Interaction_t frameType )
{
  // Clear only types of the frameset
  if(( type & Its::text ) && ( frameType & Its::text ))
  {
    textFrame.Clear();
  }
  
  if(( type & Its::audio ) && ( frameType & Its::audio ))
  {
    audioFrame.Clear();
  }

  if(( type & Its::video ) && ( frameType & Its::video ))
  {
    videoFrame.Clear();
  }
}



// normally copy from small source to large this
// at = this, destination, size = source.Capacity()
void IFrameSet::CopyFrom( IFrameSet& source, CapacityIt at )
{
  if( level != source.level )
  {
    throw IFeederException( "Can not copy from FrameSet, levels are different" );
  }

  // We can copy into a larger type, say
  // text set into text | audio set,
  // but not another way around.
  //
  if( !(( type & source.type ) == source.type ))
  {
    throw IFeederException( "Can not copy from FrameSet, types are not compatible" );
  }

  if(( source.type & Its::text ) &&
     ( at.sizeText + source.Capacity().sizeText <= textFrame.Size() ))
  {
    memcpy( textFrame + at.sizeText, source.text(), source.Capacity().sizeText );
  }
  
  if(( source.type & Its::audio ) &&
     ( at.sizeAudio + source.Capacity().sizeAudio <= audioFrame.Size() ))
  {
    memcpy( audioFrame + at.sizeAudio, source.audio(), source.Capacity().sizeAudio );
  }
  
  if(( source.type & Its::video ) &&
     ( at.sizeVideo + source.Capacity().sizeVideo <= videoFrame.Size() ))
  {
    memcpy( videoFrame + at.sizeVideo, source.video(), source.Capacity().sizeVideo );
  }  
}


// normally copy from large this to small destination
// at = this, source, size = destination.Capacity()
void IFrameSet::CopyTo( IFrameSet& destination, CapacityIt at )
{
  if( level != destination.level )
  {
    throw IFeederException( "Can not copy to FrameSet, levels are different" );
  }

  // We can copy into a smaller type, say
  // text | audio set into text set,
  // but not another way around.
  //
  if( !(( type & destination.type ) == destination.type ))
  {
    throw IFeederException( "Can not copy to FrameSet, types are not compatible" );
  }

  if(( destination.type & Its::text ) &&
     ( at.sizeText + destination.Capacity().sizeText <= textFrame.Size() ))
  {
    memcpy( destination.text(), textFrame + at.sizeText, destination.Capacity().sizeText );
  }
  
  if(( destination.type & Its::audio ) &&
     ( at.sizeAudio + destination.Capacity().sizeAudio <= audioFrame.Size() ))
  {
    memcpy( destination.audio(), audioFrame + at.sizeAudio, destination.Capacity().sizeAudio );
  }
  
  if(( destination.type & Its::video ) &&
     ( at.sizeVideo + destination.Capacity().sizeVideo <= videoFrame.Size() ))
  {
    memcpy( destination.video(), videoFrame + at.sizeVideo, destination.Capacity().sizeVideo );
  }
  
}


void IFrameSet::DebugInfo()
{
  // Have no real static members, but log is shared
  // and though it's thread-safe but to keep output
  // consistent, in one piece.
  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelVerbose, "============ Frame set content of type '%d' and level '%d' ============>", type, level );

  if( type & Its::text )
  {
    DLog::msg( DLog::LevelVerbose, "" );
    DLog::msg( DLog::LevelVerbose, "Text content:" );
    DebugInfo( textFrame, true );
  }

  if( type & Its::audio )
  {
    DLog::msg( DLog::LevelVerbose, "" );
    DLog::msg( DLog::LevelVerbose, "Audio content:" );
    DebugInfo( audioFrame, false );
  }

  if( type & Its::video )
  {
    DLog::msg( DLog::LevelVerbose, "" );
    DLog::msg( DLog::LevelVerbose, "Video content:" );
    DebugInfo( videoFrame, false );
  }

  DLog::msg( DLog::LevelVerbose, "" );
  DLog::msg( DLog::LevelVerbose, "<============ Frame set content of type '%d' and level '%d' ============", type, level );
}


void IFrameSet::DebugInfo( IFrame& frame, bool outputAsText )
{
  string debugOutput( "" );
  char   buf[ 33 ];

  for( SizeIt loopByte = 0; loopByte < frame.Size(); loopByte++ )
  {
    ByteIt data = *( frame + loopByte );

    if( outputAsText )
    {
      DebugTextByteToChar( data, buf, sizeof( buf ));
    }
    else
    {
      DebugByteToChar( data, buf, sizeof( buf ));
    }

    debugOutput += buf;
  }

  // Log limited with 1K anyway
  if( debugOutput.length() > DebugOutputLimit )
  {
    debugOutput.resize( DebugOutputLimit );
  }

  DLog::msg( DLog::LevelVerbose, "Frame size '%d' byte(s), content text size '%d', debug output is limited with '%d' chars", frame.Size(), debugOutput.size(), DebugOutputLimit );
  DLog::msg( DLog::LevelVerbose, "Frame content '%s'", debugOutput.c_str() );
}



// 0-100 %
int IFrameSet::Compare( IFrameSet& that )
{
  if( type != that.type )
  {
    throw IFeederException( "Can not compare FrameSets of different types" );
  }
  
  if( level != that.level )
  {
    throw IFeederException( "Can not compare FrameSets of different levels" );
  }
  
  int textSimilarity, audioSimilarity, videoSimilarity;
  
  textSimilarity  = Compare( textFrame,  that.textFrame,  Its::text,  level );
  //audioSimilarity = Compare( audioFrame, that.audioFrame, Its::audio, level );
  //videoSimilarity = Compare( videoFrame, that.videoFrame, Its::video, level );

  // add weights?

  // 
  // TEMP - TOFIX - just count text.
  //
  //return ( textSimilarity + audioSimilarity + videoSimilarity ) / 3;

  return textSimilarity;
}
  
  
  
int IFrameSet::Compare( IFrame& one, IFrame& two, Interaction_t frameType, LevelIt frameLevel )
{
  // For different levels we may use different functions.
  //
  // And we may want to compare different types differently,
  //
  // say, texts based on "letters proximity",  a & a = 100, a & A = 99,...
  // a & letter = 50, a & symbol (not letter) = 0
  //
  
  if( one.Size() == 0 && two.Size() == 0 )
  {
    return 100;
  }

  if( one.Size() != two.Size() )
  {
    return 0;
  }

  SizeIt  errors = 0;
  int errorsRate = 0;


  // So far this is universal byte to byte comparision
  // but we can make it based on type
  // as it may be different
  //
  // CountErrorsLevel_3 / CountErrorsLevel_4 don't make sense yet as reflections
  // will likely outnumber input.
  switch( frameLevel )
  {
    case 1:  errors = CountErrorsLevel_1( one, two ); 
             break;
    
    case 2:
    case 3: 
    case 4:  
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:  errors = CountErrorsLevel_2( one, two ); 
             break;

    default: throw IFeederException( "Can not compare at this level." );
  }

  DLog::msg( DLog::LevelDebug, "Stream of type %d and level '%d' and size %5d contains %5d errors", frameType, frameLevel, one.Size(), errors );

  errorsRate = ( int ) ((( float )errors / ( float )one.Size() ) * 100.00 );
  return 100 - errorsRate; // = PQ rate
}


// Any signal at all?
SizeIt IFrameSet::CountErrorsLevel_1( IFrame& one, IFrame& two )
{
  if( one.Size() != two.Size())
  {
    throw IFeederException( "Can not compare frames, size is different." );
  }

  bool    signalInStream     = false;
  bool    signalInBaseStream = false;
  SizeIt  errors = 0;

  for( SizeIt loopData = 0; loopData < one.Size(); loopData++ )
  {
    ByteIt byte     = *(( ByteIt* )one + loopData );
    ByteIt byteBase = *(( ByteIt* )two + loopData );

    //
    // 0 is our default data for Cells, we might want to change it to NOSIGNAL though...
    // but for now just disregard 0-s
    //
    if( byte != INITIAL_DATA && byte != NOSIGNAL )
    {
      signalInStream = true;
    }
    
    if( byteBase != INITIAL_DATA && byteBase != NOSIGNAL )
    {
      signalInBaseStream = true;
    }
    
    if( signalInStream && signalInBaseStream )
    {
      break;
    }
  }
  
  if( signalInStream == signalInBaseStream )
  {
    errors = 0;
  }
  else
  {
    errors = one.Size();
  }
  
  DLog::msg( DLog::LevelVerbose, "Count errors on level 1: '%d' signal in stream, '%d' signal in base stream", signalInStream, signalInBaseStream );
  return errors;
}


// Any signal, at least at the same positions as base? Zero tolerance.
SizeIt IFrameSet::CountErrorsLevel_2( IFrame& one, IFrame& two )
{
  if( one.Size() != two.Size())
  {
    throw IFeederException( "Can not compare frames, size is different." );
  }

  // Sample:  in  = ...x..x..t..........
  //          out = ...x..t.....t..r....
  //
  // Found 1 error
  //
  for( SizeIt loopData = 0; loopData < one.Size(); loopData++ )
  {
    ByteIt byte     = *(( ByteIt* )one + loopData );
    ByteIt byteBase = *(( ByteIt* )two + loopData );

    bool anyByte     = byte     != INITIAL_DATA && byte     != NOSIGNAL;
    bool anyByteBase = byteBase != INITIAL_DATA && byteBase != NOSIGNAL;

    if( anyByteBase && !anyByte )
    {
      return one.Size();
    }
  }
  
  return 0;
}


// Same signal at the same positions?
SizeIt IFrameSet::CountErrorsLevel_3( IFrame& one, IFrame& two )
{
  SizeIt errors = 0;
  
  if( one.Size() != two.Size())
  {
    throw IFeederException( "Can not compare frames, size is different." );
  }

  // Sample:  in  = ...t..t..t..........
  //          out = ...x..t.....t..r....
  //
  // Found 4 errors
  //
  for( SizeIt loopData = 0; loopData < one.Size(); loopData++ )
  {
    ByteIt byte     = *(( ByteIt* )one + loopData );
    ByteIt byteBase = *(( ByteIt* )two + loopData );

    // Substitute INITIAL_DATA and NOSIGNAL for the purpose of the comparison
    if( !(( byte     == INITIAL_DATA || byte     == NOSIGNAL ) && 
          ( byteBase == INITIAL_DATA || byteBase == NOSIGNAL )))
    {
      if( byte != byteBase )
      {
        errors++;
      }
    }
  }
  
  return errors;
}


// Identical signal at the same positions? Zero tolerance.
SizeIt IFrameSet::CountErrorsLevel_4( IFrame& one, IFrame& two )
{
  SizeIt errorsCount = CountErrorsLevel_3( one, two );

  if( errorsCount != 0 )
  {
    errorsCount = one.Size();
  }

  return errorsCount;
}

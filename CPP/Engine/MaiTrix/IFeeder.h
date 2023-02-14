/*******************************************************************************
 FILE         :   IFeeder.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Abstract base for data mining/collection/providers

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   09/07/2011
*******************************************************************************/


#ifndef __IFEEDER_H
#define __IFEEDER_H

#include <memory>

#include "ITypes.h"
#include "BasicCubeTypes.h"
#include "MaiTrix.h"


using namespace std;


namespace Its
{

//
// Default image size should match maiTrix size due to interlacing
// for better image view (not absolutely needed, just nice to look at)
//
#define DefaultImageSize              SizeIt2D( INITIAL_SIZE_X, INITIAL_SIZE_Y )


#define DEFAULT_TEXT_CAPACITY         50
#define DEFAULT_AUDIO_CAPACITY        50                                             // 44Khz?
#define DEFAULT_VIDEO_CAPACITY        50                                             // 30 fps, 640 x 480 ?
#define DEFAULT_IMAGE_CAPACITY        ( DefaultImageSize.X * DefaultImageSize.Y )    // 1 fps,  640 x 480 ?

#define DEFAULT_TICKS_IN_FRAME        ( INITIAL_SIZE_Z )  // initial

// 20 Feb 2013 - DMA - Note no empty ticks for now, testing
//
#define DEFAULT_EMPTY_TICKS_IN_FRAME  ( INITIAL_SIZE_Z )  // no output from feeder
//#define DEFAULT_EMPTY_TICKS_IN_FRAME  ( INITIAL_DELAY )  // no output from feeder



class MAITRIX_SPEC IFeederException: public MaiTrixException
{
public:
  DEXCEPTION_CONSTRUCTOR0( IFeederException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR1( IFeederException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR2( IFeederException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR3( IFeederException, MaiTrixException );
};



// BLOB 
struct MAITRIX_SPEC IFrame
{
public:
  IFrame( SizeIt _size ):
    data( _size ),
    size( _size )
  {
    
  }

  IFrame( IFrame& copy ):
    data( copy.Size() ),
    size( copy.Size() )
  {
    CopyFrom( copy );
  }

  //
  // always dangerous implicit data type conversion
  // better be explicit
  //
  operator ByteIt* () 
  { 
    return data.data(); 
  }

  void Clear()
  {
    // Should be data type dependant?
    // Like ' ' for text, gray for video?
    //
    memset( data.data(), NOSIGNAL, size );
  }

  bool Empty()
  {
    bool isEmpty = true;

    for( SizeIt loopByte = 0; loopByte < size; loopByte++ )
    {
      ByteIt byte = *( ( ByteIt* )data.data() + loopByte );

      if( !( byte == 0 || byte == NOSIGNAL ))
      {
        isEmpty = false;
        break;
      }
    }

    return isEmpty;
  }

  SizeIt Size() { return size; }

  void operator = ( IFrame& copy ) 
  {
    CopyFrom( copy );
  }

protected:
  void CopyFrom( IFrame& copy )
  {
    if( size != copy.size )
    {
      throw IFeederException( "Can not copy from IFrame, size is different" );
    }

    memcpy( data.data(), ( ByteIt* )copy, size );
  }


private:
  ByteArray data; // stream
  SizeIt    size;
};



// BLOB as a typed and leveled collection of BLOBs 
struct MAITRIX_SPEC IFrameSet
{
public:
  IFrameSet( CapacityIt _capacity, LevelIt _level, Interaction_t _type );

  // 0-100 %
  virtual int Compare( IFrameSet& that );

  bool operator == ( IFrameSet& that ) { return this->Compare( that ) == 100; }

  CapacityIt Capacity() { return CapacityIt( textFrame .Size(), 
                                             audioFrame.Size(), 
                                             videoFrame.Size() ); }

  void Clear(); // Clear all channels
  void Clear( Interaction_t frameType ); // Clear selected channel

  // normally copy from small source to large this
  // at = this, destination, size = source.Capacity()
  void CopyFrom( IFrameSet& source, CapacityIt at ); 
  
  // normally copy from large this to small destination
  // at = this, source, size = this.Capacity()
  void CopyTo( IFrameSet& destination, CapacityIt at );

  ByteIt* text () { return ( ByteIt* )textFrame;  }
  ByteIt* audio() { return ( ByteIt* )audioFrame; }
  ByteIt* video() { return ( ByteIt* )videoFrame; }

  void DebugInfo();

protected:
  static const int DebugOutputLimit = 900; // Log limited with 1K anyway

  static int  Compare( IFrame& one, IFrame& two, Interaction_t frameType, LevelIt frameLevel );
  static void DebugInfo( IFrame& frame, bool outputAsText ); // outputAsText - text/binary flag

  // So far this is universal byte to byte comparision
  // but we can make it based on type
  // as it may be different
  static SizeIt CountErrorsLevel_1( IFrame& one, IFrame& two );
  static SizeIt CountErrorsLevel_2( IFrame& one, IFrame& two );
  static SizeIt CountErrorsLevel_3( IFrame& one, IFrame& two );
  static SizeIt CountErrorsLevel_4( IFrame& one, IFrame& two );

private:
  static DCriticalSection section;
  IFrame        textFrame;
  IFrame        audioFrame;
  IFrame        videoFrame;
  LevelIt       level; // level depends on frame, not feeder! (Sep 30 - not sure why is that?)
  Interaction_t type;
};



/*

Tagline - space separated tags, like "sun home man";

IFeeder is collecting frames;

Frames are timed data with tags and set difficulty level;

Store them as a 1 sec frame batches? (=> implementation)

Frameset is a collection of frames of the same type (text, audio, video...)
and comparison rules.

*/


//
// ***************** ALL STREAMS MUST BE CONSISTENT ***************** 
//
// means loading the same frameId for the same tick gives the same data!
//
class MAITRIX_SPEC IFeeder
{
public:

  // in MSDN and C++ draft it's not obvious what to do
  // but not cleaning without this 
  virtual ~IFeeder(){}

  // Update feeder content with actual source
  virtual void Sync() = 0;

  // Feeder's name, type (audio/video/text) and max capacity in bytes / tick;
  virtual const char*   Name()     = 0;
  virtual Interaction_t Type()     = 0;
  virtual CapacityIt    Capacity() = 0;

  // Some type of content, like images should be aware of input dimensions
  // to be properly presented (not warped). And while this may be ok for maiTrix
  // to learn those images in any form, but for troubleshooting/verification we'd rather
  // see them straight.
  virtual void SetDimensions( const SizeIt2D & dimensions ) = 0;

  // List of all unique tags
  virtual StringList_t Tags() = 0;

  // List of all unique frame Ids with this tag(s) and difficulty
  virtual StringList_t Frames( const char* tagLine, LevelIt level ) = 0;
  
  // How long is the frame?
  virtual PeriodIt TicksInFrame( const char* frameId ) = 0;
 
  // If no data found just return 'empty' frame (type dependant, 'text spaces', black image, ...)
  virtual void LoadFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet ) = 0;
  
  virtual void SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet ) = 0;
};


/*

chat feeder

class MAITRIX_SPEC ITalk : public IFeeder
{
}


*/


}; // namespace Its


#endif // __IFEEDER_H

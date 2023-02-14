/*******************************************************************************
 FILE         :   ITypes.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Shared MaiTrix types

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/14/2011

 LAST UPDATE  :   07/14/2011
*******************************************************************************/


#ifndef __ITYPES_H
#define __ITYPES_H

#include <list>
#include <string>

#include "Init.h"
#include "Cube.h"
#include "MaiTrixDefines.h"


using namespace std;


namespace Its
{


#define ASCII_LOW_CHAR  32
#define ASCII_HIGH_CHAR 126



/*
  
    Basic conversation types supported on maiTrix level
    
    No mixed types!
    
    2 Oct 2011 - This is the feeders type now and can be mixed,
    check for maiTrix layer types in MaiTrix.h / Layer_t

*/
enum Interaction_t
{
  text  = 0x01,
  audio = 0x02,
  video = 0x04,
  
  // mixed types
  textaudio      = text  | audio,
  textvideo      = text  | video,
  audiovideo     = audio | video,
  textaudiovideo = audio | video | text
};



typedef unsigned int LevelIt;


struct MAITRIX_SPEC CapacityIt
{
  CapacityIt():
    sizeText ( 0 ),
    sizeAudio( 0 ),
    sizeVideo( 0 )
  {
  }

  CapacityIt( const CapacityIt& copy ):
    sizeText ( copy.sizeText  ),
    sizeAudio( copy.sizeAudio ),
    sizeVideo( copy.sizeVideo )
  {
  }

  CapacityIt( SizeIt _sizeText, SizeIt _sizeAudio, SizeIt _sizeVideo ):
    sizeText ( _sizeText  ),
    sizeAudio( _sizeAudio ),
    sizeVideo( _sizeVideo )
  {
  }

  // less even if one parameter is less
  bool operator <  ( const CapacityIt& that ) { return this->sizeText <  that.sizeText || this->sizeAudio <  that.sizeAudio || this->sizeVideo <  that.sizeVideo; }
  bool operator <= ( const CapacityIt& that ) { return this->sizeText <= that.sizeText || this->sizeAudio <= that.sizeAudio || this->sizeVideo <= that.sizeVideo; }
  bool operator >  ( const CapacityIt& that ) { return this->sizeText >  that.sizeText || this->sizeAudio >  that.sizeAudio || this->sizeVideo >  that.sizeVideo; }
  bool operator >= ( const CapacityIt& that ) { return this->sizeText >= that.sizeText || this->sizeAudio >= that.sizeAudio || this->sizeVideo >= that.sizeVideo; }
  bool operator == ( const CapacityIt& that ) { return this->sizeText == that.sizeText && this->sizeAudio == that.sizeAudio && this->sizeVideo == that.sizeVideo; }
  bool operator != ( const CapacityIt& that ) { return this->sizeText != that.sizeText || this->sizeAudio != that.sizeAudio || this->sizeVideo != that.sizeVideo; }

  void operator  = ( const CapacityIt& that ) { this->sizeText  = that.sizeText ;
                                                this->sizeAudio = that.sizeAudio;
                                                this->sizeVideo = that.sizeVideo; }

  CapacityIt& operator +=( const CapacityIt& that ) { this->sizeText  = this->sizeText  + that.sizeText;
                                                      this->sizeAudio = this->sizeAudio + that.sizeAudio;
                                                      this->sizeVideo = this->sizeVideo + that.sizeVideo; 
                                                      return *this;
                                                    }

  CapacityIt& operator -=( const CapacityIt& that ) { this->sizeText  = this->sizeText  - that.sizeText;
                                                      this->sizeAudio = this->sizeAudio - that.sizeAudio;
                                                      this->sizeVideo = this->sizeVideo - that.sizeVideo; 
                                                      return *this;
                                                    }

  CapacityIt  operator + ( const CapacityIt& that ) { return CapacityIt( 
                                                              this->sizeText  + that.sizeText, 
                                                              this->sizeAudio + that.sizeAudio, 
                                                              this->sizeVideo + that.sizeVideo ); }

  CapacityIt  operator - ( const CapacityIt& that ) { return CapacityIt( 
                                                              this->sizeText  - that.sizeText, 
                                                              this->sizeAudio - that.sizeAudio, 
                                                              this->sizeVideo - that.sizeVideo ); }

  SizeIt sizeText;
  SizeIt sizeAudio;
  SizeIt sizeVideo;
};


// TODO:
// Should inherit from list and add compare functions below?
typedef list< string > StringList_t;

// to support merge/sort/unique, etc...
inline bool StringOrder( string& a, string& b )
{
  return a.compare( b ) < 0;
}


inline bool StringEqual( string& a, string& b )
{
  return a.compare( b ) == 0;
}


inline bool DebugTextByteToChar( ByteIt data, char *buf, SizeIt bufSize )
{
  bool converted = true;

  // each char or code - one symbol, otherwise '*'
  if( bufSize <= 1 )
  {
    converted = false;
  }
  else if( data >= ASCII_LOW_CHAR && data <= ASCII_HIGH_CHAR )
    {
      buf[ 0 ] = data;
      buf[ 1 ] = 0;
    }
    else
    {
      // NOSIGNAL = 32 so it would be displayed above
      //
      buf[ 0 ] = data == 0 || data == NOSIGNAL ? ' ' : '*';
      buf[ 1 ] = 0;
    }

  return converted;
}


inline bool DebugByteToChar( ByteIt data, char *buf, SizeIt bufSize )
{
  bool converted = false;

  // each char or code - four symbols;
  //
  // try convert to ASCII
  if( data >= ASCII_LOW_CHAR && data <= ASCII_HIGH_CHAR && bufSize >= 5 )
  {
    buf[ 0 ] = ' ';
    buf[ 1 ] = data;
    buf[ 2 ] = ' ';
    buf[ 3 ] = ' ';
    buf[ 4 ] = 0;

    converted = true;
  }
  else
  {
    if( data == 0 || data == NOSIGNAL )
    {
      converted = sprintf_s( buf, bufSize, "    " ) != -1;
    }
    else
    {
      converted = sprintf_s( buf, bufSize, "%3d;", data ) != -1;
    }
  }

  return converted;
}


inline void DebugBytesToText( ByteIt* streamText, SizeIt textSize, string& message )
{
  message = "";
  char buf[ 33 ];

  //
  // Use low ASCII table < 127, extended symbols replaced with '*'
  //
  for( SizeIt loopByte = 0; loopByte < textSize; loopByte++ )
  {
    ByteIt data = *( streamText + loopByte );

    DebugTextByteToChar( data, buf, sizeof( buf ));

    message += buf;
  }
}


}; // namespace Its


#endif // __ITYPES_H

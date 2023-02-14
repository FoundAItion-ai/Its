/*******************************************************************************
 FILE         :   IFeederTextABC.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Text alphabet feeder

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/29/2011

 LAST UPDATE  :   09/29/2011
*******************************************************************************/


#ifndef __IFEEDERTEXTABC_H
#define __IFEEDERTEXTABC_H


#include "IFeederCommon.h"


using namespace std;


namespace Its
{


class IFeederTextABC : public virtual IFeederCommon
{
public:
  IFeederTextABC( ByteIt from_char = 'A', ByteIt to_char = 'Z' );

  virtual ~IFeederTextABC();

  virtual void Sync(){}

  virtual const char*   Name() { return "Text Alphabet"; }
  virtual Interaction_t Type() { return Its::text;       }

  // on low levels, it's just one letter, then may be more
  virtual CapacityIt Capacity() { return CapacityIt( DEFAULT_TEXT_CAPACITY, 0, 0 ); }

  // List of all unique tags
  virtual StringList_t Tags();

  // List of all frame Ids with this tag(s)
  virtual StringList_t Frames( const char* tagLine, LevelIt level );
  
  // How long is the frame?
  //virtual PeriodIt TicksInFrame( const char* frameId ) { return ASCII_HIGH_CHAR - ASCII_LOW_CHAR; } // All valid ASCII chars
  virtual PeriodIt TicksInFrame( const char* frameId ) { return DEFAULT_TICKS_IN_FRAME; } // All valid ASCII chars
 
  virtual void LoadFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );
  virtual void SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );

private:
  StringList_t tagList;
};



}; // namespace Its


#endif // __IFEEDERTEXTABC_H

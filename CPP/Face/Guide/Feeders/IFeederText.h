/*******************************************************************************
 FILE         :   IFeederText.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Text data provider

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   09/29/2011
*******************************************************************************/


#ifndef __IFEEDERTEXT_H
#define __IFEEDERTEXT_H


#include "IFeederFileCommon.h"

using namespace std;


namespace Its
{


//
// Text files feeder
//
class IFeederText : public virtual IFeederFileCommon
{
public:
  IFeederText( const char* sourcePath, LevelIt _feederLevel, const char* _sourceLocale );

  virtual const char*   Name() { return "TextFiles"; }
  virtual Interaction_t Type() { return Its::text; }

  // well, for text data types it's kind of "made up" value, sort of "one word per cycle", 100 seems to be enough
  // for one looooooooooooooooooooong word ;-)
  virtual CapacityIt Capacity() { return CapacityIt( DEFAULT_TEXT_CAPACITY, 0, 0 ); }

protected:
  virtual PeriodIt AddContentFile  ( path& sourceFile,  path& destinationFile );
  virtual void     LoadContentFile ( path& contentFile, PeriodIt tick, IFrameSet& frameSet );

  static const char* sourceExtension;  // *.txt
};



}; // namespace Its


#endif // __IFEEDERTEXT_H

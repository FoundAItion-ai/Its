/*******************************************************************************
 FILE         :   IFeederImage.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Image data provider

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   12/29/2011

 LAST UPDATE  :   12/29/2011
*******************************************************************************/


#ifndef __IFEEDERIMAGE_H
#define __IFEEDERIMAGE_H


#include "IFeederFileCommon.h"

using namespace std;


namespace Its
{


//
// Text files feeder
//
class IFeederImage : public virtual IFeederFileCommon
{
public:
  IFeederImage( const char* sourcePath, LevelIt _feederLevel, const char* imageExtension );

  virtual const char*   Name() { return "ImageFiles"; }
  virtual Interaction_t Type() { return Its::video;   }

  virtual CapacityIt Capacity() { return CapacityIt( 0, 0, DEFAULT_IMAGE_CAPACITY ); }

protected:
  virtual PeriodIt AddContentFile  ( path& sourceFile,  path& destinationFile );
  virtual void     LoadContentFile ( path& contentFile, PeriodIt tick, IFrameSet& frameSet );
};



}; // namespace Its


#endif // __IFEEDERIMAGE_H

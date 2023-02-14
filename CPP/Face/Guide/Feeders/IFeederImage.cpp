/*******************************************************************************
 FILE         :   IFeederImage.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Image data provider

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   12/29/2011

 LAST UPDATE  :   12/29/2011
*******************************************************************************/

#include "stdafx.h"


#include "IFeederImage.h"
#include "CelllayerImage.h"

#include <boost/filesystem/fstream.hpp>


using namespace Its;
using namespace DCommon;

using namespace boost::filesystem;

namespace fs = boost::filesystem;



IFeederImage::IFeederImage( const char* _sourcePath, LevelIt _feederLevel, const char* imageExtension ):
  IFeederFileCommon( Name(), _sourcePath, imageExtension, _feederLevel )
{
  DLog::msg( DLog::LevelDebug, "Loading '%s' feeder", Name() );

}


PeriodIt IFeederImage::AddContentFile( path& sourceFile, path& destinationFile )
{
  DLog::msg( DLog::LevelVerbose, "Adding content file '%s' ", sourceFile.string().c_str() );

  fs::ofstream toFile( destinationFile , std::ios_base::out | ios_base::trunc | ios_base::binary );
  DBitmapImage image( sourceFile.string().c_str(), true, DefaultImageSize );

  long  imageSizeInBytes = image.SaveAs( toFile );
  SizeIt   videoCapacity = Capacity().sizeVideo;
  PeriodIt ticksInImage  = ( imageSizeInBytes / videoCapacity ) + 1;

  toFile.close();

  if( imageSizeInBytes != videoCapacity )
  {
    throw IFeederException( DException::Error, "Image file '%s' conversion failed, size '%d', capacity '%d'", sourceFile.string().c_str(), imageSizeInBytes, videoCapacity );
  }

  return DEFAULT_TICKS_IN_FRAME;
  //return ticksInImage;
}



void IFeederImage::LoadContentFile( path& contentFile, PeriodIt tick, IFrameSet& frameSet )
{
  DLog::msg( DLog::LevelVerbose, "Loading content file '%s' for '%d' tick", contentFile.string().c_str(), tick );

  if( tick % DEFAULT_TICKS_IN_FRAME > DEFAULT_EMPTY_TICKS_IN_FRAME )
  {
    DLog::msg( DLog::LevelVerbose, "No frame in '%d' tick in '%s' feeder", tick, Name() );
    
    memset( frameSet.video(), NOSIGNAL, frameSet.Capacity().sizeVideo );
  }
  else
  {
    DLog::msg( DLog::LevelVerbose, "Loading image frame in '%d' tick in '%s' feeder", tick, Name() );
    
    fs::ifstream fromFile( contentFile, std::ios_base::in | ios_base::binary );

    fromFile.read( (char*) frameSet.video(), Capacity().sizeVideo );
    fromFile.close();
  }
}


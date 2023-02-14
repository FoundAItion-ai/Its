/*******************************************************************************
 FILE         :   IFeederText.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Text data provider

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   09/07/2011
*******************************************************************************/

#include "stdafx.h"


#include "IFeederText.h"

#include <boost/filesystem/fstream.hpp>


using namespace Its;
using namespace DCommon;

using namespace boost::filesystem;

namespace fs = boost::filesystem;


const char* IFeederText::sourceExtension = ".txt";


IFeederText::IFeederText( const char* _sourcePath, LevelIt _feederLevel, const char* _sourceLocale ):
  IFeederFileCommon( Name(), _sourcePath, sourceExtension, _feederLevel )
{
  DLog::msg( DLog::LevelDebug, "Loading '%s' feeder", Name() );

}


PeriodIt IFeederText::AddContentFile( path& sourceFile, path& destinationFile )
{
  DLog::msg( DLog::LevelVerbose, "Adding content file '%s' ", sourceFile.string().c_str() );

  fs::ifstream fromFile ( sourceFile      , std::ios_base::in );
  fs::ofstream toFile   ( destinationFile , std::ios_base::out | ios_base::trunc | ios_base::binary );
  
  string buffer; // line buffer
  std::ofstream::pos_type lastPosition;

  if( !fromFile || fromFile.bad() ) 
  { 
    throw IFeederException( DException::Error, "Can not open file '%s' from feeder '%s'", sourceFile.string().c_str(), Name() );
  }

  // Read as much as we can.
  // Should we check content for ascii?
  //
  while( !fromFile.eof() && !fromFile.bad() )
  {
    buffer = "";

    fromFile >> buffer;

    if( buffer.length() > 0 )
    {
      toFile << buffer << (char) NOSIGNAL;
      lastPosition = toFile.tellp();
    }
  }

  fromFile.close();
  toFile.close();
  
  SizeIt   textCapacity = Capacity().sizeText;
  PeriodIt ticksInText  = ( lastPosition / textCapacity ) + 1;

  return ticksInText;
}



void IFeederText::LoadContentFile( path& contentFile, PeriodIt tick, IFrameSet& frameSet )
{
  DLog::msg( DLog::LevelVerbose, "Loading content file '%s' for '%d' tick", contentFile.string().c_str(), tick );

  fs::ifstream fromFile( contentFile, std::ios_base::in | ios_base::binary );

  SizeIt textCapacity = Capacity().sizeText;

  fromFile.seekg( textCapacity * tick );
  fromFile.read( (char*) frameSet.text(), textCapacity );
  fromFile.close();
}


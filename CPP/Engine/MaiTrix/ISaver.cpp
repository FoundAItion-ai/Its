/*******************************************************************************
 FILE         :   ISaver.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Layers persistency classes

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/22/2011

 LAST UPDATE  :   08/22/2011
*******************************************************************************/


#include "stdafx.h"

#include <io.h>
#include <stdlib.h>

#include "ISaver.h"


using namespace Its;


// current version
const int FileHeaderMajorVersion = 1;
const int FileHeaderMinorVersion = 0;


struct ISaverFileHeader
{
  int       majorVersion;
  int       minorVersion;
          
  char      maiTrixName[ MAX_PATH ];
  SizeIt    layerSizeX;
  SizeIt    layerSizeY;        
  SizeIt    positionZ;
  PeriodIt  cycle; 
  Layer_t   type;

  unsigned long bodySize; 
};





ISaverFile::ISaverFile( const char* _folderPath, bool _autoCorrection ):
  folderPath( _folderPath ),
  autoCorrection( _autoCorrection )
{
  if( folderPath.empty() )
  {
    folderPath = ".\\";
  }

  if( folderPath[ folderPath.length() - 1 ] != '\\' )
  {
    folderPath += "\\";
  }
}



ISaverFile::~ISaverFile()
{
  try
  {
    if( inFile.is_open() )
    {
      inFile.close();
    }

    if( outFile.is_open() )
    {
      outFile.close();
    }
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "Unspecified file closing error. Last system error [ %s ]", DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }
}



// check if layer with that name and positionZ exists,
// return maiTrixSize and cycle
bool ISaverFile::Exist( Celllayer& layer )
{
  string      layerFileName = FileNameWithPath( layer );
  _finddata_t fileInfo;
  intptr_t    searchHandle;
  bool        fileExist;
  
  searchHandle = _findfirst( layerFileName.c_str(), &fileInfo );
  fileExist    = searchHandle != - 1;
  
  _findclose( searchHandle );

  return fileExist;
}



// load cells from layer with that name and positionZ
// verify maiTrixSize and cycle
void ISaverFile::Load( Celllayer& layer )
{
  DoLoad( layer, false );
}


void ISaverFile::Header( Celllayer& layer )
{
  DoLoad( layer, true );
}


void ISaverFile::DoLoad( Celllayer& layer, bool headerOnly )
{
  string layerFileName   = FileNameWithPath( layer );
  bool operationComplete = false;

  DLog::msg( DLog::LevelVerbose, "Loading %s of the layer '%s' in MaiTrix '%s' at position '%d'", 
    headerOnly ? "header" : "body  ",  layerFileName.c_str(), layer.maiTrixName.c_str(), layer.positionZ );

  try
  {
    inFile.open( layerFileName.c_str(), ios_base::binary | ios_base::in, _SH_SECURE );

    // Next versions should include this header
    ISaverFileHeader header;

    memset( &header, 0, sizeof( ISaverFileHeader ));
    inFile.read( ( char* )&header, sizeof( header ));

    if( !inFile.good())
    {
      throw ISaverException( DException::Error, "General header file reading error" );
    }

    // Version check
    if( header.majorVersion <= 0 ||
        header.majorVersion > FileHeaderMajorVersion )
    {
      throw ISaverException( DException::Error, "Inconsistent maiTrix version '%d' / '%d'", header.majorVersion, FileHeaderMajorVersion );
    }

    SizeIt2D layerSize( header.layerSizeX, header.layerSizeY );
            
    if( headerOnly )
    {
      layer.size  = layerSize;
      layer.cycle = header.cycle;
      layer.type  = header.type;
    }
    else
    {
      // Data consistency check
      if( layer.size != layerSize )
      {
        throw ISaverException( DException::Error, "Inconsistent layer size '%d' / '%d'", layer.size, layerSize );
      }
      
      if( layer.maiTrixName.compare( header.maiTrixName ) != 0 )
      {
        throw ISaverException( DException::Error, "Inconsistent maiTrix name '%s' / '%s'", layer.maiTrixName.c_str(), header.maiTrixName );
      }

      if( layer.positionZ != header.positionZ )
      {
        if( !autoCorrection )
        {
          throw ISaverException( DException::Error, "Inconsistent layer position '%d' / '%d'", layer.positionZ, header.positionZ );
        }
        else
        {
          DLog::msg( DLog::LevelWarning, "Layer in cycle '%d' at position '%d' was corrected to position '%d' in MaiTrix '%s'", header.cycle, header.positionZ, layer.positionZ, layer.maiTrixName.c_str());
        }
      }

      if( layer.cycle != header.cycle )
      {
        if( !autoCorrection )
        {
          throw ISaverException( DException::Error, "Inconsistent maiTrix cycle '%d' / '%d'", layer.cycle, header.cycle );
        }
        {
          DLog::msg( DLog::LevelWarning, "Layer at position '%d' in cycle '%d' was corrected to cycle '%d' in MaiTrix '%s'", layer.positionZ, header.cycle, layer.cycle, layer.maiTrixName.c_str());
        }
      }

      //
      // Do not check type - load it!
      //
      layer.type = header.type;

      SizeIt expectedBodySize = BasicCube::CubeMemorySize( layerSize.X, layerSize.Y ); 

      if( expectedBodySize != header.bodySize )
      {
        throw ISaverException( DException::Error, "Inconsistent layer body '%d' / '%d'", expectedBodySize, header.bodySize );
      }

      if( !layer.Empty() && header.bodySize > 0 )
      {
        inFile.read( ( char* )( Cell* )layer, header.bodySize );

        if( !inFile.good())
        {          
          throw ISaverException( DException::Error, "General body file reading error at '%d' position in '%d' cycle, eof flag '%d', state '%d'", 
            layer.positionZ, header.cycle, inFile.eof(), inFile.rdstate());
        }
      }
    }

    operationComplete = true;
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelError, "Loading error: '%s' in file '%s'. Last system error [ %s ]", 
               x.what(), layerFileName.c_str(), DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "Unspecified loading error in file '%s'. Last system error [ %s ]", 
               layerFileName.c_str(), DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }
  
  inFile.close();

  if( !operationComplete )
  {
    throw ISaverException( DException::Error, "File loading error in MaiTrix '%s', layer '%s'", layer.maiTrixName.c_str(), layerFileName.c_str() );
  }
}



// save cells to layer with that name and positionZ (overwrite)
// save maiTrixSize and cycle
void ISaverFile::Save( Celllayer& layer )
{
  string layerFileName   = FileNameWithPath( layer );
  bool operationComplete = false;

  DLog::msg( DLog::LevelVerbose, "Saving layer '%s' in in MaiTrix '%s'", layerFileName.c_str(), layer.maiTrixName.c_str() );

  try
  {
    outFile.open( layerFileName.c_str(), ios_base::binary | ios_base::out | ios_base::trunc, _SH_SECURE );

    ISaverFileHeader header;

    memset( &header, 0, sizeof( ISaverFileHeader ));

    header.majorVersion = FileHeaderMajorVersion;
    header.minorVersion = FileHeaderMinorVersion;
    header.layerSizeX   = layer.size.X;
    header.layerSizeY   = layer.size.Y;
    header.positionZ    = layer.positionZ;
    header.cycle        = layer.cycle;
    header.type         = layer.type;
    header.bodySize     = BasicCube::CubeMemorySize( header.layerSizeX, header.layerSizeY ); 

    strncpy_s( header.maiTrixName, MAX_PATH, layer.maiTrixName.c_str(), MAX_PATH );
    header.maiTrixName[ sizeof( header.maiTrixName ) - 1 ] = 0;

    outFile.write( ( const char* )&header, sizeof( header ));

    if( !layer.Empty())
    {
      outFile.write( ( const char* )( Cell* )layer, header.bodySize );
    }

    if( !outFile.good())
    {
      throw ISaverException( DException::Error, "General file writing error" );
    }

    operationComplete = true;
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelError, "Saving error: '%s' in file '%s'. Last system error [ %s ]", 
               x.what(), layerFileName.c_str(), DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "Unspecified saving error in file '%s'. Last system error [ %s ]", 
               layerFileName.c_str(), DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }

  outFile.close();

  if( !operationComplete )
  {
    throw ISaverException( DException::Error, "File saving error in MaiTrix '%s', layer '%s'", layer.maiTrixName.c_str(), layerFileName.c_str() );
  }
}


void ISaverFile::Void( Celllayer& layer )
{
  string layerFileName = FileNameWithPath( layer );

  if( _unlink( layerFileName.c_str() ) != 0 )
  {
    throw ISaverException( DException::Error, "File erasing error in MaiTrix '%s'", layer.maiTrixName.c_str() );
  }  
}

 
string ISaverFile::FileNameWithPath( Celllayer& layer )
{
  return layer.FileNameWithPath( folderPath.c_str(), ".dma" );
}


void ISaverFile::Flush()
{
  if( inFile.is_open() )
  {
    int result = inFile.sync();

    DLog::msg( DLog::LevelVerbose, "Flushing file complete with '%d' result", result );
  }
}


//
// ISaverFileCached implementation
//
// As one saver object binded to one maiTrix we don't map maiTrix name and just create one 'thin' cache
// per cycle
//
//
ISaverFileCached::ISaverFileCached( const char* _folderPath, PeriodIt _cacheRatio, bool _autoCorrection ):
  Base( _folderPath, _autoCorrection ),
  cacheRatio( _cacheRatio )
{
  ResetCache();
}


/*

8 Feb 2012 - DMA - Do we need caching statistics? So far it's like:

totalSaveCalls = '230', realSaveCalls = '46', totalLoadCalls = '230', realLoadCalls = '23'

but not helping with performance much, ~5-7%.

int totalSaveCalls = 0;
int totalLoadCalls = 0;
int realSaveCalls  = 0;
int realLoadCalls  = 0;
*/

ISaverFileCached::~ISaverFileCached()
{
  try
  {
    FlushCache();
    ResetCache();
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "Unspecified cache clearing error. Last system error [ %s ]", DException::TranslateSystemMessage( GetLastError() ).c_str() );
  }
}


void ISaverFileCached::Load( Celllayer& layer )
{
  Cache_t::iterator cachedItem = cache.find( layer.positionZ );

  if( cachedItem != cache.end() )
  {
    Celllayer& cachedLayer = *cachedItem->second;

    // Cache is attached to the same maiTrix all the time
    if( layer.maiTrixName != cachedLayer.maiTrixName || layer.size != cachedLayer.size )
    {
      throw ISaverException( DException::Error, "Cache reading error, cached item does't match requested in name '%s' and/or size '%d x %d'", cachedLayer.maiTrixName.c_str(), cachedLayer.size.X, cachedLayer.size.Y );
    }

    if( layer.cycle == cachedLayer.cycle )
    {
      layer.type = cachedLayer.type; // Type always stays the same 
      
      memcpy( ( Cell* )layer, ( Cell* )cachedLayer, layer.size.X * layer.size.Y * sizeof( Cell ));
    }
    else
    {
      Base::Load( layer );
    }
  }
  else
  {
    Base::Load( layer );
  }
}


void ISaverFileCached::Save( Celllayer& layer )
{
  Cache_t::iterator cachedItem = cache.find( layer.positionZ );
  Celllayer*        cachedLayer;
  bool              saveCachedItem;

  if( cachedItem != cache.end() )
  {
    cachedLayer    = cachedItem->second;
    saveCachedItem = layer.cycle % cacheRatio == 0;
    
    // Cache is attached to the same maiTrix all the time
    if( layer.maiTrixName != cachedLayer->maiTrixName || layer.size != cachedLayer->size )
    {
      throw ISaverException( DException::Error, "Cache writing error, cached item does't match requested in name '%s' and/or size '%d x %d'", cachedLayer->maiTrixName.c_str(), cachedLayer->size.X, cachedLayer->size.Y );
    }
  }
  else
  {
    // Populate cache
    cachedLayer       = new Celllayer( layer.maiTrixName.c_str(), layer.size, layer.positionZ, layer.cycle );
    saveCachedItem    = true;
    cachedLayer->type = layer.type; // Type always stays the same 

    cache.insert( Cache_t::value_type( layer.positionZ, cachedLayer ));
  }

  cachedLayer->cycle = layer.cycle;

  memcpy( ( Cell* )( *cachedLayer ), ( Cell* )layer, layer.size.X * layer.size.Y * sizeof( Cell ));

  // Save to file periodically, assuming that cycle is changing
  if( saveCachedItem )
  {
    Base::Save( layer );
  }
}


void ISaverFileCached::Void( Celllayer& layer )
{
  Base::Void( layer );
  ResetCache();
}


void ISaverFileCached::FlushCache()
{
  DLog::msg( DLog::LevelDebug, "Flushing file cache" );

  for( Cache_t::iterator cachedItem = cache.begin(); cachedItem != cache.end(); cachedItem++ )
  {
    Celllayer& cachedLayer = *cachedItem->second;

    Base::Save( cachedLayer );
  }

  DLog::msg( DLog::LevelVerbose, "Flushing file cache of '%d' layers complete", cache.size() );
}


void ISaverFileCached::ResetCache()
{
  DLog::msg( DLog::LevelDebug, "Clearing file cache" );

  for( Cache_t::iterator cachedItem = cache.begin(); cachedItem != cache.end(); cachedItem++ )
  {
    delete cachedItem->second;
  }

  DLog::msg( DLog::LevelVerbose, "Clearing file cache of '%d' layers complete", cache.size() );

  cache.clear();  
}


void ISaverFileCached::Flush()
{
  FlushCache();
}


//
// ISaverInMemory implementation
//
ISaverInMemory::ISaverInMemory( bool _autoCorrection ):
  autoCorrection(_autoCorrection)
{
}


ISaverInMemory::~ISaverInMemory()
{
}


bool ISaverInMemory::Exist( Celllayer& layer )
{
  return storage.find( ComposeKey( layer )) != storage.end();
}


void ISaverInMemory::Header( Celllayer& layer )
{
  return Load( layer );
}


void ISaverInMemory::Load( Celllayer& layer )
{
  if( Exist( layer ))
  {
    SizeIt   positionZ = layer.positionZ;
    PeriodIt cycle     = layer.cycle;

    layer = storage[ ComposeKey( layer )];

    if( autoCorrection )
    {
      layer.positionZ = positionZ;
      layer.cycle     = cycle;
    }
  }
}


void ISaverInMemory::Save( Celllayer& layer )
{
  storage[ ComposeKey( layer )] = layer;
}


void ISaverInMemory::Void( Celllayer& layer )
{
  if( storage.erase( ComposeKey( layer )) != 1 )
  {
    throw ISaverException( DException::Error, "Error removing layer at position '%d' in maiTrix '%s'", layer.positionZ, layer.maiTrixName.c_str() );
  }
}


void ISaverInMemory::Flush()
{
}


string ISaverInMemory::ComposeKey( Celllayer& layer )
{
  return layer.FileNameWithPath( "", "" );
}

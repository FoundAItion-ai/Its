/*******************************************************************************
 FILE         :   IFeederFileCommon.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Common functions for all file-based feeders 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   12/19/2011

 LAST UPDATE  :   12/19/2011
*******************************************************************************/

#include "stdafx.h"

#include <stdlib.h>
#include <stdio.h>

#include "IFeederFileCommon.h"

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>

#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/system/error_code.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/date_time/time_facet.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/local_time/local_time_io.hpp>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/traits.hpp>
#include <boost/iostreams/device/file.hpp>


using namespace Its;
using namespace DCommon;

using namespace boost::xpressive;
using namespace boost::filesystem;
using namespace boost::iostreams;
using namespace boost::range;
using namespace boost::local_time;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::algorithm;
using namespace boost::system;
using namespace boost::lambda;

using boost::format;
using boost::io::group;

namespace fs = boost::filesystem;
namespace io = boost::iostreams;


/*

  Basic considerations:

  1) Source filename - tag line with '_' as dividers, like Sun_dogs_funny_story.txt;

  2) Index file contains mapping from source file name -> content file uid prefix (frameId), also tags for the source and 
     last modification date to track changes. Name - feeder name '_' level '_' sourcePath stem, 
     like "Text_Files_02_RusBasic.fdx";

     Index file format - formatted text or xml, like:

     source file name : frameId : ticks (length) : tag1, tag2, tag3, ... : source update datetime, ISO UTC

     Sun_dogs_funny_story.txt : 000250 : 123 : Sun, dogs, funny, story : 20111111T111111


  3) Content file name - index name '_' uid prefix '_' and seq. number (or sec?), 
     like Text_Files_02_RusBasic_000250_0033.fbc;

  4) To avoid race conditions - do not run two feeders against the same folder;
                            and do not use the same feeder with two maiTrixes;

  5) Tags are taken from source file names and limited to 1-5 tags per file (otherwise formal subject line should be introduced
     in the source files);

  6) Content file format - binary, same for all types, header and body,
     text (unicode chars), audio (??) and video (8 bit uncompressed images, fixed size 640 x 480?);

     All source tex files are unencrypted, plain text in a given locale and being converted into unicode text;


  Basic logic:

  1) Load index file (if no index -> clear content folder) and parse;

  2) Iterate over source files recursively and check for modifications needed - add, update or delete; 
     Initially don't do delete / update ("source update datetime" attribute), just add content if there's no one yet.

  3) Each source file is being split into frame set batches - 24 frame sets per file;

  [ either we use persistent index file or in-memory unique hash function "file name" -> "frameId" ]


  4) LoadFrame() to cache two frames as normally there two to compare (base / main);

  5) SaveFrame() is for debugging purpose only for these feeders type;

*/


const char* IFeederFileCommon::indexExtension   = ".fdx";
const char* IFeederFileCommon::contentExtension = ".fbc";
const char* IFeederFileCommon::nameDelimeter    = "_";
const char* IFeederFileCommon::tagListSeparator = "*";

/*

Boost.locale:

•The first and most important recommendation: prefer UTF-8 encoding for narrow strings --- it represents all supported 
Unicode characters and is more convenient for general use than encodings like Latin1.

•Remember, there are many different cultures. You can assume very little about the user's language. His calendar may not have 
"January". It may be not possible to convert strings to integers using atoi because they may not use the "ordinary" digits 0..9 
at all. You can't assume that "space" characters are frequent because in Chinese the space character does not separate words. 
The text may be written from Right-to-Left or from Up-to-Down, and so on.

•Using message formatting, try to provide as much context information as you can. Prefer translating entire sentences over 
single words. When translating words, always add some context information.
 

*/

IFeederFileCommon::IFeederFileCommon( const char* _feederName, const char* _sourcePath, const char* _sourceExt, LevelIt _feederLevel ):
  feederName ( _feederName  ),
  feederLevel( _feederLevel ),
  sourcePath ( _sourcePath  ),
  sourceExt  ( _sourceExt   ),
  maxFrameId ( 0 )
{
  DLog::msg( DLog::LevelDebug, "Loading '%s' feeder", feederName.c_str() );

  path parent( sourcePath.parent_path().parent_path() );
  bool sourceCreated, indexCreated, contentCreated;

  /*
  The Effects and Postconditions of functions described in this reference may not be achieved in the presence of race conditions. 
  No diagnostic is required. (c) boost

  */

  sourceCreated = create_directories( sourcePath );

  if( !is_directory( sourcePath ) || !is_directory( parent ))
  {
    throw IFeederException( DException::Error, "Source and parent path '%s' in feeder '%s' should be a directory", _sourcePath, feederName.c_str() );
  }
 
  contentPath     = parent / "content";
  indexPath       = parent / "index";

  indexCreated    = create_directories( indexPath   );
  contentCreated  = create_directories( contentPath );

  if( sourceCreated || indexCreated || contentCreated )
  {
    DLog::msg( DLog::LevelWarning, "Created folder(s) for '%s' feeder, sourceCreated = '%d', indexCreated = '%d', contentCreated = '%d'", feederName.c_str(), sourceCreated, indexCreated, contentCreated );
  }
  else
  {
    DLog::msg( DLog::LevelDebug, "Use existing folder(s) for '%s' feeder", feederName.c_str() );
  }

  DLog::msg( DLog::LevelInfo, "Feeder '%s' initialized with source '%s', index '%s' and content '%s'", feederName.c_str(), sourcePath.string().c_str(), indexPath.string().c_str(), contentPath.string().c_str() );
}



IFeederFileCommon::~IFeederFileCommon()
{
  CleanUp();
}


void IFeederFileCommon::Sync()
{
  DLog::msg( DLog::LevelDebug, "Feeder '%s' is being synchronized with source '%s', index '%s' and content '%s'", feederName.c_str(), sourcePath.string().c_str(), indexPath.string().c_str(), contentPath.string().c_str() );

  indexFileName = string( feederName.c_str() ) + nameDelimeter + 
                  boost::lexical_cast< string >( feederLevel ) + nameDelimeter + 
                  sourcePath.stem().string();
                         
  path indexFilePath = indexPath / ( indexFileName + indexExtension );

  try
  {
    // need to change IndexFileList_t to list of auto_ptr
    // or manually clear the list
    //  
    if( !LoadIndexFile( indexFilePath ))
    {
      ClearContentFolder( contentPath );
    }

    UpdateAndSaveIndex( indexFilePath );
  }
  catch( exception& x ) // for other exceptions, like ... don't even bother to clean up
  {
    DLog::msg( DLog::LevelError, "Feeder '%s' synchronization failed with error '%s'", feederName.c_str(), x.what() );
    CleanUp();
    throw;
  }
  
  DLog::msg( DLog::LevelInfo, "Feeder '%s' is synchronized with source '%s', index '%s' and content '%s'", feederName.c_str(), sourcePath.string().c_str(), indexPath.string().c_str(), contentPath.string().c_str() );
}



void IFeederFileCommon::CleanUp()
{
  DLog::msg( DLog::LevelDebug, "Cleaning '%s' feeder", feederName.c_str() );

  for( IndexFileList_t::const_iterator loopEntry = indexFileList.begin(); loopEntry != indexFileList.end(); loopEntry++ )
  {
    IndexFileEntry* indexEntry = *loopEntry;
    
    delete indexEntry;
  }

  indexFileList.clear();
}


bool IFeederFileCommon::LoadIndexFile( path indexFilePath )
{
  DLog::msg( DLog::LevelDebug, "Loading index file '%s' ", indexFilePath.string().c_str() );

  io::filtering_istreambuf  indexStreamBuf;
  std::istream              indexStream( &indexStreamBuf );
  string                    indexEntry;
  bool                      loadResult = true;

  //  
  // TODO:
  //
  /*
    Objects of type basic_streambuf are created with an internal buffer of type char * regardless of the char_type 
    specified by the type parameter Elem. This means that a Unicode string (containing wchar_t characters) will be 
    converted to an ANSI string (containing char characters) before it is written to the internal buffer. To store 
    Unicode strings in the buffer, create a new buffer of type wchar_t and set it using the basic_streambuf::pubsetbuf() 
    method. To see an example that demonstrates this behavior, see basic_filebuf Class.

  */

  indexStreamBuf.push( file_source( indexFilePath.string()));
  indexFileList.clear();

  try{
    do
    {
      getline( indexStream, indexEntry );
    
      if( !indexEntry.empty())
      {
        IndexFileEntry* fileEntry = new IndexFileEntry();

        ParseIndexEntry( indexEntry, *fileEntry );

        indexFileList.push_back( fileEntry );
      }
    } while( !indexStream.eof() );

    loadResult = indexFileList.size() > 0;
    DLog::msg( DLog::LevelDebug, "Index file is loaded from '%s', found '%d' entries, max Frame Id is '%d'.", indexFilePath.string().c_str(), indexFileList.size(), maxFrameId );
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelWarning, "Error '%s' loading index file from '%s', the content will be reloaded.", x.what(), indexFilePath.string().c_str());
    
    indexFileList.clear();
    loadResult = false;
  }

  return loadResult;
}


void IFeederFileCommon::ParseIndexEntry( string indexEntryString, IndexFileEntry& indexEntry )
{
  DLog::msg( DLog::LevelVerbose, "Parsing index entry: '%s' ", indexEntryString.c_str() );

  // source_file_name : frameId : ticks (length) : tag1, tag2, tag3, ... : source update datetime, ISO UTC
  // Sun_dogs_funny_story.txt : 000250 : 123 : Sun, dogs, funny, story : 20111111T111111;
  //
  sregex indexRegex = sregex::compile( "^([^:]+) : ([^:]+) : ([^:]+) : ([^:]+) : ([^:]+);$" );
  smatch what;

  if( regex_match( indexEntryString, what, indexRegex ))
  {
    if( what.size() != 6 )
    {
      throw IFeederException( DException::Error, "String '%s' has less then needed tokens as index entry", indexEntryString.c_str() );
    }

    indexEntry.fileName    = what[ 1 ];
    indexEntry.frameId     = boost::lexical_cast< SizeIt   > ( what[ 2 ] );
    indexEntry.length      = boost::lexical_cast< PeriodIt > ( what[ 3 ] );
    indexEntry.tagLine     = what[ 4 ];
    indexEntry.updateStamp = from_iso_string( what[ 5 ] );
    indexEntry.operation   = ContentDelete;

    maxFrameId = max( maxFrameId, indexEntry.frameId );
  }
  else
  {
    throw IFeederException( DException::Error, "String '%s' is not recognized as index entry", indexEntryString.c_str() );
  }

  // add Critical Section for consistency?
  DLog::msg( DLog::LevelVerbose, "File Name   : '%s' "  , indexEntry.fileName.c_str() );
  DLog::msg( DLog::LevelVerbose, "Frame Id    : '%d' "  , indexEntry.frameId );
  DLog::msg( DLog::LevelVerbose, "Length      : '%d' "  , indexEntry.length  );
  DLog::msg( DLog::LevelVerbose, "Tag Line    : '%s' "  , indexEntry.tagLine.c_str() );
  DLog::msg( DLog::LevelVerbose, "Update Date : '%s' "  , to_iso_string( indexEntry.updateStamp.date()).c_str() );
  DLog::msg( DLog::LevelVerbose, "Update Time : '%s' "  , to_iso_string( indexEntry.updateStamp.time_of_day()).c_str() );
}



//
// Just tired of playing with boost and templates for 5 hours, attempting to use iequals() in lambda directly ;-(
// So created this function instead.
//
bool IFeederFileCommon::IndexFileEntry::operator == ( const string& fileName ) 
{ 
  return iequals( this->fileName, fileName ); 
}


bool IFeederFileCommon::IndexFileEntry::operator == ( const time_t& updateStamp )
{
  ptime updateStampPosix = from_time_t( updateStamp );

  return this->updateStamp == updateStampPosix;
}


void IFeederFileCommon::UpdateAndSaveIndex( path indexFilePath )
{
  DLog::msg( DLog::LevelDebug, "Updating index '%s' from source '%s'", indexFilePath.string().c_str(), sourcePath.string().c_str() );
    
  IndexFileEntry* fileEntry;

  // 1) Let's iterate over files in source folder (non-recursively) and compare to loaded index entries
  //
  for( directory_iterator loopDirectory = directory_iterator( sourcePath ); loopDirectory != directory_iterator(); loopDirectory++ )
  {
    directory_entry& directoryEntry = *loopDirectory;

    if( !is_regular_file( directoryEntry.status() ))
    {
      continue;
    }

    path   filePath    = directoryEntry.path(); // not a directory!
    string fileName    = filePath.filename().string();
    string fileExt     = filePath.extension().string();
    ptime  updateStamp = from_time_t( last_write_time( filePath ));

    // Skip unknown files
    if( !iequals( fileExt, sourceExt ))
    {
      continue;
    }    

    DLog::msg( DLog::LevelVerbose, "Processing source file '%s'", filePath.string().c_str() );

    IndexFileList_t::const_iterator fileByName = find_if( indexFileList.begin(), 
                                                          indexFileList.end(), 
                                                          *_1 == fileName );
                                                        // _1 ->* &IndexFileEntry::fileName == fileName );
    
    if( fileByName != indexFileList.end() )
    {
      fileEntry = *fileByName;

      DLog::msg( DLog::LevelVerbose, "Found index entry for '%s', entry time '%s', file time '%s'", 
                 fileEntry->fileName.c_str(), to_iso_string( fileEntry->updateStamp ).c_str(), to_iso_string( updateStamp ).c_str() );

      //
      // When updateStamp > fileEntry.updateStamp do update the entry too, what is a little suspicious but what else can we do?
      //
      fileEntry->operation   = fileEntry->updateStamp == updateStamp ? ContentKeep : ContentUpdate;
      fileEntry->updateStamp = updateStamp;
    }
    else
    {
      DLog::msg( DLog::LevelVerbose, "Not found matching entry for '%s', adding...", filePath.string().c_str() );

      fileEntry = new IndexFileEntry();

      fileEntry->fileName    =   fileName;
      fileEntry->operation   =   ContentAdd;
      fileEntry->updateStamp =   updateStamp;
      fileEntry->frameId     = ++maxFrameId;

      indexFileList.push_back( fileEntry );
    }
  }

  // 2) Update content folder
  int filesAdded   = 0;
  int filesUpdated = 0;
  int filesDeleted = 0;

  UpdateContentFolder( filesAdded, filesUpdated, filesDeleted );

  // 3) Save updated index file
  if( filesAdded != 0 || filesUpdated != 0 || filesDeleted != 0 )
  {
    DLog::msg( DLog::LevelDebug, "Updating index complete, %d files added, %d files updated, %d files deleted", filesAdded, filesUpdated, filesDeleted );

    SaveIndexFile( indexFilePath );
  }
  else
  {
    DLog::msg( DLog::LevelDebug, "Updating index complete, no changes found." );
  }
}



path IFeederFileCommon::GetContentFilePath( IndexFileEntry& fileEntry )
{
  // Content file name - index name '_' uid prefix '_' and seq. number (or sec?), 
  // like Text_Files_02_RusBasic_000250_0033.fbc;
  //
  // NB: no seq number for now, keep in one file 
  //
  format contentFileNameFormat( "%s_%d%s" );
  string contentFileName;

  contentFileName       = str( contentFileNameFormat % indexFileName % fileEntry.frameId % contentExtension );
  path contentFilePath  = contentPath / contentFileName;

  return contentFilePath;
}

   
 
void IFeederFileCommon::UpdateContentFolder( int& filesAdded, int& filesUpdated, int& filesDeleted )
{
  for( IndexFileList_t::const_iterator loopEntry = indexFileList.begin(); loopEntry != indexFileList.end(); loopEntry++ )
  {
    IndexFileEntry& fileEntry = **loopEntry;

    path destinationFile = GetContentFilePath( fileEntry );
    bool fileExists      = exists( destinationFile ); // check for time stamp too?

    switch( fileEntry.operation )
    {
      case ContentKeep    : if( fileExists ) 
                            {
                              break;
                            }
                            else
                            {
                              DLog::msg( DLog::LevelWarning, "File '%s' was deleted from content folder by external process, recreating.", destinationFile.string().c_str() );
                            }
      case ContentAdd     : AddContentFile    ( fileEntry );
                            filesAdded++;
                            break;
      case ContentUpdate  : DeleteContentFile ( fileEntry ); 
                            AddContentFile    ( fileEntry );
                            filesUpdated++;
                            break;
      case ContentDelete  : DeleteContentFile ( fileEntry ); 
                            filesDeleted++;
                            // remove from indexFileList or just ignore this status later?
                            break;
    }    
  }
}


void IFeederFileCommon::SaveIndexFile( path indexFilePath )
{
  DLog::msg( DLog::LevelDebug, "Saving index '%s' ", indexFilePath.string().c_str() );

  // Same string as parsed:
  //
  // source_file_name : frameId : ticks (length) : tag1, tag2, tag3, ... : source update datetime, ISO UTC
  // Sun_dogs_funny_story.txt : 000250 : 123 : Sun, dogs, funny, story : 20111111T111111;
  //
  fs::ofstream to_file( indexFilePath, std::ios_base::out | std::ios_base::trunc );

  for( IndexFileList_t::const_iterator loopEntry = indexFileList.begin(); loopEntry != indexFileList.end(); loopEntry++ )
  {
    IndexFileEntry& fileEntry = **loopEntry;
    
    if( fileEntry.operation != ContentDelete )
    {
      format indexFormat( "%s : %d : %d : %s : %s ;\n" );
      string timeString = to_iso_string( fileEntry.updateStamp );
      string indexString;

      indexString = str( indexFormat % 
                         fileEntry.fileName % 
                         fileEntry.frameId % 
                         fileEntry.length % 
                         fileEntry.tagLine % 
                         timeString ); 

      to_file << indexString << eol;
    }
  }

  to_file.close();
}



void IFeederFileCommon::AddContentFile( IndexFileEntry& fileEntry )
{
  path sourceFile      = sourcePath  / fileEntry.fileName;
  path destinationFile = GetContentFilePath( fileEntry );

  fileEntry.tagLine    = BuildTagLine( fileEntry );
  fileEntry.length     = AddContentFile( sourceFile, destinationFile );
}


void IFeederFileCommon::DeleteContentFile( IndexFileEntry& fileEntry )
{
  path fileToDelete = GetContentFilePath( fileEntry );

  DLog::msg( DLog::LevelVerbose, "Removing content file '%s' ", fileToDelete.string().c_str() );
  
  remove( fileToDelete ); // no exceptions if not existed
}


void IFeederFileCommon::ClearContentFolder( path contentPath )
{
  DLog::msg( DLog::LevelDebug, "Clearing content folder '%s', will be rebuilding index...", contentPath.string().c_str() );

  // Stop if can not remove file (should not happen, see race conditions above)
  //
  // Use recursive iterator or remove_all() ?
  //
  for( directory_iterator loopDirectory = directory_iterator( contentPath ); loopDirectory != directory_iterator(); loopDirectory++ )
  {
    directory_entry& directoryEntry = *loopDirectory;

    if( is_regular_file( directoryEntry.status() ))
    {
      DLog::msg( DLog::LevelVerbose, "Removing file '%s'", directoryEntry.path().string().c_str() );

      boost::system::error_code code;

      if( !remove( directoryEntry.path(), code ))
      {
        throw IFeederException( DException::Error, "Can not remove file '%s'", directoryEntry.path().string().c_str() );
      }
    }
  }
}


StringList_t IFeederFileCommon::Tags()
{
  DLog::msg( DLog::LevelDebug, "Building taglist for feeder '%s'... ", feederName.c_str() );

  StringList_t tagList;
  
  for( IndexFileList_t::const_iterator loopEntry = indexFileList.begin(); loopEntry != indexFileList.end(); loopEntry++ )
  {
    IndexFileEntry& fileEntry = **loopEntry;
    StringList_t    entryTags = ParseTagLine( fileEntry.tagLine );
    
    tagList.merge( entryTags );
  }
    
  tagList.sort   ( Its::StringOrder );
  tagList.unique ( Its::StringEqual );

  DLog::msg( DLog::LevelDebug, "Taglist compiled for '%s' feeder, '%d' unique tags found.", feederName.c_str(), tagList.size() );
  
  return tagList;
}



// List of all unique frame Ids with this tag(s)
StringList_t IFeederFileCommon::Frames( const char* _tagLine, LevelIt level )
{
  StringList_t frameIds;

  //if( level != feederLevel )
  //
  // if request simple things and we have only complex 
  //
  if( level < feederLevel )
  {
    DLog::msg( DLog::LevelDebug, "Feeder '%s' can not provide any frames for the level '%d'", feederName.c_str(), level );
    return frameIds;
  }

  StringList_t tagsInLine = ParseTagLine( _tagLine );

  for( IndexFileList_t::const_iterator loopEntry = indexFileList.begin(); loopEntry != indexFileList.end(); loopEntry++ )
  {
    IndexFileEntry& fileEntry = **loopEntry;
    StringList_t    tagList   = ParseTagLine( fileEntry.tagLine );
    bool      containsAllTags = true;

    // All tags requested should be present in the tag list of the fileEntry
    for( StringList_t::const_iterator loopTag = tagsInLine.begin(); loopTag != tagsInLine.end(); loopTag++ )
    {
      string tag = *loopTag;

      if( find( tagList.begin(), tagList.end(), tag ) == tagList.end() )
      {
        containsAllTags = false;
        break;
      }
    }

    if( containsAllTags )
    {
      string frameId = boost::lexical_cast< string >( fileEntry.frameId );

      DLog::msg( DLog::LevelVerbose, "Adding frame Id '%s' of '%s' feeder for '%s' tagline.", frameId.c_str(), feederName.c_str(), _tagLine );
      
      frameIds.push_back( frameId );
    }
  }

  frameIds.sort   ( Its::StringOrder );
  frameIds.unique ( Its::StringEqual );

  DLog::msg( DLog::LevelDebug, "Feeder '%s' for '%s' tagline provides '%d' frames.", feederName.c_str(), _tagLine, frameIds.size() );

  return frameIds;
}


// How long is the frame?
PeriodIt IFeederFileCommon::TicksInFrame( const char* frameId )
{
  IndexFileEntry* indexEntry   = FindIndexByFrameId( frameId );
  PeriodIt        ticksInFrame = 0;
  
  if( indexEntry != 0 )
  {
    ticksInFrame = indexEntry->length;
    
    DLog::msg( DLog::LevelVerbose, "Feeder '%s' has frame '%s' of '%d' ticks long.", feederName.c_str(), frameId, ticksInFrame );
  }
  else
  {
    // Or internal error and exception?
    DLog::msg( DLog::LevelWarning, "Feeder '%s' has no frame with '%s' identifier.", feederName.c_str(), frameId );
  }
  
  return ticksInFrame;
}


void IFeederFileCommon::LoadFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet )
{
  //
  // Checking preconditions
  //
  if( frameSet.Capacity() != Capacity())
  {
    throw IFeederException( DException::Error, "Feeder '%s' can not load frame '%s', inconsistent frame capacity %dT x %dA x %dV",
                            feederName.c_str(), frameId, frameSet.Capacity().sizeText, frameSet.Capacity().sizeAudio, frameSet.Capacity().sizeVideo );
  }

  IndexFileEntry* fileEntry = FindIndexByFrameId( frameId );
  
  if( fileEntry == 0 )
  {
    DLog::msg( DLog::LevelWarning, "Feeder '%s' can not find frame '%s' to load.", feederName.c_str(), frameId );
  }
  else
  {
    frameSet.Clear();

    if( tick <= fileEntry->length )
    {
      path frameContentPath = GetContentFilePath( *fileEntry );

      LoadContentFile( frameContentPath, tick, frameSet );
    }
  }
}


void IFeederFileCommon::SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet )
{
  //if( frameSet.Capacity().sizeAudio > 0 ||
  //    frameSet.Capacity().sizeVideo > 0 )
  //{
  //  throw IFeederException( DException::Error, "Feeder '%s' can only save text", feederName.c_str() );
  //}

  //DebugInfo( frameId, tick, frameSet );
}


string IFeederFileCommon::BuildTagLine( IndexFileEntry& fileEntry )
{
  // Either from subject line in file's body
  // or just for now (3-5 tags) take it from file name
  path         fileNamePath    = fileEntry.fileName;
  string       tagLineSource   = fileNamePath.stem().string(); // cut the ext!
  string::const_iterator begin = tagLineSource.begin(); 
  string::const_iterator end   = tagLineSource.end(); 
  sregex       tagLineRegex    = sregex::compile( "([^_]+)" );
  string       tagLine         = "";  
  string       tag             = "";  
  StringList_t tagList;

  match_results< string::const_iterator > what; 
  regex_constants::match_flag_type        flags = regex_constants::match_default; 

  while( regex_search( begin, end, what, tagLineRegex, flags )) 
  {
    tag   = what[ 0 ];
    begin = what[ 0 ].second; 

    to_upper( tag );    
    tagList.push_back( tag );
  }

  tagList.sort   ( Its::StringOrder );
  tagList.unique ( Its::StringEqual );
  
  for( StringList_t::const_iterator loopTag = tagList.begin(); loopTag != tagList.end(); loopTag++ )
  {
    // Tag line is a simple ordered tokenized list, use '*' as this is a special file 
    // name character
    if( !tagLine.empty())
    {
      tagLine += tagListSeparator; 
    }
    
    tagLine += *loopTag; 
  }

  DLog::msg( DLog::LevelVerbose, "TagLine for the file '%s' is '%s'", tagLine.c_str(), fileEntry.fileName.c_str() );

  return tagLine;
}


// Parse tagline and build taglist for analysis 
StringList_t IFeederFileCommon::ParseTagLine( string tagLine )
{
  typedef boost::tokenizer< boost::char_separator<char> > tokenizer;

  boost::char_separator<char> tagSeparator( tagListSeparator );
  tokenizer    tokens( tagLine, tagSeparator );
  StringList_t tagList;
    
  for( tokenizer::iterator tokenLoop = tokens.begin(); tokenLoop != tokens.end(); ++tokenLoop )
  {
    tagList.push_back( tokenLoop->c_str() );
  }
    
  // Should be in order, but just in case
  tagList.sort   ( Its::StringOrder );
  tagList.unique ( Its::StringEqual );

  return tagList;
}


IFeederFileCommon::IndexFileEntry* IFeederFileCommon::FindIndexByFrameId( const char* _frameId )
{
  SizeIt          frameId    = boost::lexical_cast< SizeIt >( _frameId );
  IndexFileEntry *indexEntry = 0;

  IndexFileList_t::const_iterator indexByFrameId = find_if( indexFileList.begin(), 
                                                            indexFileList.end(), 
                                                            _1 ->* &IndexFileEntry::frameId == frameId );
  
  if( indexByFrameId != indexFileList.end() )
  {
    indexEntry = *indexByFrameId;
  }
  
  return indexEntry; 
}




/*
void IFeederFileCommon::WatchFolder( const char* folderPath )
{
   char   driveName     [ 4          ];
   char   fileName      [ _MAX_FNAME ];
   char   fileExtension [ _MAX_EXT   ];
   DWORD  waitStatus; 
   HANDLE changeHandle; 

   if( _splitpath_s( folderPath, driveName, 4, NULL, 0, fileName, _MAX_FNAME, fileExtension, _MAX_EXT ))
   {
     throw IFeederException( DException::Error, "Can not split path '%s', internal error.", folderPath );
   }

   driveName[ 2 ] = '\\';
   driveName[ 3 ] = '\0';
 
  // Watch the directory for file creation / updating /  deletion.  
  changeHandle = FindFirstChangeNotificationA( 
      folderPath,                     // directory to watch 
      FALSE,                          // do not watch subtree 
      FILE_NOTIFY_CHANGE_FILE_NAME ); // watch file name changes 
 
  if( changeHandle == INVALID_HANDLE_VALUE ) 
  {
    throw IFeederException( DException::Error, "Error '%d' while obtaining the first change notification in Text Feeder", GetLastError() );
  }
 
  try
  {
    while( TRUE ) 
    { 
      // Wait for notification.
      waitStatus = WaitForSingleObject( changeHandle, INFINITE ); 
 
      switch( waitStatus )
      {
        case WAIT_OBJECT_0: 
          //Refresh();

          if( !FindNextChangeNotification( changeHandle )) 
          {
            throw IFeederException( DException::Error, "Error '%d' while obtaining the next change notification in Text Feeder", GetLastError() );
          }

          break;

        case WAIT_TIMEOUT:  break;
        case WAIT_FAILED:   break;
      }
    }

    FindCloseChangeNotification( changeHandle );
  }
  catch( ... )
  {
    FindCloseChangeNotification( changeHandle );
    DLog::msg( DLog::LevelError, "Unexpected error while waiting for the folder '%s' change events in Text Feeder", folderPath );
    throw; // don't expect any exceptions
  }

  //
  // Ideally use ReadDirectoryChangesW() function but for now just rescan the folder
  // thinking that the files there should be already loaded in internal list.
  // 
}
*/

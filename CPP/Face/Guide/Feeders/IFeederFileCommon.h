/*******************************************************************************
 FILE         :   IFeederFileCommon.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Common functions for all file-based feeders 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   12/19/2011

 LAST UPDATE  :   12/19/2011
*******************************************************************************/


#ifndef __IFEEDERFILECOMMON_H
#define __IFEEDERFILECOMMON_H


#include "IFeederCommon.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/filesystem.hpp"


using namespace std;
using namespace boost::filesystem;
using namespace boost::posix_time;


namespace Its
{


class IFeederFileCommon : public virtual IFeederCommon
{
public:
  IFeederFileCommon( const char* feederName, const char* sourcePath, const char* sourceExt, LevelIt _feederLevel );
  virtual ~IFeederFileCommon();

  virtual void Sync();

  // List of all unique tags
  virtual StringList_t Tags();

  // List of all frame Ids with this tag(s)
  virtual StringList_t Frames( const char* tagLine, LevelIt level );
  
  // How long is the frame?
  virtual PeriodIt TicksInFrame( const char* frameId );
 
  virtual void LoadFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );
  virtual void SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );

protected:
  virtual PeriodIt AddContentFile  ( path& sourceFile,  path& destinationFile ) = 0;
  virtual void     LoadContentFile ( path& contentFile, PeriodIt tick, IFrameSet& frameSet ) = 0;

private:
  static const char* indexExtension;   // *.fdx
  static const char* contentExtension; // *.fbc 
  static const char* nameDelimeter;    // _
  static const char* tagListSeparator; // *

  enum ContentOperation
  {
    ContentKeep, // do nothing
    ContentAdd,
    ContentUpdate,
    ContentDelete
  };

  struct IndexFileEntry
  {
    string            fileName;
    SizeIt            frameId;
    PeriodIt          length;
    string            tagLine;
    ptime             updateStamp;
    ContentOperation  operation;

    bool operator == ( const string&      fileName    );
    bool operator == ( const std::time_t& updateStamp );
  };

  typedef list< IndexFileEntry* > IndexFileList_t;

  virtual void CleanUp();

  // Top level functions to keep index current  
  virtual bool LoadIndexFile( path indexFilePath );
  virtual void UpdateAndSaveIndex( path indexFilePath );
  virtual void SaveIndexFile( path indexFilePath );
  
  // Low level functions to keep index current  
  virtual void ParseIndexEntry( string indexEntry, IndexFileEntry& fileEntry );
  virtual void ClearContentFolder( path contentPath );
  virtual void UpdateContentFolder( int& filesAdded, int& filesUpdated, int& filesDeleted );
  virtual void AddContentFile( IndexFileEntry& fileEntry );
  virtual void DeleteContentFile( IndexFileEntry& fileEntry );
  
  // helper functions to keep index current  
  virtual path            GetContentFilePath( IndexFileEntry& fileEntry );
  virtual string          BuildTagLine( IndexFileEntry& fileEntry ); // Check filename/file and build tagline as a string with '*' dividers
  virtual StringList_t    ParseTagLine( string tagLine ); // Parse tagline and build taglist for analysis 
  virtual IndexFileEntry* FindIndexByFrameId( const char* frameId );
  
  IndexFileList_t indexFileList;
  string          indexFileName; // note no extension!
  string          sourceExt;
  path            sourcePath;
  path            indexPath;
  path            contentPath;
  LevelIt         feederLevel;
  SizeIt          maxFrameId;
  locale          loc;
  string          feederName;
};



}; // namespace Its


#endif // __IFEEDERFILECOMMON_H

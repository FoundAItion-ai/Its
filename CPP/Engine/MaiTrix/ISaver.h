/*******************************************************************************
 FILE         :   ISaver.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Layers persistency classes

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/22/2011

 LAST UPDATE  :   08/22/2011
*******************************************************************************/


#ifndef __ISAVER_H
#define __ISAVER_H

#include <fstream>
#include <map>

#include "Celllayer.h"

namespace Its
{

class MAITRIX_SPEC ISaverException: public MaiTrixException
{
public:
  DEXCEPTION_CONSTRUCTOR0( ISaverException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR1( ISaverException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR2( ISaverException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR3( ISaverException, MaiTrixException );
};


// TODO: split source code into different .c/.h
class MAITRIX_SPEC ISaver
{
public:
  /*
  From 702 Thinking in C++ www.BruceEckel.com

  Pure virtual destructors
  While pure virtual destructors are legal in Standard C++, there is
  an added constraint when using them: 
  you must provide a function body for the pure virtual destructor. 
  */
  virtual ~ISaver() = 0 {} 

  // check if layer with that name and positionZ exists, no side effects
  virtual bool Exist( Celllayer& layer  ) = 0;
  
  // return maiTrixSize and cycle
  virtual void Header( Celllayer& layer ) = 0;
  
  // load cells from layer with that name and positionZ
  // verify maiTrixSize and cycle
  virtual void Load ( Celllayer& layer  ) = 0;
  
  // save cells to layer with that name and positionZ (overwrite)
  // save maiTrixSize and cycle
  virtual void Save ( Celllayer& layer  ) = 0;

  // Remove layer
  virtual void Void ( Celllayer& layer  ) = 0;

  // Flush unsaved data
  virtual void Flush() = 0;
};


class MAITRIX_SPEC ISaverFile: public ISaver
{
public:
  typedef ISaver Base;

  ISaverFile( const char* _folderPath, bool autoCorrection = false );
  virtual ~ISaverFile();

  virtual bool Exist ( Celllayer& layer );
  virtual void Header( Celllayer& layer );
  virtual void Load  ( Celllayer& layer );
  virtual void Save  ( Celllayer& layer );
  virtual void Void  ( Celllayer& layer );
  virtual void Flush ();

protected:
  string FileNameWithPath( Celllayer& layer );
  void DoLoad( Celllayer& layer, bool headerOnly );

private:
  string   folderPath;
  ifstream inFile;
  ofstream outFile;
  bool     autoCorrection;
};


class MAITRIX_SPEC ISaverFileCached: public ISaverFile
{
public:
  typedef ISaverFile Base;

  // Ratio 10 means actual save on every 10th call;
  // If cacheRatio is 1 no caching;
  //
  ISaverFileCached( const char* _folderPath, PeriodIt _cacheRatio, bool autoCorrection = false );
  virtual ~ISaverFileCached();

  // We do cache only those operations required for the main launch cycle
  virtual void Load ( Celllayer& layer );
  virtual void Save ( Celllayer& layer );
  virtual void Void ( Celllayer& layer );
  virtual void Flush();

protected:
  void ResetCache();
  void FlushCache();

private:
  typedef std::map< SizeIt, Celllayer* > Cache_t; // position -> layer map

  PeriodIt cacheRatio;
  Cache_t  cache;  
};


class MAITRIX_SPEC ISaverInMemory: public ISaver
{
public:
  typedef ISaver Base;

  // add some memory limit?
  ISaverInMemory( bool autoCorrection = false );
  virtual ~ISaverInMemory();

  virtual bool Exist ( Celllayer& layer );
  virtual void Header( Celllayer& layer );
  virtual void Load  ( Celllayer& layer );
  virtual void Save  ( Celllayer& layer );
  virtual void Void  ( Celllayer& layer );
  virtual void Flush ();

protected:
  string ComposeKey( Celllayer& layer );

private:
  typedef std::map<string, Celllayer> LayerStorage_t;

  LayerStorage_t  storage;  // FileNameWithPath -> layer map
  bool            autoCorrection;
};


}; // namespace Its


#endif // __ISAVER_H

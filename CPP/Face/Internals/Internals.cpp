/*******************************************************************************
 FILE         :   Internals.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Internals - Helpers classes being used in Face

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/02/2012

 LAST UPDATE  :   07/02/2012
*******************************************************************************/


#include "stdafx.h"

#include "Internals.h"
#include "IFeeder.h"
#include "BasicCube.h"

#include <boost/format.hpp>
#include <filesystem>


using namespace Its;
using namespace DCommon;

using namespace std;
using boost::format;

const char* DefaultMaiTrixSnapshotLegendName = ".\\MaiTrixLegend.bmp";
const int DEFAULT_LAYER_SPACE = 3;


MaiTrixSnapshot::MaiTrixSnapshot( const char* _maiTrixName, const char* _maiTrixPath, bool autoCorrection ):
  maiTrixName( _maiTrixName ),
  saver( new ISaverFile( _maiTrixPath, autoCorrection )),
  ownSaver( true )
{
  AcquireLegend();
}


MaiTrixSnapshot::MaiTrixSnapshot( MaiTrix* maiTrix ):
  maiTrixName( maiTrix->Name() ),
  saver( maiTrix->saver.get() ), // belongs to maiTrix
  ownSaver( false )
{
  AcquireLegend();
}


MaiTrixSnapshot::~MaiTrixSnapshot()
{
  if( !ownSaver )
  {
    saver.release();
  }
}


bool MaiTrixSnapshot::Clean( const char* snapshotPath )
{
  char double_terminated_path[ MAX_PATH + 1 ] = { 0 };
  strcpy_s( double_terminated_path, MAX_PATH + 1, snapshotPath );
  double_terminated_path[ strlen( snapshotPath ) + 1 ] = 0;

  SHFILEOPSTRUCT file_operation = { 0 };
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_FILESONLY;
  int err = SHFileOperation( &file_operation );
  SetLastError( err );

  if( file_operation.fAnyOperationsAborted ||
    (( err != 0 && err != ERROR_FILE_NOT_FOUND && err != 0x402 )))
  {
    DLog::msg( DLog::LevelWarning, "Failed to delete files in '%s' folder.", snapshotPath );
    return false;
  }

  DLog::msg( DLog::LevelWarning, "In '%s' folder all files have been deleted.", snapshotPath );
  return true;
}


void MaiTrixSnapshot::AcquireLegend()
{
  try
  {
    legend = auto_ptr<DBitmapImage>( new DBitmapImage( DefaultMaiTrixSnapshotLegendName, false, DefaultImageSize ));
  }
  catch( ... )
  {
    // do nothing
    DLog::msg( DLog::LevelWarning, "Can not load legend file '%s' for maiTrix '%s'. Use no legend.", DefaultMaiTrixSnapshotLegendName, maiTrixName.c_str() );
  }
}


// all layers at the moment    -> Z images, 6 ( X by Y ) size
SizeIt3D MaiTrixSnapshot::Take( const char* snapshotPath )
{
  SizeIt2D  firstLayerSize = FirstLayerSize();
  Celllayer layer( maiTrixName.c_str(), SizeIt2D( firstLayerSize.X, firstLayerSize.Y ), 1, 0 );
  DBitmapImage::ColorScheme colorScheme = DBitmapImage::TypeHighlight;
  
  DLog::msg( DLog::LevelVerbose, "Taking snapshot of Maitrix '%s'", maiTrixName.c_str() );

  while( saver->Exist( layer ))
  {
    saver->Header( layer );
    saver->Load( layer );

    string imageName = str( format( "%s\\%s_t%08d_z%03d.bmp" ) % snapshotPath % maiTrixName % layer.cycle % layer.positionZ );
    //string imageName = str( format( "%s\\%s_%05d.bmp" ) % snapshotPath % maiTrixName % layer.positionZ );

    if( legend.get() )
    {
      CelllayerImageLegend layerImageLegend( snapshotPath, layer, *legend, DEFAULT_LAYER_SPACE );    

      layerImageLegend.Set( colorScheme );
      layerImageLegend.SaveAs( imageName.c_str() );
    }
    else
    {
      CelllayerImage layerImage( snapshotPath, layer, 0, 0, DEFAULT_LAYER_SPACE );    

      layerImage.Set( colorScheme );
      layerImage.SaveAs( imageName.c_str() );
    }

    layer.positionZ++;
  }

  DLog::msg( DLog::LevelVerbose, "Snapshot of Maitrix '%s' has been taken. Discovered maiTrix size %d x %d x %d", maiTrixName.c_str(), layer.size.X, layer.size.Y, layer.positionZ - 1 );

  return SizeIt3D( layer.size.X, layer.size.Y, layer.positionZ - 1 );
}


SizeIt2D MaiTrixSnapshot::FirstLayerSize()
{
  Celllayer firstMaitrixLayer( maiTrixName.c_str(), SizeIt2D( 0, 0 ), 1, 0 );

  if (!saver->Exist(firstMaitrixLayer))
  {
    throw InternalsException( DException::Error, "MaiTrix '%s' does not exist", maiTrixName.c_str());
  }

  saver->Header( firstMaitrixLayer );

  return firstMaitrixLayer.size;
}



void MaiTrixSnapshot::CopyCellDirectional( Cell& cellFrom, Cell& cellTo, bool copy_data )
{ 
  cellTo.type       = cellFrom.type;
  cellTo.uplink     = cellFrom.uplink;
  cellTo.reflection = cellFrom.reflection;

  if( copy_data )
  {
    cellTo.data = cellFrom.data;
  }
  else
  {
    cellTo.updata = cellFrom.updata;
  }
}


void MaiTrixSnapshot::CopyCellsToLayer( Celllayer& sliceInput, Celllayer& sliceOutput, 
                                        PointIt2D position, 
                                        Cell& cellOne, 
                                        Cell& cellTwo )
{
  Cell& cellInput   = ARRAY_2D_CELL( sliceInput  , sliceInput.size  , position );
  Cell& cellOutput  = ARRAY_2D_CELL( sliceOutput , sliceOutput.size , position );

  CopyCellDirectional( cellOne, cellInput, true  );
  CopyCellDirectional( cellTwo, cellInput, true  );

  CopyCellDirectional( cellOne, cellOutput, false );
  CopyCellDirectional( cellTwo, cellOutput, false );

  // do IO cell type verification?
}


  
// IO  layers and middle slice -> 2 images - slice in / out, 6 ( max of X / Y / Z by max of X / Y / Z ) size
// 
// if detailed is true save 3 more images - slice midsectionX, midsectionY, midsectionZ (isometrical)
//
void MaiTrixSnapshot::Slice( const char* slicePath, SizeIt3D maiTrixSize, bool detailed )
{
  DLog::msg( DLog::LevelVerbose, "Taking slice of Maitrix '%s'", maiTrixName.c_str() );

  if( maiTrixSize.X == 0 || maiTrixSize.Y == 0 || maiTrixSize.Z == 0 )
  {
    throw InternalsException( DException::Error, "Can not slice MaiTrix '%s', size %d x %d x %d is not valid", 
                              maiTrixName.c_str(), maiTrixSize.X, maiTrixSize.Y, maiTrixSize.Z );
  }

  SizeIt longerSide     = max( max( maiTrixSize.X, maiTrixSize.Y ), maiTrixSize.Z ); 
  SizeIt midSectionPosX = ( SizeIt ) ( maiTrixSize.X / 2 );
  SizeIt midSectionPosY = ( SizeIt ) ( maiTrixSize.Y / 2 );
  SizeIt midSectionPosZ = ( SizeIt ) ( maiTrixSize.Z / 2 );

  if( midSectionPosX == 0 || midSectionPosY == 0 || midSectionPosZ == 0 )
  {
    throw InternalsException( DException::Error, "Can not slice MaiTrix '%s', mid-section position %d x %d x %d is not valid", 
                              maiTrixName.c_str(), midSectionPosX, midSectionPosY, midSectionPosZ );
  }

  DLog::msg( DLog::LevelDebug, "For Maitrix '%s' mid-section position is %d x %d x %d", maiTrixName.c_str(), midSectionPosX, midSectionPosY, midSectionPosZ );

  SizeIt2D sizeIOLayer      ( longerSide    , longerSide    );
  SizeIt2D sizeMaiTrixLayer ( maiTrixSize.X , maiTrixSize.Y );
  SizeItl  ioCellsByteSize  ( sizeof( Cell ) * longerSide * longerSide );

  Celllayer sliceInput   ( maiTrixName.c_str(), sizeIOLayer      , 1, 0 );
  Celllayer sliceOutput  ( maiTrixName.c_str(), sizeIOLayer      , 1, 0 );
  Celllayer sliceMiddleX ( maiTrixName.c_str(), sizeIOLayer      , 1, 0 );
  Celllayer sliceMiddleY ( maiTrixName.c_str(), sizeIOLayer      , 1, 0 );
  Celllayer layer        ( maiTrixName.c_str(), sizeMaiTrixLayer , 1, 0 );

  memset( ( Cell* )sliceInput   , 0, ioCellsByteSize );
  memset( ( Cell* )sliceOutput  , 0, ioCellsByteSize );
  memset( ( Cell* )sliceMiddleX , 0, ioCellsByteSize );
  memset( ( Cell* )sliceMiddleY , 0, ioCellsByteSize );

  for( layer.positionZ = 1; layer.positionZ <= maiTrixSize.Z; layer.positionZ++ )
  {
    saver->Header ( layer );
    saver->Load   ( layer );

    if( layer.size != sizeMaiTrixLayer )
    {
      throw InternalsException( DException::Error, "MaiTrix '%s' loaded layer size %d x %d does not match MaiTrix size %d x %d x %d", 
                                                    maiTrixName.c_str(), layer.size.X, layer.size.Y, maiTrixSize.X, maiTrixSize.Y, maiTrixSize.Z );
    }

    // Sides
    for( SizeIt loopPositionX = 0; loopPositionX < layer.size.X; loopPositionX++ )
    {
      PointIt2D positionXZ( loopPositionX, layer.positionZ - 1 );

      Cell& cellTop     = ARRAY_2D_CELL( layer, layer.size, PointIt2D( loopPositionX, 0                 ));
      Cell& cellBottom  = ARRAY_2D_CELL( layer, layer.size, PointIt2D( loopPositionX, layer.size.Y - 1  ));
      Cell& cellMiddle  = ARRAY_2D_CELL( layer, layer.size, PointIt2D( loopPositionX, midSectionPosY    )); // middle from
      Cell& cellMiddleY = ARRAY_2D_CELL( sliceMiddleY, sliceMiddleY.size, positionXZ ); // middle to

      // IO slices are "combined", copy all input or output directions into one layer
      CopyCellsToLayer( sliceInput, sliceOutput, positionXZ, cellTop, cellBottom );

      // Middle slice is a complete slice, copy everything
      memcpy( &cellMiddleY, &cellMiddle, sizeof( Cell ));
    }

    for( SizeIt loopPositionY = 0; loopPositionY < layer.size.Y; loopPositionY++ )
    {
      PointIt2D positionYZ( loopPositionY, layer.positionZ - 1 );

      Cell& cellLeft    = ARRAY_2D_CELL( layer, layer.size, PointIt2D( 0                , loopPositionY ));
      Cell& cellRight   = ARRAY_2D_CELL( layer, layer.size, PointIt2D( layer.size.X - 1 , loopPositionY ));
      Cell& cellMiddle  = ARRAY_2D_CELL( layer, layer.size, PointIt2D( midSectionPosX   , loopPositionY )); // middle from
      Cell& cellMiddleX = ARRAY_2D_CELL( sliceMiddleX, sliceMiddleX.size, positionYZ ); // middle to

      // IO slices are "combined", copy all input or output directions into one layer
      CopyCellsToLayer( sliceInput, sliceOutput, positionYZ, cellLeft, cellRight );

      // Middle slice is a complete slice, copy everything
      memcpy( &cellMiddleX, &cellMiddle, sizeof( Cell ));
    }  

    // Top and Bottom
    if( layer.positionZ == 1 || layer.positionZ == maiTrixSize.Z )
    {
      bool copy_data;

      if( layer.positionZ == 1 )
      {
        copy_data = false;
      }
      else
      {
        copy_data = true;
      }

      for( SizeIt loopPositionX = 0; loopPositionX < layer.size.X; loopPositionX++ )
      {
        for( SizeIt loopPositionY = 0; loopPositionY < layer.size.Y; loopPositionY++ )
        {
          Cell& cell       = ARRAY_2D_CELL( layer, layer.size, PointIt2D( loopPositionX, loopPositionY ));
          Cell& cellInput  = ARRAY_2D_CELL( sliceInput  , sliceInput.size  , PointIt2D( loopPositionX, loopPositionY ));
          Cell& cellOutput = ARRAY_2D_CELL( sliceOutput , sliceOutput.size , PointIt2D( loopPositionX, loopPositionY ));

          CopyCellDirectional( cell, cellInput,  copy_data );
          CopyCellDirectional( cell, cellOutput, copy_data );
        }
      }
    }

    // Save detailed isometrical view if needed
    if( detailed )
    {
      if( layer.positionZ == midSectionPosZ )
      {
        SaveSlice( slicePath, maiTrixName.c_str(), "sliceMidZ", layer, layer.cycle );
      }
    }
  }

  
  SaveSlice( slicePath, maiTrixName.c_str(), "sliceInp", sliceInput  , layer.cycle );
  SaveSlice( slicePath, maiTrixName.c_str(), "sliceOut", sliceOutput , layer.cycle );

  // Save detailed isometrical view if needed
  if( detailed )
  {
    SaveSlice( slicePath, maiTrixName.c_str(), "sliceMidX", sliceMiddleX, layer.cycle );
    SaveSlice( slicePath, maiTrixName.c_str(), "sliceMidY", sliceMiddleY, layer.cycle );
  }

  DLog::msg( DLog::LevelVerbose, "Slice of Maitrix '%s' in cycle %d has been extracted.", maiTrixName.c_str(), layer.cycle );
}


void MaiTrixSnapshot::SaveSlice( const char* slicePath, const char* maiTrixName, const char* sliceName, 
                                 Celllayer& slice, PeriodIt cycle )
{
  string imageName = str( format( "%s\\%s_%s_%05d.bmp" ) % slicePath % maiTrixName % sliceName % cycle );

  CelllayerImage imageInput( slicePath, slice, 0, 0, DEFAULT_LAYER_SPACE );    

  imageInput.Set();
  imageInput.SaveAs( imageName.c_str() );
}


// IO layer over time
void MaiTrixSnapshot::Reflection( const char* reflectionPath, PeriodIt cycle )
{
  SizeIt2D  firstLayerSize = FirstLayerSize();
  Celllayer layer( maiTrixName.c_str(), SizeIt2D( firstLayerSize.X, firstLayerSize.Y ), 1, 0 );
  
  DLog::msg( DLog::LevelVerbose, "Saving reflection of Maitrix '%s' in cycle '%d'", maiTrixName.c_str(), cycle );

  if( saver->Exist( layer ))
  {
    saver->Header( layer );
    saver->Load( layer );

    string imageName = str( format( "%s\\_%s_%05d.bmp" ) % reflectionPath % maiTrixName % cycle );

    if( legend.get() )
    {
      CelllayerImageLegend layerImageLegend( reflectionPath, layer, *legend, DEFAULT_LAYER_SPACE );    

      layerImageLegend.Set( DBitmapImage::DataHighlight );
      layerImageLegend.SaveAs( imageName.c_str() );
    }
    else
    {
      CelllayerImage layerImage( reflectionPath, layer, 0, 0, DEFAULT_LAYER_SPACE );    

      layerImage.Set( DBitmapImage::DataHighlight );
      layerImage.SaveAs( imageName.c_str() );
    }
  }

  DLog::msg( DLog::LevelVerbose, "Saved reflection of Maitrix '%s' in cycle '%d'", maiTrixName.c_str(), cycle );
}


// Collect statistics from all layers
void MaiTrixSnapshot::CollectStatistics( CubeStat * stat )
{
  assert( stat != 0 );

  LayeredStat layeredStat;
  CollectStatistics( &layeredStat );

  for_each( layeredStat.begin(), layeredStat.end(), [& stat ] ( const CubeStat & oneLayerStat ) {
    *stat = *stat + oneLayerStat;
  });

  stat->levelAverage = stat->levelAverage / layeredStat.size();
}


// Collect statistics from each layer
void MaiTrixSnapshot::CollectStatistics( LayeredStat * layeredStat )
{
  assert( layeredStat != 0 );

  SizeIt2D  firstLayerSize = FirstLayerSize();
  Celllayer layer( maiTrixName.c_str(), SizeIt2D( firstLayerSize.X, firstLayerSize.Y ), 1, 0 );
  
  DLog::msg( DLog::LevelVerbose, "Collecting layered statistics in Maitrix '%s'", maiTrixName.c_str() );

  while( saver->Exist( layer ))
  {
    saver->Header( layer );
    saver->Load( layer );

    CelllayerImage layerImage( "", layer, 0, 0, DEFAULT_LAYER_SPACE );
    layerImage.Set( DBitmapImage::DataHighlight );

    layeredStat->push_back( CubeStat( layerImage.Statistics()));
    layer.positionZ++;
  }

  DLog::msg( DLog::LevelVerbose, "Layered statistics collected in Maitrix '%s'", maiTrixName.c_str() );
}


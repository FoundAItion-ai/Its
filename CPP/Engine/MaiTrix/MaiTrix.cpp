/*******************************************************************************
 FILE         :   MaiTrix.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   MaiTrix - layered set of Cubes

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/18/2011

 LAST UPDATE  :   02/28/2014
*******************************************************************************/


#include "stdafx.h"
#include <iomanip>
#include <boost/format.hpp>

#include "CubeManager.h"
#include "MaiTrix.h"
#include "ISaver.h"
#include "DLog.h"


using namespace Its;
using namespace DCommon;

using boost::format;
using boost::io::group;


const int ExpectedStabilityLevel = 90; // %

int MaiTrix::cudaDevices = NOCUDADEVICE;

auto_ptr< CubeManager >   MaiTrix::cudaCube;
DCriticalSection          MaiTrix::section;
MaiTrix::CudaOrdinals_t   MaiTrix::cudaDevicesInUse;

VersionIt MaiTrix::version( 0, 2, 10 );


int defaultBlockSize = CubeSize;


// cudaDevices should be different from NOCUDADEVICE!
#define DO_NOT_USE_CUDA cudaDevices = 0;


void ClearCell( Cell& cell )
{
  memset( &cell.shape,       INITIAL_DATA, sizeof( cell.shape ));
  memset( &cell.reflections, INITIAL_DATA, sizeof( cell.reflections ));
  memset( &cell.memory,      INITIAL_DATA, sizeof( cell.memory ));
}

Link GetUplinkCrossPattern( SizeIt pos, bool & normalPattern )
{
  normalPattern = pos % 2 == 0;
  return Link( 0x1 << ( normalPattern ? 3 : 7 ));
}

SizeIt MaxResponseSize( SizeIt3D size )
{
  return ( size.X * size.Y ) - ( size.X * 2 ) - ( size.Y * 2 ) + 4;
}

// static
GF MaiTrix::GF2GF( int gf )
{
  return gf == 0 ? Its::Base : ( gf > 0 ? Its::Good : Its::Bad );
}

//
// DO NOT forget to update MaiTrix( SizeIt sizeX, SizeIt sizeY, SizeIt sizeZ ) 
// contructor for testing!
//
MaiTrix::MaiTrix( const char* _name, 
                  SizeIt sizeX, SizeIt sizeY, SizeIt sizeZ, 
                  SizeIt sizeT, SizeIt sizeA, SizeIt sizeV,
                  const char* _path, int options ):
  name              ( _name ),
  size              ( 0, 0, 0 ),
  capacity          ( 0, 0, 0 ),
  cycle             ( 0 ), 
  deviceOrdinal     ( NOCUDADEVICE ),
  baseLayerDataType ( Its::videolayer ),
  baseline          ( cycle ),
  cma_input         ( 0.0 ),
  responseSize      ( Sins::InitialResponseSize )
{
  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' is initializing, version %d.%d.%d", _name, version.Major, version.Minor, version.Build );
  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' requested size %d x %d x %d, requested capacity %dT x %dA x %dV",
                               _name, sizeX, sizeY, sizeZ, sizeT, sizeA, sizeV );

  if( sizeX < CubeSize || sizeY < CubeSize || sizeZ < CubeSize )
  {
    THROWMAITRIXERROR3( "MaiTrix size %d x %d x %d is too small.", sizeX, sizeY, sizeZ );
  }

  if( sizeT < CubeSize || sizeA < CubeSize || sizeV < CubeSize )
  {
    THROWMAITRIXERROR3( "MaiTrix capacity %dT x %dA x %dV is too small.", sizeT, sizeA, sizeV );
  }
  
  if( sizeX % 2 != 0 || sizeY % 2 != 0 )
  {
    // To allow growing from the bottom with cross-pattern
    THROWMAITRIXERROR2( "MaiTrix size %d x %d is not aligned.", sizeX, sizeY );
  }

  if( name.empty() )
  {
    throw MaiTrixException( "No name MaiTrix not allowed." );
  }

  #ifdef _WIN64 
    DLog::msg( DLog::LevelDebug, "Cubes with block size %d x %d x %d and %d-bytes Cells, 64-bit architecture.", defaultBlockSize, defaultBlockSize, DIRECTIONS, sizeof( Cell ));
  #else
    DLog::msg( DLog::LevelDebug, "Cubes with block size %d x %d x %d and %d-bytes Cells, 32-bit architecture.", defaultBlockSize, defaultBlockSize, DIRECTIONS, sizeof( Cell ));
  #endif


  if( cudaDevices == NOCUDADEVICE )
  {
    try
    {
      if( options & MaiTrix::CPUOnly )
      {
        DLog::msg( DLog::LevelWarning, "***** Choose to use CPU only, will not load Cuda GPU *****" );
        DO_NOT_USE_CUDA;
      }
      else
      {
        DLog::msg( DLog::LevelDebug, "Creating Cuda Manager..." );
      
        cudaCube    = auto_ptr< CubeManager > ( new CubeManager( defaultBlockSize, defaultBlockSize, DIRECTIONS ));
        cudaDevices = cudaCube->GetDeviceCount();
      }
    }
    catch( CubeException& x )
    {
      DLog::msg( DLog::LevelWarning, "No NVidia compatible devices found, code %d, message '%s'", x.GetCudaError(), x.what() );
      DO_NOT_USE_CUDA;
    }
    catch( exception& x )
    {
      DLog::msg( DLog::LevelWarning, "No NVidia compatible devices found, message '%s'", x.what() );
      DO_NOT_USE_CUDA;
    }
    catch( ... )
    {
      DLog::msg( DLog::LevelWarning, "No NVidia compatible devices found." );
      DO_NOT_USE_CUDA;
    }
  }

  bool autoCorrection = false;

  if( options & MaiTrix::AutoCorrection )
  {
    autoCorrection = true;
    DLog::msg( DLog::LevelDebug, "Enabling autocorrection." );
  }    
  else
  {
    DLog::msg( DLog::LevelDebug, "No autocorrection." );
  }

  if( options & MaiTrix::AllInMemory )
  {
    DLog::msg( DLog::LevelDebug, "Using in-memory storage." );
    saver = auto_ptr< ISaver > ( new ISaverInMemory( autoCorrection ));
  }
  else if( options & MaiTrix::CacheLayers )
    {
      DLog::msg( DLog::LevelDebug, "Using cached local storage." );
      saver = auto_ptr< ISaver > ( new ISaverFileCached( _path, 10, autoCorrection ));
    }
    else
    {
      DLog::msg( DLog::LevelDebug, "Using non-cached local storage." );
      saver = auto_ptr< ISaver > ( new ISaverFile( _path, autoCorrection ));
    }
  
  /*
  
  Assume the period (every layer has been processed for the cycle) is complete.
  But in general we need to search for unprocessed layer(s) and complete the period.

  */
  SizeIt3D   actualSize     , requestedSize     ( sizeX, sizeY, sizeZ );
  CapacityIt actualCapacity , requestedCapacity ( sizeT, sizeA, sizeV );
  bool       maiTrixExist;
  
  maiTrixExist = Load( actualSize, actualCapacity );

  if( maiTrixExist )
  {
    size     = actualSize;
    capacity = actualCapacity;

    if( requestedSize.X != actualSize.X || requestedSize.Y != actualSize.Y )
    {
      DLog::msg( DLog::LevelWarning, "MaiTrix '%s' existing layers size does not match requested, use exisitng.", _name );
    }
  }
  else
  {
    size = SizeIt3D( requestedSize.X, requestedSize.Y, 0 );
  }
  
  responseSize = MaxResponseSize( size );
  SizeIt2D layerSize( size.X, size.Y );
  
  // Allocate processing layers in memory
  layerD    = Celllayer( name.c_str(), layerSize, 0, cycle );
  layer     = Celllayer( name.c_str(), layerSize, 1, cycle );
  layerOut  = Celllayer( name.c_str(), layerSize, 1, cycle );
  layerU    = Celllayer( name.c_str(), layerSize, 2, cycle );
             
  // Initially create video maiTrix
  if( !maiTrixExist )
  {
    Grow( 0, requestedSize.Z, baseLayerDataType );    
  }
  
  if( capacity < requestedCapacity )
  {
    // this function changes the maiTrix size and capacity
    // and may be called from outside    
    Grow( requestedCapacity );
  }

  // Assign maiTrix to processing device(s)
  Assign(); 
  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' response size %d, max response size %d", _name, responseSize, MaxResponseSize( size ));
}


MaiTrix::~MaiTrix()
{
  DCommon::DCriticalSection::Lock locker( section );

  if( deviceOrdinal != NOCUDADEVICE )
  {
    cudaDevicesInUse.remove( deviceOrdinal );
  }
}


void MaiTrix::Commit()
{
  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelInfo, "Commit maiTrix %s", name.c_str() );
  saver->Flush();
}


bool MaiTrix::Load( SizeIt3D& actualSize, CapacityIt& actualCapacity )
{
  actualSize.X            = actualSize.Y             = actualSize.Z             = 0;
  actualCapacity.sizeText = actualCapacity.sizeAudio = actualCapacity.sizeVideo = 0;

  SizeIt     textLayers  = 0;
  SizeIt     audioLayers = 0;
  SizeIt     videoLayers = 0;

  // Load layer #1 and readjust maiTrix size
  layer.maiTrixName = name;
  layer.positionZ   = 1;
  
  bool layerExist = saver->Exist( layer );

  if( !layerExist )
  {
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' does not exist.", name.c_str() );
    return false;
  }
  
  // read size and cycle
  saver->Header( layer );

  // MaiTrix master header
  actualSize.X      = layer.size.X;
  actualSize.Y      = layer.size.Y;
  cycle             = layer.cycle;
  baseLayerDataType = layer.type;

  SizeIt2D layerSize( actualSize.X, actualSize.Y );
  layer = Celllayer( name.c_str(), layerSize, 1, cycle );

  bool autoRecoveryMode = true;

  // find last one
  while( layerExist )
  {
    saver->Load( layer );

    // verify that all layers in the maiTrix are the same
    if( actualSize.X != layer.size.X || 
        actualSize.Y != layer.size.Y || 
        cycle        != layer.cycle )
    {
      // can auto recover by forcing same size/cycle?
      throw MaiTrixException( DException::Warning, "MaiTrix '%s' contains different layer in size / cycle at position '%d'", name.c_str(), layer.positionZ );
    }

    layer.positionZ++;   
    layerExist = saver->Exist( layer );
   
    LayerTypeCounter( layer.type, textLayers, audioLayers, videoLayers );
  }

  // 21 Feb 2014 - DMA - capacity calculation change:
  //
  // As all data streams are being merged into one and sent to the bottom layer.
  // We share the layer, so it has the same capacity for all types.
  actualCapacity.sizeText  = LayerCapacity( Its::textlayer,  TopToNormalLayer, actualSize ).sizeText;
  actualCapacity.sizeAudio = LayerCapacity( Its::audiolayer, TopToNormalLayer, actualSize ).sizeAudio;
  actualCapacity.sizeVideo = LayerCapacity( Its::videolayer, TopToNormalLayer, actualSize ).sizeVideo;

  if( layer.positionZ > 1 )
  {
    actualSize.Z = layer.positionZ - 1;
    
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' is loaded from cycle %d, actual size %d x %d x %d, actual capacity %dT x %dA x %dV", name.c_str(), cycle, 
                                  actualSize.X, actualSize.Y, actualSize.Z, actualCapacity.sizeText, actualCapacity.sizeAudio, actualCapacity.sizeVideo );
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' has %d text  layers", name.c_str(), textLayers  );
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' has %d audio layers", name.c_str(), audioLayers );
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' has %d video layers", name.c_str(), videoLayers );
  }

  return actualSize.Z > 0;
}



void MaiTrix::LayerTypeCounter( Layer_t layerDataType, SizeIt& textLayers, SizeIt& audioLayers, SizeIt& videoLayers )
{                                        
  switch( layerDataType )
  {
    case Its::textlayer:  textLayers++  ; break;
    case Its::audiolayer: audioLayers++ ; break;
    case Its::videolayer: videoLayers++ ; break;
    default:
      THROWMAITRIXERROR2( "In MaiTrix '%s' layer's data type '%d' is unknown", name.c_str(), layerDataType );
  }
}


string MaiTrix::LayerDataTypeToString( Layer_t layerDataType )
{
  string layerDataTypeString;
  
  switch( layerDataType )
  {
    case Its::textlayer:  layerDataTypeString = "text" ; break;
    case Its::audiolayer: layerDataTypeString = "audio"; break;
    case Its::videolayer: layerDataTypeString = "video"; break;
    default:
      THROWMAITRIXERROR2( "In MaiTrix '%s' layer data type '%d' is unknown", name.c_str(), layerDataType );
  }
  
  return layerDataTypeString;
}


string MaiTrix::CellTypeToString( int cellType )
{
  string cellTypeString;
  
  switch( cellType )
  {
    case NO_CELL   :  cellTypeString = ""     ; break;
    case GOOD_CELL :  cellTypeString = "GOOD" ; break;
    case BAD_CELL  :  cellTypeString = "BAD"  ; break;
    case S_CELL    :  cellTypeString = "SIN"  ; break;
    case A_CELL    :  cellTypeString = "AUD"  ; break;
    case V_CELL    :  cellTypeString = "VID"  ; break;
    case T_CELL    :  cellTypeString = "TXT"  ; break;
    default:
      THROWMAITRIXERROR2( "In MaiTrix '%s' cell type '%d' is unknown", name.c_str(), cellType );
  }
  
  return cellTypeString;
}


CapacityIt MaiTrix::LayerCapacity( Layer_t layerDataType, InitType_t layerInitType, SizeIt3D maiTrixSize )
{
  SizeIt2D   layerSize( maiTrixSize.X, maiTrixSize.Y );
  CapacityIt layerCapacity;

  switch( layerDataType )
  {
    case Its::textlayer:  layerCapacity.sizeText  = LayerCapacity( layerInitType, layerSize ); break;
    case Its::audiolayer: layerCapacity.sizeAudio = LayerCapacity( layerInitType, layerSize ); break;
    case Its::videolayer: layerCapacity.sizeVideo = LayerCapacity( layerInitType, layerSize ); break;
    default:
      THROWMAITRIXERROR2( "In MaiTrix '%s' layer data type '%d' is unknown", name.c_str(), layerDataType );
  }

  return layerCapacity;
}


SizeIt MaiTrix::LayerCapacity( InitType_t layerInitType, SizeIt2D layerSize )
{
  SizeIt layerCapacity;
  
  switch( layerInitType )
  {
    case VoidLayer        : layerCapacity = 0; 
                            break;
                            
    case CoverTopLayer    : 
    case CoverBottomLayer : layerCapacity = layerSize.X * layerSize.Y +
                                            layerSize.X * 2 + layerSize.Y * 2; 
                            break;
                            
    case TopToNormalLayer : layerCapacity = layerSize.X * layerSize.Y; 
                            break;
                            
    case NormalLayer      : layerCapacity = layerSize.X * 2 + layerSize.Y * 2; 
                            break;

    default:
      THROWMAITRIXERROR2( "In MaiTrix '%s' layer init type '%d' is unknown", name.c_str(), layerInitType );
  }
  
  return layerCapacity;
}


SizeIt MaiTrix::LayersNeeded( SizeIt actualCapacity, SizeIt requestedCapacity )
{
  SizeIt normalLayerCapacity  = LayerCapacity( NormalLayer      , SizeIt2D( size.X, size.Y ));
  SizeIt transitLayerCapacity = LayerCapacity( TopToNormalLayer , SizeIt2D( size.X, size.Y ));
  SizeIt normalLayers         = 0;
  SizeIt compensationLayers   = 0;

  // we take into account the fact that we may loose some cells of the type on top
  // of the transitional level if they are of different type
  //
  // example: we have top text and if we grew audio then we loose
  // transitLayerCapacity text cells, so we add more layers just in case
  // we may add too much if the types are the same but ... who cares ;-)
  //

  // 30 Dec 2011 - DMA - Will grow one layer with the same type as bottom instead
  //
  // compensationLayers = ( transitLayerCapacity / normalLayerCapacity ) + 1;

  if( actualCapacity < requestedCapacity )
  {
    normalLayers = (( requestedCapacity - actualCapacity ) / normalLayerCapacity ) + 1;
  }
  
  return normalLayers + compensationLayers;
}


// or level??
bool MaiTrix::Grow( CapacityIt requestedCapacity, int _responseSize )
{
  if( _responseSize > 0 )
  {
    DLog::msg( DLog::LevelInfo, "Response size in MaiTrix '%s' changed from %d to %d, max response size %d", name.c_str(), responseSize, _responseSize, MaxResponseSize( size ));
    responseSize = _responseSize;
  }

  if( !( capacity < requestedCapacity ))
  {
    DLog::msg( DLog::LevelInfo, "MaiTrix '%s' has enough capacity %dT x %dA x %dV, not growing.", 
                                 name.c_str(), capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo );
    return true;
  }

  // number of layers needed
  SizeIt textLayers  = LayersNeeded( capacity.sizeText  , requestedCapacity.sizeText  );
  SizeIt audioLayers = LayersNeeded( capacity.sizeAudio , requestedCapacity.sizeAudio );
  SizeIt videoLayers = LayersNeeded( capacity.sizeVideo , requestedCapacity.sizeVideo );
  
  string baseLayerTypeString = LayerDataTypeToString( baseLayerDataType );

  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' will add %d text  layers", name.c_str(), textLayers  );
  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' will add %d audio layers", name.c_str(), audioLayers );
  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' will add %d video layers", name.c_str(), videoLayers );
  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' will add 1  base  layer of '%s' type", name.c_str(), baseLayerTypeString.c_str() );

  // grow in size
  SizeIt textZ   = size.Z + textLayers;
  SizeIt audioZ  = textZ  + audioLayers;
  SizeIt videoZ  = audioZ + videoLayers;

  bool grewText  = Grow( size.Z , textZ       , Its::textlayer    );
  bool grewAudio = Grow( textZ  , audioZ      , Its::audiolayer   );
  bool grewVideo = Grow( audioZ , videoZ      , Its::videolayer   );

  // 30 Dec 2011 - DMA - Will grow one layer with the same type as bottom to compensate transitional layer
  bool grewBase  = Grow( videoZ , videoZ + 1  , baseLayerDataType );

  return grewText && grewAudio && grewVideo && grewBase;
}


bool MaiTrix::Grow()
{
  return Grow( size.Z, size.Z + 1, baseLayerDataType );
}

bool MaiTrix::GrowTop()
{
  return Grow();
}

bool MaiTrix::GrowBottom()
{
  if( size.X <= 2 )
  {
    // equivalent till this size
    return Grow();
  }

  string layerTypeString = LayerDataTypeToString( baseLayerDataType ); 
  Commit(); // would help to keep data consistent if use cached saver

  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' is being resized at the bottom from %d to %d layers of '%s' type", 
                                  name.c_str(), size.Z, size.Z, layerTypeString.c_str());

  Celllayer bottomLayer( name.c_str(), layer.size, 1, cycle );
  bottomLayer.type = baseLayerDataType;

  InitLayer( bottomLayer, baseLayerDataType, CoverBottomLayer );

  // Shift all layers up
  for( SizeIt loopPosition = size.Z; loopPosition >= 1; loopPosition-- )
  {
    layer.positionZ = loopPosition;

    if( !saver->Exist( layer ))
    {
      THROWMAITRIXERROR2( "Can not resize, layer at '%d' does not exist in MaiTrix '%s'", loopPosition, name.c_str() );
    }

    saver->Header ( layer );
    saver->Load   ( layer );
    
    if( loopPosition == 1 )
    {
      GrowLayer( layer, bottomLayer );
    }

    layer.positionZ = loopPosition + 1;
    saver->Save( layer );
  }

  // Grow at the bottom
  saver->Save( bottomLayer );
  size.Z++;
    
  capacity.sizeText  = LayerCapacity( Its::textlayer,  TopToNormalLayer, size ).sizeText;
  capacity.sizeAudio = LayerCapacity( Its::audiolayer, TopToNormalLayer, size ).sizeAudio;
  capacity.sizeVideo = LayerCapacity( Its::videolayer, TopToNormalLayer, size ).sizeVideo;

  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' has grown at the bottom to size to %d x %d x %d and capacity %dT x %dA x %dV", 
                              name.c_str(), size.X, size.Y, size.Z, 
                              capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo );

  Commit();
  return true;
}

void MaiTrix::Erase()
{
  // Erase all memory, warning!
  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' memory is being erased...", name.c_str() );

  Clear( false );
}


void MaiTrix::Reboot()
{
  // Erase everything but existence, warning!
  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' is rebooting...", name.c_str() );

  Clear( true );
}


void MaiTrix::Clear( bool clearAll )
{
  Commit(); // would help to keep data consistent if use cached saver

  SizeIt2D  layerSize( Size().X, Size().Y );
  Celllayer layer( Name(), layerSize, 1, 0 );
 
  while( saver->Exist( layer ))
  {
    saver->Header( layer );

    if( layer.size != layerSize )
    {
      THROWMAITRIXERROR3( "Can not reboot, size of layer '%d x %d' does not match size of the MaiTrix '%s'", layer.size.X, layer.size.Y, name.c_str() );
    }

    saver->Load   ( layer );
    ClearLayer    ( layer, clearAll );
    saver->Save   ( layer );

    layer.positionZ++;
  }

  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' cleared.", name.c_str() );
}


void MaiTrix::Void()
{
  // Voiding all,  warning!
  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' is voiding...", name.c_str() );

  Celllayer layer( Name(), SizeIt2D( Size().X, Size().Y ), 1, 0 );
 
  while( saver->Exist( layer ))
  {
    saver->Void( layer );

    layer.positionZ++;
  }

  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' voided.", name.c_str() );
}


// Do not shrink, only grow
bool MaiTrix::Grow( SizeIt oldSize, SizeIt newSize, Layer_t layerDataType )
{
  if( oldSize == newSize )
  {
    DLog::msg( DLog::LevelWarning, "MaiTrix '%s' is not resizing, same size %d", name.c_str(), oldSize );
    return true;
  }

  if( oldSize > newSize )
  {
    DLog::msg( DLog::LevelError, "MaiTrix '%s' can not be resized from %d to %d layers", name.c_str(), oldSize, newSize );
    return false;
  }
 
  // would help to keep data consistent if use cached saver
  Commit();

  string layerTypeString = LayerDataTypeToString( layerDataType );
 
  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' is being resized from %d to %d layers of '%s' type", 
                                  name.c_str(), oldSize, newSize, layerTypeString.c_str());
                                
  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' is being resized from %d x %d x %d and capacity %dT x %dA x %dV", 
                                  name.c_str(), 
                                  size.X, size.Y, size.Z, 
                                  capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo );
                               
  // Min set of layers:
  //
  // VoidLayer - CoverBottomLayer - NormalLayer - CoverTopLayer - VoidLayer
  //
  // VoidLayers are not being saved, only for processing 
  //
  // We save non-void layers only and they are numbered from 1, but size can be 0:
  //
  // maybe size  0 - 3
  // maybe size  3 - 5
  //  ....
  //
 
  SizeIt layerFrom  = oldSize == 0 ? 1 : oldSize;
  SizeIt layerTo    = newSize + 1;
  
  InitType_t  layerInitType;
  CapacityIt  layerCapacity;
  

  for( SizeIt loopPosition = layerFrom; loopPosition < layerTo; loopPosition++ )
  {
    layer.positionZ = loopPosition;  

    if( loopPosition == layerFrom )
    {
      if( oldSize == 0 )
      {
        layerInitType = CoverBottomLayer;
      }
      else
      {
        saver->Header ( layer );
        saver->Load   ( layer );

        layerInitType = TopToNormalLayer;
      }
    }
    else if( loopPosition == layerTo - 1 )
    {
      layerInitType = CoverTopLayer;
    }
    else
    {
      layerInitType = NormalLayer;
    }

    layer.type = layerDataType; 
    InitLayer( layer, layerDataType, layerInitType );

    // note that we don't change the transitional layer's data type
    if( layerInitType != TopToNormalLayer )
    {
      size.Z++;
      layerCapacity = LayerCapacity( layerDataType, layerInitType, size );
      // 25 Feb 2014 - DMA - Capacity depends on bottom layer only now
      // capacity      = capacity + layerCapacity;
    }
    else
    {
      // After conversion the transitional layer lost its cells on the top
      // and note that size and type have not changed.
      layerCapacity = LayerCapacity( layer.type, layerInitType, size );
      // 25 Feb 2014 - DMA - Capacity depends on bottom layer only now
      // capacity = capacity - layerCapacity;
    }
    
    saver->Save( layer );
  }

  capacity.sizeText  = LayerCapacity( Its::textlayer,  TopToNormalLayer, size ).sizeText;
  capacity.sizeAudio = LayerCapacity( Its::audiolayer, TopToNormalLayer, size ).sizeAudio;
  capacity.sizeVideo = LayerCapacity( Its::videolayer, TopToNormalLayer, size ).sizeVideo;

  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' has grown from %d to %d layers of '%s' type", 
                                  name.c_str(), oldSize, newSize, layerTypeString.c_str() );
                                
  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' has changed size to %d x %d x %d and capacity %dT x %dA x %dV", 
                              name.c_str(), 
                              size.X, size.Y, size.Z, 
                              capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo );

  // would help to keep data consistent if use cached saver
  Commit();
  return true;
}


void MaiTrix::InitLayer( Celllayer& emptyLayer, Layer_t layerDataType, InitType_t layerInitType )
{
  int cellTypeIO;

  switch( layerDataType )
  {
    case Its::textlayer  : cellTypeIO = T_CELL  ; break;
    case Its::audiolayer : cellTypeIO = A_CELL  ; break;
    case Its::videolayer : cellTypeIO = V_CELL  ; break;
    case Its::emptylayer : cellTypeIO = NO_CELL ; break;
    default: 
      THROWMAITRIXERROR2( "In MaiTrix '%s' layer data type '%d' is unknown", name.c_str(), layerDataType );
  }

  for( SizeIt loopPositionY = 0; loopPositionY < emptyLayer.size.Y; loopPositionY++ )
  {
    for( SizeIt loopPositionX = 0; loopPositionX < emptyLayer.size.X; loopPositionX++ )
    {
      PointIt   index( loopPositionX, loopPositionY );
      int       cellType  = NO_CELL;
      Link      uplink; // no uplinks at all, except for the bottom
      bool      normalPattern;

      switch( layerInitType )
      {
        case CoverTopLayer: 
          cellType = S_CELL; 
          break;

        case CoverBottomLayer:
          // 23 Jan 2013 - DMA - Should be more complex logic
          // to load the layer with multiple types of IO - text, audio and video
          //
          // 13 Jan 2017 - DMA - This is no longer relevant (multiple types)
          //
          // 28 Sep 2017 - DMA - Replace random uplinks with regular 'cross' pattern
          // to allow growing from the bottom (size should be aligned to 2)
          cellType = cellTypeIO;
          uplink   = GetUplinkCrossPattern( loopPositionY, normalPattern );
          break;

        case NormalLayer: 
        case TopToNormalLayer:
        case VoidLayer:
          break;
      }

      Cell& cell = ARRAY_2D_CELL( emptyLayer, emptyLayer.size, index );
      InitCell( cell, cellType, layerInitType, uplink.Raw(), loopPositionX, loopPositionY, emptyLayer.positionZ );
    
      if( layerInitType == CoverBottomLayer )
      {
        VerifyIOCell( emptyLayer, DataLoadDirection_t::Download, index );
      }
    }
  }
}


void MaiTrix::GrowLayer( Celllayer& upperLayer, Celllayer& bottomLayer )
{
  // Convert IO layer to normal, adding mapping with pattern:
  //
  // 01234567   position Y
  // \/\/\/\/   upper layer
  // /\/\/\/\
  // \/\/\/\/   bottom layer
  // /\/\/\/\
  //
  for( SizeIt loopPositionY = 0; loopPositionY < upperLayer.size.Y; loopPositionY++ )
  {
    for( SizeIt loopPositionX = 0; loopPositionX < upperLayer.size.X; loopPositionX++ )
    {
      bool      normalPattern;
      Link      uplink = GetUplinkCrossPattern( loopPositionY, normalPattern );
      PointIt   upperIndex  ( loopPositionX, loopPositionY );
      PointIt   bottomIndex ( loopPositionX, normalPattern ? loopPositionY + 1 : loopPositionY - 1 );
      Cell&     cell        = ARRAY_2D_CELL( upperLayer,  upperLayer.size,  upperIndex  );
      Cell&     cellBottom  = ARRAY_2D_CELL( bottomLayer, bottomLayer.size, bottomIndex );

      // Check for general assumptions
      if( !IO_CELL( cell.type ))
      {
        THROWMAITRIXERROR2( "In MaiTrix '%s' grow cell type '%d' is not IO", name.c_str(), cell.type );
      }

      if( cell.uplink != uplink.Raw())
      {
        THROWMAITRIXERROR2( "In MaiTrix '%s' grow cell uplink '%d' is not following cross pattern", name.c_str(), cell.uplink );
      }

      if( ISSIGNAL( cell.data ))
      {
        DLog::msg( DLog::LevelWarning, "MaiTrix '%s' is growing while active", name.c_str());
      }

      cell.type       = NO_CELL;
      cell.data       = INITIAL_DATA;
      cell.updata     = INITIAL_DATA;
      cell.reflection = INITIAL_REFLECTION;

      MemoryArray     memory            ( cell.memory             );
      ReflectionArray reflections       ( cell.reflections        ); // uplink -> reflectedMode
      ShapeArray      shape             ( cell.shape              ); // uplink -> shape
      ReflectionArray reflectionsBottom ( cellBottom.reflections  );
      ShapeArray      shapeBottom       ( cellBottom.shape        );
      
      Link ioDownlink( uplink );
      Link ioDownlinkReflected;
      Link ioUplink;
      Link ioUplinkMirrored;

      if( shape.GetLink( ioDownlink, &ioUplink ))
      {
        if( ioUplink.Raw() != cell.uplink )
        {
          THROWMAITRIXERROR3( "In MaiTrix '%s' grow cell has wrong mapping '%d' : '%d'", name.c_str(), ioUplink.Raw(), cell.uplink );
        }

        if( !reflections.Get( ioUplink, &ioDownlinkReflected ) || ioDownlinkReflected != ioDownlink )
        {
          THROWMAITRIXERROR3( "In MaiTrix '%s' grow cell has no or wrong reflection '%d' : '%d'", name.c_str(), ioDownlinkReflected.Raw(), ioDownlink.Raw());
        }

        ioUplinkMirrored = ioUplink.Mirror();
        if( !shapeBottom.SetLink( ioUplinkMirrored, ioUplinkMirrored ) || 
            !reflectionsBottom.Set( ioUplinkMirrored, ioUplinkMirrored ))
        {
          THROWMAITRIXERROR2( "In MaiTrix '%s' bottom cell can not be set '%d' : '%d'", name.c_str(), ioUplinkMirrored.Raw() );
        }
      }
      else
      {
        reflections.Clear();
        shape.Clear();
      }

      cell.uplink = INITIAL_UPLINK;
      memory.Clear();
    }
  }
}


void MaiTrix::ClearLayer( Celllayer& layer, bool clearAll ) 
{
  for( SizeIt loopPositionY = 0; loopPositionY < layer.size.Y; loopPositionY++ )
  {
    for( SizeIt loopPositionX = 0; loopPositionX < layer.size.X; loopPositionX++ )
    {
      Cell& cell = ARRAY_2D_CELL( layer, layer.size, PointIt2D( loopPositionX, loopPositionY ));

      // erase memory, keep cells
      memset( &cell.data,       INITIAL_DATA,       sizeof( ByteIt ) * DIRECTIONS );
      memset( &cell.updata,     INITIAL_DATA,       sizeof( ByteIt ) * DIRECTIONS );
      memset( &cell.reflection, INITIAL_REFLECTION, sizeof( ByteIt ) * DIRECTIONS );

      // clear all uplinks except for the bottom, IO layer (for no particular reason,
      // just tricky to re-initialize ;-)
      if( FindLayerInitType( layer ) != CoverBottomLayer )
      {
        memset( &cell.uplink, INITIAL_UPLINK, sizeof( ByteIt ) * DIRECTIONS );
      }

      // clear type for all but IO and SIN cells
      for( int loopDirection = 0; loopDirection < DIRECTIONS; loopDirection++ )
      {
        int cellType = cell.type;

        // Check InitCell() below
        if( !IO_CELL( cellType ) && cellType != S_CELL )
        {
          cell.type = INITIAL_TYPE;
        }
      }

      if( clearAll )
      {
        ClearCell( cell );
      }
    }
  }
}


void MaiTrix::InitCell( Cell& cell, int cellType, InitType_t layerInitType, ByteIt uplink,
  SizeIt positionX, SizeIt positionY, SizeIt positionZ )
{
  if( cellType != NO_CELL && cellType != S_CELL && !IO_CELL( cellType ))
  {
    THROWMAITRIXERROR2( "Initially may be no other cells but IN/OUT/NO/SIN, type '%d' is incorrect in MaiTrix '%s'", cellType, name.c_str() );
  }

  cell.type       = cellType;
  cell.data       = INITIAL_DATA;
  cell.updata     = INITIAL_DATA;
  cell.uplink     = uplink;
  cell.reflection = INITIAL_REFLECTION;

  ClearCell( cell );
}


/*

Use this allocation logic for a new/loaded MaiTrix:

If Cuda is present then use next available device (deviceOrdinal),
otherwise use shared BasicCube with the same projection area, SizeX x SizeY

Go with simple implementation - one not shared BasicCube per Matrix;

*/
void MaiTrix::Assign()
{
  bool assignedToCuda  = false;
  bool assignedToBasic = false;

  if( cudaDevices > 0 && cudaDevices > cudaDevicesInUse.size() )
  {
    assignedToCuda = AssignToCudaDevice();
  }
  
  if( !assignedToCuda )
  {
    assignedToBasic = AssignToBasicCube();
  }
  
  if( assignedToCuda )
  {
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' is assigned to Cuda Cube, %d Kb memory allocated", name.c_str(), MemoryRequired() / 1024 );
  }
  else
    if( assignedToBasic )
    {
      DLog::msg( DLog::LevelDebug, "MaiTrix '%s' is assigned to Basic Cube, %d Kb memory allocated", name.c_str(), MemoryRequired() / 1024 );
    }
    else
    {
      DLog::msg( DLog::LevelError, "No available computing resources for MaiTrix '%s'", name.c_str() );

      throw MaiTrixException( "No available computing resources.", DException::Warning );
    }
}



SizeItl MaiTrix::MemoryRequired()
{
  return BasicCube::CubeMemorySize( size.X, size.Y ) * 4;
}


// protected function, not to be called from public -> no need in critical section
bool MaiTrix::AssignToCudaDevice()
{
  size_t memoryRequired = MemoryRequired();
  bool   assignedToCuda = false;

  try
  {
    for( int loopDevice = 0; loopDevice < cudaDevices; loopDevice++ )
    {
      int    multiProcessorCount    = 0;
      int    majorVersion           = 0;
      int    minorVersion           = 0;
      size_t memoryAvailable        = 0;
      int    sharedMemoryAvailable  = 0;
      bool   ordinalNotInUse        = true;
                                   
      cudaCube->GetDeviceInfo( loopDevice,      majorVersion,           minorVersion, 
                               memoryAvailable, sharedMemoryAvailable,  multiProcessorCount );

      // TODO: 
      // Might want to check if external app is using the device?
      //
      ordinalNotInUse = find( cudaDevicesInUse.begin(), cudaDevicesInUse.end(), loopDevice ) == cudaDevicesInUse.end();

      DLog::msg( DLog::LevelDebug, "Required GPU with memory %d Mb, shared memory %d Kb, supporting CUDA compute capability %d.%d", 
                                    memoryRequired / ( 1024 * 1024 ), 
                                    CubeRequiredSharedMemory, 
                                    3, 
                                    5 );

      DLog::msg( DLog::LevelDebug, "Found GPU #%d with %d multi processors and memory %5.0d Mb, shared memory %5.0d Kb, supporting CUDA compute capability %d.%d", 
                                    ( loopDevice + 1 ), 
                                    multiProcessorCount, 
                                    memoryAvailable       / ( 1024 * 1024 ),
                                    sharedMemoryAvailable / 1024, 
                                    majorVersion, 
                                    minorVersion );

      if( majorVersion              >= 1                    && 
          memoryRequired            < memoryAvailable * 0.8 &&
          CubeRequiredSharedMemory  < sharedMemoryAvailable &&
          ordinalNotInUse )
      {
        deviceOrdinal = loopDevice;

        DLog::msg( DLog::LevelDebug, "GPU #%d is available, compatible and has enough memory", ( loopDevice + 1 ) );
        break;
      }
    }

    if( deviceOrdinal != NOCUDADEVICE )
    {
      cudaCube->CubeSet( deviceOrdinal, size.X, size.Y );
    }
    
    cudaDevicesInUse.push_back( deviceOrdinal );
    assignedToCuda = true;
  }
  catch( CubeException& x )
  {
    DLog::msg( DLog::LevelWarning, "Can not assign MaiTrix '%s' to Cuda Cube, code %d, message '%s'", name.c_str(), x.GetCudaError(), x.what() );
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelWarning, "Can not assign MaiTrix '%s' to Cuda Cube: '%s'", name.c_str(), x.what() );
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelWarning, "Can not assign MaiTrix '%s' to Cuda Cube.", name.c_str() );
  }

  if( !assignedToCuda )
  {
    deviceOrdinal = NOCUDADEVICE;
  }

  return assignedToCuda;
}


bool MaiTrix::AssignToBasicCube()
{
  bool assignedToBasicCube = false;

  try
  {
    basicCube = auto_ptr< BasicCubeManager > ( new BasicCubeManager( size.X, size.Y ));

    assignedToBasicCube = true;
  }
  catch( exception& x )
  {
    DLog::msg( DLog::LevelWarning, "Can not assign MaiTrix '%s' to Basic Cube: '%s'", name.c_str(), x.what() );
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelWarning, "Can not assign MaiTrix '%s' to Basic Cube.", name.c_str() );
  }

  return assignedToBasicCube;
}


void MaiTrix::ProcessLayer( int gf, int rf, Probe& probe )
{
  if( deviceOrdinal != NOCUDADEVICE )
  {
    #ifdef _DEBUG
      DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' layer %d launched on Cuda Cube in cycle = %d, gf = %d, rf = %d", name.c_str(), layerOut.positionZ, layerOut.cycle, gf, rf );
    #endif

    cudaCube->CubeLaunch( deviceOrdinal, layer, layerU, layerD, layerOut, gf, rf, &probe );  
  }
  else
    if( basicCube.get() )
    {
      #ifdef _DEBUG
        DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' layer %d launched on Basic Cube in cycle = %d, gf = %d, rf = %d", name.c_str(), layerOut.positionZ, layerOut.cycle, gf, rf );
      #endif

      basicCube->Launch( layer, layerU, layerD, layerOut, gf, rf, &probe );  
    }
    else
    {
      DLog::msg( DLog::LevelError, "No available devices to launch MaiTrix '%s'", name.c_str() );
      THROWMAITRIXERROR( "No available devices to launch MaiTrix '%s'", name.c_str() );
    }
}



Cell& MaiTrix::VerifyIOCell( Celllayer& dataLayer, DataLoadDirection_t loadDirection, PointIt2D position )
{
  Cell&   cell = ARRAY_2D_CELL( dataLayer, dataLayer.size, position );

  ByteIt  actualCellType = cell.type;
  ByteIt  neededCellType;
  bool    verified = false;

  switch( dataLayer.type )
  {
    case Its::textlayer:  neededCellType = T_CELL; break;
    case Its::audiolayer: neededCellType = A_CELL; break;
    case Its::videolayer: neededCellType = V_CELL; break;
    default: 
      THROWMAITRIXERROR3( "Unknown data type '%d' in MaiTrix '%s', layer '%d'", dataLayer.type, name.c_str(), dataLayer.positionZ );
  }

  if( neededCellType != actualCellType )
  {
    throw MaiTrixException( DException::Error, __FILE__, __LINE__, 
                            "Cell at layer '%d' in position %d x %d has inappropriate type '%d', while needed '%d'", 
                            dataLayer.positionZ, position.X, position.Y, actualCellType, neededCellType );
  }

  return cell;
}


Cell& MaiTrix::VerifySinCell( Celllayer& dataLayer, PointIt2D position )
{
  Cell&   cell     = ARRAY_2D_CELL( dataLayer, dataLayer.size, position );
  ByteIt  cellType = cell.type;

  if( cell.type != S_CELL )
  {
    throw MaiTrixException( DException::Error, __FILE__, __LINE__, 
                            "Cell at layer '%d' in position %d x %d has inappropriate type '%d', while needed Sin cell", 
                            dataLayer.positionZ, position.X, position.Y, cellType  );
  }

  return cell;
}



void MaiTrix::DebugLayerInfo( Celllayer& dataLayer, bool showType, bool showData )
{
  if( DLog::level() > DLog::LevelVerbose )
  {
    return;
  }

  // DEBUG info
  if( dataLayer.size.X > 50 || dataLayer.size.Y > 50 )
  {
    DLog::msg( DLog::LevelWarning, "DebugLayerInfo: no debug info, layer is too big." );
    return;
  }

  string layerTypeName = LayerDataTypeToString( dataLayer.type );

  DLog::msg( DLog::LevelVerbose, "" );
  DLog::msg( DLog::LevelVerbose, "In MaiTrix '%s' list types ===========> ", dataLayer.maiTrixName.c_str() );

  for( SizeIt direction = 0; direction < DIRECTIONS; direction++ )
  {
    DLog::msg( DLog::LevelVerbose, "In MaiTrix '%s', Layer position = '%d', Cycle = '%d', Layer Type = '%s', types: ", 
                                    dataLayer.maiTrixName.c_str(), 
                                    dataLayer.positionZ, 
                                    dataLayer.cycle, 
                                    layerTypeName.c_str());

    for( SizeIt loopPositionY = 0; loopPositionY < dataLayer.size.Y; loopPositionY++ )
    {
      string debugString = "";

      for( SizeIt loopPositionX = 0; loopPositionX < dataLayer.size.X; loopPositionX++ )
      {
        PointIt2D position( loopPositionX, loopPositionY );
        Cell& cell = ARRAY_2D_CELL( dataLayer, dataLayer.size, position );

        ByteIt cellType = cell.type;
        string cellTypeString;
        char   buf[ 33 ];

        if( showType )
        {
          cellTypeString = CellTypeToString( cellType );
          cellTypeString.resize( 4, ' ' );
          
          debugString    = debugString.append( cellTypeString );
        }

        if( showData )
        {
          ByteIt data = cell.updata;

          DebugByteToChar( data, buf, sizeof( buf ));

          if( showType )
          {
            debugString = debugString.append( " / " );
          }

          debugString = debugString.append( buf );
        }
        
        debugString = debugString.append( " " );
      }

      // We have 1K log entry limit
      if( debugString.size() > 900 )
      {
        string::size_type oldSize = debugString.size();
        
        debugString.resize( 900 );

        DLog::msg( DLog::LevelVerbose, "Debug string truncated from %d to %d chars", oldSize, debugString.size() );
      }

      DLog::msg( DLog::LevelVerbose, "%s ", debugString.c_str() );
    }

    DLog::msg( DLog::LevelVerbose, "" );
  }

  DLog::msg( DLog::LevelVerbose, "<================ For MaiTrix '%s' types listed.", dataLayer.maiTrixName.c_str() );
  DLog::msg( DLog::LevelVerbose, "" );
}


MaiTrix::InitType_t MaiTrix::FindLayerInitType( Celllayer& dataLayer )
{
  InitType_t layerInitType;

  // TODO: maybe we should save layerInitType into DMA header instead?
  if( dataLayer.positionZ == 1 )
  {
    layerInitType = CoverBottomLayer;
  }
  else
    if( dataLayer.positionZ == size.Z )
    {
      layerInitType = CoverTopLayer;
    }
    else
    {
      layerInitType = NormalLayer;
    }

  return layerInitType;
}


SizeIt MaiTrix::LoadData( Celllayer& dataLayer, void* dataT, void* dataA, void* dataV, DataLoadDirection_t loadDirection )
{
  ByteIt*    byteStreamT    = ( ByteIt* ) dataT;
  ByteIt*    byteStreamA    = ( ByteIt* ) dataA;
  ByteIt*    byteStreamV    = ( ByteIt* ) dataV;
  string     layerTypeName  = LayerDataTypeToString( dataLayer.type );
  InitType_t layerInitType  = FindLayerInitType( dataLayer );
  SizeIt     loaded         = 0;

  if( layerInitType == CoverBottomLayer )
  {
  #ifdef _DEBUG
    if( loadDirection == Download )
    {
      DLog::msg( DLog::LevelVerbose, "Layer's cells in MaiTrix '%s', layer type = '%s'", name.c_str(), layerTypeName.c_str() );
      DebugLayerInfo( dataLayer, true, true );
    }
  #endif

    LoadDataCover( dataLayer, byteStreamT, byteStreamA, byteStreamV, loadDirection, loaded );
  }
  else
  {
    THROWMAITRIXERROR2( "Data layer '%d' of type '%s' is not at the bottom.", dataLayer.positionZ, layerTypeName.c_str() );
  }

  return loaded;
}
 

void MaiTrix::LoadDataCover( Celllayer& dataLayer, ByteIt*& byteStreamT, ByteIt*& byteStreamA, ByteIt*& byteStreamV, 
  DataLoadDirection_t loadDirection, SizeIt& loaded )
{
  for( SizeIt loopPositionY = 0; loopPositionY < dataLayer.size.Y; loopPositionY++ )
  {
    for( SizeIt loopPositionX = 0; loopPositionX < dataLayer.size.X; loopPositionX++ )
    {
      // just in case check if we load it into proper cells
      Cell& cell = VerifyIOCell( dataLayer, loadDirection, PointIt2D( loopPositionX, loopPositionY ));

      // 21 Feb 2014 - DMA - Merge streams
      // Upload to cell, download reflection from upcell
      if( loadDirection == Upload )
      {
        ByteIt byteStream = 0;

        if( loaded < capacity.sizeText )
        {
          byteStream |= *byteStreamT;
          byteStreamT++;
        }

        if( loaded < capacity.sizeAudio )
        {
          byteStream |= *byteStreamA;
          byteStreamA++;
        }

        if( loaded < capacity.sizeVideo )
        {
          byteStream |= *byteStreamV;
          byteStreamV++;
        }

        cell.data   = byteStream;
        cell.updata = INITIAL_DATA;
      }
      else
      {
        if( loaded < capacity.sizeText )
        {
          *byteStreamT = cell.updata;
          byteStreamT++;
        }

        if( loaded < capacity.sizeAudio )
        {
          *byteStreamA = cell.updata;
          byteStreamA++;
        }

        if( loaded < capacity.sizeVideo )
        {
          *byteStreamV = cell.updata;
          byteStreamV++;
        }
      }

      loaded++;
    }
  }
}


bool MaiTrix::ReadResponse( Celllayer& dataLayer, string & response, PointIt2D position )
{
  // just in case check if we count Sin cells
  Cell& cell = VerifySinCell( dataLayer, position );

  if( ISSIGNAL( cell.data ))
  {
    // from '0' - 1, may overflow
    //
    // 26 Jul 2017 - DMA - TODO:
    // We might want to coalesce single bits, then doubles, etc to make response
    // more linear/smooth.
    response += string( 1, cell.data + '0' - 1 );
  }
  else
  {
    response += " ";
  }

  return response.size() < responseSize;
}


string MaiTrix::ReadResponse( Celllayer& dataLayer )
{
  if( FindLayerInitType( dataLayer ) != CoverTopLayer )
  {
    return string();
  }

  // Starting at central point
  PointIt2D point( dataLayer.size.X / 2, dataLayer.size.Y / 2 );
  SizeIt sideLength = 1;
  PosIt  dx = 1, dy = 1;
  string response;

  // For evenly sized maitrix need to move
  if( dataLayer.size.X % 2 == 0 )
  {
    point = point - PointIt2D( 1 , 0 );
  }

  if( dataLayer.size.Y % 2 == 0 )
  {
    point = point - PointIt2D( 0 , 1 );
  }

  if( !ReadResponse( dataLayer, response, point ))
  {
    return string();
  }

  /*
       21 22 23 24 25 26
       20 07 08 09 10
       19 06 01 02 11
       18 05 04 03 12
       17 16 15 14 13
  */
  while( true )
  {
    for( SizeIt loopSide = 1; loopSide <= sideLength * 2; loopSide++ )
    {
      if( loopSide <= sideLength )
      {
        point = point + PointIt2D( dx, 0 );
      }
      else
      {
        point = point + PointIt2D( 0, dy );
      }

      if( !point.Inside( PointIt2D( 0, 0 ), PointIt2D( size.X, size.Y ))) 
      {
        THROWMAITRIXERROR2( "Can not count Sins in data layer '%d', not enough data, response size %d", dataLayer.positionZ, responseSize );
      }

      if( !ReadResponse( dataLayer, response, point )) 
      {
        return response;
      }
    }

    dx = -dx;
    dy = -dy;
    sideLength++;
  }

  assert(false);
}


void MaiTrix::CountMetrics( Celllayer& dataLayer, CubeStat& maiTrixStat, SizeItl& inputStableCells,
  SizeItl& totalStableCells )
{
  CubeStat layerStat;
  SizeItl  layerStableCells   = 0;
  SizeItl  layerUnstableCells = 0;

  // 6 Feb 2013 - DMA - Calculate basic statistic for input or 
  // all layers, the latter is in debug mode only, as it's time consuming.
  //
  // 23 Oct 2013 - DMA - Very time consuming! Disabled for a while.
  if( FindLayerInitType( dataLayer ) == CoverBottomLayer )
  {
    CountMetricsLayer( dataLayer, layerStat, layerStableCells, layerUnstableCells );

    // cumulative moving average
    if( cycle == baseline )
    {
      assert( cma_input == 0.0 );
      cma_input = layerStat.cellsInputWithData * 1.0f;
    }
    else
    {
      assert( cycle > baseline );
      PeriodIt period = cycle - baseline;
      cma_input = ( layerStat.cellsInputWithData + ( cma_input * period )) / ( period + 1 );
    }

    inputStableCells = inputStableCells + layerStableCells;
  }
  else
  {
 #ifdef MAITRIX_PROFILING
    CountMetricsLayer( dataLayer, layerStat, layerStableCells );
 #endif
  }

  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' layered statistics in cycle / layer - %005d / %005d - %d stable, %d symmetric and %d asymmetric of %d total", 
             name.c_str(),      cycle,              dataLayer.positionZ,
             layerStableCells,  layerStat.cellsGood,  layerStat.cellsBad, layerStat.CellsTotalSignificant());

  maiTrixStat      = maiTrixStat      + layerStat;
  totalStableCells = totalStableCells + layerStableCells;
}


void MaiTrix::CountMetricsLayer( Celllayer& dataLayer, CubeStat& layerStat, SizeItl& layerStableCells, SizeItl& layerUnstableCells )
{
  // Similar to CelllayerImage() but here we can also calculate
  // MaiTrix stability metric for all layers
  //
  for( SizeIt loopPositionY = 0; loopPositionY < dataLayer.size.Y; loopPositionY++ )
  {
    for( SizeIt loopPositionX = 0; loopPositionX < dataLayer.size.X; loopPositionX++ )
    {
      PointIt2D position( loopPositionX, loopPositionY );
      Cell&     cellOut  = ARRAY_2D_CELL( dataLayer, dataLayer.size, position );
      ByteIt    cellType = cellOut.type;
      ByteIt    cellData = cellOut.data;

      switch( cellType )
      {
        case S_CELL    : layerStat.cellsSin++;  break;
        case NO_CELL   : layerStat.cellsNo++;   break;
        case GOOD_CELL : layerStat.cellsGood++; break;
        case BAD_CELL  : layerStat.cellsBad++;  break;
        case DEAD_CELL : layerStat.cellsDead++; break;
        case A_CELL    : layerStat.cellsIOa++;  break;
        case V_CELL    : layerStat.cellsIOv++;  break;
        case T_CELL    : layerStat.cellsIOt++;  break;

        default: break;
      }

      // check for corruption
      if( cellType == NO_CELL && cellOut.data != NOSIGNAL )
      {
        THROWMAITRIXERROR3( "NO cell corrupted, data '%d', updata '%d' in layer '%d'", cellOut.data, cellOut.updata, dataLayer.positionZ );
      }

      // Do not throw exception, just log, also should be visible on images
      if( cellType == DEAD_CELL )
      {
        DLog::msg( DLog::LevelError, "DEAD cells present in Maitrix '%s', data '%d', updata '%d' in layer '%d", name.c_str(), cellOut.data, cellOut.updata, dataLayer.positionZ );
      }

      if( cellData > NOSIGNAL )
      {
        layerStat.cellsWithData++;
        if( IO_CELL( cellType ))
        {
          layerStat.cellsInputWithData++;
        }
      }

      if( CellStable( cellOut ))
      {
        layerStableCells++;
      }

      if( CellUnstable( cellOut ))
      {
        layerUnstableCells++;
      }
    }
  }
}



/*

—эктор и коллеги показали, что дл€ долговременного запоминани€ критически важна молекула белка под названием протеинкиназа ћ-зета
Ётот белок контролирует количество рецепторов в контакте между нейронами, которые как раз и воспринимают те молекулы, 
которыми нейроны обмениваютс€. 

ѕротеинкиназа ћ-зета каким-то образом "помечает" контакты, которые должны работать, чтобы сохран€лись те или иные воспоминани€? 

ќна даже не метит контакт, она определ€ет его эффективность - то есть, при обучении с участием протеинкиназы ћ-зета часть контактов 
станов€тс€ эффективными, а часть - нет. Ёту молекулу можно стереть - то есть ее можно заблокировать, и животное потер€ет только 
пам€ть, сохранив при этом способность нормально обучатьс€ и все прочие функции.


мозг любого животного, в том числе и человека, стремитс€ еще и еще раз воспроизвести ситуацию, св€занную с чем-то положительным, 
при€тным, и наоборот, если было что-то непри€тное, то он стараетс€ это заблокировать. 

(c) ѕавел Ѕалабан

*/

Its::Sins MaiTrix::Launch( GF gf, int rf, void* dataT, void* dataA, void* dataV, Probe& probe )
{
  if( size.Z < CubeSize )
  {
    THROWMAITRIXERROR2( "Z-size '%d' of the MaiTrix '%s' is too small.", size.Z, name.c_str() );
  }

  // Various statistics
  string   response;
  CubeStat maiTrixStat;  
  SizeItl  totalStableCells = 0;
  SizeItl  inputStableCells = 0;

  // Always read up to capacity bytes
  SizeIt   uploaded         = 0;
  SizeIt   downloaded       = 0;

  // 6 Mar 2013 - DMA - dataT, dataA and dataV should be merged together
  // so far use dataV as our main stream
  if( cycle == 0 )
  {
    baseline  = cycle;
    cma_input = 0.0;
  }

  // 1) Launch MaiTrix
  for( SizeIt loopPosition = size.Z; loopPosition > 0; loopPosition-- )
  {
    layerU.positionZ   = loopPosition + 1;
    layer .positionZ   = loopPosition;
    layerD.positionZ   = loopPosition - 1;

    layerD.cycle       = cycle;
    layer .cycle       = cycle;
    layerU.cycle       = cycle;

    // we should not process layers from different cycles 
    // so use this one as write only buffer
    layerOut.positionZ = loopPosition;
    layerOut.cycle     = cycle + 1;

    if( loopPosition == size.Z )
    {
      // doesn't really matter if the layer is of text type as it contains nothing
      InitLayer( layerU, Its::emptylayer, VoidLayer ); 
      saver->Load( layer );
    }

    if( loopPosition == 1 )
    {
      // doesn't really matter if the layer is of text type as it contains nothing
      InitLayer( layerD, Its::emptylayer, VoidLayer );
    }
    else
    {
      saver->Load( layerD );
    }

    layerOut.type = layer.type;

    // Need to load data one cycle before the bottom
    // as we do process all layers in reverse 
    if( loopPosition == 2 )
    {
      // 21 Jan 2013 - DMA - Temp fix while switching to a new loading/unloading logic
      if( layerD.type != Its::videolayer )
      {
        THROWMAITRIXERROR3( "In MaiTrix '%s', layer '%d' data type is '%d', while expecting video", name.c_str(), layer.positionZ, layerU.type );
      }

      uploaded = LoadData( layerD, ( ByteIt* )dataT, ( ByteIt* )dataA, ( ByteIt* )dataV, MaiTrix::Upload );
    }

    probe.posZ  = loopPosition;
    probe.sizeZ = this->size.Z;
    probe.cycle = this->cycle;

    ProcessLayer( gf, rf, probe );

    // Download data after the final processing cycle
    if( loopPosition == 1 )
    {
      downloaded = LoadData( layerOut, ( ByteIt* )dataT, ( ByteIt* )dataA, ( ByteIt* )dataV, MaiTrix::Download );

      if( uploaded != downloaded )
      {
        THROWMAITRIXERROR2( "In MaiTrix '%s', layer '%d' downloaded and uploaded data have different size", name.c_str(), layer.positionZ );
      }
    }

    // Response is the top layer, reflection is the bottom
    if( loopPosition == size.Z )
    {
      response = ReadResponse( layerOut );
    }

    CountMetrics( layerOut, maiTrixStat, inputStableCells, totalStableCells );

    // Save the only fully processed layer but do not use it in further processing.
    saver->Save( layerOut );

    // Shift layers data
    layerU = layerOut;
    layer  = layerD;
  }

  // 2) This cycle is complete
  cycle++;

  int input_stability = maiTrixStat.PercentageMeaningfulInput( inputStableCells ); 

  DLog::msg( DLog::LevelVerbose, "MaiTrix '%s' has been processed in cycle %d, loaded %dT x %dA x %dV, capacity %dT x %dA x %dV", 
                                  name.c_str(), cycle, 
                                  0, 0, downloaded, 
                                  capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo );
#ifdef MAITRIX_PROFILING
  
  // Total is being calculated in debug mode or when completion is expected as 
  // it's too expensive.
  // 
  int overallStability = maiTrixStat.PercentageSignificant( totalStableCells     );
  int overallAsymmetry = maiTrixStat.PercentageSignificant( maiTrixStat.cellsBad );

  DLog::msg( DLog::LevelDebug, "MaiTrix '%s' statistics in cycle %d: overall stability %d%% and asymmetry %d%%, %d stable and %d asymmetric of %d total, stable input cells %d, input cells with data %d", 
             name.c_str(),      cycle, 
             overallStability,  overallAsymmetry, 
             totalStableCells,  maiTrixStat.cellsBad, maiTrixStat.CellsTotalSignificant(),
             inputStableCells,  maiTrixStat.cellsInputWithData );
#endif

  // 3) Last Judgement
  return LastJudgement( response, input_stability );
}


float MaiTrix::RelativeCMAInput()
{
  // Baseline either 0 cycle or later but it does exists anyway
  return ( cma_input / capacity.sizeVideo ) * 100.0f;
}


void MaiTrix::EstimateStability( int & stability, SizeIt & topRed )
{
  Celllayer layer_to_estimate( name.c_str(), SizeIt2D( size.X, size.Y ), 1, cycle );
  CubeStat  maiTrixStat;
  SizeItl   totalStableCells   = 0;
  SizeItl   totalUnstableCells = 0;
  topRed = 0;

  for( SizeIt loopPosition = size.Z; loopPosition > 0; loopPosition-- )
  {
    layer_to_estimate.positionZ = loopPosition;

    saver->Load( layer_to_estimate );
  
    CubeStat  layerStat;
    SizeItl   layerStableCells   = 0;
    SizeItl   layerUnstableCells = 0;
    CountMetricsLayer( layer_to_estimate, layerStat, layerStableCells, layerUnstableCells );

    if( layerStat.cellsBad > 0 && topRed == 0 )
    {
      assert( loopPosition != 0 );
      topRed = loopPosition;
    }

    maiTrixStat      = maiTrixStat          + layerStat;
    totalStableCells = totalStableCells     + layerStableCells;
    totalUnstableCells = totalUnstableCells + layerUnstableCells;
  }

  if( totalStableCells + totalUnstableCells == 0 )
  {
    stability = 0;
  }
  else
  {
    stability = ( int )(( ( float ) totalStableCells / ( totalStableCells + totalUnstableCells )) * 100.00 );
  }
}


Its::Sins MaiTrix::LastJudgement( string response, int input_stability )
{
  Sins    sins;
  SizeIt  topRed;
  int     stability;

  sins.response   = response;
  sins.stability  = 0;
  sins.complete   = false;

  // 28 Jul 2014 - DMA - Sleep is a necessity.
  EstimateStability( stability, topRed ); // expensive operation
  sins.stability = stability; // 0-100%

  if( sins.stability >= ExpectedStabilityLevel )
  {
    // If present in base, should be in here, but also may see some old sins.
    sins.complete = true;
  }

  if( !sins.complete )
  {
    if( !Sleeping() )
    {
      DLog::msg( DLog::LevelWarning, "Sleep is recommended for MaiTrix '%s'.", name.c_str() );
    }
    else
    {
      DLog::msg( DLog::LevelInfo, "Completion is expected soon for MaiTrix '%s', need more time.", name.c_str() );
    }
  }

  auto sinsTotal = std::count_if( response.begin(), response.end(),
    []( const string::reference & val ) {
      return val != ' ';
    });

  DLog::msg( DLog::LevelDebug, "In MaiTrix '%s' all %d Sins counted in cycle %d, with response '%s' input stability %d%%, overall stability %d%%, relative cma input %3.2f, top red %d out of %d layers", 
             name.c_str(), sinsTotal, cycle, response.c_str(), input_stability, sins.stability, RelativeCMAInput(), topRed, size.Z );

  return sins;
}


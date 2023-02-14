/*******************************************************************************
 FILE         :   CelllayerImage.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   MaiTrix - visual layer representation

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/30/2011

 LAST UPDATE  :   08/30/2011
*******************************************************************************/


#include "stdafx.h"

#include "CelllayerImage.h"
#include "BasicCube.h"


using namespace Its;
using namespace DCommon;


int DBitmapImage::imageCounter  = 0;
int DBitmapImage::DefaultBPP    = 24;
int LevelNoSignal               = MAXSIGNAL / 3;
int LevelSignal                 = LevelNoSignal * 2;
int LevelZero                   = 0;

DBitmapImage::PErrorHandlerFunc DBitmapImage::errorHandler = 0;
DCriticalSection                DBitmapImage::section;


// expect fully qualified file name
DBitmapImage::DBitmapImage( const char* _fileName, int _width, int _height ):
  name         ( _fileName  ),
  width        ( _width     ),
  height       ( _height    ),
  bitsPerPixel ( DefaultBPP )
{
  DCommon::DCriticalSection::Lock locker( section );
  InitImageLibrary();

  image = FreeImage_Allocate( width, height, bitsPerPixel );

  if( !image )
  {
    throw InternalsException( "Can not allocate image" );
  }

  if( FreeImage_GetWidth  ( image ) != width ||
      FreeImage_GetHeight ( image ) != height ||
      FreeImage_GetBPP    ( image ) != bitsPerPixel )
  {
    throw InternalsException( "Non conformant image created" );
  }
}



DBitmapImage::DBitmapImage( const char* _fileName, bool rescale, SizeIt2D scaledSize ):
  name         ( _fileName  ),
  bitsPerPixel ( DefaultBPP )
{
  DCommon::DCriticalSection::Lock locker( section );
  InitImageLibrary();

  FREE_IMAGE_FORMAT imageFormat = FreeImage_GetFIFFromFilename( _fileName );

  if( imageFormat == FIF_UNKNOWN )
  {
    throw InternalsException( "Unknown image file format." );
  }

  if( !FreeImage_FIFSupportsReading( imageFormat )) 
  {
    throw InternalsException( "Can not read this image file format." );
  }

  FIBITMAP *inputBitmap = FreeImage_Load( imageFormat, _fileName, 0 );

  if( inputBitmap == NULL )
  {
    throw InternalsException( "Image not loaded." );
  }

  try{
    if( rescale )
    {
      /*
      Converts a bitmap to 8 bits. If the bitmap was a high-color bitmap (16, 24 or 32-bit) or if it was
      a monochrome or greyscale bitmap (1 or 4-bit), the end result will be a greyscale bitmap,
      otherwise (1 or 4-bit palletised bitmaps) it will be a palletised bitmap. A clone of the input
      bitmap is returned for 8-bit bitmaps. 
      
      (From FreeImage3150.pdf)
      */
      image = FreeImage_ConvertTo8Bits( inputBitmap );
      image = FreeImage_Rescale( image, scaledSize.X, scaledSize.Y, FILTER_BICUBIC );
    }
    else
    {
      image = FreeImage_ConvertTo24Bits( inputBitmap );    
    }

    width        = FreeImage_GetWidth       ( image );
    height       = FreeImage_GetHeight      ( image );  
    bitsPerPixel = FreeImage_GetBPP         ( image ); 
  }
  catch( ... )
  {
    throw InternalsException( "Image conversion error." );
  }

  if( inputBitmap != NULL )
  {
    FreeImage_Unload( inputBitmap );
  }

  //if( bitsPerPixel != DefaultBPP )
  //{
  //  throw InternalsException( "Non conformant image loaded" );
  //}
}



void DBitmapImage::InitImageLibrary()
{
  if( imageCounter++ == 0 )
  {
  #ifndef WIN32
    // Linux only
    FreeImage_Initialise( false );
  #endif

    FreeImage_SetOutputMessage( FreeImageMessageHandler );
  }
}



DBitmapImage::~DBitmapImage()
{
  DCommon::DCriticalSection::Lock locker( section );

  if( image )
  {
    FreeImage_Unload( image );
  }

  if( --imageCounter == 0 )
  {
  #ifndef WIN32
    // Linux only
    FreeImage_DeInitialise();
  #endif
  }
}


void DBitmapImage::Paste( DBitmapImage& source, int left, int top )
{
  if( image && source.image )
  {
    // FreeImage3150.pdf:
    //
    // "alpha : alpha blend factor. The source and destination images are alpha blended if
    //  alpha=0..255. If alpha > 255, then the source image is combined to the destination image."
    //
    int alpha = 255;

    // Don't check source and this image sizing ourself,
    // why this function returns bool? ;-)
    if( !FreeImage_Paste( image, source.image, left, top, alpha ))
    {
      throw InternalsException( "Image pasting failed" );
    }
  }
}



bool DBitmapImage::RepeatedSave( const char* fileName, unsigned int repeats )
{
  // There may be error(s) if the file is opened for view by external app.
  // Let's try several times.
  bool saved = false;

  for( unsigned int loopRepeat = 0; loopRepeat < repeats; loopRepeat++ )
  {
    if( FreeImage_Save( FIF_BMP, image, fileName, BMP_SAVE_RLE ))
    {
      saved = true;
      break;
    }
    
    Sleep( 200 );
  }

  return saved;
}


void DBitmapImage::Save()
{
  if( image )
  {
    if( !RepeatedSave( name.c_str(), 5 ))
    {
      throw InternalsException( "Repeated image saving error" );
    }
  }
}


long DBitmapImage::SaveAs( const char* _fileName )
{
  long imageSizeInBytes = 0;

  if( image )
  {
    if( !RepeatedSave(_fileName, 5 ))
    {
      throw InternalsException( "Repeated image saving error" );
    }

    imageSizeInBytes = width * height * ( bitsPerPixel / 8 ); // Not file size!
  }

  return imageSizeInBytes;
}



long DBitmapImage::SaveAs( std::ostream& os )
{
  // long long is too long :)
  long imageSizeStart = ( long ) os.tellp();

  if( image )
  {
    FREE_IMAGE_TYPE imageType = FreeImage_GetImageType( image );

    if( bitsPerPixel != 8 || imageType != FIT_BITMAP )
    {
      throw InternalsException( "Can save 8-bits Bitmap images only" );
    }

    /*
    In FreeImage, FIBITMAP are based on a coordinate system that is upside down
    relative to usual graphics conventions. Thus, the scanlines are stored upside down,
    with the first scan in memory being the bottommost scan in the image.
    
    (From FreeImage3150.pdf)
    */
    
    // 4 Jan 2012 - DMA - Keep upside down, looks better on screenshots ;-)

    for( unsigned int y = 0; y < ( unsigned int ) height; y++ )
    {
      BYTE *bits = FreeImage_GetScanLine( image, y );

      if( bits != NULL )
      {
        os.write( ( const char* ) bits, width );
      }
    }
  }

  // see above
  long imageSizeFinish = ( long ) os.tellp();
  return imageSizeFinish - imageSizeStart;
}



void DBitmapImage::Set( ColorScheme scheme )
{
  colorScheme = scheme;

  if( image )
  {
    int bytesPerPixel = bitsPerPixel / 8;

    for( unsigned int y = 0; y < ( unsigned int ) height; y++ ) 
    {
      BYTE *bits = FreeImage_GetScanLine( image, y );

      for( unsigned int x = 0; x < ( unsigned int ) width; x++ ) 
      {
        SetPixel( x, y, bits[ FI_RGBA_RED ], bits[ FI_RGBA_GREEN ], bits[ FI_RGBA_BLUE  ] );

        bits += bytesPerPixel;
      }
    }
  }
}




// display all 6 layer's dimensions on one image 3 x 2 
CelllayerImage::CelllayerImage( const char* folderPath, Celllayer& _layer ):
  Base  ( _layer.FileNameWithPath( folderPath, ".bmp" ).c_str(), 
          Layer6DSize( _layer.size, 0, 0, 0 ).X, 
          Layer6DSize( _layer.size, 0, 0, 0 ).Y ),
  layer       ( _layer ),
  layerSpace  ( 0 )
{
  if( layer == 0 || layer.size.X == 0 || layer.size.Y == 0 )
  {
    throw InternalsException( "Empty cellayer may not be saved as image" );
  }
}



CelllayerImage::CelllayerImage( const char* folderPath, Celllayer& _layer, 
                                int _adjWidth, int _adjHeight, int _layerSpace ):
  Base  ( _layer.FileNameWithPath( folderPath, ".bmp" ).c_str(),           
          Layer6DSize( _layer.size, _adjWidth, _adjHeight, _layerSpace ).X, 
          Layer6DSize( _layer.size, _adjWidth, _adjHeight, _layerSpace ).Y  ),
  layer       ( _layer ),
  layerSpace  ( _layerSpace )
{
  if( layer == 0 || layer.size.X == 0 || layer.size.Y == 0 )
  {
    throw InternalsException( "Empty cellayer may not be saved as image" );
  }
}


void CelllayerImage::Set( ColorScheme scheme )
{
  statistics = CubeStat();
  Base::Set( scheme );
  DLog::msg( DLog::LevelVerbose, "The layer statistics: good %d, bad %d", statistics.cellsGood, statistics.cellsBad );
}


// May increase size to _adjWidth and _adjHeight for legend, etc...
// and insert spaces in between layers of _layerSpace
SizeIt2D CelllayerImage::Layer6DSize( SizeIt2D layerSize, int _adjWidth, int _adjHeight, int _layerSpace )
{
  SizeIt2D layer6DSize( ( layerSize.X * 3 ) + ( _layerSpace * 2 ), 
                        ( layerSize.Y * 2 ) + ( _layerSpace * 1 ));

  if( ( unsigned int ) _adjWidth > layer6DSize.X )
  {
    layer6DSize.X = _adjWidth;
  }

  layer6DSize.Y += _adjHeight;

  return layer6DSize;
}



void CelllayerImage::CollectStatistics( Cell& cell )
{
  ByteIt cellType   = cell.type;
  ByteIt cellData   = cell.data;
  ByteIt cellUpdata = cell.updata;

  // Collect various statistics (this level only!)
  switch( cellType )
  {
    case NO_CELL   : statistics.cellsNo++;   break;
    case GOOD_CELL : statistics.cellsGood++; break;
    case BAD_CELL  : statistics.cellsBad++;  break;
    case DEAD_CELL : statistics.cellsDead++; break;
    case S_CELL    : statistics.cellsSin++;  break;
    case A_CELL    : statistics.cellsIOa++;  break;
    case V_CELL    : statistics.cellsIOv++;  break;
    case T_CELL    : statistics.cellsIOt++;  break;
    default:
      throw InternalsException( DException::Error, "Undefined cell type '%d' found while projecting image", cellType );
  }

  if( cellData == INITIAL_DATA )
  {
    statistics.levelCellsWithNoData++;
  }

  if( !ISSIGNAL( cellData ))
  {
    statistics.levelCellsWithNoSignal++;
  }
  else
  {
    statistics.cellsWithData++;

    if( IO_CELL( cellType ))
    {
      statistics.cellsInputWithData++;
    }
  }

  if( cellData > MAXSIGNAL / 2 )
  {
    statistics.levelCellsAboveMedian++;
  }

  statistics.levelAverage = ( statistics.levelAverage + cellData ) / 2;
}


void CelllayerImage::SetPixel( unsigned int x, unsigned int y, BYTE& r, BYTE& g, BYTE& b )
{
  SizeIt2D layer6DSize = Layer6DSize( layer.size, 0, 0, layerSpace );

  int dimensionSpaceOne = x / ( layer.size.X + layerSpace ); // 0, 1, 2
  int dimensionSpaceTwo = y / ( layer.size.Y + layerSpace ); // 0, 1
  
  // Normalize x, y (remove space)
  int normX = x - ( dimensionSpaceOne * layerSpace );
  int normY = y - ( dimensionSpaceTwo * layerSpace );

  int dimensionOne = normX / layer.size.X; // 0, 1, 2
  int dimensionTwo = normY / layer.size.Y; // 0, 1

  if( x >= layer6DSize.X || 
      y >= layer6DSize.Y )
  {
    r = g = b = MAXSIGNAL;
    return;
  }
  else if( dimensionOne != dimensionSpaceOne ||
           dimensionTwo != dimensionSpaceTwo )
    {
      r = g = b = LevelZero;
      return;
    }

  PointIt2D position( normX % layer.size.X, normY % layer.size.Y );

  Cell& cell = ARRAY_2D_CELL( layer, layer.size, position );
  int direction = dimensionOne * 2 + dimensionTwo;
  RGB rgb;
  rgb.r = rgb.g = rgb.b = LevelZero;

  switch( direction )
  {
    case 0: Direction_0( cell, rgb ); break;
    case 1: Direction_1( cell, rgb ); break;
    case 2: Direction_2( cell, rgb ); CollectStatistics( cell ); break;  // By type
    case 3: Direction_3( cell, rgb ); break;
    case 4: Direction_4( cell, rgb ); break;
    case 5: Direction_5( cell, rgb ); break;
    default:
      throw InternalsException( "Image size doesn't match layer" );
  }

  r = rgb.r;
  g = rgb.g;
  b = rgb.b;
}


void CelllayerImage::SetPixelByType( ByteIt cellType, ByteIt uplink, ByteIt reflection, RGB& rgb )
{
  switch( cellType )
  {
    case NO_CELL   : rgb.r = 0; rgb.g = 0; rgb.b = 0; break;
    case GOOD_CELL : rgb.r = 0; rgb.g = 1; rgb.b = 0; break;
    case BAD_CELL  : rgb.r = 1; rgb.g = 0; rgb.b = 0; break;
    case DEAD_CELL : rgb.r = 0; rgb.g = 0; rgb.b = 1; break;
    case S_CELL    : rgb.r = 1; rgb.g = uplink > 0 ? 0 : 1; rgb.b = 0; break; // adding uplink makes signal visible
    case A_CELL    :
    case V_CELL    :
    case T_CELL    : rgb.r = 1; rgb.g = reflection > 0 ? 1 : 0; rgb.b = 1; break; // adding reflection makes signal visible
    default:
      throw InternalsException( DException::Error, "Undefined cell type '%d' found while projecting image", cellType );
  }

  NormalizeLevel( rgb );
}


void CelllayerImage::SetPixelByData( ByteIt data, RGB& rgb )
{
  rgb.r = rgb.g = rgb.b = data > 0 ? 1 : 0;
  NormalizeLevel( rgb );
}


void CelllayerImage::SetPixelByHash( ByteIt * arrayData, SizeIt size, RGB& rgb )
{
  // Cell::shape is the biggest one to display
  assert( size > 0 && size <= 128 );
  SizeIt sumUpperBound = size * 255;
  SizeIt totalSum   = 0;
  SizeIt uniformity = 0;
  SizeIt uniformityBound = size / 3;

  // Estimate saturation by brightness
  for( SizeIt loopItem = 0; loopItem < size; loopItem++ )
  {
    ByteIt data = arrayData[ loopItem ];
    totalSum += data;
    if( data > 0 ) 
    {
      uniformity++;
    }
  }

  assert( totalSum < sumUpperBound );
  rgb.r = rgb.g = rgb.b = 0;

  // And uniformity by color
  if( uniformity > 0 )
  {
    rgb.r = 1;
  }

  int brightness = ( int ) (( float )totalSum / ( float )sumUpperBound ) * MAXSIGNAL;
  if( brightness < LevelNoSignal )
  {
    brightness = MAXSIGNAL;
  }

  rgb.r *= brightness;
  rgb.g *= brightness;
  rgb.b *= brightness;
}


// Layout:
//
// 1  3  5
// 0  2  4
//
void CelllayerImage::Direction_0( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByData( cell.updata, rgb );
  }
}


void CelllayerImage::Direction_1( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByData( cell.data, rgb );
  }
}


void CelllayerImage::Direction_2( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByType( cell.type, cell.uplink, cell.reflection, rgb );
  }
}


void CelllayerImage::Direction_3( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByHash( cell.shape, sizeof( cell.shape ), rgb );
  }
}


void CelllayerImage::Direction_4( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByHash( cell.reflections, sizeof( cell.reflections ), rgb );
  }
}


void CelllayerImage::Direction_5( Cell& cell, RGB& rgb )
{
  if( GetColorScheme() != NoMapping )
  {
    SetPixelByHash( cell.memory, sizeof( cell.memory ), rgb );
  }
}


BYTE CelllayerImage::NormalizeLevel( BYTE level )
{
  return level == LevelZero ? LevelZero : LevelSignal;
}


void CelllayerImage::NormalizeLevel( RGB& rgb )
{
  rgb.r = NormalizeLevel( rgb.r );
  rgb.g = NormalizeLevel( rgb.g );
  rgb.b = NormalizeLevel( rgb.b );
}



CelllayerImageLegend::CelllayerImageLegend( const char* folderPath, Celllayer& _layer, 
                                            DBitmapImage& _legend, int _layerSpace ):
  Base( folderPath, _layer, _legend.ImageSize().X, _legend.ImageSize().Y, _layerSpace ),
  legend( _legend )
{
}


void CelllayerImageLegend::PasteLegend()
{
  SizeIt2D legendSize = legend.ImageSize();
  SizeIt2D imageSize  = ImageSize();

  if( legendSize.X > imageSize.X ||
      legendSize.Y > imageSize.Y )
  {
    throw InternalsException( "Image with legend is not properly sized" );
  }

  Paste( legend, 0, 0 );
}


void CelllayerImageLegend::Save()
{
  PasteLegend();
  Base::Save();
}


long CelllayerImageLegend::SaveAs( const char* _fileName )
{
  PasteLegend();
  return Base::SaveAs( _fileName );
}


long CelllayerImageLegend::SaveAs( std::ostream& os )
{
  PasteLegend();
  return Base::SaveAs( os );
}

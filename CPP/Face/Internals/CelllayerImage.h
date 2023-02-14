/*******************************************************************************
 FILE         :   CelllayerImage.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   MaiTrix - visual layer representation

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/30/2011

 LAST UPDATE  :   07/02/2012
*******************************************************************************/


#ifndef __CELLLAYERIMAGE_H
#define __CELLLAYERIMAGE_H


#include <FreeImage.h>
#include <assert.h>
#include <exception>

#include "InternalsDefines.h"
#include "InternalsException.h"
#include "MaiTrix.h"


using namespace std;
using namespace DCommon;


namespace Its
{

// move to separate file and add exception class
class INTERNALS_SPEC DBitmapImage
{
public:
  enum ColorScheme
  { 
    NoMapping     = 0x1,
    TypeHighlight = 0x2,
    DataHighlight = 0x3,  // Same output as for TypeHighlight, need redesign?
    DefaultScheme = NoMapping
  };

  // expect fully qualified file name
  DBitmapImage( const char* _fileName, int _width, int _height ); // create new
  DBitmapImage( const char* _fileName, bool rescale, SizeIt2D scaledSize ); // load

  ~DBitmapImage();

  virtual void Paste( DBitmapImage& source, int left, int top );
  virtual void Save();
  virtual long SaveAs( const char* _fileName );
  virtual long SaveAs( std::ostream& os );
  virtual void Set( ColorScheme scheme = DBitmapImage::DefaultScheme );

  typedef void (*PErrorHandlerFunc)( const char *message ); 

  static void SetErrorHandler( PErrorHandlerFunc _errorHandler )
  {
    errorHandler = _errorHandler;
  }

  SizeIt2D ImageSize() { return SizeIt2D( width, height ); }


protected:
  void InitImageLibrary();
  ColorScheme GetColorScheme() { return colorScheme; }

  virtual void SetPixel( unsigned int x, unsigned int y, BYTE& r, BYTE& g, BYTE& b )
  {
    assert( colorScheme == DBitmapImage::DefaultScheme );
    // we can set gray level?
  }

  bool RepeatedSave( const char* fileName, unsigned int repeats );

  static void FreeImageMessageHandler( FREE_IMAGE_FORMAT fif, const char *message )
  {
    if( errorHandler )
    {
      errorHandler( message );
    }
  }

private:
  FIBITMAP*     image;
  string        name;  
  int           width;  // should be unsigned but follow FreeImage.h conventions
  int           height; // same
  int           bitsPerPixel;  
  ColorScheme   colorScheme;

  static DCriticalSection   section;
  static int                imageCounter;
  static PErrorHandlerFunc  errorHandler;
  static int                DefaultBPP;
};




class INTERNALS_SPEC CelllayerImage: public DBitmapImage
{
public:
  typedef DBitmapImage Base;

  CelllayerImage( const char* folderPath, Celllayer& layer );
  CelllayerImage( const char* folderPath, Celllayer& layer, int _adjWidth, int _adjHeight, int _layerSpace = 0 ); 

  virtual void Set( ColorScheme scheme = DBitmapImage::DefaultScheme );

  const CubeStat& Statistics() { return statistics; }

protected:
  struct RGB
  {
    BYTE r;
    BYTE g;
    BYTE b;
  };

  virtual void SetPixel( unsigned int x, unsigned int y, BYTE& r, BYTE& g, BYTE& b );

  virtual void SetPixelByType( ByteIt cellType, ByteIt uplink, ByteIt reflection, RGB& rgb );
  virtual void SetPixelByData( ByteIt cellData, RGB& rgb );
  virtual void SetPixelByHash( ByteIt * arrayData, SizeIt size, RGB& rgb );

  virtual void CollectStatistics( Cell& cell );

  virtual void Direction_0( Cell& cell, RGB& rgb );
  virtual void Direction_1( Cell& cell, RGB& rgb );
  virtual void Direction_2( Cell& cell, RGB& rgb );
  virtual void Direction_3( Cell& cell, RGB& rgb );
  virtual void Direction_4( Cell& cell, RGB& rgb );
  virtual void Direction_5( Cell& cell, RGB& rgb );

  void NormalizeLevel( RGB& rgb );
  BYTE NormalizeLevel( BYTE level );

  SizeIt2D Layer6DSize( SizeIt2D size2D, int _adjWidth, int _adjHeight, int _layerSpace );

private:
  Celllayer& layer;
  int        layerSpace;
  CubeStat   statistics;
};



// with color legend
class INTERNALS_SPEC CelllayerImageLegend: public CelllayerImage
{
public:
  typedef CelllayerImage Base;

  CelllayerImageLegend( const char* folderPath, Celllayer& layer, 
                        DBitmapImage& legend, int _layerSpace = 0 );

  virtual void Save();
  virtual long SaveAs( const char* _fileName );
  virtual long SaveAs( std::ostream& os );

protected:
  void PasteLegend();

private:
  DBitmapImage& legend;
};



}; // namespace Its


#endif // __CELLLAYERIMAGE_H

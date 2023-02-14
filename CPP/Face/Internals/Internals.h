/*******************************************************************************
 FILE         :   Internals.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Internals - Helpers classes being used in Face

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/02/2012

 LAST UPDATE  :   07/02/2012
*******************************************************************************/


#ifndef __INTERNALS_H
#define __INTERNALS_H

#include <memory>
#include <list>

#include "DCommonIncl.h"
#include "BasicCube.h"
#include "ISaver.h"
#include "CelllayerImage.h"


using namespace std;
using namespace DCommon;


namespace Its
{


class INTERNALS_SPEC MaiTrixSnapshot
{
public:
  MaiTrixSnapshot( const char* maiTrixName, const char* maiTrixPath, bool autoCorrection = false );
  MaiTrixSnapshot( MaiTrix* maiTrix );

  virtual ~MaiTrixSnapshot();

  // Clean this folder
  virtual bool Clean( const char* snapshotPath );

  // all layers at the moment    -> Z images, 6 ( X by Y ) size
  virtual SizeIt3D Take( const char* snapshotPath ); 
  
  // IO  layers and middle slice -> 2 images - slice in / out, 6 ( max of X / Y / Z by max of X / Y / Z ) size
  // 
  // if detailed is true save 3 more images - slice midsectionX, midsectionY, midsectionZ (isometrical)
  //
  virtual void Slice( const char* slicePath, SizeIt3D maiTrixSize, bool detailed );

  // IO data layers over time
  virtual void Reflection( const char* reflectionPath, PeriodIt cycle );

  // Collect statistics from all layers
  virtual void CollectStatistics( CubeStat * stat );

  // Collect statistics from each layer
  typedef vector<CubeStat> LayeredStat;
  virtual void CollectStatistics( LayeredStat * layeredStat );

protected:
  void AcquireLegend();
  SizeIt2D FirstLayerSize();
  void     SaveSlice( const char* slicePath, const char* maiTrixName, const char* sliceName, 
                      Celllayer& layer, PeriodIt cycle );
  
  void CopyCellDirectional( Cell& cellFrom, Cell& cellTo, bool copy_data );
  void CopyCellsToLayer   ( Celllayer& sliceInput, Celllayer& sliceOutput, 
                            PointIt2D position, 
                            Cell& cellOne, 
                            Cell& cellTwo );

private:
  string                  maiTrixName;
  bool                    ownSaver;
  auto_ptr<ISaver>        saver;
  auto_ptr<DBitmapImage>  legend; // use if present
};




}; // namespace Its


#endif // __INTERNALS_H

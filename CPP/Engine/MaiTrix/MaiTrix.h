/*******************************************************************************
 FILE         :   MaiTrix.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   MaiTrix - layered set of Cubes

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/18/2011

 LAST UPDATE  :   08/18/2011
*******************************************************************************/


#ifndef __MAITRIX_H
#define __MAITRIX_H

#include <memory>
#include <list>
#include <vector>

#include "DCommonIncl.h"
#include "BasicException.h"
#include "BasicCubeManager.h"
#include "CubeManager.h"
#include "ITypes.h"
#include "MaiTrixDefines.h"
#include "ISaver.h"


using namespace std;
using namespace DCommon;

namespace Its
{

#define ALLSINS 256
// #define MAITRIX_PROFILING


struct MAITRIX_SPEC VersionIt
{
  VersionIt( int _major, int _minor, int _build )
  {
    Major = _major;
    Minor = _minor;
    Build = _build;
  }

  int Major;
  int Minor;
  int Build;
  // 
  // version won't be static if we save adjustments options with maiTrix itself
  // 
  // string / list variations;
};


// TODO: Should be R/O for outsiders
struct MAITRIX_SPEC Sins
{
public:
  Sins( const Sins & sins ):
    response  ( sins.response  ),
    stability ( sins.stability ),
    complete  ( sins.complete  ) {}

  Sins():
    response  ( "" ),
    stability ( 0 ),
    complete  ( false ) {}

  enum
  {
    InitialResponseSize = 75
  };

  string  response;
  int     stability; // 0 - 100%
  bool    complete;  // true when complete
};


/*

MaiTrix proposed features:

1) Can grow on Z axis, depending on info stream;
2) Single layer saved in transaction;
3) Should allow recovery when not all layers were processed;
4) Ideally should allow split-processing with many Cubes working on a single maiTrix;

*/

enum GF
{
  Good = 1, // good, memorizing
  Base = 0, // learning
  Bad = -1  // bad, clear memory
};


class MAITRIX_SPEC MaiTrix
{
public:
  enum MaiTrixOptions
  {
    Default         = 0x001, // Prefer GPU over CPU, no cache
    CacheLayers     = 0x002,
    CPUOnly         = 0x004,
    AutoCorrection  = 0x008, // correct cycle/position errors while loading
    AllInMemory     = 0x010  // maiTrix is not saved, in memory only,
                             // autocorrection is still supported though. 
  };

  MaiTrix( const char* name, 
           SizeIt sizeX, SizeIt sizeY, SizeIt sizeZ, // metric size
           SizeIt sizeT, SizeIt sizeA, SizeIt sizeV, // capacity size by data type, 
                                                     // bytes per cycle, Interaction_t
           const char* _path = ".\\",
           int options = Default );
  ~MaiTrix();

  virtual Sins Launch( GF gf, int rf, void* dataT, void* dataA, void* dataV, Probe& probe );
  virtual bool Grow( CapacityIt requestedCapacity, int responseSize = Sins::InitialResponseSize ); // this function changes the maiTrix size and capacity
  virtual bool Grow();       // +1 layer on the top
  virtual bool GrowTop();    // +1 layer on the top
  virtual bool GrowBottom(); // +1 layer at the bottom

  // External actions
  virtual void Erase  (); // clear memory
  virtual void Reboot (); // clear memory and state
  virtual void Void   (); // void all 
  virtual void Commit (); // flush unsaved data 

  const char*      Name     () { return name.c_str(); }
  SizeIt3D         Size     () { return size;         }
  CapacityIt       Capacity () { return capacity;     }
  PeriodIt         Cycle    () { return cycle;        }
  static VersionIt Version  () { return version;      }
  bool             Sleeping () { return RelativeCMAInput() < 1.0; } // ot fonarya :-)}

  static GF GF2GF( int gf );

protected:
  MaiTrix( SizeIt sizeX, SizeIt sizeY, SizeIt sizeZ ): 
    size( sizeX, sizeY, sizeZ ), 
    capacity( 0, 0, 0 ),
    cycle( 0 ), 
    baseline( cycle ),
    cma_input( 0 ),
    responseSize( Sins::InitialResponseSize )
    {} // for testing only

  enum InitType_t 
  { 
    VoidLayer         , // NO_CELLs only for stub layers
    CoverTopLayer     , // IN_CELL, OUT_CELL mostly
    CoverBottomLayer  , // IN_CELL, OUT_CELL mostly, reverse orientation
    NormalLayer       , // NO_CELL with IN/OUT on the edge
    TopToNormalLayer    // CoverTopLayer -> NormalLayer conversion, preserve state
  };

  enum DataLoadDirection_t
  { 
    Upload,
    Download
  };

  virtual void    Assign     (); // to Cuda or Basic Cubes
  virtual bool    Load       ( SizeIt3D& actualSize, CapacityIt& actualCapacity ); // return actual size and capacity
  virtual bool    Grow       ( SizeIt oldSize, SizeIt newSize, Layer_t layerDataType );
  virtual void    Clear      ( bool clearAll ); // clear either memory or both memory and state

  virtual void    InitCell   ( Cell& cell, int cellType, InitType_t layerInitType, ByteIt uplink, SizeIt positionX, SizeIt positionY, SizeIt positionZ );
  virtual void    InitLayer  ( Celllayer& emptyLayer, Layer_t layerDataType, InitType_t layerInitType ); // assign cells to empty layer
  virtual void    ClearLayer ( Celllayer& layer, bool clearAll ); // erase memory and maybe cells
  virtual void    GrowLayer  ( Celllayer& upperLayer, Celllayer& bottomLayer ); // Convert IO to normal, adding mapping

  virtual void    ProcessLayer       ( int gf, int rf, Probe& probe );
  virtual SizeIt  LoadData           ( Celllayer& dataLayer, void* dataT, void* dataA, void* dataV, DataLoadDirection_t loadDirection );
  virtual void    CountMetrics       ( Celllayer& dataLayer,  CubeStat&   maiTrixStat, SizeItl& inputStableCells, SizeItl& totalStableCells );
  virtual void    CountMetricsLayer  ( Celllayer& dataLayer,  CubeStat&   layerStat,   SizeItl& layerStableCells, SizeItl& layerUnstableCells );
  virtual void    EstimateStability  ( int & stability, SizeIt & topRed );

  virtual Sins    LastJudgement      ( string response, int input_stability );
  
  CapacityIt      LayerCapacity      ( Layer_t layerDataType, InitType_t layerInitType, SizeIt3D maiTrixSize );
  SizeIt          LayerCapacity      ( InitType_t layerInitType, SizeIt2D layerSize );  
  SizeIt          LayersNeeded       ( SizeIt actualCapacity, SizeIt requestedCapacity );
  void            LayerTypeCounter   ( Layer_t layerDataType, SizeIt& textLayers, SizeIt& audioLayers, SizeIt& videoLayers );
  Cell&           VerifyIOCell       ( Celllayer& dataLayer, DataLoadDirection_t loadDirection, PointIt2D position );
  Cell&           VerifySinCell      ( Celllayer& dataLayer, PointIt2D position );
  InitType_t      FindLayerInitType  ( Celllayer& dataLayer );

  // Debug helpers functions
  string          LayerDataTypeToString( Layer_t layerDataType );
  string          CellTypeToString   ( int cellType );
  void            DebugLayerInfo     ( Celllayer& dataLayer, bool showType, bool showData );

  SizeItl         MemoryRequired     ();
  
  bool            AssignToCudaDevice ();
  bool            AssignToBasicCube  ();

  float           RelativeCMAInput();

private:
  void LoadDataCover    ( Celllayer& dataLayer, ByteIt*& byteStreamT, ByteIt*& byteStreamA, ByteIt*& byteStreamV,DataLoadDirection_t loadDirection, SizeIt& loaded );
  string ReadResponse   ( Celllayer& dataLayer );
  bool ReadResponse     ( Celllayer& dataLayer, string & response, PointIt2D position );
  bool CellStable       ( Cell&  cell ) { return CellActive( cell ) && cell.type == GOOD_CELL; }
  bool CellUnstable     ( Cell&  cell ) { return CellActive( cell ) && cell.type == BAD_CELL;  }
  bool CellActive       ( Cell&  cell ) { return cell.data > NOSIGNAL && ( cell.type != NO_CELL && cell.type != S_CELL ); }

  typedef list< int > CudaOrdinals_t;
  friend class MaiTrixSnapshot;

  PeriodIt      cycle;
  Celllayer     layerD;
  Celllayer     layer;
  Celllayer     layerU;
  Celllayer     layerOut;
  string        name;
  SizeIt3D      size;
  CapacityIt    capacity;
  int           deviceOrdinal;
  Layer_t       baseLayerDataType; // bottom and top layer's type

  // "sleep indicator":
  // cumulative moving average input level since the baseline (GF base)
  float         cma_input;
  PeriodIt      baseline; 
  int           responseSize;
  
  auto_ptr< BasicCubeManager >     basicCube;
  auto_ptr< ISaver >               saver;
    
  static VersionIt                 version;
  static DCriticalSection          section;
  static int                       cudaDevices;
  static CudaOrdinals_t            cudaDevicesInUse;
  static auto_ptr< CubeManager >   cudaCube;  
};


}; // namespace Its


#endif // __MAITRIX_H

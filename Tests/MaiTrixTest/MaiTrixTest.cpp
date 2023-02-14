// MaiTrixTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <iostream>
#include <time.h>
#include <direct.h>
#include <boost/format.hpp>

#include "ISaver.h"
#include "DLog.h"
#include "MaiTrix.h"
#include "Internals.h"
#include "BasicCubeTypes.h"



using namespace Its;
using namespace DCommon;

using boost::format;


const int LogFileEntryLimit = 900;


class TestException: public BasicException
{
public:
  DEXCEPTION_CONSTRUCTOR0( TestException, BasicException );
  DEXCEPTION_CONSTRUCTOR1( TestException, BasicException );
  DEXCEPTION_CONSTRUCTOR2( TestException, BasicException );
  DEXCEPTION_CONSTRUCTOR3( TestException, BasicException );
};




void DebugCellInfo( const char* cellOrigin1, Cell& cell1, const char* cellOrigin2, Cell& cell2 )
{
  for( int loopDirection = 0; loopDirection < DIRECTIONS; loopDirection++ )
  {
    DLog::msg( DLog::LevelDebug, "For '%s : %s', in direction '%d', cell type = '%3d : %3d', data = '%3d : %3d', uplink = '%3d : %3d'",  
                                  cellOrigin1,  cellOrigin2,
                                  loopDirection,
                                  cell1.type   , cell2.type   ,
                                  cell1.data   , cell2.data   ,
                                  cell1.uplink , cell2.uplink 
                                  );
  }
}


bool CompareCells( Cell* matrixBasic, Cell* matrixCuda, SizeIt matrixSizeX, SizeIt matrixSizeY )
{
  bool resultEqual = true;
  
  for( SizeIt loopY = 0; loopY < matrixSizeY; loopY++ )
  {
    for( SizeIt loopX = 0; loopX < matrixSizeX; loopX++ )
    {
      int loopCell = loopY * matrixSizeX + loopX;
      
      Cell& cellBasic = matrixBasic [ loopCell ];
      Cell& cellCuda  = matrixCuda  [ loopCell ];

      if( !( cellBasic == cellCuda ))
      {
        //DLog::msg( DLog::LevelError, "Cells are different in position '%3d' : '3%d'",  loopX, loopY );
        //DebugCellInfo( "CellBasic", cellBasic, "CellCuda", cellCuda );
        
        resultEqual = false;
        break;
      }
    }
  }
  
  return resultEqual;
}



void FillInputLayer(  ByteIt* dataText, ByteIt* dataAudio, ByteIt* dataVideo, 
                      CapacityIt capacity, SizeIt3D size, bool noInput, const char* matrixName,
                      const char* textPattern, int rf )
{
  memset( dataText  , NOSIGNAL, capacity.sizeText  );
  memset( dataAudio , NOSIGNAL, capacity.sizeAudio );
  memset( dataVideo , NOSIGNAL, capacity.sizeVideo );

  if( !noInput )
  {
    //const char* audioString = "gggggg11111111111111111HHHHHHHHHHHHHHHHrrrrrrrrrrrrRRRRRRRRRRRRRRRRRRR000000000000000000001111111111111gggggg11111111111111111HHHHHHHHHHHHHHHHrrrrrrrrrrrrRRRRRRRRRRRRRRRRRRR000000000000000000001111111111111gggggg11111111111111111HHHHHHHHHHHHHHHHrrrrrrrrrrrrRRRRRRRRRRRRRRRRRRR000000000000000000001111111111111";
    //const char* videoString = "aaaaaaaaAAAAAAAAAAAAAbbbbbbbbbbtttttTTTTTTTTTTyyyyyyyyyyyy00000000000000AAAAAAAAAAAAAAAaaaaaaaaAAAAAAAAAAAAAbbbbbbbbbbtttttTTTTTTTTTTyyyyyyyyyyyy00000000000000AAAAAAAAAAAAAAAaaaaaaaaAAAAAAAAAAAAAbbbbbbbbbbtttttTTTTTTTTTTyyyyyyyyyyyy00000000000000AAAAAAAAAAAAAAA";
    //const char* testString  = "test its some test 123 test its some test 123 test its some test 123";
    //const char* testString  = "                                      pt                s            ";

    string message;


    // 7 Jan 2013 - DMA - Use video layer temporarily as this is top/bottom layer and 
    // now we run maiTrix only in DIRECTION_UP
    //
    //memcpy( dataText, textPattern, dataSize );
    //SizeIt dataSize = min( capacity.sizeText, strlen( textPattern ));
    
    
    SizeIt dataSize = min( capacity.sizeVideo, strlen( textPattern ));
    memcpy( dataVideo, textPattern, dataSize );

    DebugBytesToText( dataVideo, dataSize, message );    

    // Log doesn't show more than 1K
    if( dataSize > LogFileEntryLimit )
    {
      message.resize( LogFileEntryLimit );
    }

    //DLog::msg( DLog::LevelVerbose, "Providing %d chars total, with data '%d', rf = '%d' showing first %d chars of TEXT only input '%s' for MaiTrix '%s'", capacity.sizeText, dataSize, rf, message.size(), message.c_str(), matrixName );
    DLog::msg( DLog::LevelVerbose, "Providing %d chars total, with data '%d', rf = '%d' showing first %d chars of VIDEO only input '%s' for MaiTrix '%s'", capacity.sizeVideo, dataSize, rf, message.size(), message.c_str(), matrixName );
  }
  else
  {
    DLog::msg( DLog::LevelVerbose, "Providing no input data for MaiTrix '%s' with rf = '%d'", matrixName, rf );
  }

}



SizeItl EstimateSins( const char* input )
{
  SizeItl sinsCounted[ ALLSINS ];
  SizeItl estimatedSins = 0;

  for( size_t loopSin = 0; loopSin < ALLSINS; loopSin++ )
  {
    sinsCounted[ loopSin ] = 0;
  }

  for( size_t loopChar = 0; loopChar < strlen( input ); loopChar++ )
  {
    char thisChar = input[ loopChar ];

    if( thisChar < ALLSINS && thisChar > NOSIGNAL )
    {
      sinsCounted[ thisChar ]++;
    }
  }

  // Just count unique ones, no detailed statistics
  for( size_t loopSin = 0; loopSin < ALLSINS; loopSin++ )
  {
    if( sinsCounted[ loopSin ] > 0 )
    {
      estimatedSins++;
    }
  }

  return estimatedSins;
}




void MaiTrixDebugOutput( const char* stage, ByteIt* data, SizeItl dataSize, MaiTrix& matrix, int rf, PeriodIt cycle )
{
  string dataAsText;

  DebugBytesToText( data, dataSize, dataAsText );

  // Do not truncate console out!
  cout << endl << "MaiTrix " << stage << " in cycle " << cycle << " = '"  << dataAsText << "'" << endl;

  // debug buffer is not infinite though ;-)  truncate message to 1K chars!
  if( dataSize > LogFileEntryLimit )
  {
    dataAsText.resize( LogFileEntryLimit );
  }

  DLog::msg( DLog::LevelDebug, "MaiTrix '%s' %s (first %d chars, rf = '%d') in cycle %d = '%s'", 
              matrix.Name(), stage, dataAsText.size(), rf, cycle, dataAsText.c_str() );
}


int GetNormalizedRandom( int random, int range_min, int range_max ) 
{
  float normalizedRandomFloat = ((( float )random / ( RAND_MAX + 1 )) * ( range_max - range_min ) + range_min );
  int   normalizedRandomInt;

  normalizedRandomInt = ( int )ceil( normalizedRandomFloat );

  return normalizedRandomInt;
}



void InputRandom( SizeIt3D size, ByteArray& dataInput, const char start, const char end )
{
  if( start >= end )
  {
    throw TestException( "Can not generate random input for the given range" );
  }

  SizeItl inputSize = size.X * size.Y;
  ByteIt  charRange = end - start;

  memset( dataInput.data(), NOSIGNAL, inputSize );

  for( SizeIt loopSize = 0; loopSize < size.X; loopSize++ )
  {
    SizeIt  median  = size.X / 2;
    char    charOne = GetNormalizedRandom( rand(), start, end );
    char    charTwo = GetNormalizedRandom( rand(), start, end );

    if( loopSize > median - ( median / 2 ) && loopSize < median + ( median / 2 ))
    {
      memset( dataInput.data() + ( loopSize * size.Y ),                  charOne, size.Y / 2 );
      memset( dataInput.data() + ( loopSize * size.Y ) + ( size.Y / 2 ), charTwo, size.Y / 2 );
    }
    else
    {
      memset( dataInput.data() + ( loopSize * size.Y ), NOSIGNAL, size.Y );
    }
  }  
}



void InputLinear( SizeIt3D size, ByteArray& dataInput, bool mirrored, SizeIt width )
{
  SizeItl inputSize = size.X * size.Y;

  assert( size.X > 0 );
  assert( size.Y > 0 );
  assert( size.Y >= width );

  memset( dataInput.data(), NOSIGNAL, inputSize );

  SizeIt shift = ( size.Y - width ) / 2;
  const size_t  patternscount = 2;
  const char    patternChars          [ patternscount ] = { 'A', 'B' };
  const char    reversedPatternChars  [ patternscount ] = { 'B', 'A' };
  const char   *pattern = mirrored ? reversedPatternChars : patternChars;

  for( SizeIt loopSize = 0; loopSize < size.X; loopSize++ )
  {
    SizeIt  median      = size.X / 2;
    char    charPattern = pattern[ loopSize % patternscount ];

    if( loopSize > median - 7 && loopSize < median + 7 )
    {
      memset( dataInput.data() + ( loopSize * size.Y ) + shift, charPattern, width );
    }
  }  
}


void InputDuo( SizeIt3D size, ByteArray& dataInput, char one, char other )
{
  SizeItl inputSize = size.X * size.Y;
  SizeItl midSize   = ( inputSize / 2 ) + ( size.Y / 2 );

  memset( dataInput.data(), NOSIGNAL, inputSize );
  
  dataInput[ midSize + ( size.Y * 0 ) + 0  ] = one;
  dataInput[ midSize + ( size.Y * 1 ) + 10 ] = other;
  /*
  dataInput[ midSize + ( size.Y * 0 ) + 0 ] = one;
  dataInput[ midSize + ( size.Y * 0 ) + 1 ] = other;
  dataInput[ midSize + ( size.Y * 0 ) + 2 ] = 'Z';
  dataInput[ midSize + ( size.Y * 0 ) + 3 ] = other;
  dataInput[ midSize + ( size.Y * 1 ) + 0 ] = one;
  dataInput[ midSize + ( size.Y * 1 ) + 1 ] = 'Z';
  /*dataInput[ midSize + ( size.Y * 1 ) + 2 ] = one;
  dataInput[ midSize + ( size.Y * 1 ) + 3 ] = other;
  dataInput[ midSize + ( size.Y * 2 ) + 0 ] = 'Z';
  dataInput[ midSize + ( size.Y * 2 ) + 1 ] = other;
  dataInput[ midSize + ( size.Y * 2 ) + 2 ] = one;
  dataInput[ midSize + ( size.Y * 2 ) + 3 ] = 'Z';*/
}



/*
.......................
.......................
XXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXX
.......................
.......................
.......................
.......................
YYYYYYYYYYYYYYYYYYYYYYY
YYYYYYYYYYYYYYYYYYYYYYY
YYYYYYYYYYYYYYYYYYYYYYY
.......................
.......................
*/
void InputWideBlocks( SizeIt3D size, ByteArray& dataInput, bool mirrored, int step, 
                      const char one, const char two, SizeIt width )
{
  assert( size.X > 0 );
  assert( size.Y > 0 );
  assert( size.Y >= width );

  if( size.X < 30 )
  {
    throw TestException( "MaiTrix is too small for wide block input" );
  }

  SizeItl inputSize = size.X * size.Y;
  SizeIt  shift = ( size.Y - width ) / 2;

  memset( dataInput.data(), NOSIGNAL, inputSize );

  const char  patternChars          [ 2 ] = { one, two };
  const char  reversedPatternChars  [ 2 ] = { two, one };
  const char* pattern = mirrored ? reversedPatternChars : patternChars;
  
  for( SizeIt loopSize = 0; loopSize < size.X; loopSize++ )
  {
    SizeIt  median = size.X / 2;

    if( loopSize > median - ( 7 * step ) && loopSize <= median - ( 7 * ( step - 1 )))
    {
      memset( dataInput.data() + ( loopSize * size.Y ) + shift, pattern[ 0 ], width );
    }
    else
    {
      if( loopSize > median + ( 7 * ( step - 1 )) && loopSize < median + ( 7 * step ))
      {
        memset( dataInput.data() + ( loopSize * size.Y ) + shift, pattern[ 1 ], width );
      }
    }
  }  
}



/*
.......................
.......................
XXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXX
YYYYYYYYYYYYYYYYYYYYYYY
YYYYYYYYYYYYYYYYYYYYYYY
YYYYYYYYYYYYYYYYYYYYYYY
.......................
.......................
*/
void InputBlocksXY( SizeIt3D size, ByteArray& dataInput, bool mirrored, SizeIt width )
{
  InputWideBlocks( size, dataInput, mirrored, 1, 'X', 'Y', width );
}


void InputBlocksAB( SizeIt3D size, ByteArray& dataInput, bool mirrored, SizeIt width )
{
  InputWideBlocks( size, dataInput, mirrored, 1, 'A', 'B', width );
}


void InputBlocksXB( SizeIt3D size, ByteArray& dataInput, bool mirrored, SizeIt width )
{
  InputWideBlocks( size, dataInput, mirrored, 1, 'X', 'B', width );
}




bool LaunchReflectionMaiTrix( MaiTrix& matrix, ByteArray& dataInput, int gf, int finalGf, PeriodIt cycles, 
                              const char * maiTrixPath, const char * maiTrixDebPath )
{
  clock_t startTime, finishTime;

  CapacityIt capacity = matrix.Capacity();
  SizeIt3D   size     = matrix.Size();

  ByteArray dataTextOut ( capacity.sizeText  );
  ByteArray dataAudioOut( capacity.sizeAudio );
  ByteArray dataVideoOut( capacity.sizeVideo );

  cout << endl << "Launching Reflection MaiTrix '" << matrix.Name() << "'" << endl;
  DLog::msg( DLog::LevelError, "Launching Reflection MaiTrix '%s'",  matrix.Name() );
  startTime = clock();

  Sins  sins;
  Probe probe;

  string message, messageAudio, messageVideo;
  MaiTrixSnapshot snapshot( matrix.Name(), maiTrixPath );
  
  PeriodIt loopCycle = 0;
  SizeItl  inputSize = size.X * size.Y;

  for( loopCycle = 0; loopCycle < cycles; loopCycle++ )
  {
    srand( GetTickCount() );
    int rf = rand(); 
    
    memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
    memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
    
    if( loopCycle == 0 ) 
    {
      memcpy( dataVideoOut.data() , dataInput.data(), inputSize );
    }
    else 
    {
      memset( dataVideoOut.data() , NOSIGNAL, inputSize );
    }

    // Note that 'j' is the only log-unique char! ;-)
    // const char* textPatternMirror = "test one two three";
    // FillInputLayer( dataTextOut.data(), dataAudioOut.data(), dataVideoOut.data(), capacity, size, true, matrix.Name(), textPatternMirror, rf );

    MaiTrixDebugOutput( "input", dataVideoOut.data(), inputSize, matrix, rf, loopCycle );
    cout << endl << "Gf: " << gf << ", Rf: " << rf << endl;
    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' Gf: '%d'", matrix.Name(), gf );

    sins = matrix.Launch( MaiTrix::GF2GF( gf ), rf, dataTextOut.data(), dataAudioOut.data(), dataVideoOut.data(), probe );        
    matrix.Commit();

    MaiTrixDebugOutput( "output", dataVideoOut.data(), inputSize, matrix, rf, loopCycle );

    /*    
    31 Jan 2013 - DMA - General rule:

    1) GF is 0 during the initial delay, ~Z-size, give MaiTrix time to react;
    2) The stable state is having all input and output Sins;
    */
    if( sins.complete || loopCycle == cycles - 1 )
    {
      cout << endl << "Final cycle: " << finalGf << endl;
      matrix.Launch( MaiTrix::GF2GF( finalGf ), rf, dataTextOut.data(), dataAudioOut.data(), dataVideoOut.data(), probe );
      break;
    }

    // Sins summary
    cout << endl << "Sins: " << " response "  << sins.response
                             << " stability " << sins.stability << "%" << endl;

    DLog::msg( DLog::LevelDebug, "MaiTrix '%s' Sins: response %s", matrix.Name(), sins.response.c_str() );

    // do not pollute input with output data!
    memset( dataTextOut .data(), NOSIGNAL, capacity.sizeText  );
    memset( dataAudioOut.data(), NOSIGNAL, capacity.sizeAudio );
    memset( dataVideoOut.data(), NOSIGNAL, capacity.sizeVideo );

    snapshot.Take( maiTrixDebPath );
  }

  finishTime = clock();
  
  cout << "Time: " << finishTime - startTime << endl;
  DLog::msg( DLog::LevelInfo, "Time: %d", finishTime - startTime );

  snapshot.Take( maiTrixDebPath );

  if( sins.complete )
  {
    cout << endl << "MaiTrix complete in cycle " << loopCycle << endl;
    DLog::msg( DLog::LevelDebug, "MaiTrix complete in cycle '%d'", loopCycle );
  }
  else
  {
    cout << endl << "MaiTrix incomplete in final cycle " << loopCycle << endl;
    DLog::msg( DLog::LevelDebug, "MaiTrix incomplete in final cycle '%d'", loopCycle );
  }

  return sins.complete;
}


void SnapshotMaiTrix( const char* maiTrixPath, const char* maiTrixDebPath, const char* maitrixName )
{
  MaiTrixSnapshot snapshot( maitrixName, maiTrixPath );

  SizeIt3D maiTrixSize = snapshot.Take( maiTrixDebPath );
  cout << "Snapshot taken" << endl;
}



void ProcessResultMaiTrix( const char* maiTrixPath, 
                           string&     matrix1Name, string&     matrix2Name,
                           SizeIt3D&   matrix1Size, SizeIt3D&   matrix2Size )
{
  ISaverFile saver( maiTrixPath );
  Celllayer  layer1( matrix1Name.c_str(), SizeIt2D( matrix1Size.X, matrix1Size.Y ), 1, 0 );
  Celllayer  layer2( matrix2Name.c_str(), SizeIt2D( matrix2Size.X, matrix2Size.Y ), 1, 0 );
 
  bool doCompare = matrix1Size == matrix2Size;
  
  DLog::msg( DLog::LevelDebug, "matrix1 size = %d x %d x %d, matrix2 size = %d x %d x %d", 
                                matrix1Size.X, 
                                matrix1Size.Y, 
                                matrix1Size.Z, 
                                matrix2Size.X,
                                matrix2Size.Y,
                                matrix2Size.Z );
  
  while( saver.Exist( layer1 ))
  {
    if( doCompare )
    {
      if( CompareCells( ( Cell* ) layer1, ( Cell* ) layer2, layer1.size.X, layer1.size.Y ))
      {
        cout << "For layer #" << layer1.positionZ << " results are identical."<< endl;
        DLog::msg( DLog::LevelInfo, "For layer %d results are identical.", layer1.positionZ );
      }
      else
      {
        cout << "For layer #" << layer1.positionZ << " results are NOT identical."<< endl;
        DLog::msg( DLog::LevelError, "For layer %d results are NOT identical.", layer1.positionZ );
      }
    }

    layer1.positionZ++;
    layer2.positionZ++;
  }

  cout << endl << "Totals layers = " << layer1.positionZ << " of maiTrix '" << layer1.maiTrixName.c_str() << endl;
}


int _tmain( int argc, _TCHAR* argv[] )
{
  int matrixSizeXY, matrixSizeZ, gf = 0, cycles, pause;
  const char* maiTrixPath    = ".\\dma\\";
  const char* maiTrixDebPath = ".\\deb\\";
  bool snapshotOnly          = false;
  bool runOnCPU              = false;

  if( argc <= 1 )
  {
    cout << endl << "Usage: MaiTrixTest SizeXY [SizeZ] [Cycles] [Snapshot/CPU]" << endl;
    cout << "defaults: SizeZ = " << INITIAL_SIZE_Z << ", Cycles = " 
         << INITIAL_SIZE_Z * 2 << endl;
    exit( 0 );
  }

  matrixSizeXY  = argc > 1 ? atoi( argv[ 1 ] ) : INITIAL_SIZE_X;
  matrixSizeZ   = argc > 2 ? atoi( argv[ 2 ] ) : INITIAL_SIZE_Z;
  cycles        = argc > 3 ? atoi( argv[ 3 ] ) : INITIAL_SIZE_Z * 2;
  snapshotOnly  = argc > 4 ? string( "Snapshot" ) == argv[ 4 ] : false;
  runOnCPU      = argc > 4 ? string( "CPU" )      == argv[ 4 ] : false;

  MaiTrix::MaiTrixOptions maiTrixOptions = ( MaiTrix::MaiTrixOptions )( MaiTrix::CacheLayers | MaiTrix::AutoCorrection );
  
  if( runOnCPU )
  {
    maiTrixOptions = ( MaiTrix::MaiTrixOptions )( maiTrixOptions | MaiTrix::CPUOnly );
  }

  cout << endl << "Running MaiTrix with parameters:" << endl;
  cout << "SizeXY = " << matrixSizeXY << ", SizeZ = " << matrixSizeZ << 
           ", cycles = " << cycles << ", Gf = " << gf << endl;
  cout << "Snapshot Only: " << snapshotOnly << endl;
  cout << "Run On CPU: " << runOnCPU << endl;

  if( _chdir( maiTrixPath ) != 0 )
  {
    _mkdir( maiTrixPath );
  }
  else
  {
    _chdir( "..\\" );
  }

  if( _chdir( maiTrixDebPath ) != 0 )
  {
    _mkdir( maiTrixDebPath );
  }
  else
  {
    _chdir( "..\\" );
  }
    
  string logFilePath( maiTrixDebPath );

  logFilePath += "MaiTrix.log";

  //DLog::init( logFilePath.c_str(), DLog::LevelVerbose );
  //DLog::init( logFilePath.c_str(), DLog::LevelInfo );
  DLog::init( logFilePath.c_str(), DLog::LevelDebug );

  try
  {
    string    matrix1Name( "First"  );
    string    matrix2Name( "Second" );
    SizeIt3D  matrix1Size;
    SizeIt3D  matrix2Size;

    if( cycles < matrixSizeZ )
    {
      DLog::msg( DLog::LevelWarning, "Warning: Running MaiTrix with cycles %d less then needed %d to complete MaiTriX", cycles, matrixSizeZ );
    }

    if( snapshotOnly )
    {
      SnapshotMaiTrix( maiTrixPath, maiTrixDebPath, matrix1Name.c_str() );
      return 0;
    }

    gf = 0;

    DLog::msg( DLog::LevelInfo, "Running MaiTrix test with parameters:" );
    DLog::msg( DLog::LevelInfo, "SizeXY = %d, SizeZ = %d, cycles to go = %d, Gf = %d, snapshotOnly = %d ", matrixSizeXY, matrixSizeZ, cycles, gf, snapshotOnly );
    DLog::msg( DLog::LevelInfo, "Direction = %d ", INITIAL_UPLINK );

    // at least, otherwise text will not cover all the side
    // and we will miss some output as casted to the other types
    SizeIt textCapacity = ( matrixSizeXY * 4 ) * 3; // 3 layers

    MaiTrix matrixFirst( matrix1Name.c_str(), matrixSizeXY, matrixSizeXY, matrixSizeZ, textCapacity, 10, 10, maiTrixPath, maiTrixOptions );
    matrix1Size = matrixFirst.Size();

    ByteArray dataInput( matrix1Size.X * matrix1Size.Y );
    SizeIt pattern_width = 40;// matrix1Size.Y;

    // InputBlocksXY( matrix1Size, dataInput, false, pattern_width );
    InputDuo( matrix1Size, dataInput, 'A', 'Z' );

    LaunchReflectionMaiTrix( matrixFirst, dataInput, gf, -1, cycles, maiTrixPath, maiTrixDebPath );

    /*while( !LaunchReflectionMaiTrix( matrixFirst, dataInput, gf, +1, cycles, maiTrixPath, maiTrixDebPath ))
    {
      DLog::msg( DLog::LevelInfo, "Maitrix is growing" );
      matrixFirst.Grow();
      srand( GetTickCount() );

      // we need to stop eventually? TBD
      cycles--;
      if( cycles <= 1 )
      {
        DLog::msg( DLog::LevelInfo, "Maitrix is growing too much" );
        return 1;
      }
    }*/

    cout << "Block XY complete, Gf +1, pause..."; cin >> pause;
    matrixFirst.Erase();

    InputLinear( matrix1Size, dataInput, false, pattern_width );
    LaunchReflectionMaiTrix( matrixFirst, dataInput, gf, +1, cycles, maiTrixPath, maiTrixDebPath );
    cout << "Linear complete, Gf +1, pause..."; cin >> pause;

    matrixFirst.Erase();

    InputBlocksXY( matrix1Size, dataInput, false, pattern_width );
    LaunchReflectionMaiTrix( matrixFirst, dataInput, gf, -1, cycles, maiTrixPath, maiTrixDebPath );

    matrixFirst.Erase();
    cout << "Block XY complete, Gf -1, pause..."; cin >> pause;

    InputLinear( matrix1Size, dataInput, false, pattern_width );
    LaunchReflectionMaiTrix( matrixFirst, dataInput, gf, -1, cycles, maiTrixPath, maiTrixDebPath );
    cout << "Linear complete, Gf -1, pause..."; cin >> pause;
  }
  catch( exception& x )
  {
    cout << "MaiTrix error:" << x.what() << endl;
    DLog::msg( DLog::LevelError, "MaiTrix error: %s", x.what() );
  }
  catch( ... )
  {
    cout << "MaiTrix general error" << endl;
    DLog::msg( DLog::LevelError, "MaiTrix general error" );
  }

  return 0;
}


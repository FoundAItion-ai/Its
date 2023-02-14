/*******************************************************************************
 FILE         :   AdvancedLabyrinthUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2016

 DESCRIPTION  :   MaiTrix - Advanced Labyrinth unit tests (flexible pre/post conditions)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   06/14/2016

 LAST UPDATE  :   06/14/2016
*******************************************************************************/

#include "stdafx.h"
#include <algorithm>
#include <functional>
#include <list>
#include <set>
#include <stdlib.h>

#include "BaseUnitTest.h"
#include "MaiTrix.h"
#include "Internals.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


extern SizeIt3D smallLabyrinthSize;
extern SizeIt3D normalLabyrinthSize;
extern SizeIt3D wideLabyrinthSize;
extern SizeIt3D shortLabyrinthSize;
extern SizeIt3D largeLabyrinthSize;

const int LABYRINTH_ITERATIONS = 1;


namespace BaseUnitTest
{
class Dot
  {
  public:
    Dot( PointIt _pos ) :
      pos( _pos )
    {
      assert( pos.X >= 0 );
      assert( pos.Y >= 0 );
    }

    PosIt   Pos1D( SizeIt sizeX ) { return ( sizeX * pos.Y ) + pos.X; }
    PointIt Pos2D() { return pos; }

  private:
    PointIt pos;
  };


  class Input
  {
  public:
    static const char basicChar      = 'A';
    static const char basicChar_None = NOSIGNAL;

    virtual ~Input() {}
    virtual void Apply( GF& gf, ByteArray2D& data, PeriodIt cycle ) = 0;
  };


  // ************************************ Input classes ************************************  
  class DotInput : public Input, public Dot
  {
  public:
    DotInput( PointIt _pos, PeriodIt _from, PeriodIt _to ) :
      Dot( _pos ), from( _from ), to( _to )
    {
      assert( from <= to );
    }

    virtual void Apply( GF& gf, ByteArray2D& data, PeriodIt cycle )
    {
      if( cycle >= from && cycle <= to )
      {
        PosIt posOffset = Pos1D( data.size.X );
        assert( posOffset >= 0 );
        assert( static_cast<ByteArray::size_type>( posOffset ) <= data.byteArray.size());
        memset( data.byteArray.data() + posOffset, basicChar, 1 );
      }
    }

  private:
    PeriodIt from;
    PeriodIt to;
  };


  class GfInput : public Input
  {
  public:
    GfInput( GF _gf, PeriodIt _at ) :
      gf( _gf ), at( _at ) {}

    virtual void Apply( GF& _gf, ByteArray2D& data, PeriodIt cycle )
    {
      if( cycle == at )
      {
        _gf = gf;
      }
    }

  private:
    GF       gf;
    PeriodIt at;
  };


  class Inputs : public Input
  {
  public:
    Inputs & operator + ( const DotInput & input )
    {
      inputs.push_back( make_shared<DotInput>( input ));
      return *this;
    }

    Inputs & operator + ( const GfInput & input )
    {
      inputs.push_back( make_shared<GfInput>( input ));
      return *this;
    }

    Inputs & operator + ( const shared_ptr<Input> & input )
    {
      inputs.push_back( input );
      return *this;
    }

    virtual void Apply( GF& gf, ByteArray2D& data, PeriodIt cycle)
    {
      for_each( inputs.begin(), inputs.end(), [ &gf, &data, cycle ]( InputList::value_type & input )
      {
        input->Apply( gf, data, cycle );
      });
    }

  private:
    typedef list<shared_ptr< Input >> InputList;
    InputList inputs;
  };


  class Output : public BaseTest
  {
  public:
    virtual ~Output() {}
    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins ) = 0;
  };


  class PeriodicOutput : public Output
  {
  public:
    PeriodicOutput( PeriodIt _cycleFrom, PeriodIt _cycleTo ) :
      cycleFrom( _cycleFrom ),
      cycleTo( _cycleTo )
    {
      assert( cycleFrom <= cycleTo );
    }

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins ) = 0;

  protected:
    bool Within( PeriodIt cycle )
    {
      return cycle >= cycleFrom && cycle <= cycleTo;
    }

    bool LastCycle( PeriodIt cycle )
    {
      return cycle == cycleTo;
    }

    // pos -> char is unique mapping, not the other way around
    typedef std::map<string::size_type, string::value_type> CharPosition;

    CharPosition ParseStream( const ByteIt * data, int size, ByteIt space )
    {
      CharPosition charPosition;

      for( int pos = 0; pos < size; pos++ )
      {
        ByteIt character = *( data + pos );
        if( ISSIGNAL( character ) && character != space )
        {
          charPosition[ pos ] = character;
        }
      }

      return charPosition;
    }

  private:
    PeriodIt cycleFrom;
    PeriodIt cycleTo;
  };


  // ************************************ Reflection classes ************************************  
  class AnyReflectionOutput : public PeriodicOutput
  {
  public:
    AnyReflectionOutput( PeriodIt _from, PeriodIt _to ) :
      PeriodicOutput( _from, _to ) {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle )) 
      {
        CharPosition charPosition = ParseStream( data.byteArray.data(), data.byteArray.size(), NOSIGNAL );
        if( charPosition.size() == 0 )
        {
          WriteMessage( "In %d cycle anat least one reflection is expected, none found", cycle );
          return false;
        }
      }
      return true;
    }
  };


  class ReflectionOutput : public PeriodicOutput
  {
  public:
    static const char reflectionChar = '\x1';

    // Not capturing reflections as they must be in known positions.
    ReflectionOutput( PeriodIt _from, PeriodIt _to, Dot dot ) :
      PeriodicOutput( _from, _to ),
      dotExpected( dot )
      {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle )) 
      {
        CharPosition charPosition = ParseStream( data.byteArray.data(), data.byteArray.size(), NOSIGNAL );
        PosIt         posExpected = dotExpected.Pos1D( data.size.X );  // 2D => flat pos
        CharPosition::const_iterator foundMapping = charPosition.find( posExpected );

        if( foundMapping == charPosition.end() || reflectionChar != foundMapping->second )
        {
          WriteMessage( "In %d cycle (%d) reflection is expected at %d (%d, %d), but not found", cycle, reflectionChar, posExpected, dotExpected.Pos2D().X, dotExpected.Pos2D().Y );
          return false;
        }
      }
      return true;
    }

  private:
    Dot dotExpected;
  };

  // During the period total number of reflections must be _countTotal
  class ReflectionsTotalOutput : public PeriodicOutput
  {
  public:
    ReflectionsTotalOutput( PeriodIt _from, PeriodIt _to, SizeIt _countTotal ) :
      PeriodicOutput( _from, _to ),
      countTotal( _countTotal )
      {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle )) 
      {
        CharPosition charPosition = ParseStream( data.byteArray.data(), data.byteArray.size(), NOSIGNAL );

        if( charPosition.size() != countTotal )
        {
          WriteMessage( "In %d cycle %d reflection(s) is expected, found: %d", cycle, countTotal, charPosition.size() );
          return false;
        }
      }
      return true;
    }

  private:
    SizeIt countTotal;
  };

  
  // During the period total number of reflections must be within range
  // and increase gradually
  class ReflectionsTotalRangeOutput : public PeriodicOutput
  {
  public:
    ReflectionsTotalRangeOutput( PeriodIt _from, PeriodIt _to, SizeIt _countTotalMin, SizeIt _countTotalMax ) :
      PeriodicOutput( _from, _to ),
      countTotal( 0 ),
      countTotalMin( _countTotalMin ),
      countTotalMax( _countTotalMax )
      {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle )) 
      {
        CharPosition charPosition = ParseStream( data.byteArray.data(), data.byteArray.size(), NOSIGNAL );

        if( charPosition.size() < countTotalMin || charPosition.size() > countTotalMax )
        {
          WriteMessage( "In %d cycle number of reflection(s) is expected to be within %d - %d range, found: %d", cycle, countTotalMin, countTotalMax, charPosition.size() );
          return false;
        }

        if( charPosition.size() < countTotal )
        {
          WriteMessage( "In %d cycle number of reflection(s) is expected to increase gradually from %d to %d range, found: %d after %d", cycle, countTotalMin, countTotalMax, charPosition.size(), countTotal );
          return false;
        }

        countTotal = charPosition.size();
        if( LastCycle( cycle ) && countTotal != countTotalMax )
        {
          WriteMessage( "In the last %d cycle number of reflection(s) is expected to reach maximum %d, found: %d", cycle, countTotalMax, countTotal );
          return false;
        }
      }
      return true;
    }

  private:
    SizeIt countTotal;
    SizeIt countTotalMin;
    SizeIt countTotalMax;
  };


  // ************************************ Response classes ************************************  
  class BaseResponseOutput : public PeriodicOutput
  {
  public:
    typedef PeriodicOutput Base;

    BaseResponseOutput( PeriodIt _from, PeriodIt _to ) :
      PeriodicOutput( _from, _to ) {}

  protected:
    CharPosition ParseStream( Sins sins )
    {
      // if invalid chars are being casted they should not pass validation down the stream
      return Base::ParseStream( reinterpret_cast<const ByteIt*>( sins.response.c_str()), sins.response.size(), responseCharSpace );
    }
  };

  class NoResponseOutput : public BaseResponseOutput
  {
  public:
    NoResponseOutput( PeriodIt _from, PeriodIt _to ) :
      BaseResponseOutput( _from, _to ) {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle ))
      {
        CharPosition charPosition = ParseStream( sins );
        if( charPosition.size() > 0 )
        {
          WriteMessage( "In %d cycle no response is expected, at least one '%c' char found at %d position", cycle, charPosition.begin()->second, charPosition.begin()->first );
          return false;
        }
      }
      return true;
    }
  };

  class AnyResponseOutput : public BaseResponseOutput
  {
  public:
    AnyResponseOutput( PeriodIt _from, PeriodIt _to ) :
      BaseResponseOutput( _from, _to ) {}

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle ) && ParseStream( sins ).size() == 0 )
      {
        WriteMessage( "In %d cycle any response is expected, no one found", cycle );
        return false;
      }
      return true;
    }
  };

  class SingleResponseOutput : public BaseResponseOutput
  {
  public:
    // Capturing response
    SingleResponseOutput( PeriodIt _from, PeriodIt _to, ByteIt * _dataCaptured, PosIt * posCaptured ) :
      BaseResponseOutput( _from, _to ),
      dataCaptured( _dataCaptured ),
      dataExpected( *_dataCaptured ),
      posCaptured( posCaptured ),
      posExpected( *posCaptured )
    {
      assert( dataCaptured != NULL );
      assert( posCaptured != NULL );
    }

    // Verifying response, pass data by reference so we could use capturing and verifying
    // predicates in the same sentence.
    SingleResponseOutput( PeriodIt _from, PeriodIt _to, ByteIt & _dataExpected, PosIt & _posExpected ) :
      BaseResponseOutput( _from, _to ),
      dataCaptured( NULL ),
      dataExpected( _dataExpected ),
      posCaptured( NULL ),
      posExpected( _posExpected )
    {
    }

    // Verifying response, do not check position.
    SingleResponseOutput( PeriodIt _from, PeriodIt _to, ByteIt & _dataExpected ) :
      BaseResponseOutput( _from, _to ),
      dataCaptured( NULL ),
      dataExpected( _dataExpected ),
      posCaptured( NULL ),
      posExpected( posNotExpected )
    {
    }

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      if( Within( cycle ))
      {
        CharPosition charPosition = ParseStream( sins );

        // Singular response expected
        if( charPosition.size() != 1 )
        {
          WriteMessage( "In %d cycle single response is expected, found: %d", cycle, charPosition.size() );
          return false;
        }

        PosIt  pos  = charPosition.begin()->first;
        ByteIt data = charPosition.begin()->second;

        if( dataCaptured && posCaptured )
        {
          *posCaptured  = pos;
          *dataCaptured = data;
        }
        else
        {
          assert( posCaptured  == NULL );
          assert( dataCaptured == NULL );

          if( dataExpected != data || ( posExpected != posNotExpected && posExpected != pos ))
          {
            WriteMessage( "In %d cycle '%c' response is expected at %d, but '%c' found at %d",
              cycle, dataExpected, posExpected, data, pos );
            return false;
          }

          // Extra check as we expect response to be within specific char range
          if( data < responseCharLow || data > responseCharHigh )
          {
            WriteMessage( "In %d cycle single valid response is expected, invalid character '%c' found", cycle, data );
            return false;
          }
        }
      }
      return true;
    }

  private:
    ByteIt * dataCaptured;
    ByteIt & dataExpected;
    PosIt  * posCaptured;
    PosIt  & posExpected;
    static PosIt posNotExpected;
  };

  PosIt SingleResponseOutput::posNotExpected = -1;

  class Outputs : public Output
  {
  public:
    Outputs & operator + ( const AnyReflectionOutput & output )
    {
      outputs.push_back( make_shared<AnyReflectionOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const ReflectionsTotalOutput & output )
    {
      outputs.push_back( make_shared<ReflectionsTotalOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const ReflectionsTotalRangeOutput & output )
    {
      outputs.push_back( make_shared<ReflectionsTotalRangeOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const ReflectionOutput & output )
    {
      outputs.push_back( make_shared<ReflectionOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const NoResponseOutput & output )
    {
      outputs.push_back( make_shared<NoResponseOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const AnyResponseOutput & output )
    {
      outputs.push_back( make_shared<AnyResponseOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const SingleResponseOutput & output )
    {
      outputs.push_back( make_shared<SingleResponseOutput>( output ));
      return *this;
    }

    Outputs & operator + ( const shared_ptr<Output> & output )
    {
      outputs.push_back( output );
      return *this;
    }

    virtual bool Check( ByteArray2D& data, PeriodIt cycle, Sins sins )
    {
      bool result = true;

      for_each( outputs.begin(), outputs.end(), [ &data, cycle, sins, &result ]( OutputList::value_type & output )
      {
        if( !output->Check( data, cycle, sins ))
        {
          result = false;
          return;
        }
      });

      return result;
    }

  private:
    typedef list<shared_ptr< Output >> OutputList;
    OutputList outputs;
  };



  class AdvancedMaiTrixLauncher : public BaseTest
  {  
  public:
    AdvancedMaiTrixLauncher( MaiTrix * _maiTrix, PeriodIt _finishCycle ) :
      maiTrix( _maiTrix ),
      size( maiTrix->Size()),
      finishCycle( _finishCycle )
    {
      Assert::IsTrue( maiTrix != NULL );
      WriteMessage( "AdvancedMaiTrixLauncher %d x %d x %d size", size.X, size.Y, size.Z );
    }

    bool Run( Inputs inputs, Outputs outputs, bool stopOnFail )
    {
      CapacityIt capacity = maiTrix->Capacity();
      bool complete = true;
      Probe probe;
      GF gf;

      // Gravity model is not important in advanced tests
      probe.gravityMode = Probe::ConstantGravity;

      if( probe.gravityMode == Probe::ConstantGravity )
      {
        WriteMessage( "Gravity is constant" );
      }
      else if ( probe.gravityMode == Probe::VariableGravity )
      {
        WriteMessage( "Gravity is variable" );
      }
      else if ( probe.gravityMode == Probe::NoGravity )
      {
        WriteMessage( "No gravity" );
      }
      else
      {
        assert( false );
      }

      MaiTrixSnapshot cleaner( maiTrix );
      cleaner.Clean( ".\\deb\\*.*" );  // ok to fail, just makes more troubles for analysis

      for( PeriodIt loopCycle = 1; loopCycle <= finishCycle; loopCycle++ )
      {
        unsigned int rf = 0;
        Sins  sins;
        assert( rand_s( &rf ) == 0 );

        SizeIt2D    size2D( maiTrix->Size().X, maiTrix->Size().X );
        ByteArray2D dataTextOut  ( capacity.sizeText, size2D  );
        ByteArray2D dataAudioOut ( capacity.sizeAudio, size2D );
        ByteArray2D dataVideoOut ( capacity.sizeVideo, size2D );

        inputs.Apply( gf, dataVideoOut, loopCycle );
        WriteMessage( "Cycle relative:absolute %d:%d, gf %d, rf %u", loopCycle, maiTrix->Cycle(), gf, rf );
        MaiTrixDebugOutput( "input", dataVideoOut );

        sins = maiTrix->Launch( gf,
                                rf, 
                                dataTextOut.byteArray.data(), 
                                dataAudioOut.byteArray.data(), 
                                dataVideoOut.byteArray.data(),
                                probe );
      
        WriteMessage( "MaiTrix response and size %d '%s'", sins.response.size(), sins.response.c_str());
        MaiTrixDebugOutput( "reflection", dataVideoOut );

        MaiTrixSnapshot snapshot( maiTrix );
        snapshot.Reflection( ".\\deb", maiTrix->Cycle() );

        if( !outputs.Check( dataVideoOut, loopCycle, sins ))
        {
          complete = false;
          break;
        }
      }

      if( stopOnFail )
      {
        MaiTrixSnapshot snapshot( maiTrix );
        snapshot.Take( ".\\deb" );
        Assert::IsTrue( complete );
        WriteMessage( "Test is complete" );
      }
      else
      {
        if( complete )
        {
          WriteMessage( "Test is passed" );
        }
        else
        {
          WriteMessage( "Test is failed" );
        }
      }

      return complete;
    }
  
  protected:
   
    virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteArray2D& stream )
    {
      bool noReflections = NoSignal( stream );
      bool noResponse    = sins.response.find_first_not_of( " " ) == string::npos;
      WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());

      // we should see reflections up until size.Z cycle and no reflections after
      if( loopCycle < size.Z )
      {
        if( noReflections )
        {
          WriteMessage( "Reflections ended abruptly" );
          return false;
        }
        if( loopCycle == size.Z - 1 && noResponse )
        {
          WriteMessage( "No response" );
          return false;
        }
      }
      else
      {
        if( !noReflections )
        {
          WriteMessage( "Reflections should not continue" );
          return false;
        }
        if( !noResponse )
        {
          WriteMessage( "Response unexpected" );
          return false;
        }
      }

      return true;
    }

    void MaiTrixDebugOutput( const char* phase, ByteArray2D& stream )
    {
      if( NoSignal( stream ))
      {
        WriteMessage( "No %s", phase );
      }
      else 
      {
        string dataAsText;
        DebugBytesToText( stream.byteArray.data(), stream.byteArray.size(), dataAsText );
        WriteMessage( "MaiTrix data %s: %s", phase, dataAsText.c_str() );
      }
    }

    bool NoSignal( ByteArray2D& stream )
    {
      for( SizeIt loopData = 0; loopData < stream.byteArray.size(); loopData++ )
      {
        if( ISSIGNAL( stream.byteArray[ loopData ]))
        {
          return false;
        }
      }

      return true;
    }

    int CountReflections( ByteIt* data, SizeItl dataSize, SizeItl & firstPos, SizeItl & secondPos )
    {
      assert( data != NULL );
      int reflections = 0;
      firstPos        = 0;
      secondPos       = 0;

      for( SizeItl loopData = 0; loopData < dataSize; loopData++ )
      {
        if( ISSIGNAL( data[ loopData ]))
        {
          reflections++;
          if( reflections == 1 )
          {
            firstPos = loopData;
            continue;
          }
          if( reflections == 2 )
          {
            secondPos = loopData;
            continue;
          }
        }
      }

      return reflections;
    }

    MaiTrix * maiTrix;
    SizeIt3D  size;
    PeriodIt  finishCycle;
  };


  // ************************************ Unit tests ************************************  
  TEST_CLASS(AdvancedLabyrinthUnitTest), BaseTest
  {
  public:
    AdvancedLabyrinthUnitTest()
    {
      BaseTest::WriteMessage( "AdvancedLabyrinthUnitTest" );
    }

    // Increase # of impulses from different points/dots and expect increasing
    // number of reflections
    TEST_METHOD(AdvancedLabyrinthReflectionsGrowth)
    {
      BaseTest::WriteMessage( "Advanced Labyrinth Reflections Growth" );

      SizeIt3D size( normalLabyrinthSize );
      SizeIt   full_capacity( size.X * size.Y );
      ByteIt   response1     = 0;
      PosIt    responsePos1  = 0;
      PeriodIt pauseDuration = 5;
      PeriodIt cycleFinal    = size.Z + ( pauseDuration * 3 );
      PeriodIt cycleBegin1   = pauseDuration;
      PeriodIt cycleBegin2   = cycleBegin1 + pauseDuration;
      PeriodIt cycleEnd1     = cycleBegin1 + size.Z - 2;
      PeriodIt cycleEnd2     = cycleEnd1 + pauseDuration;

      MaiTrix maiTrix( "Advanced", 
        size.X, size.Y, size.Z, full_capacity, full_capacity, full_capacity,
        ".\\dma", MaiTrix::AllInMemory | MaiTrix::CPUOnly /*| MaiTrix::AutoCorrection */);

      AdvancedMaiTrixLauncher launcher( &maiTrix, cycleFinal );

      for( int i = 1; i <= 6; i++ )
      {
        DotInput firstDotInput( PointIt( 2 + ( i % 4 ) , 2 + (( i * 2 ) % 5 )), cycleBegin1, cycleBegin1 );
        std::unique_ptr<SingleResponseOutput> responseCheck;

        if( i == 1 )
        {
          // Capturing
          responseCheck.reset(new SingleResponseOutput( cycleEnd1, cycleEnd1, &response1, &responsePos1 ));
        }
        else 
        {
          // Verifying response remains the same
          responseCheck.reset(new SingleResponseOutput( cycleEnd1, cycleEnd1, response1, responsePos1 ));
        }

        launcher.Run(
          Inputs() +
            GfInput( GF::Base, 1 ) +
            firstDotInput,
          Outputs() + 
            ReflectionsTotalOutput      ( 1             , cycleBegin1 - 1 , 0                 ) +
            ReflectionsTotalRangeOutput ( cycleBegin1   , cycleEnd1       , 1             , i ) +
            ReflectionsTotalOutput      ( cycleEnd1 + 1 , cycleFinal      , 0                 ) +
            ReflectionOutput            ( cycleBegin1   , cycleEnd1       , firstDotInput     ) +
            //
            NoResponseOutput            ( 1             , cycleEnd1 - 1                       ) +
            *responseCheck.get() +
            NoResponseOutput            ( cycleEnd1 + 1 , cycleFinal                          ),
          /*stopOnFail=*/ true
        );

        BaseTest::WriteMessage( "AdvancedLabyrinthReflectionsGrowth: first response %d : %c found at position %d", response1, response1, responsePos1 );
        BaseTest::WriteMessage( "AdvancedLabyrinthReflectionsGrowth: iteration %d complete", i );
      }
    }

    // Send two impulses from different points/dots at different time and confirm
    // they always get the same response & reflection. This also tests for diamond-shape
    // reflection problem, see BasicCube::BuildPlane for details.
    TEST_METHOD(AdvancedLabyrinthDiamondShape)
    {
      BaseTest::WriteMessage( "Advanced Labyrinth Diamond Shape" );

      SizeIt3D size( wideLabyrinthSize );
      SizeIt   full_capacity( size.X * size.Y );
      ByteIt   responseSaved    = 0;
      PosIt    responsePosSaved = 0;
      PeriodIt pauseDuration = 5;
      PeriodIt cycleFinal    = size.Z + ( pauseDuration * 3 );
      PeriodIt cycleBegin1   = pauseDuration;
      PeriodIt cycleBegin2   = cycleBegin1 + pauseDuration;
      PeriodIt cycleEnd1     = cycleBegin1 + size.Z - 2;
      PeriodIt cycleEnd2     = cycleEnd1 + pauseDuration;

      MaiTrix maiTrix( "Advanced", 
        size.X, size.Y, size.Z, full_capacity, full_capacity, full_capacity,
        ".\\dma", MaiTrix::AllInMemory | MaiTrix::CPUOnly /*| MaiTrix::AutoCorrection */);

      AdvancedMaiTrixLauncher launcher( &maiTrix, cycleFinal );
      DotInput firstDotInput  ( PointIt(  2,  2 ), cycleBegin1, cycleBegin1 );
      DotInput secondDotInput ( PointIt( 13, 13 ), cycleBegin2, cycleBegin2 );
      
      for( int i = 1; i <= 1; i++ )
      {
        ByteIt response = 0;
        PosIt  responsePos = 0;

        launcher.Run(
          Inputs() +
            GfInput( GF::Base, 1 ) +
            firstDotInput +
            secondDotInput,
          Outputs() + 
            ReflectionsTotalOutput( 1             , cycleBegin1 - 1 , 0              ) +
            ReflectionsTotalOutput( cycleBegin1   , cycleBegin2 - 1 , 1              ) +
            ReflectionsTotalOutput( cycleBegin2   , cycleEnd2       , 2              ) +
            ReflectionsTotalOutput( cycleEnd2 + 1 , cycleFinal      , 0              ) +
            ReflectionOutput      ( cycleBegin1   , cycleEnd2       , firstDotInput  ) +
            ReflectionOutput      ( cycleBegin2   , cycleEnd2       , secondDotInput ) +
            //            
            NoResponseOutput      ( 1             , cycleEnd1 - 1                              ) +
            SingleResponseOutput  ( cycleEnd1     , cycleEnd1       , &response , &responsePos ) +
            NoResponseOutput      ( cycleEnd1 + 1 , cycleEnd2 - 1                              ) +
            SingleResponseOutput  ( cycleEnd2     , cycleEnd2       , response  , responsePos  ) +
            NoResponseOutput      ( cycleEnd2 + 1 , cycleFinal                                 ),
          /*stopOnFail=*/ true
        );

        if( i == 1 )
        {
          responseSaved = response;
          responsePosSaved = responsePos;
        }
        else
        {
          Assert::IsTrue( response == responseSaved );
          Assert::IsTrue( responsePos == responsePosSaved );
        }

        BaseTest::WriteMessage( "AdvancedLabyrinthDiamondShape: first response %d : %c found at position %d", response, response, responsePos );
        BaseTest::WriteMessage( "AdvancedLabyrinthDiamondShape: iteration %d complete", i );
      }
    }

    TEST_METHOD(AdvancedLabyrinthNegativeFeedback)
    {
      BaseTest::WriteMessage( "Advanced Labyrinth Negative Feedback" );

      SizeIt3D size( shortLabyrinthSize );
      SizeIt   full_capacity( size.X * size.Y );
      PeriodIt pauseDuration = 5;
      PeriodIt cycleFinal    = size.Z + ( pauseDuration * 3 );
      PeriodIt cycleBegin    = pauseDuration;
      PeriodIt cycleEnd      = cycleBegin + size.Z - 2;

      MaiTrix maiTrix( "Advanced", 
        size.X, size.Y, size.Z, full_capacity, full_capacity, full_capacity,
        ".\\dma", MaiTrix::AllInMemory | MaiTrix::CPUOnly );

      AdvancedMaiTrixLauncher launcher( &maiTrix, cycleFinal );
      DotInput firstDotInput( PointIt( 8, 8 ), cycleBegin, cycleBegin );
      GF gf = GF::Base;
      bool result = false;
      set<ByteIt> responses;
      set<PosIt>  positions;

      for( int i = 1; i <= 16; i++ )
      {
        ByteIt response;
        PosIt pos;

        // If needed reset Gf during only one cycle
        Assert::IsTrue( launcher.Run(
          Inputs() +
            GfInput( gf, 1 ) +
            GfInput( GF::Base, 2 ) +
            firstDotInput,
          Outputs() + 
            ReflectionsTotalOutput ( 1            , cycleBegin - 1 , 0                  ) +
            ReflectionsTotalOutput ( cycleBegin   , cycleEnd       , 1                  ) +
            ReflectionsTotalOutput ( cycleEnd + 1 , cycleFinal     , 0                  ) +
            ReflectionOutput       ( cycleBegin   , cycleEnd       , firstDotInput      ) +
            //
            NoResponseOutput       ( 1            , cycleEnd - 1                        ) +
            SingleResponseOutput   ( cycleEnd     , cycleEnd       , &response,   &pos  ) +
            NoResponseOutput       ( cycleEnd + 1 , cycleFinal                          ),
          /*stopOnFail=*/ false ));

        gf = GF::Bad;
        responses.insert( response );
        positions.insert( pos );
        BaseTest::WriteMessage( "AdvancedLabyrinthNegativeFeedbackVariety, response: %d, pos: %d", response, pos );
      }

      MaiTrixSnapshot snapshot( &maiTrix );
      snapshot.Take( ".\\deb" );

      BaseTest::WriteMessage( "AdvancedLabyrinthNegativeFeedbackVariety, found %d distinctive responses and %d positions", responses.size(), positions.size() );
      Assert::IsTrue( responses.size() >= 5 );
      Assert::IsTrue( positions.size() >= 3 ); // narrow choice due to gravity, but there should be some variety
    }

    TEST_METHOD(AdvancedLabyrinthPositiveFeedback)
    {
      BaseTest::WriteMessage( "Advanced Labyrinth Positive Feedback" );

      SizeIt3D size( shortLabyrinthSize );
      SizeIt   full_capacity( size.X * size.Y );
      PeriodIt pauseDuration = 5;
      PeriodIt cycleFinal    = size.Z + ( pauseDuration * 3 );
      PeriodIt cycleBegin    = pauseDuration;
      PeriodIt cycleEnd      = cycleBegin + size.Z - 2;

      MaiTrix maiTrix( "Advanced", 
        size.X, size.Y, size.Z, full_capacity, full_capacity, full_capacity,
        ".\\dma", MaiTrix::AllInMemory | MaiTrix::CPUOnly );

      AdvancedMaiTrixLauncher launcher( &maiTrix, cycleFinal );
      DotInput firstDotInput( PointIt( 8, 8 ), cycleBegin, cycleBegin );
      GF gf = GF::Base;
      bool result = false;
      ByteIt firstResponse;
      PosIt firstPos;

      for( int i = 1; i <= 5; i++ )
      {
        ByteIt response;
        PosIt pos;

        // If needed reset Gf during only one cycle
        Assert::IsTrue( launcher.Run(
          Inputs() +
            GfInput( gf, 1 ) +
            GfInput( GF::Base, 2 ) +
            firstDotInput,
          Outputs() + 
            ReflectionsTotalOutput ( 1            , cycleBegin - 1 , 0                  ) +
            ReflectionsTotalOutput ( cycleBegin   , cycleEnd       , 1                  ) +
            ReflectionsTotalOutput ( cycleEnd + 1 , cycleFinal     , 0                  ) +
            ReflectionOutput       ( cycleBegin   , cycleEnd       , firstDotInput      ) +
            //
            NoResponseOutput       ( 1            , cycleEnd - 1                        ) +
            SingleResponseOutput   ( cycleEnd     , cycleEnd       , &response,   &pos  ) +
            NoResponseOutput       ( cycleEnd + 1 , cycleFinal                          ),
          /*stopOnFail=*/ false ));

        gf = GF::Good;

        if( i == 1 )
        {
          firstResponse = response;
          firstPos = pos;
        }
        else
        {
          Assert::IsTrue( response == firstResponse );
          Assert::IsTrue( pos == firstPos );
        }

        BaseTest::WriteMessage( "AdvancedLabyrinthCleaningComplete, response: %d, firstResponse: %d, pos: %d, firstPos: %d",
          response, firstResponse, pos, firstPos );
      }
    }
  };
}
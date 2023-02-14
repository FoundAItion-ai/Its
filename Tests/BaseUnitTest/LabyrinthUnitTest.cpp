  /*******************************************************************************
 FILE         :   LabyrinthUnitTest.cpp

 COPYRIGHT    :   DMAlex, 2014

 DESCRIPTION  :   MaiTrix - Labyrinth unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   06/19/2014

 LAST UPDATE  :   03/17/2016
*******************************************************************************/

#include "stdafx.h"
#include <functional>
#include <algorithm>
#include <stdlib.h>

#include "BaseUnitTest.h"
#include "MaiTrix.h"
#include "Internals.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


// Size must be aligned to 2
SizeIt3D smallLabyrinthSize  (  6 ,  6 ,  10 );
SizeIt3D squareLabyrinthSize ( 10 , 10 ,  20 );
SizeIt3D normalLabyrinthSize ( 10 , 10 ,  90 );
SizeIt3D wideLabyrinthSize   ( 16 , 16 , 100 );
SizeIt3D shortLabyrinthSize  ( 10 , 10 ,  30 );
SizeIt3D largeLabyrinthSize  ( 30 , 30 ,  70 );

const int LABYRINTH_ITERATIONS = 1;
const int RESPONSE_POSITION_THRESHOLD = 50;

#define NOT_EXPECT( condition, message ) if( condition ) \
  { \
    WriteMessage( message ); \
    return false; \
  }


namespace BaseUnitTest
{
  class MaiTrixLauncher : public BaseTest
  {  
  public:
    static const char basicChar_A     = 'A';
    static const char basicChar_z     = 'z';
    static const char basicChar_None  = 0;
    const float signalPrevalenceLimit = 100.0; // in PrevalentData(), %

    enum Grow_t
    {
      None,
      Top,
      Bottom
    };

    MaiTrixLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle = 0, Probe _probe = Probe()):
      size( _size ),
      capacity( _capacity ), 
      finishCycle( GetFinishCycle( _finishCycle )), 
      probe( _probe )
    {
      WriteMessage( "MaiTrixLauncher %d x %d x %d size, %d finish cycle", size.X, size.Y, size.Z, finishCycle );
    }

    virtual void Run( bool useInclusiveLinks = false )
    {
      MaiTrix maiTrix( "I", size.X, size.Y, size.Z, 
        capacity.sizeText, capacity.sizeAudio, capacity.sizeVideo, 
        "", MaiTrix::AllInMemory | MaiTrix::CPUOnly );
      
      Assert::IsTrue( capacity == maiTrix.Capacity() );

      Sins sins;
      unsigned int rf = 0;      

      SizeItl   inputSize = GetInputSize();
      ByteArray dataInput( inputSize );
      ByteArray dataTextOut ( capacity.sizeText  );
      ByteArray dataAudioOut( capacity.sizeAudio );
      ByteArray dataVideoOut( capacity.sizeVideo );
      PeriodIt  loopCycle = 0;

      SetInputPattern( dataInput );

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
        Assert::IsTrue( false );
      }

      if( useInclusiveLinks )
      {
        probe.testMode = Probe::InclusiveLinks;
        WriteMessage( "Use Inclusive Links" );
      }

      while( true )
      {
        loopCycle++;
        WriteMessage( "LoopCycle %d", loopCycle );

        Assert::IsTrue( rand_s( &rf ) == 0 );

        // All tests should complete before that or fail early
        if( loopCycle == finishCycle )
        {
          WriteMessage( "Test is complete" );
          MaiTrixSnapshot snapshot( &maiTrix );
          snapshot.Take( ".\\deb" );
          break;
        }

        GF gf = Base;
        Grow_t growType = Grow_t::None;
        bool precondition = PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );

        MaiTrixDebugOutput( "input", dataVideoOut.data(), inputSize );

        if( growType ==  Grow_t::Top )
        {
          Assert::IsTrue( maiTrix.GrowTop());
        }
        else if( growType ==  Grow_t::Bottom )
        {
          Assert::IsTrue( maiTrix.GrowBottom());
        }

        sins = maiTrix.Launch( gf, rf, 
                               dataTextOut.data(), 
                               dataAudioOut.data(), 
                               dataVideoOut.data(), 
                               probe );
      
        MaiTrixDebugOutput( "reflection", dataVideoOut.data(), inputSize );

        if( precondition )
        {
          MaiTrixSnapshot snapshot( &maiTrix );
          snapshot.Reflection( ".\\deb", maiTrix.Cycle() );
          //snapshot.Take( ".\\deb" );
        }

        if( !PostLaunchBlock( loopCycle, maiTrix, sins, dataVideoOut.data(), inputSize )) 
        {
          WriteMessage( "Test is incomplete" );
          MaiTrixSnapshot snapshot( &maiTrix );
          snapshot.Take( ".\\deb" );
          Assert::IsTrue( false );
          break;
        }

        memset( dataTextOut .data(), NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data(), NOSIGNAL, capacity.sizeAudio );
        memset( dataVideoOut.data(), NOSIGNAL, capacity.sizeVideo );
      }
    }
  
  protected:
    int GetFinishCycle( int _finishCycle )
    {
      if( _finishCycle == 0 )
      {
        return size.Z * 2;
      }
      
      return _finishCycle;
    }
    
    SizeItl GetInputSize() 
    { 
      return size.X * size.Y; 
    }

    virtual void SetInputPattern( ByteArray& dataInput )
    {
      SizeIt    pattern_width = size.Y / 2;
      WriteMessage( "Launching MaiTrix" );

      InputBlocks( size, dataInput, false, pattern_width );
    }
    
    virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, 
          ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, 
          ByteArray& dataVideoOut, GF& gf, Grow_t& growType )
    {
      // Input in the very first cycle
      if( loopCycle == 1 )
      {
        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
      
        WriteMessage( "Uniform block input" );
      }

      return true;
    }

    virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
    {
      return ValidateResponseAndReflections(  loopCycle, size.Z, sins, data, dataSize );
    }

    virtual bool ValidateResponseAndReflections( PeriodIt loopCycle, PeriodIt lastCycle, Sins& sins, ByteIt* data, SizeItl dataSize )
    {
      bool noReflections = NoReflections( data, dataSize );
      bool noResponse    = sins.response.find_first_not_of( " " ) == string::npos;
      WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());

      // we should see reflections up until size.Z cycle and no reflections after
      if( loopCycle < lastCycle )
      {
        if( noReflections )
        {
          WriteMessage( "Reflections ended abruptly in %d cycle out of %d", loopCycle, lastCycle );
          return false;
        }
        if( loopCycle == lastCycle - 1 && noResponse )
        {
          WriteMessage( "No response in %d cycle out of %d", loopCycle, lastCycle );
          return false;
        }
      }
      else
      {
        if( !noReflections )
        {
          WriteMessage( "Reflections should not continue in %d cycle out of %d", loopCycle, lastCycle );
          return false;
        }
        if( !noResponse )
        {
          WriteMessage( "Response unexpected in %d cycle out of %d", loopCycle, lastCycle );
          return false;
        }
      }

      return true;
    }


    void InputBlocks( SizeIt3D size, ByteArray& dataInput, bool mirrored, SizeIt width )
    {
      InputWideBlocks( size, dataInput, mirrored, 1, basicChar_A, basicChar_z, width );
    }

    void InputWideBlocks( SizeIt3D size, ByteArray& dataInput, bool mirrored, int step, 
                          const char one, const char two, SizeIt width, SizeIt interval = 0 )
    {
      Assert::IsTrue( size.X > 0 );
      Assert::IsTrue( size.Y > 0 );
      Assert::IsTrue( size.Y >= width );

      SizeIt  shift = ( size.Y - width ) / 2;
      memset( dataInput.data(), NOSIGNAL, GetInputSize());

      const char  patternChars          [ 2 ] = { one, two };
      const char  reversedPatternChars  [ 2 ] = { two, one };
      const char* pattern = mirrored ? reversedPatternChars : patternChars;
      SizeIt      median  = size.X / 2;
      SizeIt      range   = ( size.X * 5 ) / 20;
      SizeIt      count   = 0;

      for( SizeIt loopSize = 0; loopSize < size.X; loopSize++ )
      {
        if( loopSize >= median - ( range * step ) && loopSize <= median - ( range * ( step - 1 )) - interval )
        {
          memset( dataInput.data() + ( loopSize * size.Y ) + shift, pattern[ 0 ], width );
          count++;
        }
        else
        {
          if( loopSize > median + ( range * ( step - 1 )) + interval && loopSize < median + ( range * step ))
          {
            memset( dataInput.data() + ( loopSize * size.Y ) + shift, pattern[ 1 ], width );
            count++;
          }
        }
      }

      // To verify bottom level cross pattern we need evenly aligned input pattern
      Assert::IsTrue( count % 2 == 0 );
    }

    void MaiTrixDebugOutput( const char* phase, ByteIt* data, SizeItl dataSize )
    {
      if( NoReflections( data, dataSize ))
      {
        WriteMessage( "No %s", phase );
      }
      else 
      {
        string dataAsText;
        DebugBytesToText( data, dataSize, dataAsText );
        WriteMessage( "MaiTrix data %s: %s", phase, dataAsText.c_str() );
      }
    }

    bool NoReflections( ByteIt* data, SizeItl dataSize )
    {
      Assert::IsTrue( data != NULL );

      for( SizeIt loopData = 0; loopData < dataSize; loopData++ )
      {
        if( ISSIGNAL( data[ loopData ]))
        {
          return false;
        }
      }

      return true;
    }

    int CountReflections( ByteIt* data, SizeItl dataSize, SizeItl & firstPos, SizeItl & secondPos )
    {
      Assert::IsTrue( data != NULL );
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

    ByteIt PrevalentData( ByteIt* data, SizeItl dataSize, bool skipSpaces )
    {
      Assert::IsTrue( data != NULL );
      SizeItl dataSpread[ 255 ] = { 0 };
      SizeItl totalSignalCount = 0;
      SizeItl topSignalCount = 0;
      ByteIt  topSignal = 0;

      for( SizeItl loopData = 0; loopData < dataSize; loopData++ )
      {
        ByteIt signal = data[ loopData ];
        if( ISSIGNAL( signal ))
        {
          if( skipSpaces && signal == 32 )
          {
            continue;
          }
          dataSpread[ signal ]++;
        }
      }

      for( ByteIt loopSpread = 0; loopSpread < ARRAYSIZE( dataSpread ); loopSpread++ )
      {
        SizeItl signalCount = dataSpread[ loopSpread ];
        totalSignalCount += signalCount;

        if( signalCount > topSignalCount )
        {
          topSignalCount = signalCount;
          topSignal = loopSpread;
        }
      }

      Assert::IsTrue( totalSignalCount >= topSignalCount );
      float signalPrevalence = 0;

      if( totalSignalCount > 0 )
      {
        signalPrevalence = ( topSignalCount * 100.0f ) / totalSignalCount;
      }

      WriteMessage( "Dominantce level: %3.2f", signalPrevalence );
      return signalPrevalence >= signalPrevalenceLimit ? topSignal : NOSIGNAL;
    }

    SizeIt3D   size;
    CapacityIt capacity;
    Probe      probe;
    int        finishCycle;
  };


  class MaiTrixLabyrinthLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixLabyrinthLauncher( SizeIt3D _size, CapacityIt _capacity ) :
        MaiTrixLauncher( _size, _capacity ) {}
    
    protected:
      /*virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf ) override
      {
        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
      
        WriteMessage( "Blocks input" );
      }*/
  };


  class MaiTrixUniformityLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixUniformityLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle ) {}
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        SizeIt    pattern_width = size.Y / 3;
        WriteMessage( "Launching MaiTrix, uniform pattern" );

        InputWideBlocks( size, dataInput, false, 1, basicChar_A, basicChar_A, pattern_width );
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // Input in the very first cycle
        if( loopCycle <= 1 )
        {
          memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
          memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
          memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
      
          WriteMessage( "Uniform block input" );
        }

        return false;
      }
  };


  class MaiTrixVariableGravityLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixVariableGravityLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle, Probe _probe ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle, _probe ) {}
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        SizeIt    pattern_width = ( size.Y * 2 ) / 3;
        WriteMessage( "Launching MaiTrix, two symmetrical squares pattern" );

        InputWideBlocks( size, dataInput, false, 2, basicChar_A, basicChar_A, pattern_width, /*interval=*/ 4 );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        if( loopCycle == size.Z - 1 )
        {
          auto sinsTotal = std::count_if( sins.response.begin(), sins.response.end(),
            []( const string::reference & val ) {
              return val != ' ';
            });

          WriteMessage( "MaiTrix response: '%s', sins: %d", sins.response.c_str(), sinsTotal );
          if( sinsTotal <= 1 )
          {
            WriteMessage( "Not enough sins: '%d'", sinsTotal );
            return false;
          }

          // Ideally 2
          if( sinsTotal >= 10 )
          {
            WriteMessage( "Too many sins: '%d'", sinsTotal );
            return false;
          }

          return true;
        }

        return MaiTrixLauncher::PostLaunchBlock( loopCycle, maiTrix, sins, data, dataSize );
      }
  };

  
  class MaiTrixAsymmetryLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixAsymmetryLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle ) {}
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        WriteMessage( "Launching MaiTrix, asymmetrical pattern" );

        SizeItl inputSize = size.X * size.Y;
        SizeIt  width  = 1;
        SizeIt  median = size.X / 2;

        memset( dataInput.data(), NOSIGNAL, inputSize );

        for( SizeIt loopSize = 0; loopSize < size.X; loopSize++ )
        {
          if( loopSize > 1 && loopSize < ( median * 2 ) - 1 )
          {
            memset( dataInput.data() + ( loopSize * size.Y ) + 1, basicChar_A, size.X - 2 );
            //memset( dataInput.data() + ( loopSize * size.Y ) + 1, basicChar_A, width++ );
          }
        }  
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // Input in the very first cycle
        if( loopCycle <= 1 )
        {
          memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
          memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
          memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
      
          WriteMessage( "Asymmetrical block input" );
        }

        return true;
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());
        return true;
      }
  };


  // Simple pattern only one or two dots at the same time
  class MaiTrixDotLauncher : public MaiTrixLauncher
  {
    public:
      typedef MaiTrixLauncher Base;

      MaiTrixDotLauncher( SizeIt3D _size, CapacityIt _capacity, 
          char _charOne = basicChar_A, char _charTwo = basicChar_z, int _finishCycle = 0 ) :
        Base( _size, _capacity, _finishCycle, Probe()),
        charOne( _charOne ),
        charTwo( _charTwo )
        {
          Assert::IsTrue( charOne != basicChar_None );
        }
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // Simple pattern, one dot
        if( loopCycle == 1 )
        {
          DotInput( true, charTwo != basicChar_None, dataInput, dataTextOut, dataAudioOut, dataVideoOut );
        }

        return true;
      }

      virtual void DotInput( bool firstDot, bool secondDot, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut )
      {
        SizeItl inputSize  = GetInputSize();
        PosIt   charOnePos, charTwoPos;

        if( size.X <= 6 )
        {
          charOnePos = inputSize / 5 + ( size.X * 1 );
          charTwoPos = charOnePos + ( size.X * 2 );
        }
        else
        {
          charOnePos = 2 + ( size.X * 2 );
          charTwoPos = 2 + ( size.X * 4 ) + charOnePos;
        }

        Assert::IsTrue( static_cast<SizeItl>( charOnePos ) < inputSize );
        Assert::IsTrue( static_cast<SizeItl>( charTwoPos ) < inputSize );
        // not on the border
        Assert::IsTrue( ( static_cast<SizeItl>( charOnePos ) + 0 ) % size.X != 0 );
        Assert::IsTrue( ( static_cast<SizeItl>( charTwoPos ) + 0 ) % size.X != 0 );
        Assert::IsTrue( ( static_cast<SizeItl>( charOnePos ) + 1 ) % size.X != 0 );
        Assert::IsTrue( ( static_cast<SizeItl>( charTwoPos ) + 1 ) % size.X != 0 );

        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memset( dataVideoOut.data() , NOSIGNAL, inputSize );

        // Simple pattern, one dot
        if( firstDot )
        {
          memset( dataVideoOut.data() + charOnePos, charOne, 1 );
          WriteMessage( "First dot input" );
        }
        
        if( secondDot )
        {
          memset( dataVideoOut.data() + charTwoPos, charTwo, 1 );
          WriteMessage( "Second dot input" );
        }
      }

    private:
      char charOne;
      char charTwo;
  };

  // Test edges - expect no response or reflections
  class MaiTrixSideDotLauncher : public MaiTrixDotLauncher
  {
    public:
      typedef MaiTrixDotLauncher Base;

      MaiTrixSideDotLauncher( SizeIt3D _size, CapacityIt _capacity, int _iteration ) :
        Base( _size, _capacity, basicChar_A, basicChar_None, 0 ),
        iteration( _iteration )
      {
      }
    
    protected:
      virtual void DotInput( bool firstDot, bool secondDot, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut )
      {
        SizeItl inputSize  = GetInputSize();
        PosIt   charOnePos = 1;
        int posSelector    = iteration % 8;


        // Test coverage for sides/edges
        //
        // *    **
        //
        // *     *
        // **    *
        switch( posSelector )
        {
          case 0: charOnePos = 0; break;
          case 1: charOnePos = 1; break;
          case 2: charOnePos = size.X - 1; break;
          case 3: charOnePos = size.X; break;
          case 4: charOnePos = ( size.X * 2 ) - 1; break;
          case 5: charOnePos = ( size.X * size.Y ) - 2; break;
          case 6: charOnePos = ( size.X * size.Y ) - 1; break;
          case 7: charOnePos = ( size.X * ( size.Y - 1 )); break;
        }

        Assert::IsTrue( static_cast<SizeItl>( charOnePos ) < inputSize );
        Assert::IsTrue( secondDot == false );

        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memset( dataVideoOut.data() , NOSIGNAL, inputSize );

        // Simple pattern, one dot
        memset( dataVideoOut.data() + charOnePos, basicChar_A, 1 );
        WriteMessage( "First on the border dot input: %d", posSelector );
      }

    /*
    //
    // 2 Nov 2017 - DMA - Borders are being included in IO now, so we expect
    //                    reflections and reponse
    //
    bool ValidateResponseAndReflections( PeriodIt loopCycle, PeriodIt lastCycle, Sins& sins, ByteIt* data, SizeItl dataSize )
    {
      bool noReflections = NoReflections( data, dataSize );
      bool noResponse    = sins.response.find_first_not_of( " " ) == string::npos;
      WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());

      // we should not see any reflections or response
      if( !noReflections )
      {
        WriteMessage( "Reflections unexpected in %d cycle out of %d", loopCycle, lastCycle );
        return false;
      }

      if( !noResponse )
      {
        WriteMessage( "Response unexpected in %d cycle out of %d", loopCycle, lastCycle );
        return false;
      }

      return true;
    }
    */

   private:
    int iteration;
  };

  // Test layer next to edges - expect response and reflections
  class MaiTrixInsideDotLauncher : public MaiTrixDotLauncher
  {
    public:
      typedef MaiTrixDotLauncher Base;

      MaiTrixInsideDotLauncher( SizeIt3D _size, CapacityIt _capacity, int _iteration ) :
        Base( _size, _capacity, basicChar_A, basicChar_None, 0 ),
        iteration( _iteration )
      {
      }
    
    protected:
      virtual void DotInput( bool firstDot, bool secondDot, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut )
      {
        SizeItl inputSize  = GetInputSize();
        PosIt   charOnePos = 1;
        int posSelector    = iteration % 8;


        // Test coverage for inside (next to edges)
        //
        // 000000000
        // 0*    **0
        // 0       0
        // 0*     *0
        // 0**    *0
        // 000000000
        switch( posSelector )
        {
          case 0: charOnePos = size.X + 1; break;
          case 1: charOnePos = size.X + 2; break;
          case 2: charOnePos = ( size.X * 2 ) - 2; break;
          case 3: charOnePos = ( size.X * 2 ) + 1; break;
          case 4: charOnePos = ( size.X * 3 ) - 2; break;
          case 5: charOnePos = ( size.X * ( size.Y - 1 ) - 2 ); break;
          case 6: charOnePos = ( size.X * ( size.Y - 1 ) - 3 ); break;
          case 7: charOnePos = ( size.X * ( size.Y - 2 ) + 1 ); break;
        }

        Assert::IsTrue( static_cast<SizeItl>( charOnePos ) < inputSize );
        Assert::IsTrue( secondDot == false );

        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memset( dataVideoOut.data() , NOSIGNAL, inputSize );

        // Simple pattern, one dot
        memset( dataVideoOut.data() + charOnePos, basicChar_A, 1 );
        WriteMessage( "First inside dot input: %d", posSelector );
      }

   private:
    int iteration;
  };


  class MaiTrixPositiveFeedbackLauncher : public MaiTrixDotLauncher
  {
    public:
      typedef MaiTrixDotLauncher Base;

      MaiTrixPositiveFeedbackLauncher( SizeIt3D _size, CapacityIt _capacity ) :
        Base( _size, _capacity, basicChar_A, basicChar_z, /*finishCycle=*/ _size.Z * 2 + 1 )
        {
        }
    
    protected:
      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        if( loopCycle == size.Z )
        {
          gf = Its::GF::Good;
        }

        // Check if everything ok after positive feedback
        if( loopCycle == size.Z + 1 )
        {
          DotInput( true, true, dataInput, dataTextOut, dataAudioOut, dataVideoOut );
          return true;
        }

        return Base::PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        bool beforeFinalBaseCycle = loopCycle == size.Z - 2 ? true : false;
        bool finalBaseCycle = loopCycle == size.Z - 1 ? true : false;
        bool firstGoodCycle = loopCycle == size.Z ? true : false;
        bool finalCycle     = loopCycle == size.Z * 2 ? true : false;

        if( !finalBaseCycle && !firstGoodCycle && !beforeFinalBaseCycle && !finalCycle )
        {
          return ValidateResponseAndReflections(  loopCycle, finishCycle, sins, data, dataSize );
        }

        MaiTrixSnapshot snapshot( &maiTrix );
        CubeStat stat;
        snapshot.CollectStatistics( &stat );
        WriteMessage( "MaiTrix stat, good-bad-dead-data: '%d-%d-%d-%d'", stat.cellsGood, stat.cellsBad, stat.cellsDead, stat.cellsWithData );

        if( finalCycle )
        {
          if( stat.cellsGood < size.Z - 1 || stat.cellsBad > 0 || stat.cellsDead > 0 || stat.cellsNo < size.X * size.Y )
          {
            WriteMessage( "Not valid cells found in MaiTrix in final cycle" );
            return false;
          }

          WriteMessage( "Final cycle passed" );
          return true;
        }

        if( beforeFinalBaseCycle )
        {
          if( stat.cellsWithData < 1 )
          {
            WriteMessage( "No cells with data found in MaiTrix before final base cycle" );
            return false;
          }

          return true;
        }
        
        if( finalBaseCycle )
        {
          if( stat.cellsGood > 0 || stat.cellsBad < size.Z - 2 || stat.cellsDead > 0 || stat.cellsNo < size.X * size.Y )
          {
            WriteMessage( "Not valid cells found in MaiTrix in final base cycle" );
            return false;
          }

          return true;
        }
        
        if( firstGoodCycle )
        {
          if( stat.cellsGood > 0 || stat.cellsBad > 0 || stat.cellsDead > 0 || stat.cellsNo < size.X * size.Y )
          {
            WriteMessage( "Not valid cells found in MaiTrix in first good cycle" );
            return false;
          }

          if( stat.cellsWithData > 0 )
          {
            WriteMessage( "Cells with data found in MaiTrix in first good cycle" );
            return false;
          }
        }

        return true;
      }
  };


  class MaiTrixTemporalSymmetryauncher : public MaiTrixDotLauncher
  {
    public:
      typedef MaiTrixDotLauncher Base;

      MaiTrixTemporalSymmetryauncher( SizeIt3D _size, CapacityIt _capacity ) :
        Base( _size, _capacity, basicChar_A, basicChar_None, 0 )
      {
      }
    
    protected:
      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // One dot, sent again
        if( loopCycle == 1 || /*loopCycle == 2 || */loopCycle == 3 || loopCycle == 5 )
        {
          DotInput( true, false, dataInput, dataTextOut, dataAudioOut, dataVideoOut );
        }

        return true;
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());
        return true;
      }

  };

  // 1) Send both one and two for delay cycles, expect both back
  // 2) Send nothing for delay cycles
  // 3) Send the first one for delay cycles, expect both back
  // 4) Send nothing for delay cycles
  // 5) Send the second one for delay cycles, expect both back
  class MaiTrixDotReflectionLauncher : public MaiTrixLauncher
  {
    public:
      typedef MaiTrixLauncher Base;

      MaiTrixDotReflectionLauncher( SizeIt3D _size, CapacityIt _capacity,  char _charOne = basicChar_A, char _charTwo = basicChar_z ) :
        Base( _size, _capacity, /* finishFactor= */_size.Z * 6 ), charOne( _charOne ), charTwo( _charTwo ), 
        phaseDuration( size.Z ), firstPos( 0 ), secondPos( 0 ), secondReflectionDelay( -1 ), responseAveragePos( 0 )
        {
          Assert::IsTrue( charOne != basicChar_None && charTwo != basicChar_None );
        }
    
      virtual void Run()
      {
        Base::Run( /*useInclusiveLinks=*/ true );

        // There should be three responses per iteration
        responseAveragePos /= 3;
        if( responseAveragePos > RESPONSE_POSITION_THRESHOLD )
        {
          WriteMessage( "Average response position %d exceeds threshold", responseAveragePos, RESPONSE_POSITION_THRESHOLD );
          Assert::IsTrue( false );
        }
        else
        {
          WriteMessage( "Average response position %d", responseAveragePos );
        }
      }

    protected:
      bool InPhase( PeriodIt loopCycle, int phase )
      {
        PeriodIt from = phaseDuration * ( phase - 1 );
        PeriodIt to = phaseDuration * phase;
        return loopCycle > from && loopCycle < to;
      }

      bool InPhaseBeginning( PeriodIt loopCycle, int phase )
      {
        return PhaseDelay( loopCycle, phase ) == 0;
      }

      bool InPhaseEnding( PeriodIt loopCycle, int phase )
      {
        return loopCycle == ( phaseDuration * phase ) - 1;
      }

      long PhaseDelay( PeriodIt loopCycle, int phase )
      {
        // Can overflow
        return ( long ) loopCycle - (( long ) phaseDuration * ( phase - 1 ) + 1 );
      }
      

      virtual void SetInputPattern( ByteArray& dataInput )
      {
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        SizeItl inputSize  = GetInputSize();
        PosIt   charOnePos = inputSize / 7 + ( size.X * 1 );
        PosIt   charTwoPos = charOnePos + ( size.X * 5 );

        memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
        memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
        memset( dataVideoOut.data() , NOSIGNAL, inputSize );

        if( InPhase( loopCycle, 2 ) || InPhase( loopCycle, 4 )) 
        {
          return true;
        }

        if( InPhaseBeginning( loopCycle, 1 ) || InPhaseBeginning( loopCycle, 3 ))
        {
          memset( dataVideoOut.data() + charOnePos, charOne, 1 );
          WriteMessage( "First dot input at %d position", charOnePos );
        }

        if( InPhaseBeginning( loopCycle, 1 ) || InPhaseBeginning( loopCycle, 5 ))
        {
          memset( dataVideoOut.data() + charTwoPos, charTwo, 1 );
          WriteMessage( "Second dot input at %d position", charTwoPos );
        }

        return true;
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        SizeItl first, second;
        int  reflections      = CountReflections( data, dataSize, first, second );
        string::size_type pos = sins.response.find_first_not_of( " " );
        bool noResponse       = pos == string::npos;
        bool multiResponse    = false;

        if( !noResponse )
        {
          multiResponse = sins.response.find_first_not_of( " ", pos + 1 ) != string::npos;
        }

        WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());

        if( InPhaseEnding( loopCycle, 1 ) || InPhaseEnding( loopCycle, 3 ) || InPhaseEnding( loopCycle, 5 ))
        {
          NOT_EXPECT( noResponse, "Response not found" );
          NOT_EXPECT( multiResponse, "Multiple response found" );
          responseAveragePos += pos;
          char response = sins.response.at( pos );
          WriteMessage( "MaiTrix single response: '%s'", string( 1, response ).c_str());
          WriteMessage( "MaiTrix %d reflection(s) at %d and %d", reflections, first, second );

          if( InPhaseEnding( loopCycle, 1 ))
          {
            firstResponse = response;
          }
          else
          {
            NOT_EXPECT( firstResponse != response, "Different response found" );
          }
        }

        if( InPhase( loopCycle, 1 ))
        {
          NOT_EXPECT( reflections != 2, "Unexpected reflections count" );
          if( InPhaseBeginning( loopCycle, 1 ))
          {
            firstPos = first;
            secondPos = second;
            secondReflectionDelay = -1;
            responseAveragePos = 0;
          }
          else
          {
            NOT_EXPECT( firstPos != first || secondPos != second, "Reflections are moving" );
          }
        }

        if( InPhase( loopCycle, 2 ) || InPhase( loopCycle, 4 )) 
        {
          NOT_EXPECT( reflections != 0, "Reflections should not continue" );
          NOT_EXPECT( !noResponse,      "Response unexpected" );
        }

        // Second reflection must begin at some point, but we don't know when.
        if( InPhase( loopCycle, 3 ) || InPhase( loopCycle, 5 )) 
        {
          if( InPhaseBeginning( loopCycle, 3 ) || InPhaseBeginning( loopCycle, 5 ))
          {
            foundSecondReflection = false;
          }
          
          if( reflections < 1 || reflections > 2 )
          {
            WriteMessage( "Should be one or two reflections" );
            return false;
          }

          if( reflections == 2 )
          {
            foundSecondReflection = true;
            if( secondReflectionDelay == -1 )
            {
              if( InPhase( loopCycle, 3 )) 
              {
                secondReflectionDelay = PhaseDelay( loopCycle, 3 );
              }
            }
            else
            {
              if( InPhase( loopCycle, 5 )) 
              {
                NOT_EXPECT( secondReflectionDelay != PhaseDelay( loopCycle, 5 ), "Delay is different" );
                secondReflectionDelay = -1;
              }
            }
          }

          if( foundSecondReflection )
          {
            NOT_EXPECT( reflections != 2, "Second reflection ended unexpectedly" );
            NOT_EXPECT( firstPos != first || secondPos != second, "Reflections are moving" );
          } 
          else
          {
            // CountReflections returns one reflection as 'first', pointing ether to the first
            // or second signal, depending on the phase
            if( InPhase( loopCycle, 3 ) && first != firstPos )
            {
              WriteMessage( "First reflection #1 is moving from %d to %d", firstPos, first );
              return false;
            }

            if( InPhase( loopCycle, 5 ) && first != secondPos )
            {
              WriteMessage( "Second reflection #2 is moving from %d to %d", secondPos, first );
              return false;
            }
          }
        }

        return true;
      }

    private:
      bool     foundSecondReflection;
      char     charOne;
      char     charTwo;
      char     firstResponse;
      PeriodIt secondReflectionDelay;
      PeriodIt phaseDuration;
      SizeItl  firstPos;   // reflection #1
      SizeItl  secondPos;  // reflection #2
      string::size_type responseAveragePos;
  };


  class MaiTrixReflectionsLauncher : public MaiTrixLauncher
  {
    public:
      const static PeriodIt StableInputEndCycle = 10;

      MaiTrixReflectionsLauncher( SizeIt3D _size, CapacityIt _capacity, Probe _probe, PeriodIt _inputEndCycle ) :
        MaiTrixLauncher( _size, _capacity, /* finishCycle= */0, _probe ), inputEndCycle( _inputEndCycle ) {}
    
    protected:
      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // Need to repeat to avoid testing pitfall with 2 outputs, first A, then double z
        if( loopCycle >= 1 && loopCycle <= inputEndCycle )
        {
          memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
          memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
          memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
      
          WriteMessage( "Blocks input" );
        }

        return true;
      }
    
    virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
    {
      bool noReflections = NoReflections( data, dataSize );
      bool noResponse    = sins.response.find_first_not_of( " " ) == string::npos;
      WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());

      // we should see reflections up until size.Z + inputEndCycle cycle and no reflections after
      if( loopCycle < size.Z - 1 + inputEndCycle )
      {
        if( noReflections )
        {
          WriteMessage( "Reflections ended abruptly" );
          return false;
        }
        if( loopCycle >= size.Z - 1 && loopCycle < size.Z - 1 + inputEndCycle && noResponse )
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


    private:
      PeriodIt inputEndCycle;
  };


  class MaiTrixFocusLauncher : public MaiTrixUniformityLauncher
  {
    public:
      typedef MaiTrixUniformityLauncher Base;

      MaiTrixFocusLauncher( SizeIt3D _size, CapacityIt _capacity, Probe _probe ) :
        MaiTrixUniformityLauncher( _size, _capacity, _size.Z + 10 ) 
        {
          // Probability spread 30 / 30 / 40
          // probe.divertLow  = 30;
          // probe.divertHigh = 60;
        }

    protected:
      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        Base::PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );
        return false;
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        string::size_type posFirst = sins.response.find_first_not_of( " " );
        string::size_type posLast = sins.response.find_last_not_of( " " );

        if( posFirst != string::npos && posFirst > 33 )
        {
          WriteMessage( "MaiTrix inaccurate response at: '%d'", posFirst );
          return false;
        }

        if( posFirst != posLast )
        {
          WriteMessage( "MaiTrix double response at: '%d', '%d'", posFirst, posLast );
          return false;
        }

        return Base::PostLaunchBlock( loopCycle, maiTrix, sins, data, dataSize );
      }
  };

  
  class MaiTrixDominanceLauncher : public MaiTrixLauncher
  {
    public:
      typedef MaiTrixLauncher Base;

      MaiTrixDominanceLauncher( SizeIt3D _size, CapacityIt _capacity ) :
        Base( _size, _capacity )
        {}

    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        SizeIt    pattern_width = size.Y - 4;
        WriteMessage( "Launching MaiTrix, wide uniform pattern" );

        Assert::IsTrue( pattern_width > 0 );
        InputWideBlocks( size, dataInput, false, 1, basicChar_A, basicChar_z, pattern_width );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        ByteIt dominantReflection = PrevalentData( data, dataSize, /* skipSpaces= */false );
        ByteIt dominantResponse   = PrevalentData( ( ByteIt* )sins.response.c_str(), sins.response.size(), /* skipSpaces= */true );

        if( loopCycle < size.Z )
        {
          // Only expect '1' as reflections, no other signals and signalPrevalenceLimit is 100%
          Assert::IsTrue( dominantReflection == SIGNAL );
          string dominantText;
          DebugBytesToText( &dominantReflection, 1, dominantText );
          WriteMessage( "MaiTrix dominant signal is '%s' in cycle %d", dominantText.c_str(), loopCycle );
          
          if( loopCycle == size.Z - 1 )
          {
            WriteMessage( "MaiTrix response: '%s'", sins.response.c_str());
            WriteMessage( "MaiTrix dominant response code: '%d'", dominantResponse );
            Assert::IsTrue( dominantResponse >= responseCharLow && dominantResponse <= responseCharHigh );
          }
        } 
        else
        {
          Assert::IsTrue( dominantReflection == NOSIGNAL );
        }

        return Base::PostLaunchBlock( loopCycle, maiTrix, sins, data, dataSize );
      }
  };
  
  class MaiTrixTestModeLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixTestModeLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle, Probe _probe ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle, _probe ) {}
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        SizeIt    pattern_width = size.Y - 4;
        WriteMessage( "Launching MaiTrix, wide uniform pattern" );

        Assert::IsTrue( pattern_width > 0 );
        InputWideBlocks( size, dataInput, false, 1, basicChar_A, basicChar_z, pattern_width );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        if( loopCycle == size.Z - 1 )
        {
          MaiTrixSnapshot snapshot( &maiTrix );
          CubeStat stat;
          snapshot.CollectStatistics( &stat );
          WriteMessage( "MaiTrix stat, good-bad-dead: '%d-%d-%d'", stat.cellsGood, stat.cellsBad, stat.cellsDead );

          if( stat.cellsDead < 10 )
          {
            WriteMessage( "Not enough dead cells found in MaiTrix" );
            return false;
          }

          return true;
        }

        return MaiTrixLauncher::PostLaunchBlock( loopCycle, maiTrix, sins, data, dataSize );
      }
  };


  class MaiTrixGrowMoreLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixGrowMoreLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle )
      {
      }
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        WriteMessage( "Launching MaiTrix, random input pattern" );
        memset( dataInput.data(), NOSIGNAL, GetInputSize() );
        unsigned int random = 0;

        for( SizeItl loopPos = 0; loopPos < GetInputSize(); loopPos++ )
        {
          Assert::IsTrue( rand_s( &random ) == 0 );
          if( random % 13 == 0 )
          {
            memset( dataInput.data() + loopPos, basicChar_A, 1 );
          }
        }
        //memset( dataInput.data() + size.Y * 3 + ( size.Y / 2 ), basicChar_A, 1 );
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        if( loopCycle % size.Z == 0 )
        {
          growType = Grow_t::Bottom;
          WriteMessage( "Repeating random input pattern" );
          memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
          memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
          memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
        }

        return MaiTrixLauncher::PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        // Searching for crash, no extra checks
        return true;
      }
  };

  class MaiTrixGrowLauncher : public MaiTrixLauncher
  {
    public:
      MaiTrixGrowLauncher( Grow_t _growType, SizeIt3D _size, CapacityIt _capacity, int _finishCycle ) :
        MaiTrixLauncher( _size, _capacity, _finishCycle ),
        growType( _growType )
      {
        Assert::IsFalse( growType == Grow_t::None );
      }
    
    protected:
      virtual void SetInputPattern( ByteArray& dataInput )
      {
        WriteMessage( "Launching MaiTrix, all available input pattern" );

        memset( dataInput.data(), basicChar_A, GetInputSize());
      }

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& _growType ) override
      {
        if( loopCycle == size.Z )
        {
          _growType = growType;
          WriteMessage( "Repeating all available input pattern" );
          memset( dataTextOut.data()  , NOSIGNAL, capacity.sizeText  );
          memset( dataAudioOut.data() , NOSIGNAL, capacity.sizeAudio );
          memcpy( dataVideoOut.data() , dataInput.data(), GetInputSize() );
        }

        return MaiTrixLauncher::PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        if( loopCycle == size.Z - 1 || loopCycle == size.Z * 2 )
        {
          MaiTrixSnapshot snapshot( &maiTrix );
          MaiTrixSnapshot::LayeredStat layeredStat;
          snapshot.CollectStatistics( &layeredStat );

          for( SizeItl layerPos = 0; layerPos < layeredStat.size(); layerPos++ )
          {
            auto stat = layeredStat[ layerPos ];
            WriteMessage( "MaiTrix layered stat for %d, good-bad-dead: '%d-%d-%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead );
          }

          if( loopCycle == size.Z - 1 )
          {
            savedLayeredStat = layeredStat;
          }
          else
          {
            for( SizeItl layerPos = 0; layerPos < layeredStat.size(); layerPos++ )
            {
              auto stat = layeredStat[ layerPos ];

              if( layerPos == 0 || layerPos == layeredStat.size() - 1 )
              {
                if( !( stat.cellsGood == 0 && stat.cellsBad == 0 && stat.cellsDead == 0 ))
                {
                  WriteMessage( "MaiTrix cover stat mismatch for %d, good-bad-dead: '%d-%d-%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead );
                  return false;
                }
                continue;
              }
              
              if( layerPos == 1 )
              {
                if( stat.cellsGood != GetInputSize())
                {
                  WriteMessage( "MaiTrix cover IO size mismatch for %d, good-bad-dead: '%d-%d-%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead );
                  return false;
                }
                continue;
              }

              if( growType == Grow_t::Bottom )
              {
                if( !( stat.cellsGood > 0 && stat.cellsBad == 0 && stat.cellsDead == 0 ))
                {
                  WriteMessage( "MaiTrix bottom stat mismatch for %d, good-bad-dead: '%d-%d-%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead );
                  return false;
                }
                continue;
              }

              // savedLayeredStat.size() == layeredStat.size() - 1 due to growth
              auto savedStat = savedLayeredStat[ layerPos ];

              if( layerPos == layeredStat.size() - 2 )
              {
                if( stat.cellsGood != 0 || stat.cellsBad == 0 || stat.cellsDead > 0 )
                {
                  WriteMessage( "MaiTrix layered stat mismatch for %d, good-bad-dead: '%d-%d-%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead );
                  return false;
                }
                continue;
              }
              
              if( savedStat.cellsBad != stat.cellsGood || stat.cellsBad > 0 || stat.cellsDead > 0 )
              {
                WriteMessage( "MaiTrix layered stat mismatch for %d, good-bad-dead: '%d-%d-%d', saved '%d'", layerPos, stat.cellsGood, stat.cellsBad, stat.cellsDead, savedStat.cellsBad );
                return false;
              }
            }
          }
        
          return true;
        }

        return true;
      }
      
    private:
      MaiTrixSnapshot::LayeredStat savedLayeredStat;
      Grow_t growType;
  };

  class MaiTrixNegativeFeedbackLauncher : public MaiTrixDotLauncher
  {
    public:
      typedef MaiTrixDotLauncher Base;

      MaiTrixNegativeFeedbackLauncher( SizeIt3D _size, CapacityIt _capacity, int _finishCycle ) :
        MaiTrixDotLauncher( _size, _capacity, basicChar_A, basicChar_z, _finishCycle ),
        mergeLayer( -1 )
      {
      }
    
    protected:
      typedef MaiTrixSnapshot::LayeredStat::size_type LayerSize;
      static const int AnyCount = -1;

      virtual bool PreLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, ByteArray& dataInput, ByteArray& dataTextOut, ByteArray& dataAudioOut, ByteArray& dataVideoOut, GF& gf, Grow_t& growType ) override
      {
        // Second and third pass - 1 dot
        if( loopCycle == size.Z + 2 || loopCycle == 2 * size.Z + 3 )
        {
          DotInput( /*firstDot=*/true, /*secondDot=*/false, dataInput, dataTextOut, dataAudioOut, dataVideoOut );
          return true;
        }

        // After first pass - reinforcement
        if( loopCycle == size.Z + 1 )
        {
          WriteMessage( "Path reinforcement at %d cycle", loopCycle );
          gf = Its::GF::Good;
        }

        // After second pass - correction
        if( loopCycle == 2 * size.Z + 2 )
        {
          WriteMessage( "Path correction at %d cycle", loopCycle );
          gf = Its::GF::Bad;
        }

        // First pass - 2 dots
        return Base::PreLaunchBlock( loopCycle, maiTrix, dataInput, dataTextOut, dataAudioOut, dataVideoOut, gf, growType );
      }

      virtual bool PostLaunchBlock( PeriodIt loopCycle, MaiTrix& maiTrix, Sins& sins, ByteIt* data, SizeItl dataSize )
      {
        if( loopCycle == size.Z || loopCycle == 2 * size.Z + 1 || loopCycle == 3 * size.Z + 2 )
        {
          MaiTrixSnapshot snapshot( &maiTrix );
          MaiTrixSnapshot::LayeredStat layeredStat;

          snapshot.CollectStatistics( &layeredStat );
          Assert::IsTrue( layeredStat.size() > 0 );

          LayerSize lastLayerNumber = layeredStat.size() - 1;
          CubeStat & firstLayer = layeredStat[ 0 ];
          CubeStat & lastLayer  = layeredStat[ lastLayerNumber ];
          Range range;
          
          if( firstLayer.cellsBad > 0 || firstLayer.cellsGood > 0 || firstLayer.cellsDead > 0 ||
              firstLayer.cellsIOa + firstLayer.cellsIOv + firstLayer.cellsIOt == 0 )
          {
            WriteMessage( "Found invalid cells in first layer" );
            return false;
          }

          if( lastLayer.cellsBad > 0 || lastLayer.cellsGood > 0 || lastLayer.cellsDead > 0 ||
              lastLayer.cellsSin == 0 )
          {
            WriteMessage( "Found invalid cells in last layer" );
            return false;
          }

          if( SearchRange( layeredStat, DEAD_CELL, AnyCount, 0, lastLayerNumber + 1, range ))
          {
            WriteMessage( "Found dead cells" );
            return false;
          }

          // 2nd & 3rd passes, 1 dot
          if( loopCycle == 2 * size.Z + 1 || loopCycle == 3 * size.Z + 2 )
          {
            Range badAnyRangeBeforeMerge;
            Range bad1RangeAfterMerge,   bad2RangeAfterMerge, badAnyRangeAfterMerge;
            Range good1RangeBeforeMerge, good2RangeBeforeMerge;
            Range good1RangeAfterMerge,  good2RangeAfterMerge;

            bool foundBadAnyBeforeMerge = SearchRange( layeredStat, BAD_CELL,  AnyCount, 0, mergeLayer, badAnyRangeBeforeMerge );
            bool foundGood1BeforeMerge  = SearchRange( layeredStat, GOOD_CELL, 1, 0, mergeLayer, good1RangeBeforeMerge );
            bool foundGood2BeforeMerge  = SearchRange( layeredStat, GOOD_CELL, 2, 0, mergeLayer, good2RangeBeforeMerge );

            bool foundBad1AfterMerge    = SearchRange( layeredStat, BAD_CELL,  1, mergeLayer, lastLayerNumber + 1, bad1RangeAfterMerge  );
            bool foundBad2AfterMerge    = SearchRange( layeredStat, BAD_CELL,  2, mergeLayer, lastLayerNumber + 1, bad2RangeAfterMerge  );
            bool foundBadAnyAfterMerge  = SearchRange( layeredStat, BAD_CELL,  AnyCount, mergeLayer, lastLayerNumber + 1, badAnyRangeAfterMerge );
            bool foundGood1AfterMerge   = SearchRange( layeredStat, GOOD_CELL, 1, mergeLayer, lastLayerNumber + 1, good1RangeAfterMerge );
            bool foundGood2AfterMerge   = SearchRange( layeredStat, GOOD_CELL, 2, mergeLayer, lastLayerNumber + 1, good2RangeAfterMerge );

            //
            // Till the merging point we have single good cells.
            //
            // Alternatives after the merge:
            //
            // 1) signal is following exactly the same path
            //    (one BAD_CELL in the middle, single GOOD_CELLs after)
            // 2) signal is choosing new path
            //    (single BAD_CELLs after)
            // 3) signal is diverging but merging back later on
            //    (several single BAD_CELLs in the middle, single good -> single bed -> ... -> single good cells)
            //
            if( foundGood2BeforeMerge )
            {
              WriteMessage( "Found misplaced good cells couple %d - %d before merge",
                good2RangeBeforeMerge.from, good2RangeBeforeMerge.to );
              return false;
            }

            if( foundBadAnyBeforeMerge )
            {
              WriteMessage( "Found any bad cells %d - %d before merge",
                badAnyRangeBeforeMerge.from, badAnyRangeBeforeMerge.to );
              return false;
            }

            if( !foundGood1BeforeMerge )
            {
              WriteMessage( "Not found single good cells %d - %d before merge",
                good1RangeBeforeMerge.from, good1RangeBeforeMerge.to );
              return false;
            }

            if( good1RangeBeforeMerge.from != 1 || good1RangeBeforeMerge.to != mergeLayer - 1 )
            {
              WriteMessage( "Invalid range of single good cells %d - %d before merge",
                good1RangeBeforeMerge.from, good1RangeBeforeMerge.to );
              return false;
            }

            if( foundGood2AfterMerge )
            {
              WriteMessage( "Found good cells couples %d - %d after merge",
                good2RangeAfterMerge.from, good2RangeAfterMerge.to );
              return false;
            }

            if( !foundBadAnyAfterMerge )
            {
              WriteMessage( "Not found any bad cells %d - %d after merge",
                badAnyRangeAfterMerge.from, badAnyRangeAfterMerge.to );
              return false;
            }

            if( !foundBad1AfterMerge )
            {
              WriteMessage( "Not found single bad cells %d - %d after merge",
                bad1RangeAfterMerge.from, bad1RangeAfterMerge.to );
              return false;
            }

            if( bad1RangeAfterMerge.from != badAnyRangeAfterMerge.from || bad1RangeAfterMerge.to != badAnyRangeAfterMerge.to )
            {
              WriteMessage( "Bad single/any range mismatch %d - %d / %d - %d after merge",
                bad1RangeAfterMerge.from, bad1RangeAfterMerge.to, badAnyRangeAfterMerge.from, badAnyRangeAfterMerge.to );
              return false;
            }

            if( foundBad2AfterMerge )
            {
              WriteMessage( "Found double bad cells %d - %d after merge",
                bad2RangeAfterMerge.from, bad2RangeAfterMerge.to );
              return false;
            }

            if( badAnyRangeAfterMerge.from != mergeLayer )
            {
              WriteMessage( "Invalid bad cells range beginning %d after merge",
                badAnyRangeAfterMerge.from );
              return false;
            }

            bool diverging = true;

            if( foundGood1AfterMerge )
            {
              if( bad1RangeAfterMerge.to + 1 != good1RangeAfterMerge.from || good1RangeAfterMerge.to != lastLayerNumber - 1 )
              {
                WriteMessage( "Good/bad range mismatch %d / %d - %d after merge",
                  badAnyRangeAfterMerge.to, good1RangeAfterMerge.from, good1RangeAfterMerge.to );
                return false;
              }

              if( bad1RangeAfterMerge.from == bad1RangeAfterMerge.to )
              {
                diverging = false;
                WriteMessage( "Signal is following exactly the same path" );
              }
            }
            else
            {
              diverging = false;
              WriteMessage( "Signal is choosing new path" );
            }

            if( diverging )
            {
              WriteMessage( "Signal is diverging but merging back later on at %d", good1RangeAfterMerge.from );
              if( !foundGood1AfterMerge || bad1RangeAfterMerge.from < mergeLayer ||
                  bad1RangeAfterMerge.to >= good1RangeAfterMerge.from )
              {
                WriteMessage( "Good/bad ranges mismatch %d / %d - %d after merge while diverging",
                  good1RangeAfterMerge.from, bad1RangeAfterMerge.from, bad1RangeAfterMerge.to );
                return false;
              }
            }

            WriteMessage( "Pass is verified" );
            return true;
          }

          // 1st pass, 2 dots
          //
          // continouos range
          Range bad1Range, bad2Range;

          if( SearchRange( layeredStat, GOOD_CELL, AnyCount, 0, lastLayerNumber + 1, range ) ||
              SearchRange( layeredStat, DEAD_CELL, AnyCount, 0, lastLayerNumber + 1, range ))
          {
            WriteMessage( "Found at least one range of good and/or dead cells %d - %d", range.from, range.to );
            return false;
          }

          if( !SearchRange( layeredStat, BAD_CELL, 1, 0, lastLayerNumber + 1, bad1Range ) ||
              !SearchRange( layeredStat, BAD_CELL, 2, 0, lastLayerNumber + 1, bad2Range ))
          {
            WriteMessage( "Can not find bad cells" );
            return false;
          }

          if( bad2Range.from != 1 || bad2Range.to + 1 != bad1Range.from || bad1Range.to != lastLayerNumber - 1 )
          {
            WriteMessage( "Invalid bad cells range two cells %d-%d, one %d-%d", bad2Range.from, bad2Range.to, bad1Range.from, bad1Range.to );
            return false;
          }

          WriteMessage( "Dots merged at %d layer", bad1Range.from );
          mergeLayer = bad1Range.from;
          return true;
        }

        if( loopCycle > size.Z * 2 + 2 )
        {
          return ValidateResponseAndReflections( loopCycle, size.Z * 3 + 2, sins, data, dataSize );
        }

        if( loopCycle > size.Z + 1 )
        {
          return ValidateResponseAndReflections( loopCycle, size.Z * 2 + 2, sins, data, dataSize );
        }

        return MaiTrixLauncher::PostLaunchBlock( loopCycle, maiTrix, sins, data, dataSize );
      }

  private:
    struct Range
    {
      LayerSize from;
      LayerSize to;
    };

    bool SearchRange( MaiTrixSnapshot::LayeredStat & layeredStat, ByteIt type, LayerSize count, LayerSize posFrom, LayerSize posTo, Range & range )
    {
      if( posFrom < 0 || posFrom > layeredStat.size() ||
          posTo < 0 || posTo > layeredStat.size() ||
          posFrom > posTo )
      {
        return false;
      }

      if( type != GOOD_CELL && type != BAD_CELL && type != DEAD_CELL )
      {
        return false;
      }

      bool rangeFound = false;
      range.from = range.to = 0;

      for( LayerSize layerNumber = posFrom; layerNumber < posTo; layerNumber++ )
      {
        CubeStat stat = layeredStat[ layerNumber ];
        bool typeCountFound = false;

        switch( type )
        {
          case GOOD_CELL: typeCountFound = ( count == AnyCount && stat.cellsGood > 0 ) || stat.cellsGood == count ? true : false; break;
          case BAD_CELL:  typeCountFound = ( count == AnyCount && stat.cellsBad  > 0 ) || stat.cellsBad  == count ? true : false; break;
          case DEAD_CELL: typeCountFound = ( count == AnyCount && stat.cellsDead > 0 ) || stat.cellsDead == count ? true : false; break;
          default: Assert::IsTrue( false );
        }

        if( rangeFound )
        {
          if( typeCountFound )
          {
            range.to = layerNumber;
          }
          else
          {
            break;
          }
        }
        else
        {
          if( typeCountFound )
          {
            rangeFound = true;
            range.from = range.to = layerNumber;
          }
        }
      }

      return rangeFound;
    }

    LayerSize mergeLayer;
  };
  

  class BaseLabyrinthTest
  {
  public:
    BaseLabyrinthTest( SizeIt3D _size = normalLabyrinthSize ) :
      size( _size )
    {
      SizeIt full_capacity = size.X * size.Y;

      capacity.sizeText  = full_capacity;
      capacity.sizeAudio = full_capacity;
      capacity.sizeVideo = full_capacity;
    }

  protected:
    void Test( const char *testName, int testIterations, 
      function<void ( SizeIt3D size, CapacityIt capacity )> test )
    {
      BaseTest::WriteMessage( "Labyrinth %s", testName );

      try{
        for( int iteration = 1; iteration <= testIterations; iteration++ )
        {
          BaseTest::WriteMessage( "Iteration: %d", iteration );
          test( size, capacity );
        }
      }
      catch( exception& x )
      {
        BaseTest::WriteMessage( "Problem in Labyrinth %s: %s", testName, x.what());
        Assert::IsTrue( false );
      }
    }

    void Test( const char *testName, int testIterations, 
      function<void ( SizeIt3D size, CapacityIt capacity, int iteration )> test, int unused )
    {
      BaseTest::WriteMessage( "Labyrinth %s", testName );

      try{
        for( int iteration = 1; iteration <= testIterations; iteration++ )
        {
          BaseTest::WriteMessage( "Iteration: %d", iteration );
          test( size, capacity, iteration );
        }
      }
      catch( exception& x )
      {
        BaseTest::WriteMessage( "Problem in Labyrinth %s: %s", testName, x.what());
        Assert::IsTrue( false );
      }
    }

    SizeIt3D   size;
    CapacityIt capacity;
  };


  TEST_CLASS(LabyrinthUnitTest), BaseLabyrinthTest, BaseTest
  {
  public:  
    /*
    Assumptions:

    1) Create in memory maiTrix => created.
    2) Provide continuous input pattern, launch => stable results, sins.complete = true.
    3) Provide the same input pattern impulsively => expect waves:

    -reflection level 0, identical to the pattern;
    -reflection level 1, very close to the pattern;
    -reflection level 2, close to the pattern;
    ...
    -reflection level XX-1, similar to the pattern;
    -reflection level XX, somewhat similar to the pattern;

    */
    LabyrinthUnitTest()
    {
      BaseTest::WriteMessage( "LabyrinthUnitTest" );
    }

    TEST_METHOD(Labyrinth)
    {
      Test( "Base", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixLabyrinthLauncher launcher(size, capacity);
        launcher.Run();        
      });
    }

    TEST_METHOD(LabyrinthUniformity)
    {
      Test( "Uniformity", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixUniformityLauncher launcher( size, capacity, 0 );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthSingleDot)
    {
      Test( "Single Dot", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixDotLauncher launcher( size, capacity, MaiTrixLauncher::basicChar_A, MaiTrixLauncher::basicChar_None );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthTwoDots)
    {
      Test( "Two Dots", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixDotLauncher launcher( size, capacity, MaiTrixLauncher::basicChar_A, MaiTrixLauncher::basicChar_A );
        launcher.Run();
      });
    }
    
    TEST_METHOD(LabyrinthTwoReflectedDots)
    {
      Test( "Two Reflected Dots", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixDotReflectionLauncher launcher( size, capacity, MaiTrixLauncher::basicChar_A, MaiTrixLauncher::basicChar_A );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthOfReflections)
    {
      Test( "Reflections", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        Probe probe;
        MaiTrixReflectionsLauncher launcher( size, capacity, probe, MaiTrixReflectionsLauncher::StableInputEndCycle );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthDominance)
    {
      Test( "Dominance", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixDominanceLauncher launcher( size, capacity );
        launcher.Run();
      });
    }   

    TEST_METHOD(LabyrinthTestMode)
    {
      Test( "LabyrinthTestMode", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        Probe probe;
        probe.testMode = Probe::DeadCells;
        MaiTrixTestModeLauncher launcher( size, capacity, /*finishCycle=*/ 0, probe );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthGrowTop)
    {
      Test( "LabyrinthGrowTop", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixGrowLauncher launcher( MaiTrixLauncher::Grow_t::Top, size, capacity, /*finishCycle=*/ size.Z * 2 + 1 );
        launcher.Run();
      });
    }    

    TEST_METHOD(LabyrinthGrowBottom)
    {
      Test( "LabyrinthGrowBottom", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixGrowLauncher launcher( MaiTrixLauncher::Grow_t::Bottom, size, capacity, /*finishCycle=*/ size.Z * 2 + 1 );
        launcher.Run();
      });
    }    


    TEST_METHOD(LabyrinthGrowMore)
    {
      Test( "LabyrinthGrowMore", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixGrowMoreLauncher launcher( size, capacity, /*finishCycle=*/ size.Z * 4 );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthNegativeFeedback)
    {
      Test( "LabyrinthNegativeFeedback", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixNegativeFeedbackLauncher launcher( size, capacity, /*finishCycle=*/ size.Z * 3 + 4 );
        launcher.Run();
      });
    }
  };


  TEST_CLASS(LabyrinthUnitTestSmall), BaseLabyrinthTest, BaseTest
  {
  public:
    LabyrinthUnitTestSmall() : BaseLabyrinthTest( smallLabyrinthSize )
    {
      BaseTest::WriteMessage( "LabyrinthUnitTestSmall" );
    }

    TEST_METHOD(LabyrinthSingleSmallDots)
    {
      Test( "Single Small Dots", /* iterations = */30, [] ( SizeIt3D size, CapacityIt capacity )
      {
        // For labyrinth that small we need response to cover all output layer
        MaiTrixDotLauncher launcher( size, capacity, MaiTrixLauncher::basicChar_A,
          MaiTrixLauncher::basicChar_None );
        launcher.Run();
      });

      Test( "Single Small On The Border Dots", /* iterations = */40, [] ( SizeIt3D size, CapacityIt capacity, int iteration )
      {
        MaiTrixSideDotLauncher launcher( size, capacity, iteration );
        launcher.Run();
      }, /*unused=*/0 );

      Test( "Single Inside Border Dots", /* iterations = */40, [] ( SizeIt3D size, CapacityIt capacity, int iteration )
      {
        MaiTrixInsideDotLauncher launcher( size, capacity, iteration );
        launcher.Run();
      }, /*unused=*/0 );      
    }    

    TEST_METHOD(LabyrinthPositiveFeedback)
    {
      Test( "Positive Feedback", /* iterations = */1, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixPositiveFeedbackLauncher launcher( size, capacity );
        launcher.Run();
      });
    }
  };


  TEST_CLASS(LabyrinthUnitTestLarge), BaseLabyrinthTest, BaseTest
  {
  public:
    LabyrinthUnitTestLarge() : BaseLabyrinthTest( largeLabyrinthSize )
    {
      BaseTest::WriteMessage( "LabyrinthUnitTestLarge" );
    }

    TEST_METHOD(LabyrinthFocus)
    {
      Test( "Focus", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        Probe probe;
        MaiTrixFocusLauncher launcher( size, capacity, probe );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthVariableGravity)
    {
      Test( "Variable Gravity", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        Probe probe;
        probe.gravityMode = Probe::VariableGravity;
        MaiTrixVariableGravityLauncher launcher( size, capacity, /*finishCycle=*/ 0, probe );
        launcher.Run();
      });
    }
  };

  TEST_CLASS(LabyrinthUnitTestSquare), BaseLabyrinthTest, BaseTest
  {
  public:
    LabyrinthUnitTestSquare() : BaseLabyrinthTest( squareLabyrinthSize )
    {
      BaseTest::WriteMessage( "LabyrinthUnitTestSquare" );
    }

    TEST_METHOD(LabyrinthAsymmetry)
    {
      Test( "Asymmetry", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixAsymmetryLauncher launcher( size, capacity, size.Z );
        launcher.Run();
      });
    }

    TEST_METHOD(LabyrinthTemporalSymmetry)
    {
      Test( "TemporalSymmetry", LABYRINTH_ITERATIONS, [] ( SizeIt3D size, CapacityIt capacity )
      {
        MaiTrixTemporalSymmetryauncher launcher( size, capacity );
        launcher.Run();
      });
    }
  };
}
/*******************************************************************************
 FILE         :   PatternAnalyzer.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   MaiTrix unit testing - stream pattern analyzer

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   05/25/2012

 LAST UPDATE  :   05/25/2012
*******************************************************************************/


#ifndef __PATTERNANALYZER_H
#define __PATTERNANALYZER_H


#include "IFeeder.h"


using namespace std;
using namespace DCommon;


namespace Its
{



//
// UnitTest subsystem exception  (Should we create Face subsystem exception and merge with it?)
//
//
class UnitTestException: public BasicException
{
public:
  DEXCEPTION_CONSTRUCTOR0( UnitTestException, BasicException );
  DEXCEPTION_CONSTRUCTOR1( UnitTestException, BasicException );
  DEXCEPTION_CONSTRUCTOR2( UnitTestException, BasicException );
  DEXCEPTION_CONSTRUCTOR3( UnitTestException, BasicException );
};



/*

  PatternAnalyzer  - class to find any regularities in byte streams (text only so far)

  Defines: 
  
  Stream slice    - all stream output at any given moment, snapshot of one tick
  Pitch           - non degenerate stream slice, with bytes != 0 && NOSIGNAL

  Single pitch traits:

  a) symbolic     - yes / no / mixed, all ASCII chars?      Samples: a b c | \x12 \x200 | a b \x12
  b) width        - number of meaningful chars,             Samples: a b c \x12 = 4
  c) regularity   - yes / no, all symbols are the same      Samples: a a a | a b c
                              (no matter ASCII or not)? 
  d) recurrency   - yes / no, contains recurrent pattern?   Samples: ab ab ab | ab abc
                            first "word" sets pattern

  e) content      - text, can be filtered                   Samples: a b c | a ** c


  Multiple pitch traits:

  a) period       - ticks between similar pitches (similar in terms of single pitch traits above)   Samples: 12
  b) repetitions  - number of similar pitches                                                       Samples: 5
  c) identity     - pitches similarity in terms of the single pitch traits above,                   Samples: 12%  (100% for the same content only!)


  Algorithm:

  1) Take first pitch as a sample;

  2) Find pitches similar to the first pitch; Start with 100% identity and stop when 
     reach repetitions limit (= in stream repetitions) or ticks limit ( 2-3 * maiTrix Size Z )

  3) Do validity/sanity check - run PatternAnalyzer against patterns created with PatternGenerator
     and check if those are the same?


  PatternGenerator - class to generate simple patterns for unit testing

  1 pitch considered as 1 repetition too!!
*/


struct PitchTraits
{
  enum Symbolic_t
  {
    Empty, // delimeters only
    Chars,
    NonChars,
    Mixed
  };

  Symbolic_t  symbolic;
  SizeIt      width;
  //bool        regular;
  //bool        recurrent;
};



enum Identity_t
{
  Content   = 0, // 100% identical on char level, all non chars = '*', so this is not 100% "byte level identical"
  Width     = 1, // Same number of chars (not delimeters)
  CharTypes = 2, // Same types - Char or Non or Mixed (good idea or????)
  Present   = 3, // Just any pitch will do

  // not identity, just counter
  MaxIdentityLevel = Present + 1
};


//
// Simple pattern description for impulse patterns like this:
// 
// Empty Frame, Empty Frame, ... Empty Frame, PatternedFrame, Empty Frame, PatternedFrame, Empty Frame, PatternedFrame, Empty Frame ... Empty Frame
//
struct ImpulseFramesPattern
{
  ImpulseFramesPattern( Identity_t _identity ):
    start( 0 ),
    period( 0 ),
    count( 0 ),
    identity( _identity ),
    established( true )
  {
  }

  //PeriodIt duration;      // Total pattern duration
  PeriodIt    start;        // Tick of the first pitch after start sending / receiving patternized frames to maiTrix
  PeriodIt    period;       // Period between impulses of patternized frames being sent to / received from maiTrix
  PeriodIt    count;        // Number of patternized frames repetitions, well...
                            // Rather "number" then "repetitions" as 1 pitch considered as 1 repetition too!
  Identity_t  identity;     // 0-100%, should normally be 100% when sending (all frames are the same!) and 
                            // when analyzing our generated pattern should also be 100%!!
  bool        established;  // true if pattern occurs during the period of the analysis
};


//
// DO NOT USE this general frame pattern to simplify
// correlation analysis between generated and analysed patterns.
// For now use ImpulseFramesPattern.
//
struct GeneralFramesPattern
{
  // 0 - Empty Frame
  // 1 - Frame pattern type 1
  typedef int PatternType;

  auto_ptr< PatternType > pattern;
};


//
// Unit testing logic:
//
// 1) For a variety of different pitch traits (loop) and
// 2) For a variety of different patterns (loop, impulse patterns only for now)
// 3) Test maiTrix with a representative set of RFs  (well, like a 100 out of 1000000000 ;-)
//    and find pattern correlations
//
//
// Then we can either use all those classes in a separate Unit test module
//
// or 
//
// Subclass IFeeder to create IFeederPatternText and send patterns in LoadFrame() and analyze output in SaveFrame()
//
class PatternGenerator
{
public:
  PatternGenerator( SizeIt _frameSize, PeriodIt _maxDuration ):
    frameSize   ( _frameSize   ),
    maxDuration ( _maxDuration )
  {
    if( maxDuration == 0 || frameSize == 0 )
    {
      throw UnitTestException( DException::Error, "Pattern can not be generated for zero duration %d or frame size %d", _maxDuration, _frameSize );
    }
  }

  // 
  // As we can create a lot of patterns / traits we limit the amount with index which establishes
  // a hash: unique index - more or less unique pattern / trait
  //
  virtual ImpulseFramesPattern  CreateImpulsePattern( SizeIt index );  
  virtual PitchTraits           CreateTraits( SizeIt index );  

  virtual IFrame CreatePatternFrame( PitchTraits traits ) = 0;  
  virtual IFrame CreateEmptyFrame() = 0;  

protected:
  SizeIt FrameSize() { return frameSize; }

private:
  SizeIt   frameSize;
  PeriodIt maxDuration;
};



// Move generators into another header/file?
class TextPatternGenerator: public PatternGenerator
{
public:
  typedef PatternGenerator Base;

  TextPatternGenerator( SizeIt _frameSize, PeriodIt _maxDuration ):
    Base( _frameSize, _maxDuration )
  {
  }

  virtual IFrame CreatePatternFrame( PitchTraits traits );  
  virtual IFrame CreateEmptyFrame();  
};



class TextPatternAnalyzer
{
public:
  TextPatternAnalyzer( const char* name, SizeIt pitchSize, PeriodIt ticksLimit, SizeIt countLimit /*, bool useFiltering */ );

  // Returns true when analysis is complete
  virtual bool AnalyzePitch( IFrame& pitch, bool continueWhenPatternFound );

  // Returns results of the analysis, best pattern found, ready when AnalyzePitch() returns true
  virtual ImpulseFramesPattern GetPattern();

  // Output currently recorder view to stream
  // When MaxIdentityLevel output everything (raw view)
  //
  virtual void OutputView( ostream& os, Identity_t identity );

  // like a header
  virtual void OutputView( ostream& os, const char* name, ImpulseFramesPattern* pattern );

  // Helper function, just for naming
  static const char* IdentityToString( Identity_t identity );


protected:
  // No copy contructurs needed?
  struct PitchInfo
  {
    PitchInfo():      
      plainView( "" )
    {
      traits.symbolic = PitchTraits::Empty;
      traits.width    = 0;
    }

    PitchTraits traits;
    string      plainView; // like '    abc   * *d * **tta  '
    PeriodIt    tick;
  };

  //
  // Useful for results verification:
  //
  typedef list<string> PatternPlainView;

  virtual PitchInfo ParsePitch( IFrame& pitch );
  virtual void      RecordPitch( PitchInfo& pitchInfo, Identity_t level );
  virtual bool      PatternSearchComplete();

  PatternPlainView*     GetViewbyIdentity    ( Identity_t identity );
  ImpulseFramesPattern* GetPatternbyIdentity ( Identity_t identity );

private:
  PitchInfo baseLine;     // first pitch to compare
  PeriodIt  tick;         // current, the one is being analyzed
  PeriodIt  ticksLimit;
  SizeIt    countLimit;   // repetitions, recognized pitches
  string    analyzerName; // just for debugging

  //
  // Results, identified patterns:
  //
  ImpulseFramesPattern patternLevel0; // Content   = 0, 
  ImpulseFramesPattern patternLevel1; // Width     = 1, 
  ImpulseFramesPattern patternLevel2; // CharTypes = 2, 
  ImpulseFramesPattern patternLevel3; // Present   = 3  

  PatternPlainView viewRaw;    // everything passed to AnalyzePitch (with '*' substituted, not raw-raw ;-)
  PatternPlainView viewLevel0; // identified frames matching identity level Content
  PatternPlainView viewLevel1; // -- " -- " -- Width    
  PatternPlainView viewLevel2; // -- " -- " -- CharTypes
  PatternPlainView viewLevel3; // -- " -- " -- Present   
};



}; // namespace Its


#endif // __PATTERNANALYZER_H

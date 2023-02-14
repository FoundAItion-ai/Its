/*******************************************************************************
 FILE         :   PatternAnalyzer.cpp

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   MaiTrix unit testing - stream pattern analyzer

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   05/25/2012

 LAST UPDATE  :   05/25/2012
*******************************************************************************/


#include "stdafx.h"

#include <boost/format.hpp>
#include "PatternAnalyzer.h"


using namespace std;
using namespace DCommon;
using namespace Its;

using boost::format;


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
ImpulseFramesPattern PatternGenerator::CreateImpulsePattern( SizeIt index )
{
  ImpulseFramesPattern pattern( Its::Content );

  pattern.start  = index % maxDuration; // can be 0, 1, 2, ... maxDuration
  pattern.period = index % maxDuration; // can be 0, 1, 2, ... maxDuration 
                                        // period is 0 for 1 repetition

  if( pattern.period == 0 )
  {
    pattern.count = 1; // single pitch, same for period 1 and repetitions 1!!!
  }
  else
  {
    pattern.count = min( pattern.period + 1, maxDuration / pattern.period ); 
  }

  // count = 1, 2, 3 ... should fit into period, otherwise sanity check won't work!

  return pattern;
}


/*

Regular Recurrent | Samples
y / n   y / n     | 
__________________|_________
*       *         | aa_aa
*           *     | a_aaa
    *   *         | ab_ab
    *       *     | a_ab


*/

PitchTraits PatternGenerator::CreateTraits( SizeIt index )
{
  PitchTraits traits;

  index = index == 0 ? 1 : index;

  traits.symbolic  = PitchTraits::Chars; // generate chars only, analyse all types
  //traits.regular   = true; // Sending only regular patterns only, analyse all types
  //traits.recurrent = index % 4 == 0;
  traits.width     = index;

  return traits;
}



IFrame TextPatternGenerator::CreatePatternFrame( PitchTraits traits )
{
  IFrame patternFrame = CreateEmptyFrame();

  if( traits.symbolic != PitchTraits::Chars /*||
      traits.regular  != true */)
  {
    throw UnitTestException( DException::Error, "Pattern generator is currently not supporing this symbolic traits %d", traits.symbolic );
    //throw UnitTestException( DException::Error, "Pattern generator is currently not supporing this symbolic %d or regularity traits %d", traits.symbolic, traits.regular );
  }

  // All regular symbolic patterns will be like a, aa, aaa
  // All irregular like ab, aba, abab (not supporing for now!)
  char regularChar = 'a';

  for( SizeIt charPosition = 0; charPosition < traits.width && charPosition < FrameSize(); charPosition++ )
  {
    *(( ByteIt* )patternFrame + charPosition ) = regularChar;
  }

  return patternFrame;
}



IFrame TextPatternGenerator::CreateEmptyFrame()
{
  IFrame emptyFrame( FrameSize() );

  emptyFrame.Clear();

  return emptyFrame;
}



TextPatternAnalyzer::TextPatternAnalyzer( const char* name, SizeIt pitchSize, PeriodIt _ticksLimit, SizeIt _countLimit /*, bool useFiltering */ ):
  analyzerName  ( name           ),
  tick          ( 0              ),
  ticksLimit    ( _ticksLimit    ),
  countLimit    ( _countLimit    ),
  patternLevel0 ( Its::Content   ),
  patternLevel1 ( Its::Width     ),
  patternLevel2 ( Its::CharTypes ),
  patternLevel3 ( Its::Present   )
{
  viewRaw.clear();

  viewLevel0.clear();
  viewLevel1.clear();
  viewLevel2.clear();
  viewLevel3.clear();
}


// Returns true when analysis is complete
bool TextPatternAnalyzer::AnalyzePitch( IFrame& pitchFrame, bool continueWhenPatternFound )
{
  string message;

  if( PatternSearchComplete() && !continueWhenPatternFound )
  {
    throw UnitTestException( DException::Warning, "Analyzer '%s' has finished patterns search, can not continue, need to retrieve analysed pattern and stop.", analyzerName.c_str() );
  }

  DebugBytesToText(( ByteIt* )pitchFrame, pitchFrame.Size(), message );
  DLog::msg( DLog::LevelVerbose, "In tick %3d analyzer '%s' received frame '%s' of data with size '%d'", tick, analyzerName.c_str(), message.c_str(), pitchFrame.Size() );
 
  PitchInfo currentPitchInfo = ParsePitch( pitchFrame );

  // Only non empty pitches are being recorded in patterns
  if( currentPitchInfo.traits.symbolic != PitchTraits::Empty )
  {
    // record base line first
    if( baseLine.traits.symbolic == PitchTraits::Empty )
    {
      baseLine = currentPitchInfo; 
    }

    RecordPitch( currentPitchInfo, Its::Content      );
    RecordPitch( currentPitchInfo, Its::Width        );
    RecordPitch( currentPitchInfo, Its::CharTypes    );
    RecordPitch( currentPitchInfo, Its::Present      );
  }
  else
  {
    // record empty frames too 
    viewLevel0.push_back( currentPitchInfo.plainView );
    viewLevel1.push_back( currentPitchInfo.plainView );
    viewLevel2.push_back( currentPitchInfo.plainView );
    viewLevel3.push_back( currentPitchInfo.plainView );
  }

  // raw view recording
  viewRaw.push_back( currentPitchInfo.plainView );

  tick++;

  return PatternSearchComplete();
}


// Returns results of the analysis, best pattern found 
// Ready when AnalyzePitch() returns true
ImpulseFramesPattern TextPatternAnalyzer::GetPattern()
{
  if( !PatternSearchComplete())
  {
    DLog::msg( DLog::LevelVerbose, "Analyzer '%s' has not finished yet in tick %d with limit %d and count limit %d ", analyzerName.c_str(), tick, ticksLimit, countLimit );
  }


  // Do not consider ImpulseFramesPattern::established in the priority list, level only 
  //
  // Reached repetitions limit?
  // 
  // Sort and see on what identity level we have most repetitions? just use best level for now:
  //
  if( patternLevel0.count >= countLimit )
  {
    return patternLevel0;
  }

  if( patternLevel1.count >= countLimit )
  {
    return patternLevel1;
  }

  if( patternLevel2.count >= countLimit )
  {
    return patternLevel2;
  }

  if( patternLevel3.count >= countLimit )
  {
    return patternLevel3;
  }


  // Take most repeted pattern
  //
  // eeeeh... ultra quick sort ;-)
  //
  if( patternLevel0.count >= patternLevel1.count &&
      patternLevel0.count >= patternLevel2.count && 
      patternLevel0.count >= patternLevel3.count )
  {
    return patternLevel0;
  }

  if( patternLevel1.count >= patternLevel0.count &&
      patternLevel1.count >= patternLevel2.count && 
      patternLevel1.count >= patternLevel3.count )
  {
    return patternLevel1;
  }

  if( patternLevel2.count >= patternLevel0.count &&
      patternLevel2.count >= patternLevel1.count && 
      patternLevel2.count >= patternLevel3.count )
  {
    return patternLevel2;
  }

  if( patternLevel3.count >= patternLevel0.count &&
      patternLevel3.count >= patternLevel1.count && 
      patternLevel3.count >= patternLevel2.count )
  {
    return patternLevel3;
  }



  // Just any repetitions?
  if( patternLevel0.count > 0 )
  {
    return patternLevel0;
  }

  if( patternLevel1.count > 0 )
  {
    return patternLevel1;
  }

  if( patternLevel2.count > 0 )
  {
    return patternLevel2;
  }

  // Anything?
  return patternLevel3;
}



TextPatternAnalyzer::PitchInfo TextPatternAnalyzer::ParsePitch( IFrame& pitch )
{
  //
  // similar to DebugBytesToText / DebugTextByteToChar functions in a way, just add counting
  //
  SizeIt charsCount     = 0;
  SizeIt nonCharsCount  = 0;
  string plainView      = "";
  char   symbol[ 33 ];

  for( SizeIt loopByte = 0; loopByte < pitch.Size(); loopByte++ )
  {
    ByteIt data = *( ( ByteIt* )pitch + loopByte );

    if( data == 0 || data == NOSIGNAL )
    {
      symbol[ 0 ] = ' ';
    }
    else
    {
      if( data >= ASCII_LOW_CHAR && data <= ASCII_HIGH_CHAR )
      {
        symbol[ 0 ] = data;
        charsCount++;
      }
      else
      {
        symbol[ 0 ] = '*';
        nonCharsCount++;
      }
    }

    symbol[ 1 ] = 0;

    plainView += symbol;
  }

  PitchInfo info;

  info.tick         = tick;
  info.plainView    = plainView;
  info.traits.width = charsCount + nonCharsCount;

  if( charsCount == 0 && nonCharsCount == 0 )
  {
    info.traits.symbolic = PitchTraits::Empty;
  }
  else if( charsCount != 0 && nonCharsCount == 0 ) 
    {
      info.traits.symbolic = PitchTraits::Chars;
    }
    else if( charsCount == 0 && nonCharsCount != 0 ) 
      {
        info.traits.symbolic = PitchTraits::NonChars;
      }
      else
      {
        info.traits.symbolic = PitchTraits::Mixed;
      }

  return info;
}


bool TextPatternAnalyzer::PatternSearchComplete()
{
  bool reachedCyclesLimit = 
          ( tick >= ticksLimit );

  bool reachedCountLimit = 
          patternLevel0.count >= countLimit ||
          patternLevel1.count >= countLimit ||
          patternLevel2.count >= countLimit ||
          patternLevel3.count >= countLimit;

  bool noEstablishedPatterns = 
          !patternLevel0.established &&
          !patternLevel1.established &&
          !patternLevel2.established &&
          !patternLevel3.established;

  if( reachedCyclesLimit )
  {
    DLog::msg( DLog::LevelVerbose, "Analyzer '%s' has finished pattern search in tick %d as reached cycles limit %d ", analyzerName.c_str(), tick, ticksLimit );
  }

  if( reachedCountLimit )
  {
    DLog::msg( DLog::LevelVerbose, "Analyzer '%s' has finished pattern search in tick %d as reached repetitions limit %d ", analyzerName.c_str(), tick, countLimit );
  }

  if( noEstablishedPatterns )
  {
    DLog::msg( DLog::LevelVerbose, "Analyzer '%s' has finished pattern search in tick %d as there are no established patterns", analyzerName.c_str(), tick );
  }

  return reachedCyclesLimit || reachedCountLimit || noEstablishedPatterns;
}


void TextPatternAnalyzer::RecordPitch( PitchInfo& pitchInfo, Identity_t identity )
{
  bool         pitchesIdentical = false; // same as base?
  bool         doRecording      = false; 

  PatternPlainView*     view    = GetViewbyIdentity    ( identity ); // can not be null
  ImpulseFramesPattern* pattern = GetPatternbyIdentity ( identity ); // can be null

  if( pitchInfo.traits.symbolic == PitchTraits::Empty )
  {
    throw UnitTestException( DException::Error, "Analyzer '%s' can not register empty pitch into pattern", analyzerName.c_str() );
  }

  if( baseLine.tick > pitchInfo.tick )
  {
    throw UnitTestException( DException::Error, "Current pitch's tick less than for baseline in analyzer '%s'", analyzerName.c_str() );
  }

  switch( identity )
  {
    case Its::Content   : pitchesIdentical = baseLine.plainView       == pitchInfo.plainView;       break;
    case Its::Width     : pitchesIdentical = baseLine.traits.width    == pitchInfo.traits.width;    break;
    case Its::CharTypes : pitchesIdentical = baseLine.traits.symbolic == pitchInfo.traits.symbolic; break;
    case Its::Present   : pitchesIdentical = true; break;

    default:
      throw UnitTestException( DException::Error, "Unknown identity type in pattern analyzer '%s'", analyzerName.c_str() );
  }

  // Pattern determination logic:
  //
  // 1) Base pitch tick recorded as a start
  // 2) First identical pitch establishes a period for the level
  // 3) When another identical pitch doesn't fall into the period, stop analyzing this level
  //
  if( baseLine.tick == pitchInfo.tick )
  {
    pattern->start        = pitchInfo.tick;
    pattern->period       = 0;
    pattern->count        = 1;
    pattern->established  = true;

    doRecording = true; 
  }
  else 
  {
    if( pitchesIdentical )
    {
      PeriodIt periodFromBase = pitchInfo.tick - baseLine.tick;

      if( pattern->period == 0 )
      {
        pattern->period = periodFromBase;
        pattern->count++;

        doRecording = true;
      }
      else
      {
        // 1) check if the period is sustained
        // 2) check if no periods are missing
        //
        if(( pattern->established ) && 
           ( periodFromBase %  pattern->period == 0 ) &&
           ( periodFromBase == pattern->period * pattern->count ))
        {
          pattern->count++;

          doRecording = true;
        }
        else
        {
          pattern->established = false;
        }
      }
    }
  }
 
  if( doRecording )
  {
    view->push_back( pitchInfo.plainView );
  }
  else
  {
    view->push_back( "." ); // placeholder for pitches not falling into the pattern
  }
}


const char* TextPatternAnalyzer::IdentityToString( Identity_t identity )
{
  switch( identity )
  {
    case Its::Content          : return "Content"   ;
    case Its::Width            : return "Width"     ;
    case Its::CharTypes        : return "CharTypes" ;
    case Its::Present          : return "Present"   ;
    case Its::MaxIdentityLevel : return "Raw"       ;
    default: throw UnitTestException( DException::Error, "Unknown identity type in unit test output" );
  }
}


void TextPatternAnalyzer::OutputView( ostream& os, const char* name, ImpulseFramesPattern* pattern )
{
  if( pattern != 0 )
  {
    os << boost::format( "Pattern [ start %3d / period %3d / count %3d, identity = %-10s ] named '%s'" ) % 
          pattern->start % pattern->period % pattern->count % IdentityToString( pattern->identity ) % name << endl;

    //os << "Pattern [ start " << pattern->start   << 
    //      " / period "       << pattern->period  << 
    //      " / count "        << pattern->count   << 
    //      " / identity '"    << IdentityToString( pattern->identity ) << 
    //      "' ] " <<
    //      " with name '" << name << "'" << endl;
  }
  else
  {
    os << "Raw view, no patterns" << endl;
  }
}


// Output currently recorder view to stream
// When MaxIdentityLevel output everything (raw view)
//
void TextPatternAnalyzer::OutputView( ostream& os, Identity_t identity )
{
  PatternPlainView*     view    = GetViewbyIdentity    ( identity ); // can not be null
  ImpulseFramesPattern* pattern = GetPatternbyIdentity ( identity ); // can be null

  OutputView( os, analyzerName.c_str(), pattern );

  os << endl;

  for( PatternPlainView::const_iterator loopView = view->begin(); loopView != view->end(); loopView++ )
  {
    const string& line = *loopView;

    os << ">" << line << endl;
  }
}


TextPatternAnalyzer::PatternPlainView* TextPatternAnalyzer::GetViewbyIdentity( Identity_t identity )
{
  PatternPlainView* view = 0;

  switch( identity )
  {
    case Its::Content   : view = &viewLevel0; break;
    case Its::Width     : view = &viewLevel1; break;
    case Its::CharTypes : view = &viewLevel2; break;
    case Its::Present   : view = &viewLevel3; break;
    default             : view = &viewRaw;    break; 
  }      

  return view;
}


ImpulseFramesPattern* TextPatternAnalyzer::GetPatternbyIdentity( Identity_t identity )
{
  ImpulseFramesPattern* pattern = 0;

  switch( identity )
  {
    case Its::Content   : pattern = &patternLevel0; break;
    case Its::Width     : pattern = &patternLevel1; break;
    case Its::CharTypes : pattern = &patternLevel2; break;
    case Its::Present   : pattern = &patternLevel3; break;
    default: break; // empty pattern
  }      

  return pattern;
}

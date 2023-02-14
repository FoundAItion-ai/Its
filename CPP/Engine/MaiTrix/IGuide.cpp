/*******************************************************************************
 FILE         :   IGuide.cpp

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Integration point for everything

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   05/05/2014
*******************************************************************************/



#include "stdafx.h"
#include <stdlib.h>
#include <direct.h>
#include <time.h>
#include <set>

#include <boost/algorithm/string.hpp>

#include "Init.h"
#include "IGuide.h"

using namespace std;
using namespace Its;
using namespace boost::algorithm;


const int    SubLevelIncrement = 10;
const int    GrowThreshold = 50;

// static
const char*  MaiTrixName       = "First";
const char*  MaiTrixPath       = ".\\dma\\";
const char*  MaiTrixDebPath    = ".\\deb\\";

DCriticalSection IGuide::section;


IGuide::IGuide():
  lessonNumber          ( 0                   ),
  suggestedDelay        ( INITIAL_DELAY       ),
  suggestedRepetitions  ( INITIAL_REPETITIONS )
{  
  CreateEnvironment();

  DLog::msg( DLog::LevelInfo, "IGuide here." );
}



IGuide::~IGuide()
{  
  try
  {
    DLog::msg( DLog::LevelInfo, "IGuide is leaving..." );

    ClearBoard();
  }
  catch( ... )
  {
    DLog::msg( DLog::LevelError, "General guiding error." );
  }

  DLog::msg( DLog::LevelInfo, "IGuide is not here." );
}



void IGuide::CreateEnvironment()
{
  if( _chdir( MaiTrixPath ) != 0 )
  {
    _mkdir( MaiTrixPath );
  }
  else
  {
    _chdir( "..\\" );
  }

  if( _chdir( MaiTrixDebPath ) != 0 )
  {
    _mkdir( MaiTrixDebPath );
  }
  else
  {
    _chdir( "..\\" );
  }
    
  string logFilePath( MaiTrixDebPath );

  logFilePath += "Guide.log";

  probe.divertLow   = INITIAL_PROBE_DIVERT_LOW;
  probe.divertHigh  = INITIAL_PROBE_DIVERT_HIGH;
  probe.mixLow      = INITIAL_PROBE_MIX_LOW;
  probe.mixHigh     = INITIAL_PROBE_MIX_HIGH;
  probe.createLow   = INITIAL_PROBE_CREATE_LOW;
  probe.createHigh  = INITIAL_PROBE_CREATE_HIGH;
  probe.gravityMode = Probe::VariableGravity;
  probe.testMode    = Probe::Disabled;

  DLog::init( logFilePath.c_str(), DLog::LevelDebug );
  // DLog::init( logFilePath.c_str(), DLog::LevelVerbose );
}


// Build a dictionary of words, tags...
void IGuide::CreateWords()
{
  DLog::msg( DLog::LevelInfo, "Creating words..." );

  words.clear();
  
  for( FeedersList_t::iterator feederLoop = feeders.begin(); feederLoop != feeders.end(); feederLoop++ )
  {
    IFeeder&    feeder = **feederLoop;
    StringList_t& tags = feeder.Tags();
  
    DLog::msg( DLog::LevelInfo, "Feeder '%s' provides '%d' word(s)", feeder.Name(), tags.size() );
    
    words.merge( tags, Its::StringOrder );
  }
  
  words.sort   ( Its::StringOrder );
  words.unique ( Its::StringEqual );

  DLog::msg( DLog::LevelInfo, "%d word(s) created:", words.size() );
  for( auto word: words )
  {
    DLog::msg( DLog::LevelInfo, "'%s'", word.c_str() );
  }
}


// Collect/update data via all feeders from various sources
void IGuide::CreatePlayground( FeedersList_t& _feeders )
{
  DLog::msg( DLog::LevelInfo, "Creating playground..." );

  feeders = _feeders;
  
  for( FeedersList_t::iterator feederLoop = feeders.begin(); feederLoop != feeders.end(); feederLoop++ )
  {
    IFeeder& feeder = **feederLoop;
    feeder.Sync();
  }
  
  DLog::msg( DLog::LevelInfo, "Playground created. Found '%d' feeders.", feeders.size() );
}


// Based on playground size and given time determine the size initially / grow
void IGuide::CreateMaiTrix()
{
  DLog::msg( DLog::LevelInfo, "Creating One MaiTrix..." );

  // 3 Feb 2012 - DMA - Note:
  //
  // Image size should be the same as the maiTrix size otherwise
  // won't be good due to interleaving.
  //
  // Even if the size is aligned (image 100, maiTrix 200) then you will see
  // 'ghost' images composed of even and odd lines.
    
  //SizeIt xySize = ( SizeIt ) sqrt( ( float ) DEFAULT_IMAGE_CAPACITY ); //??? based on hdd space?

  bool cacheLayers;

#ifdef OPTIMIZE_IT
  cacheLayers = true;
#else
  cacheLayers = false;
#endif

  MaiTrix::MaiTrixOptions options = cacheLayers ?
    ( MaiTrix::MaiTrixOptions )( MaiTrix::CacheLayers | MaiTrix::AutoCorrection ) : MaiTrix::AutoCorrection;

  oneMaiTrix = auto_ptr< MaiTrix > ( new MaiTrix( MaiTrixName, 
                                                  INITIAL_SIZE_X, 
                                                  INITIAL_SIZE_Y, 
                                                  INITIAL_SIZE_Z,
                                                  DEFAULT_TEXT_CAPACITY, 
                                                  DEFAULT_AUDIO_CAPACITY, 
                                                  DEFAULT_IMAGE_CAPACITY,
                                                  MaiTrixPath,
                                                  options ));

  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' created.", oneMaiTrix->Name() );
}



// Devise a lesson as a set/subset of frames on a given topic with various difficulty levels... "basic, advanced topic"
// Sub level allows for some variety within the same level.
IGuide::Lesson_t IGuide::CreateLesson( const char* topic, LevelIt level, LevelIt subLevel )
{
  DLog::msg( DLog::LevelInfo, "Creating Lesson..." );

  if( level == 0 )
  {
    throw IGuideException( "Can't find a lesson for the MaiTrix. Level zero." );
  }
      
  if( feeders.empty() )
  {
    throw IGuideException( "No feeders found to create a lesson." );
  }

  Lesson_t lessonInfo( topic, level );
  
  ClearBoard();

  // Suppose we have 3 feeders (text, audio, video) and topic "let's make circles",
  // on the topic we consume this input:
  //
  // feeder #1 - circle (10 ticks), crop circles (20 ticks), artistic circles (2 ticks), argue in a circle (5 ticks);
  // feeder #2 - audio fragment 'circle' (20 ticks), fragment 'circle of Willis' (30 ticks), fragment 'turning circle' (10 ticks);
  // feeder #3 - video fragment 'sun as a perfect circle' (100 ticks), fragment 'play with circles' (30 ticks), fragment 'circle action' (100 ticks); 
  //
  // So for level 2 we take 2 frames from each feeder and compose a lesson:
  //
  // circle (10 ticks)
  // crop circles (20 ticks)
  // audio fragment 'circle' (20 ticks)
  // fragment 'circle of Willis' (30 ticks)
  // video fragment 'sun as a perfect circle' (100 ticks)
  // fragment 'play with circles' (30 ticks)
  //
  // These ^^^^^^^ are the "complex lessons".
  //
  // Let's start with simple lessons - one short frame, repeated with high frequency.
  //
  // Allow only one feeder on level 1, two on level 2, etc.
  // Use only one frame from feeder, depending on subLevel. Mixing different
  // frames from the same feeder in one lesson may be confusing. Like if you mix
  // frame showing 1 symbol at position 1 with another frame showing 1 symbol at position 3 with 
  // you get two symbols with topic "1".
  //
  LevelIt feederCount = 0;

  for( FeedersList_t::iterator feederLoop = feeders.begin(); feederLoop != feeders.end(); feederLoop++ )
  {
    IFeeder*     feeder       = *feederLoop;
    StringList_t frameIds     = feeder->Frames( topic, level );
    int          frameIdCount = 0;
    SizeIt3D     maiTrixSize  = oneMaiTrix->Size();

    feeder->SetDimensions( SizeIt2D( maiTrixSize.X, maiTrixSize.Y ));

    if( frameIds.empty() )
    {
      DLog::msg( DLog::LevelWarning, "Feeder '%s' has nothing for topic '%s', level '%d'", feeder->Name(), topic, level );
      // nothing from this feeder on the topic
      continue;
    }

    if( feederCount++ >= level )
    {
      break;
    }

    for( StringList_t::iterator frameIdLoop = frameIds.begin(); frameIdLoop != frameIds.end(); frameIdLoop++ )
    {
      if( frameIdCount++ == subLevel % frameIds.size())
      {
        string frameId = *frameIdLoop;

        // limit lesson's size by level
        // normally 1-3 frames per feeder
        Fragment_t fragment;

        CapacityIt  feederCapacity = feeder->Capacity();
        PeriodIt    frameLength    = feeder->TicksInFrame( frameId.c_str() );

        fragment.frameId = frameId; 
        fragment.feeder  = feeder;
        fragment.type    = feeder->Type();

        DLog::msg( DLog::LevelInfo, "Feeder '%s' will stream '%d' ticks of %dT x %dA x %dV capacity in lesson '%d' as fragment '%d'", 
                                     feeder->Name(), frameLength,
                                     feederCapacity.sizeText, feederCapacity.sizeAudio, feederCapacity.sizeVideo, 
                                     lessonNumber + 1, lesson.size() );

        // not going to loose memory due to exceptions or multithreading 
        // as we save to lesson right away, which is cleared in destructor.
        fragment.frameSet = new IFrameSet( feederCapacity, level, fragment.type ); 

        lesson.push_back( fragment );

        // the longest fragment defines the total length
        lessonInfo.length  = max< PeriodIt > ( lessonInfo.length, frameLength );
        lessonInfo.volume += feederCapacity; 
        break;
      }
    }
  }

  lessonInfo.number = ++lessonNumber;

  if( lesson.size() == 0 )
  {
    throw IGuideException( "Can not create a lesson. No input from feeders." );
  }

  DLog::msg( DLog::LevelInfo, "Lesson #%d created. Length '%d', repetitions '%d', delay '%d', topic '%s', level '%d', sub-level '%d'", 
                               lessonInfo.number, lessonInfo.length, lessonInfo.repetitions, lessonInfo.delay, lessonInfo.topic.c_str(), level, subLevel );
                                
  DLog::msg( DLog::LevelInfo, "Lesson's volume: %dT x %dA x %dV",
                               lessonInfo.volume.sizeText, lessonInfo.volume.sizeAudio, lessonInfo.volume.sizeVideo );
  
  return lessonInfo;
}



void IGuide::ClearBoard()
{
  for( FragmentList_t::iterator fragmentLoop = lesson.begin(); fragmentLoop != lesson.end(); fragmentLoop++ )
  {
    Fragment_t& fragment = *fragmentLoop;
    delete fragment.frameSet;
  }

  lesson.clear();
}


//
// Feed the maiTrix and estimate the result.
// Provide the feedback to it and track PQ (Prediction Quality, 0 - 100) against VQ (Vital Quality, 0 - 100) 
//
// 
void IGuide::Teach( FeedersList_t& _feeders, PCallback callback, PFeedbackCallback feedbackCallback )
{
  DLog::msg( DLog::LevelInfo, "Teaching..." );

  CreatePlayground( _feeders );
  CreateMaiTrix();
  CreateWords();  // Update tags and reload feeders content.
  
  // Level assignment sample:
  //
  // level 1: 'a', sublevel 1-10
  // level 1: 'b', sublevel 1-10
  // level 1: 'c', sublevel 1-10
  //          .....
  //
  // level 2: 'a', sublevel 11-20
  // level 2: 'b', sublevel 11-20
  // level 2: 'c', sublevel 11-20
  //          .....
  //
  // level 3: 'a', sublevel 21-30
  // level 3: 'b', sublevel 21-30
  // level 3: 'c', sublevel 21-30
  //          .....
  //
  // Note that we go down somewhat quicker :)

  TeachingResult          teachingResult = Unknown;
  bool                    leaving        = false;
  LevelIt                 level;
  LevelIt                 subLevel;
  LevelIt                 subLevelSaved;
  Lesson_t                lessonInfo;
  SizeItl                 lessonNumberSaved = lessonNumber + 1;
  const char*             topic;
  StringList_t::iterator  word;
  teachingResult          = Unknown;
  int noChangesCounter    = 0; // Are we stuck?
  bool takeSnapshot       = false; // extra snapshot, not regular to troubleshoot when we're stuck

  word = ResetLearningPoint( &level, &subLevel );

  if( level != 1 || subLevel != 1 )
  {
    // Possible troubleshooting, just a reminder
    DLog::msg( DLog::LevelWarning, "***** Incorrect lesson level and/or sublevel '%d : %d' *****", level, subLevel );
  }

  while( !leaving )
  {
    topic          = word->c_str();
    lessonInfo     = CreateLesson( topic, level, subLevel );
    teachingResult = DoTeach( lessonInfo, level, callback, feedbackCallback, takeSnapshot ); // pq passed by reference
    subLevelSaved  = subLevel;

    switch( teachingResult )
    {
      case MarkExcellent:
      case MarkGood:
        noChangesCounter = 0;
        subLevel++;
        if( subLevel % SubLevelIncrement == 1 )
        {
          word++;
          if( word == words.end() )
          {
            word = words.begin();
            level++;
          }
          else
          {
            subLevel -= SubLevelIncrement;
          }
        }
        break;
      case MarkFair:
        noChangesCounter++;
        break;
      case MarkBad:
        noChangesCounter = 0;
        subLevel--;
        if( subLevel % SubLevelIncrement == 0 )
        {
          word = words.begin();
          level--;
        }
        break;
      case NeedLeave:
        leaving = true;
        break;
      case AskLeave:
        leaving = true;
        break;
      default: 
        throw IGuideException( "Unknown teaching result. Internal error." );
    }

    DLog::msg( DLog::LevelInfo, "Lesson with topic '%s' complete at level '%d', sub-level '%d', counter '%d' and result '%s'",
               topic, level, subLevelSaved, noChangesCounter, TeachingResultAsString( teachingResult ).c_str());

    // Growth strategy:
    //
    // 1) Grows naturally, with time.
    // 2) If level is getting down or we're stuck.
    //
    takeSnapshot = noChangesCounter == GrowThreshold - 1;
    if( noChangesCounter >= GrowThreshold )
    {
      //if( !oneMaiTrix->GrowBottom())
      //{
      //  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' can not grow at the bottom", oneMaiTrix->Name());
      //}
      //else
      //{
      //  DLog::msg( DLog::LevelInfo, "MaiTrix '%s' is growing at the bottom to %d layers after %d lesssons", oneMaiTrix->Name(), oneMaiTrix->Size().Z, lessonNumber - lessonNumberSaved );
      //}

      // 26 Oct 2017 - DMA - maybe we don't need it now as growing from the bottom does not invalidate SINs.
      //
      // IMPORTANT NB: after we grow we must start learning from the very beginning as out SINs
      // state would be invalidated but re-learning should be much faster. And even though growing
      // may fail we still try and start learning from the very beginning.
      DLog::msg( DLog::LevelWarning, "MaiTrix '%s' learning process restarted at '%d' cycle", oneMaiTrix->Name(), oneMaiTrix->Cycle() );

      word = ResetLearningPoint( &level, &subLevel );
      lessonNumberSaved = lessonNumber;
      noChangesCounter = 0;
    }

    if( level >= 10 || level == 0 )
    {
      leaving  = true;
      DLog::msg( DLog::LevelWarning, "Lesson interrupted at level '%d'", level );
    }

    assert( subLevel <= SubLevelIncrement * 9 + 1 );
  }

  DLog::msg( DLog::LevelInfo, "Teaching complete." );
}


void IGuide::Complete( PFeedbackCallback feedbackCallback )
{
  GuideFeedback_t feedback = {};

  feedback.maiTrix        = &(*oneMaiTrix);
  feedback.maiTrixPath    = MaiTrixPath;
  feedback.maiTrixDebPath = MaiTrixDebPath;

  feedbackCallback( feedback );
}


IGuide::TeachingResult IGuide::DoTeach( Lesson_t& lessonInfo, LevelIt level, PCallback callback, PFeedbackCallback feedbackCallback, bool takeSnapshot )
{
  TeachingResult  teachingResult = MarkFair;
  GuideWay_t      way;
  SizeIt3D        maiTrixSize;
  string          message;
  int             gf = 1;
  int             pq = 0;
  
  // Grow based on:
  //
  // 1) info stream (initially);
  // 2) or oneMaiTrix->Grow( level ) - unclear;
  // 3) or just ticks? natural grow;
  //
  // while growing along Z axis is the easiest way, we might want to think of X x Y too.
  //
  // TODO: need to adjust response size properly from Sins::InitialResponseSize
  // Keep it at max at the moment.
  //
  //int responseSize = 0; 
  //if( !oneMaiTrix->Grow( lessonInfo.volume, responseSize ) && lessonInfo.volume > oneMaiTrix->Capacity())
  //{
  //  DLog::msg( DLog::LevelWarning, "MaiTrix '%s' can't grow, reajust lesson's level", oneMaiTrix->Name(), level );
  //  return MarkBad;
  //}
    
  // Note that maiTrix grows layer by layer so actual capacity
  // can be more than requested, so use it.
  lessonInfo.volume = oneMaiTrix->Capacity();
    
  // Merged data from all feeders
  // and baseline to compare
  //
  IFrameSet mainStream( lessonInfo.volume, lessonInfo.level, Its::textaudiovideo );
  IFrameSet baseStream( lessonInfo.volume, lessonInfo.level, Its::textaudiovideo );

  maiTrixSize = oneMaiTrix->Size();

  // set repetition count, based on MaiTrix size ( N * times Max size )
  //
  PeriodIt maxMaiTrixSize = max< SizeIt >( maiTrixSize.X, 
                            max< SizeIt >( maiTrixSize.Y, 
                                           maiTrixSize.Z ));
    
  // With the same gf level try all possible directions at the "split"
  lessonInfo.repetitions = 9;
    
  // does't make sense to check for result unless signal can reach from one side to another * N?
  lessonInfo.delay = maiTrixSize.Z - 1;   

  if( suggestedDelay > 0 )
  {
    lessonInfo.delay = suggestedDelay;
  }

  if( suggestedRepetitions > 0 )
  {
    lessonInfo.repetitions = suggestedRepetitions;
  }

  int result = 0;
  int discontent = 0;
  std::set<string> wrongResponses;

  for( PeriodIt lessonLoop = 0; lessonLoop < lessonInfo.repetitions; lessonLoop++ )
  {
    string response = "";
    // Note that we're not taking one but many snapshots, for each repetition to troubleshoot
    // situation when we're stuck as ideally such snapshots should be different.
    if( Lesson( gf, lessonInfo, mainStream, baseStream, message, response, lessonLoop, feedbackCallback, takeSnapshot ))
    {
      pq++;
      continue;
    }

    if( wrongResponses.find( response ) != wrongResponses.end() )
    {
      discontent++;
      continue;
    }

    wrongResponses.insert( response );
  }

  result = ( pq * 100 ) / lessonInfo.repetitions;

  DLog::msg( DLog::LevelInfo, "Lesson's actual repetitions = '%d', delay = '%d', result = '%d', discontent = '%d'", lessonInfo.repetitions, lessonInfo.delay, result, discontent );

  if( callback != 0 )
  {
    way.action            = None;
    way.delay             = suggestedDelay;
    way.repetitions       = suggestedRepetitions;
    way.probe             = probe;
  
    // 11 Aug 2017 - DMA - TODO: rename VQ and probably remove it if not needed.
    // pq and result is pretty much the same.
    callback( pq, result, message.c_str(), way );

    suggestedDelay        = way.delay;
    suggestedRepetitions  = way.repetitions;
    probe                 = way.probe;

    DLog::msg( DLog::LevelInfo, "Lesson's UI updated: delay = '%d', repetitions = '%d', divertLow = '%d', divertHigh = '%d', mixLow = '%d', mixHigh = '%d', createLow = '%d', createHigh = '%d' ", 
                                 suggestedDelay, suggestedRepetitions, probe.divertLow, probe.divertHigh,
                                 probe.mixLow, probe.mixHigh, probe.createLow, probe.createHigh );
  }

  switch( way.action )
  {
    case Pause  : teachingResult = AskLeave;  break;
    case Reboot : oneMaiTrix->Reboot();       break;
    case Void   : oneMaiTrix->Void(); 
                  CreateMaiTrix();            break;
    case None   :
    default     : teachingResult = result > 90 ? MarkExcellent : ( result > 10 ? MarkGood : MarkFair );
  }
    
  return teachingResult;   
}


//
// There should be a choice on M side?
//
// ....vq?
bool IGuide::Lesson( int& gf, Lesson_t& lessonInfo, IFrameSet& mainStream, IFrameSet& baseStream,
                     string& message, string& response, PeriodIt lessonRepetition, PFeedbackCallback feedbackCallback, bool takeSnapshot )
{
  string baseMessage;
  unsigned int rf;
  Sins sins;

  // The biggest issue with long lesson is uncertain feedback when signal 'A    ' (tick 0) is making
  // a new path (all BAD_CELLs) and signal '  A ' (tick 1 or later)  may follow that path partially
  // producing GOOD_CELLs along the way. But if result of the first signal is not satisfactory and
  // we decide to erase the path with gf = -1, we can only erase red part of the invalid path.
  //
  // That's why for every signal we better wait till the very end before providing feedback.
  //
  // 18 Jul 2017 - DMA - TODO: we can probably mark only NO cells as GOOD to fix the problem.
  if( lessonInfo.length != 1 )
  {
    throw IGuideException( "Don't know how to evaluate long lesson's results yet" );
  }

  // 21 Feb 2013 - DMA - Start refactoring for a new 'mirror' kernel
  //
  PeriodIt totalLessonLength = lessonInfo.length + ( lessonInfo.delay - 1 );
  PeriodIt tick;
  gf = 0;  // Baseline

  DLog::msg( DLog::LevelDebug, "Lesson #%d with topic '%s' started, length %d, %s taking snapshot", lessonInfo.number, lessonInfo.topic.c_str(), totalLessonLength, takeSnapshot ? "" : "not" );

  for( tick = 0; tick < totalLessonLength; tick++ )
  {    
    if( rand_s( &rf ) != 0 )
    {
      rf = tick;
      DLog::msg( DLog::LevelError, "Can not generate rf, use sequential # %d", tick );
    }

    // load all frames for MaiTrix, when beyond lesson's length it's populated with 'empty' frames
    // and remember that:
    //
    // ***************** ALL STREAMS MUST BE CONSISTENT ***************** 
    //
    // means loading the same frameId for the same tick gives the same data
    // to get corrent comparison results.
    //
    LoadStream( tick, mainStream );

    DLog::msg( DLog::LevelVerbose, "Lesson #%d with topic '%s' is in progress, tick = '%d', delay = '%d', gf = '%d'", lessonInfo.number, lessonInfo.topic.c_str(), tick, lessonInfo.delay, gf );

    /*
    ========> Idea - what if we short circuit maiTrix output to input???
    */
    sins = oneMaiTrix->Launch( MaiTrix::GF2GF( gf ), rf, mainStream.text(), mainStream.audio(), mainStream.video(), probe );
  }


  // output to interactive feeders
  // 17 Jul 2012 - DMA - TODO: should we save the response instead of reflection now?
  SaveStream( tick, mainStream );

  // load frame to compare
  LoadStream( 0 /* lessonInfo.length - 1 */, baseStream );

  // Logging
  DebugInfoStreams( mainStream, tick, baseStream, lessonInfo.length - 1, oneMaiTrix->Cycle() );
  OutputMessage( mainStream.text(), mainStream.Capacity().sizeText, message     );
  OutputMessage( baseStream.text(), baseStream.Capacity().sizeText, baseMessage );

  DLog::msg( DLog::LevelDebug, "Lesson #%d with topic '%s' is in progress, this tick = %5d, gf = '%2d', rf = '%6u'", lessonInfo.number, lessonInfo.topic.c_str(), tick, gf, rf );
  DLog::msg( DLog::LevelDebug, "Lesson #%d message      = '%s'", lessonInfo.number, message.c_str());
  DLog::msg( DLog::LevelDebug, "Lesson #%d base message = '%s'", lessonInfo.number, baseMessage.c_str());
  DLog::msg( DLog::LevelDebug, "Lesson #%d response = '%s'",     lessonInfo.number, sins.response.c_str());

  if( sins.response.find_first_not_of( " " ) == string::npos )
  {
    DLog::msg( DLog::LevelError, "In lesson #%d empty response found", lessonInfo.number, sins.response.c_str());
    throw IGuideException( "Empty response found." );
  }

  // Lesson complete, let's verify:
  //
  // 1) There are reflections in mainStream, at least at the signal positions (may be a lot more with time)
  int similarity = mainStream.Compare( baseStream ); // similarity in %
             
  // 2) Response is valid (same as topic), whitespace is not trimmed on the left, exact match required.
  // 28 Aug 2017 - DMA - Relax response match requirements, exact match is not needed anymore, 
  //                     trimmed response match should be enough. This should allow faster matching.
  // 29 Aug 2017 - DMA - Looks like we can not relax response matching as:
  //                     1) It doesn't help as direction still influences result considerably
  //                     2) It would cause reflection fragmentation for the same result
  //
  string trimmedResponse = sins.response;
  trim_right( trimmedResponse );
  response = trimmedResponse;

  // 3) No dead cells
  //
  // Note that this condition is being checked in Guide::LoadStatistics periodically (too expensive)
  // and may not be corrected with -1 feedback but causes hard stop (exception).

  // Collective feedback
  // Black or white logic for now, may come up with more flexible later
  bool lessonResult = similarity > 90 && trimmedResponse == lessonInfo.topic;
  gf = lessonResult ? 1 : -1;

  // Provide debug info to upper level
  if( feedbackCallback != 0 )
  {
    GuideFeedback_t feedback = {};
                                   
    feedback.sins              = sins;
    feedback.baseMessage       = baseMessage.c_str();
    feedback.message           = message.c_str();
    feedback.tick              = tick;
    feedback.gf                = gf;
    feedback.pqEstimate        = lessonResult ? 1 : 0;  // Not really needed, TBR
    feedback.lessonLevel       = lessonInfo.level;
    feedback.lessonTopic       = lessonInfo.topic;
    feedback.lessonDelay       = lessonInfo.delay;
    feedback.lessonTotalLength = totalLessonLength;
    feedback.lessonRepetition  = lessonRepetition;
    feedback.lessonRepetitions = lessonInfo.repetitions;
    feedback.maiTrix           = oneMaiTrix.get();
    feedback.maiTrixPath       = MaiTrixPath;
    feedback.maiTrixDebPath    = MaiTrixDebPath;
    feedback.probe             = probe;
    feedback.takeSnapshot      = takeSnapshot;
        
    feedbackCallback( feedback );
  }

  // Apply correction if needed (after feedback as it may erase all state) 
  oneMaiTrix->Launch( MaiTrix::GF2GF( gf ), rf, mainStream.text(), mainStream.audio(), mainStream.video(), probe );

  // If some sins can not be merged into one then we need to grow.
  // 1  May 2014 - DMA - may be we should grow all the time, no matter what? Seems natural :)
  // 25 Mar 2014 - DMA - when do we grow?
 
  DLog::msg( DLog::LevelInfo, "Completed %5d periods, Maitrix cycle #%5d, similarity rate is %3d, GF is %2d",  
             tick, oneMaiTrix->Cycle(), similarity, gf );

  return lessonResult;
}


void IGuide::OutputMessage( ByteIt* mainStreamText, SizeIt textSize, string& message )
{
  DebugBytesToText( mainStreamText, textSize, message );
}



void IGuide::DebugInfoStreams( IFrameSet& stream, PeriodIt tick, IFrameSet& baseStream, PeriodIt baseTick, PeriodIt maiTrixCycle )
{
  //if( DLog::level() > DLog::LevelVerbose )
  //{
  //  return;
  //}

  DCommon::DCriticalSection::Lock locker( section );

  DLog::msg( DLog::LevelDebug, "" );  
  DLog::msg( DLog::LevelDebug, "Stream content in cycle '%d' and tick '%d' / base tick '%d' output", maiTrixCycle, tick, baseTick );
  DLog::msg( DLog::LevelDebug, "" );
  
  stream.DebugInfo();
  
  DLog::msg( DLog::LevelDebug, "" );
  DLog::msg( DLog::LevelDebug, "Base stream content in cycle '%d' and tick '%d' / base tick '%d' output", maiTrixCycle, tick, baseTick );
  DLog::msg( DLog::LevelDebug, "" );
  
  baseStream.DebugInfo();
  
  DLog::msg( DLog::LevelDebug, "" );
}



void IGuide::LoadStream( PeriodIt tick, IFrameSet& stream )
{
  CapacityIt streamLoad;

  stream.Clear();

  // load all frames, we should always do this in the same order for the lesson
  for( FragmentList_t::iterator fragmentLoop = lesson.begin(); fragmentLoop != lesson.end(); fragmentLoop++ )
  {
    Fragment_t& fragment = *fragmentLoop;
    IFrameSet&  frameSet = *fragment.frameSet;

    if( frameSet.Capacity() != fragment.feeder->Capacity() )
    {
      throw IGuideException( DException::Error, "Not consistent data from feeder found, capacity %dT x %dA x %dV",
                             frameSet.Capacity().sizeText, frameSet.Capacity().sizeAudio, frameSet.Capacity().sizeVideo );
    }

    // If no data found we get 'empty' frame (type dependant, 'text spaces', black image, ...)
    fragment.feeder->LoadFrame( fragment.frameId.c_str(), tick, frameSet );

    stream.CopyFrom( frameSet, streamLoad );
    
    streamLoad += frameSet.Capacity();
  }
}


void IGuide::SaveStream( PeriodIt tick, IFrameSet& stream )
{
  CapacityIt streamSaved;

  // save all frames, we should always do this in the same order for the lesson
  for( FragmentList_t::iterator fragmentLoop = lesson.begin(); fragmentLoop != lesson.end(); fragmentLoop++ )
  {
    Fragment_t& fragment = *fragmentLoop;
    IFrameSet&  frameSet = *fragment.frameSet;

    if( frameSet.Capacity() != fragment.feeder->Capacity() )
    {
      throw IGuideException( DException::Error, "Not consistent data for feeder found, capacity %dT x %dA x %dV",
                             frameSet.Capacity().sizeText, frameSet.Capacity().sizeAudio, frameSet.Capacity().sizeVideo );
    }

    stream.CopyTo( frameSet, streamSaved );
    
    // If no data found we get 'empty' frame (type dependant, 'text spaces', black image, ...)
    fragment.feeder->SaveFrame( fragment.frameId.c_str(), tick, frameSet );

    streamSaved += frameSet.Capacity();
  }
}

string IGuide::TeachingResultAsString( TeachingResult result )
{
  switch( result )
  {
    case MarkExcellent  : return "Excellent";
    case MarkGood       : return "Good";
    case MarkFair       : return "Fair";
    case MarkBad        : return "Bad";
    case NeedLeave      : return "Need To Leave";
    case AskLeave       : return "Ask To Leave";
      default: 
        throw IGuideException( "Unknown teaching result. Internal error." );
  }
}


StringList_t::iterator IGuide::ResetLearningPoint( LevelIt *level, LevelIt *subLevel )
{
  assert( level && subLevel );
  *level    = 1;
  *subLevel = 1;
  return words.begin();
}


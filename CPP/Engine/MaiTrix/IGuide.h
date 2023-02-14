/*******************************************************************************
 FILE         :   IGuide.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Integration point for everything

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   09/07/2011
*******************************************************************************/


#ifndef __IGUIDE_H
#define __IGUIDE_H

#include <memory>

#include "MaiTrix.h"
#include "IFeeder.h"


using namespace std;


namespace Its
{

static const char*  MaiTrixName;
static const char*  MaiTrixPath;
static const char*  MaiTrixDebPath;


class MAITRIX_SPEC IGuideException: public MaiTrixException
{
public:
  DEXCEPTION_CONSTRUCTOR0( IGuideException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR1( IGuideException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR2( IGuideException, MaiTrixException );
  DEXCEPTION_CONSTRUCTOR3( IGuideException, MaiTrixException );
};




class MAITRIX_SPEC IGuide
{
public:
  typedef list< IFeeder* > FeedersList_t;
  
  struct GuideFeedback_t
  {
    Sins         sins;
    PeriodIt     tick;
    int          gf;
    int          pqEstimate;
    LevelIt      lessonLevel;
    string       lessonTopic;
    PeriodIt     lessonDelay;
    PeriodIt     lessonTotalLength;
    PeriodIt     lessonRepetition;  // Current repetition
    PeriodIt     lessonRepetitions; // Total repetitions
    MaiTrix*     maiTrix;
    Probe        probe;
    const char*  baseMessage;
    const char*  message;
    const char*  maiTrixPath;
    const char*  maiTrixDebPath;
    bool         takeSnapshot;
  };

  enum GuideAction_t
  {
    None,
    Pause,
    Reboot, 
    Void
  };

  struct GuideWay_t
  {
    PeriodIt      delay;
    PeriodIt      repetitions;
    Probe         probe;
    GuideAction_t action;
  };

  // callback to track PQ (Prediction Quality) / VQ (Vital Quality) 
  typedef void (*PCallback)( int pq, int vq, const char *message, GuideWay_t& way ); 

  // callback to debug
  typedef void (*PFeedbackCallback)( GuideFeedback_t& feedback ); 

  virtual ~IGuide();

  void Teach( FeedersList_t& _feeders, PCallback callback, PFeedbackCallback feedbackCallback = 0 );
  void Complete( PFeedbackCallback feedbackCallback );

protected:
  IGuide();

  // nothing virtual, all real ;-)
  void CreateEnvironment();
  void CreateWords();
  void CreatePlayground( FeedersList_t& _feeders );
  void CreateMaiTrix();
  void VoidMaiTrix();

  struct Lesson_t
  {
    Lesson_t():
      topic       ( "" ),
      level       ( 0  ),
      length      ( 0  ),
      repetitions ( 1  ),
      delay       ( 1  ),
      number      ( 0  )
    {
    }

    Lesson_t( const char* _topic, LevelIt _level ):
      topic       ( _topic ),
      level       ( _level ),
      length      ( 0  ),
      repetitions ( 1  ),
      delay       ( 1  ),
      number      ( 0  )
    {
    }

    SizeItl    number;
    string     topic;
    LevelIt    level;
    CapacityIt volume;      // one data frame size, by type
    PeriodIt   length;      // total lesson's duration, in ticks (basically, the longest frame and may be pauses)
    PeriodIt   repetitions; // times to repeat everything to get some, hopefully good result
    PeriodIt   delay;       // delay to compare and estimate gf ( < length)
  };

  enum TeachingResult
  {
    Unknown,
    MarkExcellent,
    MarkGood,
    MarkFair,
    MarkBad,
    NeedLeave,
    AskLeave
  };

  TeachingResult DoTeach( Lesson_t& lessonInfo, LevelIt level, PCallback callback, PFeedbackCallback feedbackCallback, bool takeSnapshot );

  Lesson_t CreateLesson( const char* topic, LevelIt level, LevelIt subLevel );
  void     ClearBoard();

  bool Lesson( int& gf, Lesson_t& lessonInfo, IFrameSet& mainStream, IFrameSet& baseStream,
               string& message, string& response, PeriodIt lessonRepetition, PFeedbackCallback feedbackCallback, bool takeSnapshot );

  void LoadStream( PeriodIt tick, IFrameSet& stream );
  void SaveStream( PeriodIt tick, IFrameSet& stream );
  
  void DebugInfoStreams( IFrameSet& stream, PeriodIt tick, IFrameSet& baseStream, PeriodIt baseTick, PeriodIt maiTrixCycle );
  void OutputMessage( ByteIt* mainStreamText, SizeIt textSize, string& message );
  string TeachingResultAsString( TeachingResult result );
  StringList_t::iterator ResetLearningPoint( LevelIt *level, LevelIt *subLevel );

  static DCriticalSection section;

  auto_ptr< MaiTrix > oneMaiTrix; // Only one is to be created!

private:
  // ...of mosaic composing the board
  struct Fragment_t
  {
    string        frameId;
    IFrameSet*    frameSet;
    IFeeder*      feeder;
    Interaction_t type;
  };

  typedef list< Fragment_t  > FragmentList_t;
  
  FeedersList_t       feeders;
  FragmentList_t      lesson;     // limit lesson's size by level
  StringList_t        words;
  SizeItl             lessonNumber;
  PeriodIt            suggestedDelay;
  PeriodIt            suggestedRepetitions;
  Probe               probe;
};



}; // namespace Its


#endif // __IGUIDE_H

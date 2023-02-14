/*******************************************************************************
 FILE         :   Guide.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Integration point for everything

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/09/2012

 LAST UPDATE  :   01/09/2012
*******************************************************************************/


#ifndef __GUIDE_H
#define __GUIDE_H

#include <time.h>

#include "IGuide.h"


namespace Its
{


class Guide
{
public:
  Guide( PeriodIt _statisticPeriod = INITIAL_PERIOD_STATISTIC, PeriodIt _showMaiTrixPeriod = INITIAL_PERIOD_SHOWMAITRIX );

  virtual ~Guide();

  void Its();

  struct BasicInfo
  {
    Sins       sins;
    PeriodIt   tick;
    SizeIt3D   maiTrixSize;
    CapacityIt maiTrixCapacity;
    PeriodIt   cycle;
    int        averageCorrections;
    float      seriesAverageTime;
    float      statisticsTime;
    int        pqEstimate;
    LevelIt    lessonLevel;
    string     lessonTopic;
    PeriodIt   lessonTotalLength;
    PeriodIt   lessonRepetition;  // Current repetition
    PeriodIt   lessonRepetitions; // Total repetitions
    PeriodIt   effectiveInPeriod; // cycles left for adjustments to take effect
    string     baseMessage;
    string     message;
  };

  struct AdjustmentInfo
  {
    // Guide adjustments
    PeriodIt   statisticPeriod;
    PeriodIt   showMaiTrixPeriod;

    // MaiTrix info
    int        pq;
    int        vq;
    string     message;

    // MaiTrix adjustments
    PeriodIt   delay;
    PeriodIt   repetitions;
    Probe      probe;

    IGuide::GuideAction_t action;
  };

protected:
  void CompleteFeedbackCallback( IGuide::GuideFeedback_t& feedback );
  void FeedbackCallbackToGuide( IGuide::GuideFeedback_t& feedback );
  void CallbackToGuide( int pq, int vq, const char *message, IGuide::GuideWay_t& way );
  void LogMessage( const char *message, exception* x );
  
  virtual void LoadStatistics ( MaiTrix& maiTrix, const char* maiTrixPath, CubeStat& maiTrixStat );
  virtual void ShowMaiTrix    ( MaiTrix& maiTrix, const char* maiTrixPath, const char* maiTrixDebPath );
  
  virtual void ShowStatistics ( CubeStat&       maiTrixStat     );
  virtual void ShowBasicInfo  ( BasicInfo&      basicInfo       );
  virtual void ShowAndAdjust  ( AdjustmentInfo& adjustmentInfo  );
  virtual void ShowMessage    ( const char* message             );

  bool IsThisSingleton() { return Guide::instance == this; }
  void It();

private:
  static void OneCompleteFeedbackCallback( IGuide::GuideFeedback_t& feedback );
  static void OneFeedbackCallbackToGuide( IGuide::GuideFeedback_t& feedback );
  static void OneCallbackToGuide( int pq, int vq, const char *message, IGuide::GuideWay_t& way );

  clock_t                 seriesStartTime;
  PeriodIt                seriesStartCycle;
  PeriodIt                correctionsCounter;
  PeriodIt                callbackCounter;
  PeriodIt                statisticPeriod;
  PeriodIt                showMaiTrixPeriod;
  PeriodIt                effectiveInPeriod; // cycles left for adjustments to take effect
  IGuide::FeedersList_t   feeders;
  static Guide*           instance; // Singleton
};




}; // namespace Its


#endif // __GUIDE_H

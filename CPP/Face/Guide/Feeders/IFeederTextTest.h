/*******************************************************************************
 FILE         :   IFeederTextTest.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Simple text data provider for testing

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   09/07/2011

 LAST UPDATE  :   09/29/2011
*******************************************************************************/


#ifndef __IFEEDERTEXTTEST_H
#define __IFEEDERTEXTTEST_H


#include "IFeederCommon.h"


using namespace std;


namespace Its
{

class IFeederTextTest : public virtual IFeederCommon
{
public:
  enum TestFeederType {
    NoFeed,
    BaseFeed,
    SimpleFeed,
    RussianFeed
  };

  IFeederTextTest( TestFeederType feedType = BaseFeed );
  virtual ~IFeederTextTest();

  virtual void Sync(){}

  virtual const char*   Name() { return "Text Simple Test"; }
  virtual Interaction_t Type() { return Its::text;          }
  virtual CapacityIt Capacity();

  // List of all unique tags
  virtual StringList_t Tags();

  // List of all frame Ids with this tag(s)
  virtual StringList_t Frames( const char* tagLine, LevelIt level );
  
  // How long is the frame?
  virtual PeriodIt TicksInFrame( const char* frameId );
 
  virtual void LoadFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );  
  virtual void SaveFrame( const char* frameId, PeriodIt tick, IFrameSet& frameSet );
  
private:
  TestFeederType feedType;
};



}; // namespace Its


#endif // __IFEEDERTEXTTEST_H

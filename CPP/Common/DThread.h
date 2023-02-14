/*******************************************************************************
 FILE         :   DThread.h

 COPYRIGHT    :   DMAlex, 2001

 DESCRIPTION  :   Multithreading support

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   02/28/2001

 LAST UPDATE  :   02/28/2001
*******************************************************************************/


#ifndef __DTHREAD_H
#define __DTHREAD_H


#include "DSync.h"


namespace DCommon
{

class ITS_SPEC DThread
{
public:
   enum DTState { inactive, running, suspended };

   void start();
   void stop(DWORD timeOut = INFINITE);
   void suspend();
   void resume();
   void terminate();

   DTState state() { return curState; }

protected:
   DThread();
   virtual ~DThread();

   virtual void run() = 0;
   virtual void beforeStart();
   virtual void beforeStop();

   void init();
   bool shouldExit() { return fExit; }

private:
   bool     fExit;
   DTState  curState;
   HANDLE   hThread; 
   DEvent   eInit;
   static void entryPoint(void *);
};


}; // namespace DCommon


#endif // __DTHREAD_H

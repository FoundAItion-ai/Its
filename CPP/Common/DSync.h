/*******************************************************************************
 FILE         :   DSync.h

 COPYRIGHT    :   DMAlex, 2001

 DESCRIPTION  :   Synchronization support

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   02/28/2001

 LAST UPDATE  :   02/28/2001
*******************************************************************************/



#ifndef __DSYNC_H
#define __DSYNC_H


#include "DException.h"


namespace DCommon
{

class ITS_SPEC XDSync: public DException
{
public:
  DEXCEPTION_CONSTRUCTOR0( XDSync, DException );
  DEXCEPTION_CONSTRUCTOR1( XDSync, DException );
};



class ITS_SPEC DSemaphore
{
public:
   DSemaphore(long initCoun = 1, long maxCoun = 1);
   ~DSemaphore();

   void release(long relCount = 1);

   class ITS_SPEC Lock
      {
      public:
         Lock(DSemaphore& sem, bool checkOnly = false);
         ~Lock();

         bool wasNonSignaled() { return fNonSignaled; }
      
      private:
         DSemaphore& sem;
         bool fNonSignaled;
      };

   friend Lock;

private:
   HANDLE hSem;
};



class ITS_SPEC DEvent
{
public:
   DEvent(bool fManual = false, bool fSignaled = true);
   ~DEvent();

   void pulse();
   void set(bool fSignaled = true);

   class ITS_SPEC Lock
      {
      public:
         Lock(DEvent& e, DWORD timeout = INFINITE);
         
         bool isTimeout() { return fTimeout; }
         
      private:
        bool fTimeout;
      };

   friend Lock;

private:
   HANDLE hEvent;
   bool   fManual;
};



class ITS_SPEC DCriticalSection
{
public:
   enum Mode { cmMulti, cmSingle };

   DCriticalSection(Mode mode = cmMulti);
   ~DCriticalSection();

   const Mode getMode() const { return mode; }

   class ITS_SPEC Lock
   {
   public:
      Lock(const DCriticalSection&);
      ~Lock();

   private:
      CRITICAL_SECTION *cSectionObj;
   };                                  
   friend Lock;               

private:
   CRITICAL_SECTION cSection;
   Mode mode;
};


}; // namespace DCommon


#endif // __DSYNC_H
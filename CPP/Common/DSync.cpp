/*******************************************************************************
 FILE         :   DSync.cpp

 COPYRIGHT    :   DMAlex, 2001

 DESCRIPTION  :   Synchronization support

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   02/28/2001

 LAST UPDATE  :   02/28/2001
*******************************************************************************/



#include "stdafx.h"

namespace DCommon
{


DSemaphore::DSemaphore(long initCoun, long maxCoun):
   hSem(INVALID_HANDLE_VALUE)
{
   hSem = CreateSemaphore(0, initCoun, maxCoun, 0);

   if(!hSem)
      throw XDSync("can't create semaphore (TSync)");
}


DSemaphore::~DSemaphore()
{
   if(hSem != INVALID_HANDLE_VALUE)
      CloseHandle(hSem);
}


void DSemaphore::release(long relCount)
{
   if(!ReleaseSemaphore(hSem, relCount, 0))
      throw XDSync("can't release semaphore (TSync)");
}



DSemaphore::Lock::Lock(DSemaphore& _sem, bool checkOnly):
   sem(_sem),
   fNonSignaled(false)
{
   DWORD res = WaitForSingleObject(sem.hSem, checkOnly ? 0 : INFINITE);

   if(res == WAIT_TIMEOUT)
      fNonSignaled = true;         
}


DSemaphore::Lock::~Lock()
{
   if(!fNonSignaled)
      ReleaseSemaphore(sem.hSem, 1, NULL);
}




DEvent::DEvent(bool _fManual, bool fSignaled):
   hEvent(INVALID_HANDLE_VALUE),
   fManual(_fManual)
{
   hEvent = CreateEvent(0, fManual, fSignaled, 0);

   if(!hEvent)
      throw XDSync("can't create event (TSync)");
}



DEvent::~DEvent()
{
   if(hEvent != INVALID_HANDLE_VALUE)
      CloseHandle(hEvent);
}



void DEvent::pulse()
{
   if(!PulseEvent(hEvent))
      throw XDSync("can't pulse event (TSync)");
}


void DEvent::set(bool fSignaled)
{
   if(fSignaled && !SetEvent(hEvent))
      throw XDSync("can't set event (TSync)");

   if(!fSignaled && !ResetEvent(hEvent))
      throw XDSync("can't reset event (TSync)");
}



DEvent::Lock::Lock(DEvent& e, DWORD timeout):
  fTimeout(false)
{
   fTimeout = WaitForSingleObject(e.hEvent, timeout) == WAIT_TIMEOUT;
}



DCriticalSection::DCriticalSection(Mode _mode):
   mode(_mode)
{
   if(mode == cmMulti)
      ::InitializeCriticalSection((LPCRITICAL_SECTION)&cSection);
}


DCriticalSection::~DCriticalSection()
{
   if(mode == cmMulti)
      ::DeleteCriticalSection((LPCRITICAL_SECTION)&cSection);
}


DCriticalSection::Lock::Lock(const DCriticalSection& sec):
   cSectionObj((CRITICAL_SECTION*)&sec.cSection)
{
   if(sec.mode != DCriticalSection::cmMulti)
      {
      cSectionObj = 0;
      return;
      }

   ::EnterCriticalSection(cSectionObj);
}


DCriticalSection::Lock::~Lock()
{
   if(cSectionObj)
      ::LeaveCriticalSection(cSectionObj);
}


}; // namespace DCommon

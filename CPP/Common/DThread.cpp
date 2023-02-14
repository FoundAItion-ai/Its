/*******************************************************************************
 FILE         :   DThread.cpp

 COPYRIGHT    :   DMAlex, 2001-2008

 DESCRIPTION  :   Multithreading support

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   02/28/2001

 LAST UPDATE  :   02/28/2001
*******************************************************************************/



#include "stdafx.h"
#include <process.h>


namespace DCommon
{


DThread::DThread():
   hThread(INVALID_HANDLE_VALUE),
   fExit(false),
   curState(DThread::inactive),
   eInit(true, false)
{
}



DThread::~DThread()
{
   if(hThread != INVALID_HANDLE_VALUE)
      ::CloseHandle(hThread);
}



void DThread::entryPoint(void *pThread)
{
   ((DThread*)pThread)->init();   
   ((DThread*)pThread)->run(); 
}


void DThread::beforeStart()
{
}


void DThread::beforeStop()
{
}


void DThread::init()
{
   if(!::DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &hThread,
                         0, false, DUPLICATE_SAME_ACCESS))
      hThread = INVALID_HANDLE_VALUE;

   eInit.set();
}


void DThread::start()
{
   if(curState != DThread::inactive)
      return;

   eInit.set(false);
   fExit = false;
   
   beforeStart();

   if(_beginthread(entryPoint, 0, (void*)this) != -1)
      curState = DThread::running;
}



void DThread::stop(DWORD timeOut)
{
   if(curState != DThread::running)
      return;         

   DEvent::Lock locker(eInit);
   fExit = true;
   
   beforeStop();

   if(hThread != INVALID_HANDLE_VALUE &&
      WaitForSingleObject(hThread, timeOut) == WAIT_OBJECT_0)   
      curState = DThread::inactive;
}



void DThread::suspend()
{
   if(curState != DThread::running)
      return;         

   if(SuspendThread(hThread) != -1)
      curState = DThread::suspended;
}



void DThread::resume()
{
   if(curState != DThread::suspended)
      return;         

   if(ResumeThread(hThread) != -1)
      curState = DThread::running;
}



void DThread::terminate()
{
   if(curState == DThread::inactive)
      return;
      
   if(TerminateThread(hThread, 0))
      curState = DThread::inactive;
}


}; // namespace DCommon

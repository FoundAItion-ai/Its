/*******************************************************************************
 FILE         :   DCommonIncl.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Export/import DLL declarations for common lib

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/19/2011

 LAST UPDATE  :   06/16/2011, renamed from DCommon.h 
*******************************************************************************/


#ifndef _DCOMMONINCL_H_
#define _DCOMMONINCL_H_


/* Disable warning messages C4251

" class '...' needs to have dll-interface to be used by clients of class 
  All your static data is access through functions that are exported from the DLL.

  No inlined methods of your class can modify static data.

  No inlined methods of your class use CRT functions or other library functions use static data 
  (see Potential Errors Passing CRT Objects Across DLL Boundaries for more information). 

  No methods of your class (regardless of inlining) can use types where the instantiation in 
  the EXE and DLL have static data differences." (c) MSDN



 Not really helpful, normally it must work ;-)

*/


#pragma warning( disable : 4251 )

#include "DCommonDefines.h"

#include <windows.h>

#include "DLog.h"
#include "DLogC.h"
#include "DSync.h"
#include "DThread.h"
#include "DException.h"


#endif // _DCOMMONINCL_H_

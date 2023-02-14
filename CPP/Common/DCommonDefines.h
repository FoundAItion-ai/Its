/*******************************************************************************
 FILE         :   DCommonDefines.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Export/import DLL declarations for common lib

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/19/2011

 LAST UPDATE  :   07/19/2011
 *******************************************************************************/


#ifndef __DCOMMONDEFINES_H
#define __DCOMMONDEFINES_H


#if defined(_MSC_VER)
   #define __ITS_EXPMODIF __declspec(dllexport)
   #define __ITS_IMPMODIF __declspec(dllimport)
#endif

#if defined(__ITS_DLL_BUILD)
   #define ITS_SPEC __ITS_EXPMODIF
#else
   #define ITS_SPEC __ITS_IMPMODIF
#endif


/*

***************** MSDN ***************** 

To minimize the possibility of data corruption when exporting a class with __declspec(dllexport), ensure that: 


All your static data is access through functions that are exported from the DLL.

No inlined methods of your class can modify static data.

No inlined methods of your class use CRT functions or other library functions use static 
data (see Potential Errors Passing CRT Objects Across DLL Boundaries for more information). 

No methods of your class (regardless of inlining) can use types where the instantiation 
in the EXE and DLL have static data differences.

You can avoid exporting classes by defining a DLL that defines a class with virtual functions, 
and functions you can call to instantiate and delete objects of the type. You can then just 
call virtual functions on the type.

*/

#endif // __DCOMMONDEFINES_H

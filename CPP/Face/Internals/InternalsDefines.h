/*******************************************************************************
 FILE         :   InternalsDefines.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Export defines for Helpers classes being used in Face

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/02/2012

 LAST UPDATE  :   07/02/2012
*******************************************************************************/


#ifndef __INTERNALSDEFINES_H
#define __INTERNALSDEFINES_H


#if defined(_MSC_VER)
   #define __INTERNALS_EXPMODIF __declspec(dllexport)
   #define __INTERNALS_IMPMODIF __declspec(dllimport)
#endif

#if defined(__INTERNALS_DLL_BUILD)
   #define INTERNALS_SPEC __INTERNALS_EXPMODIF
#else
   #define INTERNALS_SPEC __INTERNALS_IMPMODIF
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

#endif // __INTERNALSDEFINES_H

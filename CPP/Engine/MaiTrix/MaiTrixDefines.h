/*******************************************************************************
 FILE         :   MaiTrixDefines.h 

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   export defines 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/18/2011

 LAST UPDATE  :   08/18/2011
*******************************************************************************/


#ifndef __MAITRIXDEFINES_H
#define __MAITRIXDEFINES_H


#if defined(_MSC_VER)
   #define __MAITRIX_EXPMODIF __declspec(dllexport)
   #define __MAITRIX_IMPMODIF __declspec(dllimport)
#endif

#if defined(__MAITRIX_DLL_BUILD)
   #define MAITRIX_SPEC __MAITRIX_EXPMODIF
#else
   #define MAITRIX_SPEC __MAITRIX_IMPMODIF
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

#endif // __MAITRIXDEFINES_H

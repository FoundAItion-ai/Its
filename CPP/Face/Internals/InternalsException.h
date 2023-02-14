/*******************************************************************************
 FILE         :   InternalsException.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Internals - Exceptions for helpers classes used in Face

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/02/2012

 LAST UPDATE  :   07/02/2012
*******************************************************************************/


#ifndef __INTERNALSEXCEPTION_H
#define __INTERNALSEXCEPTION_H

#include "DCommonIncl.h"
#include "InternalsDefines.h"


using namespace DCommon;


namespace Its
{


// Do not inheric Engine exceptions
// as this is another subsystem - interface subsystem.
//
class INTERNALS_SPEC InternalsException: public DException
{
public:
  DEXCEPTION_CONSTRUCTOR0( InternalsException, DException );
  DEXCEPTION_CONSTRUCTOR1( InternalsException, DException );
  DEXCEPTION_CONSTRUCTOR2( InternalsException, DException );
  DEXCEPTION_CONSTRUCTOR3( InternalsException, DException );
};



}; // namespace Its


#endif // __INTERNALSEXCEPTION_H

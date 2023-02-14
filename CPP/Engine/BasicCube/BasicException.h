/*******************************************************************************
 FILE         :   BasicException.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Its base exception

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   10/06/2011

 LAST UPDATE  :   10/06/2011
*******************************************************************************/


#ifndef __BASICEXCEPTION_H
#define __BASICEXCEPTION_H


#include "BasicCubeDefines.h"
#include "DCommonIncl.h"


using namespace DCommon;


namespace Its
{


// Base exception for all errors in Its namespace
// move to Engine Common folder?
//
class BASICCUBE_SPEC BasicException: public DException
{
public:
  DEXCEPTION_CONSTRUCTOR0( BasicException, DException );
  DEXCEPTION_CONSTRUCTOR1( BasicException, DException );
  DEXCEPTION_CONSTRUCTOR2( BasicException, DException );
  DEXCEPTION_CONSTRUCTOR3( BasicException, DException );
};



// Engine subsystem exception 
class BASICCUBE_SPEC EngineException: public BasicException
{
public:
  DEXCEPTION_CONSTRUCTOR0( EngineException, BasicException );
  DEXCEPTION_CONSTRUCTOR1( EngineException, BasicException );
  DEXCEPTION_CONSTRUCTOR2( EngineException, BasicException );
  DEXCEPTION_CONSTRUCTOR3( EngineException, BasicException );
};




// Basic cube project exception 
class BASICCUBE_SPEC BasicCubeException: public EngineException
{
public:
  DEXCEPTION_CONSTRUCTOR0( BasicCubeException, EngineException );
  DEXCEPTION_CONSTRUCTOR1( BasicCubeException, EngineException );
  DEXCEPTION_CONSTRUCTOR2( BasicCubeException, EngineException );
  DEXCEPTION_CONSTRUCTOR3( BasicCubeException, EngineException );
};


}; // namespace Its


#endif // __BASICEXCEPTION_H

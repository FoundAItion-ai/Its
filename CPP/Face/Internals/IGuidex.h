/*******************************************************************************
 FILE         :   IGuidex.h

 COPYRIGHT    :   DMAlex, 2018

 DESCRIPTION  :   Integration point for everything, extended

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/29/2018

 LAST UPDATE  :   08/29/2018
*******************************************************************************/


#ifndef __IGUIDEX_H
#define __IGUIDEX_H

#include "InternalsDefines.h"
#include "IGuide.h"

using namespace std;

namespace Its
{

class INTERNALS_SPEC IGuideX: public IGuide
{
public:
  IGuideX();
  virtual ~IGuideX();

  void TakeSnapshot();
  static void TakeSnapshot( const char* maiTrixName, const char* maiTrixPath, const char* snapshotPath );  
};

}; // namespace Its


#endif // __IGUIDEX_H

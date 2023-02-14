/*******************************************************************************
 FILE         :   Init.h

 COPYRIGHT    :   DMAlex, 2012

 DESCRIPTION  :   Shared constants, defines

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   04/09/2012

 LAST UPDATE  :   04/09/2012
*******************************************************************************/


#ifndef __INIT_H
#define __INIT_H

#include "stdafx.h"
#include "Cube.h"


namespace Its
{

//
// DMA - TODO - Merge into a class and load from config?
//




// 20 Apr 2012 - DMA - Testing
//
// Somewhat puzzling results - optimized code works 20% faster on CPU
// but 20% slower on GPU!
//
// Global:
//

#define OPTIMIZE_IT       1 // Mostly on Cube level, plus MaiTrix layers caching
#define CUBE_GROWTH_RATE  3 // creating new cells vs suppression on negative feedback


//
// Generic:
//

#define MinByteValue                  0
#define MaxByteValue                  7



//
// MaiTrix related:
//

//#define INITIAL_SIZE_X                100
//#define INITIAL_SIZE_Y                100
//#define INITIAL_SIZE_Z                ( CubeSize * 3 )

#define INITIAL_SIZE_X                20
#define INITIAL_SIZE_Y                20
#define INITIAL_SIZE_Z                70


//
// Guide related:
//

#define INITIAL_DELAY                 0 // ( INITIAL_SIZE_X > INITIAL_SIZE_Y ? INITIAL_SIZE_X : INITIAL_SIZE_Y )

#define INITIAL_REPETITIONS           0 // means the real value will be determined in runtime
#define INITIAL_PERIOD_STATISTIC      1
#define INITIAL_PERIOD_SHOWMAITRIX    50

// What is loaded if no input, "gray data level" 
// There are 7 colors & zero level
#define NOSIGNAL                      0
#define SIGNAL                        1
#define MAXSIGNAL                     255



//
// Cell related:
//

#define INITIAL_TYPE                  NO_CELL
#define INITIAL_DATA                  0

// 0 1 2
// 7 8 3
// 6 5 4
#define INITIAL_UPLINK                0
#define INITIAL_REFLECTION            0
#define INITIAL_DEVIATION             0


#define INITIAL_PROBE_DIVERT_LOW      80  // VariableGravity force, divertion factor, 1-100%
#define INITIAL_PROBE_DIVERT_HIGH     90  // Unused, this one and below
#define INITIAL_PROBE_MIX_LOW         MinByteValue
#define INITIAL_PROBE_MIX_HIGH        MaxByteValue
#define INITIAL_PROBE_CREATE_LOW      1
#define INITIAL_PROBE_CREATE_HIGH     7


}; // namespace Its

#endif // __INIT_H

/*******************************************************************************
 FILE         :   DLogC.h

 COPYRIGHT    :   DMAlex, 2009

 DESCRIPTION  :   Log file (C-style declarations)

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/23/2009

 LAST UPDATE  :   01/23/2009
*******************************************************************************/


#ifndef __DLOGC_H
#define __DLOGC_H


#define DLog_LevelDebug 0 


namespace DCommon
{

// just a C-style call to DLog::msg()
extern "C" void ITS_SPEC DLog_msg( int level, const char *format, ... );

}; // namespace DCommon

#endif // __DLOGC_H

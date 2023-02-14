/*******************************************************************************
 FILE         :   Cube.h

 COPYRIGHT    :   DMAlex, 2011

 DESCRIPTION  :   Elementary Cube 

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   07/17/2011

 LAST UPDATE  :   08/06/2013
*******************************************************************************/


#ifndef __CUBE_H
#define __CUBE_H

// Disable warning messages C4251
//
// " class '...' needs to have dll-interface to be used by clients of class '...'"
//
// Not really helpful, normally it must work ;-)
//
#pragma warning( disable : 4251 )


namespace Its
{

#define CubeSize          3U
#define HyperCubeSize ( CubeSize + 2 ) // With "edges" +/- 1

// directions with gravity, may increase it later till 6
#define DIRECTIONS         1 

// 19 Jan 2016 - DMA - TODO: Cell types renaming/refactoring
// S_CELL is no longer needed, to be replaced with IO_CELL?
// A_, V_ & T_CELLs may be merged into IO_CELL;
// GOOD & BAD may be renamed to ACTIVE & EXCITED;

// Cell type
#define NO_CELL            0
#define GOOD_CELL          1
#define BAD_CELL           2

// Singularity cell, outer limit
#define S_CELL             3
// Internal error occured
#define DEAD_CELL          4

// Bi-directional I/O
#define A_CELL            11 // audio
#define V_CELL            12 // video
#define T_CELL            13 // text

#define IO_CELL( type )   ( type ==  A_CELL || type ==  V_CELL || type == T_CELL )


typedef unsigned char ByteIt;
typedef unsigned int  SizeIt;  // Note that unsigned types are dangerous in loops,
typedef unsigned long SizeItl; // be cautious. 
typedef unsigned long PeriodIt; 

// can be +/- for relative coordinates
typedef int PosIt;           

class Uplink {
};

//
// 1 direction cell, 143 bytes
//
//struct __declspec(align(16)) Cell
//
//
// ************************** NOTE **************************
//
// Changes of the structure requires changes in:
// 
// BasicCube.h,         like CopyCubeData / CopyCubeDataDirect...
// CubeUtils.h,         like MemoryArray
// Init.h,              like INITIAL_...
// MaiTrix.cpp,         like InitCell...
// CelllayerImage.cpp,  like CelllayerImage::SetPixel...
// Unit tests
//
struct Cell
{
  ByteIt type;                        // Cell type, Cell, NoCell
  ByteIt data;                        // Direct out
  ByteIt updata;                      // Mirrored data
  ByteIt uplink;                      // Current up connection (single)
  ByteIt reflection;                  // Current reflection (single or multi-directional)
  ByteIt shape       [ 256 * 4 / 8 ]; // Direct shape mapping, down -> uplink
  ByteIt reflections [ 8 ];           // Reversed shape mapping, uplink -> down
  ByteIt memory      [ 2 ];           // [0] lesson memory, [1] cycle memory
  ByteIt deviation;                   // Temporal deviation (mapping)
};


typedef Cell* Plane_t[ CubeSize * CubeSize ];


}; // namespace Its

#endif // __CUBE_H

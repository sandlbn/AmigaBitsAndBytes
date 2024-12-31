/*
   mpega.library (C) 1997-1998 St�phane TAVENARD
*/
#ifndef PRAGMAS_MPEGA_PRAGMAS_H
#define PRAGMAS_MPEGA_PRAGMAS_H

#ifndef CLIB_MPEGA_PROTOS_H
#include <clib/mpega_protos.h>
#endif

#pragma libcall MPEGABase MPEGA_open         01E 9802
#pragma libcall MPEGABase MPEGA_close        024 801
#pragma libcall MPEGABase MPEGA_decode_frame 02A 9802
#pragma libcall MPEGABase MPEGA_seek         030 0802
#pragma libcall MPEGABase MPEGA_time         036 9802
#pragma libcall MPEGABase MPEGA_find_sync    03C 0802
#pragma libcall MPEGABase MPEGA_scale        042 0802


#endif /* PRAGMAS_MPEGA_PRAGMAS_H */

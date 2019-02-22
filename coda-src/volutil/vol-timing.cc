/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <util.h>

extern int probingon;

#define TIMINGFILE "/tmp/timing.tmp"
static FILE *timingfile;
/*
  BEGIN_HTML
  <a name="S_VolTiming"><strong>Toggle timing flag and process timing trace</strong></a>
  END_HTML
*/
long S_VolTiming(RPC2_Handle rpcid, RPC2_Integer OnFlag,
                 SE_Descriptor *formal_sed)
{
    return RPC2_INVALIDOPCODE;
}

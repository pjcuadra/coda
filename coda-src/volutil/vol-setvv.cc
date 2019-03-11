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

/* vol-setvv.c */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <volutil.h>

#ifdef __cplusplus
}
#endif

/*
  S_VolSetVV: Set the version vector for a given object
*/
long S_VolSetVV(RPC2_Handle rpcid, RPC2_Unsigned formal_volid,
                RPC2_Unsigned vnodeid, RPC2_Unsigned unique,
                ViceVersionVector *vv)
{
    return RPC2_INVALIDOPCODE;
}

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

#include <res.h>

/*
  S_VolSetLogParms: Set the parameters for the resolution log
*/
long S_VolSetLogParms(RPC2_Handle rpcid, VolumeId Vid, RPC2_Integer OnFlag,
                      RPC2_Integer maxlogsize)
{
    return RPC2_INVALIDOPCODE;
}

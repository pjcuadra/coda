/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/




/*
 *
 * Specification of the Venus CallBack server.
 *
 */


#ifndef	_VASTRO_MARINER_H_
#define _VASTRO_MARINER_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "mariner.h"

enum chunk_state {
    CHUNK_STATE_UNKNOWN,
    CHUNK_STATE_CACHED,
    CHUNK_STATE_DISCARDED,
    CHUNK_STATE_ACTIVE,
};

void post(char * fid, uint64_t chunk_idx, enum chunk_state state);

void notify_mariner(char * fid, uint64_t start, int64_t len, enum chunk_state state);

#endif /* _VASTRO_MARINER_H_ */

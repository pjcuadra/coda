/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2018 Carnegie Mellon University
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

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <stdlib.h>


#ifdef __cplusplus
}
#endif

/* from venus */
#include "vastromariner.h"

void post(char *fid, uint64_t chunk_idx, enum chunk_state state) {
    MarinerLog("tomvis, %s, %d, %d, \n",  fid, chunk_idx, state);
}

void notify_mariner(char * fid, uint64_t start, int64_t len, enum chunk_state state) {
    uint64_t start_b = ccblock_start(start);
    uint64_t end_b = ccblock_end(start, len);
    for (uint64_t i = start_b; i < end_b; i++) {
        post(fid, i, state);
    }
}



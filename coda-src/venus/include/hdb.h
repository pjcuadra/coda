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

/*
 *  Hoard database management: first part used by Venus & hoard, latter only by Venus
 */

#ifndef _HDB_H_
#define _HDB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/param.h>
#include <time.h>
#include <netdb.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>
#include <coda.h>

#define HDB_ASSERT(ex)                                                  \
    {                                                                   \
        if (!(ex)) {                                                    \
            CHOKE("Assertion failed: file \"%s\", line %d\n", __FILE__, \
                  __LINE__);                                            \
        }                                                               \
    }

/* Hoard priority range. */
#define H_MAX_PRI 1000
#define H_DFLT_PRI 10
#define H_MIN_PRI 1

/* Hoard attribute flags. */
#define H_INHERIT 1 /* New children inherit their parent's hoard status. */
#define H_CHILDREN 2 /* Meta-expand directory to include its children. */
#define H_DESCENDENTS \
    4 /* Meta-expand directory to include all its descendents. */
#define H_DFLT_ATTRS 0

/* *****  Definition of pioctl message structures.  ***** */

/* Should be in venusioctl.h! -JJK */

struct hdb_clear_msg {
    uid_t cuid;
    int spare;
};

struct hdb_add_msg {
    VolumeId vid;
    char realm[MAXHOSTNAMELEN + 1];
    char name[CODA_MAXPATHLEN];
    int priority;
    int attributes;
};

struct hdb_delete_msg {
    VolumeId vid;
    char realm[MAXHOSTNAMELEN + 1];
    char name[CODA_MAXPATHLEN];
};

struct hdb_list_msg {
    char outfile[CODA_MAXPATHLEN];
    uid_t luid;
    int spare;
};

struct hdb_walk_msg {
    int spare;
};

struct hdb_verify_msg {
    char outfile[CODA_MAXPATHLEN];
    uid_t luid;
    int spare;
    int verbosity;
};

#endif /* _HDB_H_ */

/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
 *    Specification of the Venus Recoverable Storage manager.
 *
 */


#ifndef _VENUS_RECOV_SUBSYSTEM_H_
#define _VENUS_RECOV_SUBSYSTEM_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

/* Configuration constants */
const int UNSET_IMD = 0; /* do not initialize meta data */
#define	DFLT_RVMT UFS			/* meta data store type */
const unsigned long DFLT_VDDS =	0x400000; /* Venus meta-data device size */
const unsigned long UNSET_VDDS = (unsigned long)-1;
const unsigned long MIN_VDDS = 0x080000;
const int DataToLogSizeRatio = 4;
const unsigned long DFLT_VLDS =	DFLT_VDDS / DataToLogSizeRatio;	/* Venus log device size */
const unsigned long UNSET_VLDS = (unsigned long)-1;
const unsigned long MIN_VLDS = MIN_VDDS / DataToLogSizeRatio;
const int DFLT_RDSCS = 64;		/* RDS chunk size */
const int UNSET_RDSCS = -1;
const int DFLT_RDSNL = 16;		/* RDS nlists */
const int UNSET_RDSNL = -1;
const int DFLT_CMFP = 600;		/* Connected-Mode Flush Period */
const int UNSET_CMFP = -1;
const int DFLT_DMFP = 30;		/* Disconnected-Mode Flush Period */
const int UNSET_DMFP = -1;
const int DFLT_MAXFP = 3600;		/* Maximum Flush Period */
const int UNSET_MAXFP = -1;
const int DFLT_WITT = 60;		/* Worker-Idle time threshold */
const int UNSET_WITT = -1;
const unsigned long DFLT_MAXFS = 64 * 1024;	/* Maximum Flush-Buffer Size */
const unsigned long UNSET_MAXFS = (unsigned long)-1;
const unsigned long DFLT_MAXTS = 256 * 1024;	/* Maximum Truncate Size */
const unsigned long UNSET_MAXTS = (unsigned long)-1;

struct venusrecov_config {
    int InitMetaData;
    int InitNewInstance;
    rvm_type_t RvmType;
    const char *VenusLogDevice;
    unsigned long VenusLogDeviceSize;
    const char *VenusDataDevice;
    unsigned long VenusDataDeviceSize;
    int RdsChunkSize;
    int RdsNlists;
    int CMFP;
    int DMFP;
    int WITT;
    int MAXFP;
    unsigned long MAXFS;
    unsigned long MAXTS;
};

static venusrecov_config venusrecov_default_config = {
    .InitMetaData = UNSET_IMD,
    .InitNewInstance = UNSET_IMD,
    .RvmType = UNSET,
    .VenusLogDevice = NULL,
    .VenusLogDeviceSize = UNSET_VLDS,
    .VenusDataDevice = NULL,
    .VenusDataDeviceSize = UNSET_VDDS,
    .RdsChunkSize = UNSET_RDSCS,
    .RdsNlists = UNSET_RDSNL,
    .CMFP = UNSET_CMFP, 
    .DMFP = UNSET_DMFP,
    .WITT = UNSET_WITT,
    .MAXFP = UNSET_MAXFP,
    .MAXFS = UNSET_MAXFS,
    .MAXTS = UNSET_MAXTS
};


/*  *****  Functions  *****  */
void RecovSetup(struct venusrecov_config config);
void RecovInit();
void RecovTerminate();
int RecovIsInitialized();

#endif /* _VENUS_RECOV_SUBSYSTEM_H_ */

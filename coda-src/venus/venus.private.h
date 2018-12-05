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
 * Manifest constants for Venus, plus declarations for source files without
 * their own headers.
 */


#ifndef	_VENUS_PRIVATE_H_
#define _VENUS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>

#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <util.h>
#ifdef __cplusplus
}
#endif

#include "coda_assert.h"

/* interfaces */
#include <vice.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "venusstats.h"
#include "venusfid.h"
#include "venusconsts.private.h"

/*  *****  Locking macros.  *****  */

enum LockLevel { NL, RD, SH, WR };

#define ObtainLock(lock, level)\
{\
    switch(level) {\
	case RD:\
	    ObtainReadLock(lock);\
	    break;\
\
	case SH:\
	    ObtainSharedLock(lock);\
	    break;\
\
	case WR:\
	    ObtainWriteLock(lock);\
	    break;\
\
	case NL:\
	default:\
	    CODA_ASSERT(0);\
    }\
}

#define ReleaseLock(lock, level)\
{\
    switch(level) {\
	case RD:\
	    ReleaseReadLock(lock);\
	    break;\
\
	case SH:\
	    ReleaseSharedLock(lock);\
	    break;\
\
	case WR:\
	    ReleaseWriteLock(lock);\
	    break;\
\
	case NL:\
	default:\
	    CODA_ASSERT(0);\
    }\
}


/*  *****  Timing macros.  *****  */
#ifdef	TIMING
#define SubTimes(end, start) \
    ((((end)->tv_sec  - (start)->tv_sec)  * 1000.0) + \
     (((end)->tv_usec - (start)->tv_usec) / 1000.0))

#define	START_TIMING()\
    struct timeval StartTV, EndTV;\
    gettimeofday(&StartTV, 0);

#define END_TIMING()\
    gettimeofday(&EndTV, 0);\
    float elapsed;\
    elapsed = SubTimes(&EndTV, &StartTV);
#else
#define	SubTimes(end, start) (0.0)
#define START_TIMING()
#define END_TIMING()\
    float elapsed = 0.0;
#endif /* !TIMING */


/*  *****  Cache Stuff *****  */
enum CacheType {    ATTR,
		    DATA
};

#undef WRITE

enum CacheEvent	{   HIT,
		    MISS,
		    RETRY,
		    TIMEOUT,
		    NOSPACE,
		    FAILURE,
		    CREATE,
		    WRITE,
		    REMOVE,
		    REPLACE
};

/* "blocks" field is not relevant for all events */
/* "blocks" is constant for (relevant) ATTR events */
/* Now the same struct named CacheEventEntry is generated from mond.rpc2 */
struct CacheEventRecord {
   int count;
   int blocks;
};

struct CacheStats {
    CacheEventRecord events[10];	    /* indexed by CacheEvent type! */
};

/*  *****  Misc stuff  *****  */
#define TRANSLATE_TO_LOWER(s)\
{\
    for (char *c = s; *c; c++)\
	if (isupper(*c)) *c = tolower(*c);\
}
#define TRANSLATE_TO_UPPER(s)\
{\
    for (char *c = s; *c; c++)\
	if (islower(*c)) *c = toupper(*c);\
}

void rds_printer(char * ...);
const char *VenusOpStr(int);
const char *IoctlOpStr(unsigned char nr);
const char *VenusRetStr(int);
void VVPrint(FILE *, ViceVersionVector **);
int binaryfloor(int);
void Terminate();
void DumpState();
void RusagePrint(int);
void RPCPrint(int);
void GetCSS(RPCPktStatistics *);
void SubCSSs(RPCPktStatistics *, RPCPktStatistics *);
void MallocPrint(int);
void StatsInit();
void ToggleMallocTrace();
const char *lvlstr(LockLevel);
int GetTime(long *, long *);
time_t Vtime();
int FAV_Compare(ViceFidAndVV *, ViceFidAndVV *);
void DaemonInit();
void FireAndForget(const char *name, void (*f)(void), int interval,
		   int stacksize=32*1024);
void RegisterDaemon(unsigned long, char *);
void DispatchDaemons();

void VenusPrint(int argc, const char **argv);

void VenusPrint(FILE *, int argc, const char **argv);

void VenusPrint(int, int argc, const char **argv);

void VFSPrint(int);

/* Helper to add a file descriptor with callback to main select loop. */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata);

extern long int RPC2_DebugLevel;
extern long int SFTP_DebugLevel;
extern long int RPC2_Trace;
extern int MallocTrace;
extern const VenusFid NullFid;
extern const ViceVersionVector NullVV;
extern VFSStatistics VFSStats;
extern RPCOpStatistics RPCOpStats;
extern struct timeval DaemonExpiry;

/* venus.c */
class vproc;
extern vproc *Main;
extern VenusFid rootfid;
extern int parent_fd;
extern long rootnodeid;
extern int CleanShutDown;
extern const char *venusRoot;
extern const char *kernDevice;
extern const char *realmtab;
extern const char *CacheDir;
extern const char *CachePrefix;
extern unsigned int CacheBlocks;
extern const char *SpoolDir;
extern uid_t PrimaryUser;
extern const char *VenusPidFile;
extern const char *VenusControlFile;
extern const char *consoleFile;
extern const char *MarinerSocketPath;
extern int   mariner_tcp_enable;
extern int   plan9server_enabled;
extern int   nofork;
extern int   allow_reattach;
extern int   masquerade_port;
extern int   PiggyValidations;
extern int   T1Interval;
extern const char *ASRLauncherFile;
extern const char *ASRPolicyFile;
extern int   option_isr;
extern int   detect_reintegration_retry;
extern int   redzone_limit, yellowzone_limit;

/* spool.cc */
extern void MakeUserSpoolDir(char *, uid_t);


/* ASR misc */
/* Note: At some point, it would be nice to run ASRs in different
 * volumes concurrently. This requires replacing the globals
 * below with a table or other data structure. Due to token
 * assignment constraints, though, this is not possible as of 06/2006.
 */

extern pid_t ASRpid;
extern VenusFid ASRfid;
extern uid_t ASRuid;

#endif /* _VENUS_PRIVATE_H_ */

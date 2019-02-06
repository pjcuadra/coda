/* BLURB lgpl

			    Coda File System
				Release 5

	    Copyright (c) 1987-2008 Carnegie Mellon University
		    Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

/*
			    IBM COPYRIGHT NOTICE

			    Copyright (C) 1986
		International Business Machines Corporation
			    All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.
*/

#ifndef _LWP_LWP_H_
#define _LWP_LWP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include <valgrind/callgrind.h>

/*
    LWP_VERSION is first argument to LWP_Init().
    Ensures header matches library.
*/
/* Change this if you make an incompatible change of some kind */
#if !defined(_REENTRANT) && !defined(_THREAD_SAFE)
#define LWP_VERSION 210888002 /* old lwp */
#else
#define LWP_VERSION 210888003 /* pthreaded lwp */
#endif

#define LWP_SUCCESS 0
#define LWP_EBADPID -1
#define LWP_EBLOCKED -2
#define LWP_EINIT -3
#define LWP_EMAXPROC -4
#define LWP_ENOBLOCK -5
#define LWP_ENOMEM -6
#define LWP_ENOPROCESS -7
#define LWP_ENOWAIT -8
#define LWP_EBADCOUNT -9
#define LWP_EBADEVENT -10
#define LWP_EBADPRI -11
#define LWP_NO_STACK -12
/* These two are for the signal mechanism. */
#define LWP_EBADSIG -13 /* bad signal number */
#define LWP_ESYSTEM -14 /* system call failed */
/* These are for the rock mechanism */
#define LWP_ENOROCKS -15 /* all rocks are in use */
#define LWP_EBADROCK -16 /* the specified rock does not exist */

/* Maximum priority permissible (minimum is always 0) */
#define LWP_MAX_PRIORITY 4

/* Usual priority used by user LWPs */
#define LWP_NORMAL_PRIORITY (LWP_MAX_PRIORITY - 1)

/* Users aren't really supposed to know what a pcb is, but .....*/
typedef struct lwp_pcb *PROCESS;

extern int lwp_debug; /* ON = show LWP debugging trace */

/* Action to take on stack overflow. */
#define LWP_SOQUIET 1 /* do nothing */
#define LWP_SOABORT 2 /* abort the program */
#define LWP_SOMESSAGE 3 /* print a message and be quiet */
extern int lwp_overflowAction;

/* Tells if stack size counting is enabled. */
extern int lwp_stackUseEnabled;

/* variables used for checking work time of an lwp */
extern struct timeval last_context_switch; /* how long a lwp was running */
extern struct timeval cont_sw_threshold; /* how long a lwp is allowed to run */

extern struct timeval run_wait_threshold;

void LWP_SetLog(FILE *file, int level);
extern int LWP_QWait();
extern int LWP_QSignal(PROCESS pid);
extern int LWP_Init(int version, int priority, PROCESS *pid);
extern int LWP_TerminateProcessSupport();
extern int LWP_CreateProcess(void (*ep)(void *), int stacksize, int priority,
                             void *parm, const char *name, PROCESS *pid);
extern int LWP_CurrentProcess(PROCESS *pid);
PROCESS LWP_ThisProcess();
extern void LWP_SetLog(FILE *, int);
extern int LWP_DestroyProcess(PROCESS pid);
extern int LWP_DispatchProcess();
extern int LWP_GetProcessPriority(PROCESS pid, int *priority);
extern int LWP_INTERNALSIGNAL(const void *event, int yield);
extern int LWP_WaitProcess(const void *event);
extern int LWP_MwaitProcess(int wcount, const void *evlist[]);
extern int LWP_StackUsed(PROCESS pid, int *max, int *used);
extern int LWP_NewRock(int Tag, char *Value);
extern int LWP_GetRock(int Tag, char **Value);
extern char *LWP_Name(); /* NOTE: returs a char * !! */
extern int LWP_Index();
extern int LWP_HighestIndex();
extern void LWP_UnProtectStacks(); /* only available for newlwp */
extern void LWP_ProtectStacks();

#define LWP_SignalProcess(event) LWP_INTERNALSIGNAL(event, 1)
#define LWP_NoYieldSignal(event) LWP_INTERNALSIGNAL(event, 0)

void PRE_Concurrent(int on);
void PRE_BeginCritical(void);
void PRE_EndCritical(void);

/* extern definitions for the io manager routines */
extern int IOMGR_SoftSig(void (*aproc)(void *), char *arock);
extern int IOMGR_Initialize();
extern int IOMGR_Finalize();
extern int IOMGR_Poll();
extern int IOMGR_Select(int fds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout);
extern int IOMGR_Cancel(PROCESS pid);
extern int IOMGR_Signal(int signo, char *event);
extern int IOMGR_CancelSignal(int signo);

/* declarations for fasttime.c routines */
extern int FT_Init(int printErrors, int notReally);
extern int FT_GetTimeOfDay(struct timeval *tv, struct timezone *tz);
extern int TM_GetTimeOfDay(struct timeval *tv, struct timezone *tz);
extern int FT_AGetTimeOfDay(struct timeval *tv, struct timezone *tz);
extern unsigned int FT_ApproxTime();

void ProfileInit(bool state);

void ProfileEnableSet(bool state);

#ifdef __cplusplus
}
#endif

#endif /* _LWP_LWP_H_ */

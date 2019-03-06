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
 *
 * Implementation of the Server Monitor.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <errno.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include "mond.h"
#include "vice.h"
#include <callback.h>

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <voldump.h>

/* *****  Private constants  ***** */

#define DFLT_MONDHOST "barber.coda.cs.cmu.edu"
#define DFLT_MONDPORT 1356

static const int SmonMaxDataSize      = 1024 * 1024;
static const int SmonBindInterval     = 300;
static const int TIMERINTERVAL        = 3600;
static const int callReportInterval   = 4 * 60 * 60;
static const int rvmresReportInterval = 4 * 60 * 60;

/* Need this for the definition of rvmrese */

static RPC2_Handle SmonHandle = 0;

/* ***** Private types ***** */

static SmonViceId MyViceId;

/* OverflowEvent Entry */
struct smoe {
    SmonViceId Vice;
    RPC2_Unsigned Time;
    RPC2_Unsigned StartTime;
    RPC2_Unsigned EndTime;
    RPC2_Integer Count;

    void Init(RPC2_Unsigned time, RPC2_Unsigned starttime,
              RPC2_Unsigned endtime, RPC2_Integer count)
    {
        Vice      = MyViceId;
        Time      = time;
        StartTime = starttime;
        EndTime   = endtime;
        Count     = count;
    }
};

/* ***** Private variables  ***** */

static int SmonEnabled = 0;
static int SmonInited  = 0;
static smoe SOE;
static olist *RVMResList                 = 0;
static unsigned long LastSmonBindAttempt = 0;
static SmonStatistics stats;

/* ***** Private routines  ***** */

static void CheckCallStat(); /* Daemon Report Entries */
static int ValidateSmonHandle();
static long CheckSmonResult(long);
static int GetRawStatistics(SmonStatistics *);

/*  *****  External variables  *****  */

const char *SmonHost = DFLT_MONDHOST; /* may be overridden from command line */
int SmonPort         = DFLT_MONDPORT; /* may be overridden from command line */
/*  *****  Smon  *****  */

static void SmonInit()
{
    MyViceId.IPAddress = gethostid();
    MyViceId.BirthTime = ::time(0);
    SOE.Init(0, 0, 0, 0);
    RVMResList = new olist;

    SmonInited = 1;
}

/*
** we really only need to send data if the mond is running,
** enqueuing is a waste of resources, since the new result will
** be wiped out by the old result anyway.
*/

static void CheckCallStat()
{
    static unsigned long last_time = 0;

    if (!SmonEnabled || !SmonInited || !ValidateSmonHandle())
        return;
    unsigned long curr_time = ::time(0);
    if ((long)(curr_time - last_time) > callReportInterval) {
        last_time = curr_time;
        GetRawStatistics(&stats);
        long code = SmonReportCallEvent(SmonHandle, &MyViceId, curr_time,
                                        cbOPARRAYSIZE, cb_CallCount, 0, 0,
                                        mondOPARRAYSIZE, mond_CallCount,
                                        volDumpOPARRAYSIZE, volDump_CallCount,
                                        NULL, 0, &stats);
        code      = CheckSmonResult(code);
    }
}

static void CheckSOE()
{
    if (SOE.Count > 0) {
        if (!SmonEnabled || !SmonInited)
            return;

        unsigned long curr_time = ::time(0);

        SOE.EndTime = curr_time;
        long code   = SmonReportOverflow(SmonHandle, &SOE.Vice, curr_time,
                                       SOE.StartTime, SOE.EndTime, SOE.Count);
        code        = CheckSmonResult(code);
        if (code != 0)
            return;

        SOE.Init(0, 0, 0, 0);
    }
}

static int ValidateSmonHandle()
{
    RPC2_HostIdent hid;
    RPC2_PortIdent pid;
    RPC2_SubsysIdent sid;
    RPC2_BindParms bp;

    if (SmonHandle != 0)
        return (1);

    long curr_time = ::time(0);

    /* Try to bind unless the most recent attempt was within SmonBindInterval seconds. */
    if ((long)(curr_time - LastSmonBindAttempt) < SmonBindInterval)
        return (0);

    /* Attempt the bind. */
    LastSmonBindAttempt = curr_time;

    hid.Tag = RPC2_HOSTBYNAME;
    strcpy(hid.Value.Name, SmonHost);

    pid.Tag                  = RPC2_PORTBYINETNUMBER;
    pid.Value.InetPortNumber = htons(SmonPort);

    sid.Tag            = RPC2_SUBSYSBYID;
    sid.Value.SubsysId = MondSubsysId;

    bp.SecurityLevel  = RPC2_OPENKIMONO;
    bp.EncryptionType = 0;
    bp.SideEffectType = 0;
    bp.ClientIdent    = NULL;
    bp.SharedSecret   = NULL;

    long code = RPC2_NewBinding(&hid, &pid, &sid, &bp, &SmonHandle);

    LogMsg(1, SrvDebugLevel, stdout,
           "ValidateSmonHandle: bind to [ %s, %d, %d ] returned (%d, %d)",
           SmonHost, SmonPort, MondSubsysId, code, SmonHandle);
    if (code != 0) {
        SmonHandle = 0;
        return (0);
    }

    /* Successful bind. */

    LogMsg(0, SrvDebugLevel, stdout, "Trying to establish a connection");
    code = (int)MondEstablishConn(SmonHandle, MOND_CURRENT_VERSION,
                                  MOND_VICE_CLIENT, 0, (SpareEntry *)NULL);
    LogMsg(0, SrvDebugLevel, stdout, "Connection establishment returned %d",
           code);
    if (code != MOND_OK && code != MOND_OLDVERSION && code != MOND_CONNECTED) {
        LogMsg(0, SrvDebugLevel, stdout,
               "ValidateSmonHandle: MondEstablishConn failed with return (%d)",
               code);
        RPC2_Unbind(SmonHandle);
        SmonHandle = 0;
        return (0);
    }
    if (code == MOND_OLDVERSION)
        LogMsg(
            0, SrvDebugLevel, stdout,
            "ValidateSmonHandle: MondEstablishConn recieved MOND_OLDVERSION");

    return (1);
}

static long CheckSmonResult(long code)
{
    if (code == 0)
        return (0);

    LogMsg(1, SrvDebugLevel, stdout, "CheckSmonResult: failure (%d)", code);

    RPC2_Unbind(SmonHandle);
    SmonHandle = 0;

    return (ETIMEDOUT);
}

static int GetRawStatistics(SmonStatistics *stats)
{
#ifdef __MACH__
#ifdef __APPLE__
#warning Need to port this to Mach 3.0 on Mac OS X
    return -1;
#else
    static int kmem  = 0;
    static int hertz = 0;
    int i;
    long busy[CPUSTATES];
    long xfer[DK_NDRIVE];
    struct timeval bootTime;

    if (kmem == -1) {
        return -1;
    }

    if (kmem == 0) {
        nlist("/vmunix", RawStats);
        if (RawStats[0].n_type == 0) {
            kmem = -1;
            return -1;
        }
        kmem = open("/dev/kmem", 0, 0);
        if (kmem <= 0) {
            kmem = -1;
            return -1;
        }
        /* if phz is non-zero, that is the clock rate
	   for statistics gathering used by the kernel.
	   Otherwise, use hz
	 */
        lseek(kmem, (long)RawStats[PHZ].n_value, 0);
        read(kmem, (char *)&hertz, sizeof(hertz));
        if (hertz == 0) {
            lseek(kmem, (long)RawStats[HZ].n_value, 0);
            read(kmem, (char *)&hertz, sizeof(hertz));
        }
        /* XXX - if hertz is still 0, use the BSD default of 100 */
        if (hertz == 0)
            hertz = 100;
    }

    stats->UserCPU   = 0;
    stats->SystemCPU = 0;
    stats->IdleCPU   = 0;
    stats->BootTime  = 0;
    stats->TotalIO   = 0;

    lseek(kmem, (long)RawStats[CPTIME].n_value, 0);
    read(kmem, (char *)busy, sizeof(busy));
    stats->SystemCPU = busy[CP_SYS] / hertz;
    stats->UserCPU   = (busy[CP_USER] + busy[CP_NICE]) / hertz;
    stats->IdleCPU   = busy[CP_IDLE] / hertz;
    lseek(kmem, (long)RawStats[BOOT].n_value, 0);
    read(kmem, (char *)&bootTime, sizeof(bootTime));
    stats->BootTime = bootTime.tv_sec;
    lseek(kmem, (long)RawStats[DISK].n_value, 0);
    read(kmem, (char *)xfer, sizeof(xfer));
    for (i = 0; i < DK_NDRIVE; i++) {
        stats->TotalIO += xfer[i];
    }
#endif /* !__APPLE__ */
#else
    return 0;
#endif
}

// *********************************************************************
// 			Smon Daemon Definition
// *********************************************************************

static const int SmonDaemonInterval = TIMERINTERVAL;

void SmonDaemon(void *)
{
    struct timeval time;
    SmonInit();

    time.tv_sec  = SmonDaemonInterval;
    time.tv_usec = 0;

    while (1) {
        LogMsg(0, SrvDebugLevel, stdout, "Starting SmonDaemon timer");
        if (IOMGR_Select(0, 0, 0, 0, &time) == 0) {
            LogMsg(0, SrvDebugLevel, stdout, "SmonDaemon timer expired");
            CheckCallStat();
            CheckSOE();

            if (time.tv_sec != SmonDaemonInterval)
                time.tv_sec = SmonDaemonInterval;
        }
    }
}

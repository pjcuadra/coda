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
 *    Implementation of the Venus Recoverable Storage manager.
 *
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef HAVE_OSRELDATE_H
#include <osreldate.h>
#endif

/* from rvm */
#include <rvm/rds.h>
#include <rvm/rvm.h>
#include <rvm/rvm_segment.h>
#include <rvm/rvm_statistics.h>

/* function defined in rpc2.private.h, which we need to seed the random
 * number generator, _before_ we create a new VenusGenId. */
void rpc2_InitRandom();

#ifdef __cplusplus
}
#endif

#include <venus/fso/fso.h>
#include <venus/hdb.h>
#include <venus/local.h>
#include <venus/mariner.h>
#include "venus.private.h"
#include <venus/recov/recov.h>
#include <venus/worker.h>

int Recov::TimeToFlush = 0;

Recov *Recov::impl = NULL;

/*  *****  Recovery Module  *****  */

int RecovVenusGlobals::validate()
{
    if (recov_MagicNumber != RecovMagicNumber)
        return (0);
    if (recov_VersionNumber != RecovVersionNumber)
        return (0);

    if (recov_CleanShutDown != 0 && recov_CleanShutDown != 1)
        return (0);

    if (!VALID_REC_PTR(recov_FSDB))
        return (0);
    if (!VALID_REC_PTR(recov_VDB))
        return (0);
    if (!VALID_REC_PTR(recov_REALMDB))
        return (0);
    if (!VALID_REC_PTR(recov_HDB))
        return (0);

    return (1);
}

void RecovVenusGlobals::print()
{
    print(stdout);
}

void RecovVenusGlobals::print(FILE *fp)
{
    print(fileno(fp));
}

/* local-repair modification */
void RecovVenusGlobals::print(int fd)
{
    fdprint(fd, "RVG values: what they are (what they should be)\n");
    fdprint(fd, "Magic = %x(%x), Version = %d(%d), CleanShutDown= %d(0 or 1)\n",
            recov_MagicNumber, RecovMagicNumber, recov_VersionNumber,
            RecovVersionNumber, recov_CleanShutDown);
    fdprint(fd, "The following pointers should be between %p and %p:\n",
            recov_HeapAddr, recov_HeapAddr + recov_HeapLength);
    fdprint(fd, "Ptrs = [%p %p %p %p], Heap = [%p] HeapLen = %x\n", recov_FSDB,
            recov_VDB, recov_HDB, recov_REALMDB, recov_HeapAddr,
            recov_HeapLength);

    fdprint(fd, "UUID = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
            ntohl(recov_UUID.fields.time_low),
            ntohs(recov_UUID.fields.time_mid),
            ntohs(recov_UUID.fields.time_hi_version),
            recov_UUID.fields.clock_seq_hi_variant,
            recov_UUID.fields.clock_seq_low, recov_UUID.fields.node[0],
            recov_UUID.fields.node[1], recov_UUID.fields.node[2],
            recov_UUID.fields.node[3], recov_UUID.fields.node[4],
            recov_UUID.fields.node[5]);
    fdprint(fd, "StoreId = %d\n", recov_StoreId);
}

/*  *****  RVM String Routines  *****  */

RPC2_String Copy_RPC2_String(RPC2_String &src)
{
    int len = (int)strlen((char *)src) + 1;

    RPC2_String tgt = (RPC2_String)Recov::getInstance()->malloc(len);
    Recov::getInstance()->recordRange(tgt, len);
    memcpy(tgt, src, len);

    return (tgt);
}

void Free_RPC2_String(RPC2_String &STR)
{
    Recov::getInstance()->free(STR);
}

/*  *****  recov_daemon.c  *****  */

static const int RecovDaemonInterval = 5;
static const int RecovDaemonStackSize =
    262144; /* MUST be big to handle rvm_trucates! */

static char recovdaemon_sync;

void RECOVD_Init(void)
{
    (void)new vproc("RecovDaemon", &RecovDaemon, VPT_RecovDaemon,
                    RecovDaemonStackSize);
}

void RecovDaemon(void)
{
    /* Hack!!!  Vproc must yield before data members become valid! */
    /* suspect interaction between LWP creation/dispatch and C++ initialization. */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(RecovDaemonInterval, &recovdaemon_sync);

    for (;;) {
        VprocWait(&recovdaemon_sync);

        /* First task is to get statistics. */
        Recov::getInstance()->getStatistics();

        /* Truncating. */
        Recov::getInstance()->truncate();

        /* Flushing. */
        Recov::getInstance()->setTimeToFlush(
            Recov::getInstance()->getTimeToFlush() - RecovDaemonInterval);
        Recov::getInstance()->flush();

        /* Bump sequence number. */
        vp->seq++;
    }
}

int Recov::getTimeToFlush()
{
    return Recov::TimeToFlush;
}

void Recov::setTimeToFlush(int ttf)
{
    Recov::TimeToFlush = ttf;
}

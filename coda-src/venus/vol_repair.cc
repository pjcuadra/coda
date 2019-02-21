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
 * Implementation of the Venus Repair facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <rpc2/rpc2.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <inconsist.h>
#include <copyfile.h>

/* from libal */
#include <prs.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "realmdb.h"

/* Enable ASR invocation for this volume (as a volume service)  */
void reintvol::EnableASR(uid_t uid)
{
    LOG(100, ("reintvol::EnableASR: vol = %x, uid = %d\n", vid, uid));

    /* Place volume in "repair mode." */
    if (IsASREnabled())
        LOG(0, ("volent::EnableASR: ASR for %x already enabled", vid));

    flags.enable_asrinvocation = 1;
}

int reintvol::DisableASR(uid_t uid)
{
    LOG(100, ("reintvol::DisableASR: vol = %x, uid = %d\n", vid, uid));

    if (asr_running())
        return EBUSY;

    if (!IsASREnabled())
        LOG(0, ("volent::DisableASR: ASR for %x already disabled", vid));

    flags.enable_asrinvocation = 0;

    return 0;
}

/* Allow ASR invocation for this volume (this is user's permission). */
int reintvol::AllowASR(uid_t uid)
{
    LOG(100, ("reintvol::AllowASR: vol = %x, uid = %d\n", vid, uid));

    /* Place volume in "repair mode." */
    if (IsASRAllowed())
        LOG(0, ("volent::AllowASR: ASR for %x already allowed", vid));

    flags.allow_asrinvocation = 1;

    return 0;
}

int reintvol::DisallowASR(uid_t uid)
{
    LOG(100, ("reintvol::DisallowASR: vol = %x, uid = %d\n", vid, uid));

    if (asr_running())
        return EBUSY;

    if (!IsASRAllowed())
        LOG(0, ("volent::DisallowASR: ASR for %x already disallowed", vid));

    flags.allow_asrinvocation = 0;

    return 0;
}

void reintvol::lock_asr()
{
    CODA_ASSERT(flags.asr_running == 0);
    flags.asr_running = 1;
}

void reintvol::unlock_asr()
{
    CODA_ASSERT(flags.asr_running == 1);
    flags.asr_running = 0;
}

void reintvol::asr_pgid(pid_t new_pgid)
{
    CODA_ASSERT(flags.asr_running == 1);
    pgid = new_pgid;
}

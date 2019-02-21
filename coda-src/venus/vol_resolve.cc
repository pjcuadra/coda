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
 *    Implementation of the Venus Resolve facility.
 *
 *    ToDo:
 *       1. Simplify.  There is no need to Mwait on an array of fids! (waiting on a single fid will suffice)
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <struct.h>

#include <unistd.h>
#include <stdlib.h>

/* interfaces */
#include <vice.h>
#ifdef __cplusplus
}
#endif

/* from util */
#include <olist.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"

#define MAX_REQUEUE 3

#ifdef VENUSDEBUG
int resent::allocs   = 0;
int resent::deallocs = 0;
#endif

/* Asynchronous resolve is indicated by NULL waitblk. */
void repvol::ResSubmit(char **waitblkp, VenusFid *fid, resent **requeue)
{
    if (requeue && !(*requeue)->requeues)
        return;

    if (fid->Realm != realm->Id() || fid->Volume != vid ||
        (requeue && FID_EQ(fid, &(*requeue)->fid)))
        *fid = NullFid;

    /* Create a new resolver entry for the fid, unless one already exists. */
    if (!FID_EQ(fid, &NullFid)) {
        olist_iterator next(*res_list);
        resent *r = NULL;
        while ((r = (resent *)next()))
            if (FID_EQ(&r->fid, fid))
                break;

        if (r == NULL) {
            r = new resent(fid);
            res_list->append(r);
        }

        /* Bump refcnt and set up pointer to resent in case of SYN resolve. */
        if (waitblkp) {
            r->refcnt++;
            *(resent **)waitblkp = r;
        }
    }

    /* Append requeue'd entries at the end of the list (this way they are
     * guaranteed to be behind whatever entry we just found or created) */
    if (requeue) {
        if (FID_EQ(fid, &NullFid))
            (*requeue)->requeues = 0;
        else
            (*requeue)->requeues--;

        res_list->append(*requeue);
        *requeue = NULL;
    } else {
        /* Force volume state transition at next convenient point. */
        flags.transition_pending = 1;
    }

    /* Demote the object (if cached). */
    fsobj *f = FSDB->Find(fid);
    if (f)
        f->Demote();
}

/* Wait for resolve to complete. */
int repvol::ResAwait(char *waitblk)
{
    int code = 0;

    resent *r = (resent *)waitblk;
    while (r->result == -1)
        VprocWait((char *)r);

    code = r->result;
    if (--(r->refcnt) == 0)
        delete r;

    return (code);
}

/* *****  Resent  ***** */

resent::resent(VenusFid *Fid)
{
    LOG(10, ("resent::resent: fid = (%s)\n", FID_(Fid)));

    fid      = *Fid;
    result   = -1;
    refcnt   = 0;
    requeues = MAX_REQUEUE;

#ifdef VENUSDEBUG
    allocs++;
#endif
}

/*
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
resent::resent(resent &r)
{
    abort();
}

int resent::operator=(resent &r)
{
    abort();
    return (0);
}

resent::~resent()
{
#ifdef VENUSDEBUG
    deallocs++;
#endif

    LOG(10, ("resent::~resent: fid = (%s)\n", FID_(&fid)));

    CODA_ASSERT(refcnt == 0);
}

void resent::HandleResult(int code)
{
    result = code;

    if (result == EINCONS) {
        /* Purge the object if it is cached. */
        fsobj *f = FSDB->Find(&fid);
        if (f != 0) {
            f->Lock(WR);
            Recov_BeginTrans();
            f->Kill();
            Recov_EndTrans(CMFP);
            FSDB->Put(&f);
        }
    }

    if (refcnt == 0)
        delete this;
    else
        VprocSignal((char *)this);
}

void resent::print()
{
    print(stdout);
}

void resent::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void resent::print(int afd)
{
    fdprint(afd, "%#08x : fid = (%s), result = %d, refcnt = %d\n", (long)this,
            FID_(&fid), result, refcnt);
}

/* *****  Resolver  ***** */

static const int ResolverStackSize = 32768;
static const int MaxFreeResolvers  = 2;

class resolver : public vproc {
    friend void Resolve(volent *);

    static olist freelist;
    olink handle;

    resolver();
    resolver(resolver &); /* not supported! */
    int operator=(resolver &); /* not supported! */
    ~resolver();

protected:
    virtual void main(void);
};

olist resolver::freelist;

void Resolve(volent *v)
{
    /* Get a free resolver. */
    resolver *r;
    olink *o = resolver::freelist.get();
    r        = (o == 0) ? new resolver : strbase(resolver, o, handle);
    CODA_ASSERT(r->idle);

    /* Set up context for resolver. */
    r->u.Init();
    r->u.u_vol = v;
    v->hold(); /* vproc::End_VFS() will do release */

    /* Set it going. */
    r->idle = 0;
    VprocSignal((char *)r);
}

resolver::resolver()
    : vproc("Resolver", NULL, VPT_Resolver, ResolverStackSize)
{
    LOG(100,
        ("resolver::resolver(%#x): %-16s : lwpid = %d\n", this, name, lwpid));

    /* Poke main procedure. */
    start_thread();
}

resolver::resolver(resolver &r)
    : vproc((vproc &)r)
{
    abort();
}

int resolver::operator=(resolver &r)
{
    abort();
    return (0);
}

resolver::~resolver()
{
    LOG(100, ("resolver::~resolver: %-16s : lwpid = %d\n", name, lwpid));
}

void resolver::main()
{

}

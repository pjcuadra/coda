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
 *    Specification of the Venus Recoverable Storage manager.
 *
 */

#ifndef _VENUS_RECOV_H_
#define _VENUS_RECOV_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* from venus */
// #include "venus.private.h"

/* Forward declarations. */
struct RecovVenusGlobals;
class fsdb;
class vdb;
class RealmDB;
class hdb;

const int RecovMagicNumber   = 0x8675309;
const int RecovVersionNumber = 40; /* Update this when format changes. */

/*  *****  Types  *****  */
/* local-repair modification */
struct RecovVenusGlobals {
    int recov_MagicNumber; /* Sanity check */
    int recov_VersionNumber;
    time_t recov_LastInit; /* last initialization time */

    int recov_CleanShutDown;

    fsdb *recov_FSDB; /* FSO database */
    vdb *recov_VDB; /* Volume database */
    RealmDB *recov_REALMDB; /* Realm database */
    hdb *recov_HDB; /* Hoard database */

    char *recov_HeapAddr; /* Base of recoverable heap */
    unsigned int recov_HeapLength; /* Length of recoverable heap (in bytes) */

    /* We need to have a identifier that is guaranteed to be identical across
     * crashes and reboots, but unique with respect to all other venii (that
     * do stores to the same server/volume), _and_ venus reinitializations. So
     * we cannot use a timestamp, or the local ip/ether-address. This calls
     * for a UUID, but using that will require modifications to the RPC2
     * protocol. So for now a random integer is used. */
#define VenusGenID (rvg->recov_UUID.fields.time_low)

    /* This UUID should be stored in network byte order.
     * "draft-leach-uuids-guids-01.txt". */
    union {
        unsigned char bytes[16];
        struct {
            unsigned int time_low;
            unsigned short time_mid;
            unsigned short time_hi_version;
            unsigned char clock_seq_hi_variant;
            unsigned char clock_seq_low;
            unsigned char node[6];
        } fields;
    } recov_UUID;
    unsigned int recov_StoreId;

    int validate();
    void print();
    void print(FILE *);
    void print(int);
};

/*  *****  Variables  *****  */

extern int RecovInited;
extern RecovVenusGlobals *rvg;
extern int TransCount;
extern float TransElapsed;
extern int RecovTimeToFlush;

extern int CMFP;
extern int DMFP;
extern int WITT;
extern int MAXFP;
extern unsigned long MAXFS;
extern unsigned long MAXTS;

/*  *****  Functions  *****  */

#define Recov_BeginTrans() Recov::getInstance()->beginTrans(__FILE__, __LINE__)
#define Recov_EndTrans Recov::getInstance()->endTrans
#define Recov_SetBound Recov::getInstance()->setBound
#define RecovFlush Recov::getInstance()->flush
#define RecovTruncate Recov::getInstance()->truncate
#define RecovTerminate Recov::getInstance()->endTrans
#define Recov_GenerateStoreId Recov::getInstance()->generateStoreId
#define RecovPrint Recov::getInstance()->print
#define RecovTerminate()             \
    {                                \
        delete Recov::getInstance(); \
    }

class Recov {
    static Recov *impl;
    static int TimeToFlush;

public:
    static Recov *getInstance()
    {
        CODA_ASSERT(impl);
        return impl;
    }

    static void setImplementation(Recov *impl) { Recov::impl = impl; }

    void setTimeToFlush(int ttf);
    int getTimeToFlush();

    virtual void *malloc(size_t size)                    = 0;
    virtual void free(void *ptr)                         = 0;
    virtual void recordRange(void *ptr, size_t size)     = 0;
    virtual void beginTrans(const char file[], int line) = 0;
    virtual void endTrans(int)                           = 0;
    virtual void setBound(int bound)                     = 0;
    virtual void flush(int = 0)                          = 0;
    virtual void truncate(int = 0)                       = 0;
    virtual void print(int)                              = 0;
    virtual void generateStoreId(ViceStoreId *sid)       = 0;
    virtual void getStatistics()                         = 0;
};

RPC2_String Copy_RPC2_String(RPC2_String &);
void Free_RPC2_String(RPC2_String &);

void RECOVD_Init(void);
void RecovDaemon(void);

#define VALID_REC_PTR(rec_ptr)                   \
    ((char *)(rec_ptr) >= rvg->recov_HeapAddr && \
     (char *)(rec_ptr) < rvg->recov_HeapAddr + rvg->recov_HeapLength)

#endif /* _VENUS_RECOV_H_ */

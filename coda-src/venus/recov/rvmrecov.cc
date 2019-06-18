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

#include <rvmlib.h>

#include <venus/fso/fso.h>
#include <venus/hdb.h>
#include <venus/local.h>
#include <venus/mariner.h>
#include "venus.private.h"
#include <venus/recov/recov.h>
#include <venus/worker.h>
#include <venus/recov/rvmrecov.h>

int RecovInited        = 0;
RecovVenusGlobals *rvg = 0;
int TransCount         = 0;
float TransElapsed     = 0.0;

int CMFP            = UNSET_CMFP;
int DMFP            = UNSET_DMFP;
int MAXFP           = UNSET_MAXFP;
int WITT            = UNSET_WITT;
unsigned long MAXFS = UNSET_MAXFS;
unsigned long MAXTS = UNSET_MAXTS;

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/*  *****  Private Constants  *****  */

#if defined(NetBSD1_3) || defined(__NetBSD_Version__) ||                       \
    defined(__OpenBSD__) || defined(__linux__) || defined(__CYGWIN32__) ||     \
    defined(__FreeBSD_version) || (defined(__APPLE__) && defined(__MACH__)) || \
    defined(ANDROID)
static const char *VM_RVMADDR = (char *)0x50000000;

/* Pretty much every platform seems to be able to handle 0x50000000 as the RVM
 * start address without a problem.
 *
 * Sun/solaris could probably use it as well. I do wonder why sparc-linux is
 * using such an unusual RVM start address. */
#elif defined(__linux__) && defined(sparc)
static const char *VM_RVMADDR = (char *)0xbebd000;
#elif defined(sun)
static const char *VM_RVMADDR = (char *)0x40000000;
#else
#error "Please define RVM address for this platform."
#endif

#ifdef __CYGWIN32__
#include <windows.h>
#endif

/*  *****  Private Variables  *****  */

static rvm_options_t Recov_Options;
static char *Recov_RvgAddr          = 0;
static rvm_length_t Recov_RvgLength = 0;
static char *Recov_RdsAddr          = 0;
static rvm_length_t Recov_RdsLength = 0;
static rvm_statistics_t Recov_Statistics;

static int detect_reintegration_retry = 0;
static int RdsChunkSize               = UNSET_RDSCS;
static int RdsNlists                  = UNSET_RDSNL;

/* Crude formula for estimating recoverable data requirements! */
/* (assuming worst case 4k chunk size for VASTRO object bitmaps) */
#define RECOV_BYTES_NEEDED()                                                \
    (MLEs * (sizeof(cmlent) + 64) + CacheFiles * (sizeof(fsobj) + 64) +     \
     ((CacheFiles * PartialCacheFilesRatio) / 100.0) *                      \
         (sizeof(bitmap7) + (96 * 1024)) +                                  \
     (CacheFiles / 4) * (sizeof(VenusDirData) + 3072) +                     \
     (CacheFiles / 256) * sizeof(repvol) +                                  \
     (CacheFiles / 512) * sizeof(volrep) + HDBEs * (sizeof(hdbent) + 128) + \
     64 * 1024 * 1024)

int CleanShutDown;

void RvmRecov::createNewInstance()
{
    /* We need to initialize the random number generator before first use */
    rpc2_InitRandom();

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(rvg->recov_UUID);
    RVMLIB_REC_OBJECT(rvg->recov_StoreId);

    VenusGenID = rpc2_NextRandom(NULL);

    /* server disables replay detection when storeid.uniquifier > INT_MAX */
    rvg->recov_StoreId =
        detect_reintegration_retry ? 0 : ((unsigned int)INT_MAX + 1);
    Recov_EndTrans(0);
}

void RvmRecov::loadConfiguration()
{
    CacheFiles          = GetVenusConf().get_int_value("cachefiles");
    MLEs                = GetVenusConf().get_int_value("cml_entries");
    HDBEs               = GetVenusConf().get_int_value("hoard_entries");
    VenusLogDevice      = GetVenusConf().get_value("rvm_log");
    VenusLogDeviceSize  = GetVenusConf().get_int_value("rvm_log_size");
    VenusDataDevice     = GetVenusConf().get_value("rvm_data");
    VenusDataDeviceSize = GetVenusConf().get_int_value("rvm_data_size");
    detect_reintegration_retry =
        GetVenusConf().get_int_value("detect_reintegration_retry");
    RvmType      = (rvm_type_t)GetVenusConf().get_int_value("rvmtype");
    RdsChunkSize = GetVenusConf().get_int_value("rds_chunk_size");
    RdsNlists    = GetVenusConf().get_int_value("rds_list_size");
    InitMetaData = GetVenusConf().get_bool_value("initmetadata");
}

RvmRecov::RvmRecov()
{
    loadConfiguration();
    checkConfigurationParms();
    initMemorySpace();
}

void RvmRecov::initMemorySpace()
{
    if (RvmType == VM) {
        if ((rvg = (RecovVenusGlobals *)malloc(sizeof(RecovVenusGlobals))) == 0)
            CHOKE("RecovInit: malloc failed");
        memset(rvg, 0, sizeof(RecovVenusGlobals));
        rvg->recov_MagicNumber   = RecovMagicNumber;
        rvg->recov_VersionNumber = RecovVersionNumber;
        rvg->recov_LastInit      = Vtime();

        createNewInstance();

        RecovInited = 1;
        return;
    }

    /* Initialize the RVM package. */
    initRVM();
    initRDS();
    loadRDS();

    /* Read-in bounds for bounded recoverable data structures. */
    if (!InitMetaData) {
        int override = 0;
        if (MLEs != VDB->GetMaxMLEs()) {
            eprint(
                "Ignoring requested # of cml entries (%ld), "
                "using rvm value (%ld)",
                MLEs, VDB->GetMaxMLEs());
            override = 1;
            MLEs     = VDB->GetMaxMLEs();
        }
        if (CacheFiles != FSDB->GetMaxFiles()) {
            eprint(
                "Ignoring requested # of cache files (%ld), "
                "using rvm value (%ld)",
                CacheFiles, FSDB->GetMaxFiles());
            override   = 1;
            CacheFiles = FSDB->GetMaxFiles();
        }
        if (HDBEs != HDB->GetMaxHDBEntries()) {
            eprint(
                "Ignoring requested # of hoard entries (%ld), "
                "using rvm value (%ld)",
                HDBEs, HDB->GetMaxHDBEntries());
            override = 1;
            HDBEs    = HDB->GetMaxHDBEntries();
        }
        if (override)
            eprint("\t(restart venus with the -init flag to reset RVM values)");

        LOG(10, "RecovInit: MLEs = %d, CacheFiles = %d, HDBEs = %d\n", MLEs,
            CacheFiles, HDBEs);
    }

    RecovInited = 1;

    /* Fire up the daemon. */
    RECOVD_Init();
}

void RvmRecov::checkConfigurationParms()
{
    unsigned int PartialCacheFilesRatio =
        GetVenusConf().get_int_value("partialcachefilesratio");
    /* From recov module. */
    if (RvmType == UNSET)
        RvmType = DFLT_RVMT;

    switch (RvmType) {
    case RAWIO:
        eprint("RAWIO not yet supported");
        exit(EXIT_FAILURE);
        break;
    case VM:
        InitMetaData = 1; /* VM RvmType forces a brain-wipe! */
        // Fall through
    case UFS:
        break;
    default:
        CHOKE("Recov_CheckParms: bogus RvmType (%d)", RvmType);
    }

    if (InitMetaData) {
        /* Compute recoverable storage requirements, and verify that log/data sizes are adequate. */
        unsigned long RecovBytesNeeded = RECOV_BYTES_NEEDED();

        /* Set segment sizes if necessary. */
        if (VenusDataDeviceSize == UNSET_VDDS)
            VenusDataDeviceSize = RecovBytesNeeded;
        if (VenusLogDeviceSize == UNSET_VLDS)
            VenusLogDeviceSize = VenusDataDeviceSize / DataToLogSizeRatio;

        /* Check that sizes meet minimums. */
        if (VenusLogDeviceSize < MIN_VLDS) {
            eprint("log segment too small (%#x); minimum %#x",
                   VenusLogDeviceSize, MIN_VLDS);
            exit(EXIT_FAILURE);
        }
        if (VenusDataDeviceSize < MAX(RecovBytesNeeded, MIN_VDDS)) {
            eprint("data segment too small (%#x); minimum %#x",
                   VenusDataDeviceSize, MAX(RecovBytesNeeded, MIN_VDDS));
            exit(EXIT_FAILURE);
        }

        LOG(0, "RecovDataSizes: Log = %#x, Data = %#x\n", VenusLogDeviceSize,
            VenusDataDeviceSize);
    } else /* !InitMetaData */
    {
        const char *failure = NULL;

        /* Specifying log or data size requires a brain-wipe! */
        if (VenusLogDeviceSize != UNSET_VLDS) {
            failure = "VLDS";
            goto fail;
        }
        if (VenusDataDeviceSize != UNSET_VDDS) {
            failure = "VDDS";
            goto fail;
        }
        /* These parameters are only needed for a brain-wipe anyway! */
        if (RdsChunkSize != UNSET_RDSCS) {
            failure = "RDS chunk size";
            goto fail;
        }
        if (RdsNlists != UNSET_RDSNL) {
            failure = "RDS nlists";
        fail:
            eprint("setting %s requires InitMetaData", failure);
            exit(EXIT_FAILURE);
        }
    }

    if (RdsChunkSize == UNSET_RDSCS)
        RdsChunkSize = DFLT_RDSCS;
    if (RdsNlists == UNSET_RDSNL)
        RdsNlists = DFLT_RDSNL;

    /* Flush/Truncate parameters. */
    if (CMFP == UNSET_CMFP)
        CMFP = DFLT_CMFP;
    if (DMFP == UNSET_DMFP)
        DMFP = DFLT_DMFP;
    if (MAXFP == UNSET_MAXFP)
        MAXFP = DFLT_MAXFP;
    if (WITT == UNSET_WITT)
        WITT = DFLT_WITT;
    if (MAXFS == UNSET_MAXFS)
        MAXFS = DFLT_MAXFS;
    if (MAXTS == UNSET_MAXTS)
        MAXTS = DFLT_MAXTS;

    /* If you are looking for the checks and calculations for MLEs, CacheFiles,
 * and HDBEs. They have been moved to venus.cc:DefaultCmdlineParms --JH */
}

void RvmRecov::initRVM()
{
    rvm_return_t ret;
    char *logdev = strdup(VenusLogDevice);

    rvm_init_options(&Recov_Options);
    Recov_Options.log_dev  = logdev;
    Recov_Options.truncate = 0;
    //Recov_Options.flags = RVM_COALESCE_TRANS;  /* oooh, daring */
    Recov_Options.flags = RVM_ALL_OPTIMIZATIONS;

    if (GetVenusConf().get_bool_value("mapprivate"))
        Recov_Options.flags |= RVM_MAP_PRIVATE;

    rvm_init_statistics(&Recov_Statistics);

    if (InitMetaData) /* Initialize log. */
    {
        /* Get rid of any old log */
        unlink(VenusLogDevice);

        /* Pass in the correct parameters so that RVM_INIT can create
         * a new logfile */
        Recov_Options.create_log_file = rvm_true;
        Recov_Options.create_log_size = RVM_MK_OFFSET(0, VenusLogDeviceSize);
        Recov_Options.create_log_mode = 0600;
        /* as far as the log is concerned RVM_INIT will now handle the
         * rest of the creation. */
    } else /* Validate log segment. */
    {
        struct stat tstat;

        if (stat(VenusLogDevice, &tstat) < 0) {
            eprint("Recov_InitRVM: stat of (%s) failed (%d)", VenusLogDevice,
                   errno);
            exit(EXIT_FAILURE);
        }

        VenusLogDeviceSize = tstat.st_size;
        if (VenusLogDeviceSize == 0) {
            eprint("Recov_InitRVM: Unexpected empty RVM log (%s) found",
                   VenusLogDevice);
            exit(EXIT_FAILURE);
        }

        if (stat(VenusDataDevice, &tstat) < 0)
            CHOKE("ValidateDevice: stat of (%s) failed (%d)", VenusDataDevice,
                  errno);

        VenusDataDeviceSize = tstat.st_size;
        if (VenusDataDeviceSize == 0) {
            eprint("Recov_InitRVM: Unexpected empty RVM data (%s) found",
                   VenusDataDevice);
            exit(EXIT_FAILURE);
        }
    }
    eprint("%s size is %ld bytes", VenusLogDevice, VenusLogDeviceSize);
    eprint("%s size is %ld bytes", VenusDataDevice, VenusDataDeviceSize);

    ret = RVM_INIT(&Recov_Options);
    free(logdev);
    if (ret == RVM_ELOG_VERSION_SKEW) {
        eprint("Recov_InitRVM: RVM_INIT failed, RVM log version skew");
        eprint("Venus not started");
        exit(EXIT_FAILURE);
    } else if (ret != RVM_SUCCESS) {
        eprint("Recov_InitRVM: RVM_INIT failed (%s)", rvm_return(ret));
        exit(EXIT_FAILURE);
    }
}

void RvmRecov::initRDS()
{
    rvm_return_t ret;
    rvm_length_t devsize;
    char *datadev;

    devsize       = RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE(VenusDataDeviceSize);
    Recov_RdsAddr = (char *)VM_RVMADDR;
    Recov_RvgLength =
        RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(sizeof(RecovVenusGlobals));
    Recov_RdsLength = devsize - Recov_RvgLength - RVM_SEGMENT_HDR_SIZE;

    eprint("%s size is %ld bytes", VenusDataDevice, VenusDataDeviceSize);

    if (!InitMetaData)
        return;

    /* Initialize data segment. */
    int fd;
    fd = open(VenusDataDevice, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (fd < 0) {
        eprint("Recov_InitRVM: create of %s failed (%d)", VenusDataDevice,
               errno);
        exit(EXIT_FAILURE);
    }
    if (ftruncate(fd, VenusDataDeviceSize) < 0) {
        eprint("Recov_InitRVM: growing %s failed (%d)", VenusDataDevice, errno);
        exit(EXIT_FAILURE);
    }
    if (close(fd) < 0) {
        eprint("Recov_InitRVM: close of %s failed (%d)", VenusDataDevice,
               errno);
        exit(EXIT_FAILURE);
    }

    eprint("Initializing RVM data...");
    datadev = strdup(VenusDataDevice);
    rds_zap_heap(datadev, RVM_LENGTH_TO_OFFSET(devsize), Recov_RdsAddr,
                 Recov_RvgLength, Recov_RdsLength, (unsigned long)RdsNlists,
                 (unsigned long)RdsChunkSize, &ret);
    free(datadev);
    if (ret != SUCCESS) {
        eprint("Recov_InitRDS: rds_zap_heap failed (%s)", rvm_return(ret));
        exit(EXIT_FAILURE);
    }
    eprint("...done");
}

void RvmRecov::loadRDS()
{
    rvm_return_t ret;
    char *datadev;
    int detecting_retries;

    eprint("Loading RVM data");
    datadev = strdup(VenusDataDevice);
    rds_load_heap(datadev, RVM_LENGTH_TO_OFFSET(VenusDataDeviceSize),
                  &Recov_RvgAddr, &ret);
    free(datadev);
    if (ret != SUCCESS) {
        eprint("Recov_InitRDS: rds_load_heap failed (%s)", rvm_return(ret));
        exit(EXIT_FAILURE);
    }
    rvg = (RecovVenusGlobals *)Recov_RvgAddr;

    /* Initialize or validate the segment. */
    if (InitMetaData) {
        Recov_BeginTrans();
        /* Initialize the block of recoverable Venus globals. */
        RVMLIB_REC_OBJECT(*rvg);
        memset((void *)rvg, 0, (int)sizeof(RecovVenusGlobals));
        rvg->recov_MagicNumber   = RecovMagicNumber;
        rvg->recov_VersionNumber = RecovVersionNumber;
        rvg->recov_LastInit      = Vtime();
        rvg->recov_HeapAddr      = Recov_RdsAddr;
        rvg->recov_HeapLength    = (unsigned int)Recov_RdsLength;
        Recov_EndTrans(0);
    } else {
        /* Sanity check RVG fields. */
        if (rvg->recov_HeapAddr != Recov_RdsAddr ||
            rvg->recov_HeapLength != Recov_RdsLength)
            CHOKE("Recov_LoadRDS: heap mismatch (%p, %lx) vs (%p, %lx)",
                  rvg->recov_HeapAddr, rvg->recov_HeapLength, Recov_RdsAddr,
                  Recov_RdsLength);
        if (!rvg->validate()) {
            rvg->print(stderr);
            CHOKE(
                "Recov_InitSeg: rvg validation failed, "
                "restart venus with -init");
        }

        eprint("Last init was %s",
               strtok(ctime((time_t *)&rvg->recov_LastInit), "\n"));

        /* Copy CleanShutDown to VM global, then set it FALSE. */
        CleanShutDown = rvg->recov_CleanShutDown;
        eprint("Last shutdown was %s", (CleanShutDown ? "clean" : "dirty"));
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(rvg->recov_CleanShutDown);
        rvg->recov_CleanShutDown = 0;
        Recov_EndTrans(0);
    }

    detecting_retries = rvg->recov_StoreId <= INT_MAX;
    if (InitMetaData || InitNewInstance ||
        (detect_reintegration_retry && !detecting_retries) ||
        (!detect_reintegration_retry && detecting_retries))
        createNewInstance();

    /* Plumb the heap here? */
    if (GetVenusConf().get_bool_value("rdstrace")) {
        rds_trace_on(GetLogFile());
        rds_trace_dump_heap();
    }
}

/* Venus transaction handling */
void RvmRecov::beginTrans(const char file[], int line)
{
    _rvmlib_begin_transaction(no_restore, file, line);
}

void RvmRecov::endTrans(int time)
{
    rvmlib_end_transaction(no_flush, 0);
    setBound(time);
}

/* Bounds the (non)persistence of committed no_flush transactions. */
void RvmRecov::setBound(int bound)
{
    if (bound < Recov::getTimeToFlush())
        Recov::setTimeToFlush(bound);
}

void RvmRecov::getStatistics()
{
    if (RvmType == VM)
        return;

    rvm_return_t ret = RVM_STATISTICS(&Recov_Statistics);
    if (ret != RVM_SUCCESS)
        CHOKE("Recov_GetStatistics: rvm_statistics failed (%d)", ret);
}

void RvmRecov::flush(int Force)
{
    getStatistics();
    time_t WorkerIdleTime = GetWorkerIdleTime();
    unsigned long FlushSize =
        RVM_OFFSET_TO_LENGTH(Recov_Statistics.no_flush_length);
    if (RvmType == VM)
        return;

    if (Recov::getTimeToFlush() > 0 && FlushSize < MAXFS &&
        WorkerIdleTime < WITT)
        return;

    if (FlushSize < MAXFS)
        return;

    int FlushCount     = (int)Recov_Statistics.n_no_flush;
    const char *reason = (Force) ? "F" :
                                   (Recov::getTimeToFlush() <= 0) ?
                                   "T" :
                                   (FlushSize >= MAXFS) ? "S" : "I";

    Recov::setTimeToFlush(MAXFP);
    if (FlushSize == 0)
        return;

    LOG(0, "BeginRvmFlush (%d, %d, %s)\n", FlushCount, FlushSize, reason);
    START_TIMING();
    rvm_return_t ret = rvm_flush();
    if (ret != RVM_SUCCESS)
        CHOKE("RecovFlush: rvm_flush failed (%d)", ret);
    END_TIMING();
    LOG(0, "EndRvmFlush\n");

    LOG(1, "RecovFlush: count = %d, size = %d, elapsed = %3.1f\n", FlushCount,
        FlushSize, elapsed);
}

void RvmRecov::truncate(int Force)
{
    time_t WorkerIdleTime = GetWorkerIdleTime();
    getStatistics();
    unsigned long TruncateSize =
        RVM_OFFSET_TO_LENGTH(Recov_Statistics.log_written);
    if (TruncateSize < MAXTS && WorkerIdleTime < WITT)
        return;

    if (RvmType == VM)
        return;

    int TruncateCount = (int)Recov_Statistics.n_flush_commit +
                        (int)Recov_Statistics.n_no_flush_commit;
    const char *reason = (Force) ? "F" : (TruncateSize >= MAXTS) ? "S" : "I";

    if (TruncateSize == 0)
        return;

    LOG(0, "BeginRvmTruncate (%d, %d, %s)\n", TruncateCount, TruncateSize,
        reason);
    START_TIMING();
    rvm_return_t ret = rvm_truncate();
    if (ret != RVM_SUCCESS)
        CHOKE("RecovTruncate: rvm_truncate failed (%d)", ret);
    END_TIMING();
    LOG(0, "EndRvmTruncate\n");

    /*    if (post_vm_usage - pre_vm_usage != 0)*/
    LOG(1, "RecovTruncate: count = %d, size = %d, elapsed = %3.1f\n",
        TruncateCount, TruncateSize, elapsed);
}

RvmRecov::~RvmRecov()
{
    if (RvmType == VM)
        return;
    if (!RecovInited)
        return;

    /* Record clean shutdown indication if possible. */
    getStatistics();
    int n_uncommit = (int)Recov_Statistics.n_uncommit;
    if (n_uncommit == 0) {
        /* N.B.  Can't use rvmlib macros here, since we're likely being called in the */
        /* context of a signal handler, which does not have the state assumed by the macros! */
        {
            rvm_tid_t tid;
            rvm_init_tid(&tid);
            rvm_return_t ret;
            ret = rvm_begin_transaction(&tid, no_restore);
            CODA_ASSERT(ret == RVM_SUCCESS);
            ret = rvm_set_range(&tid, (char *)&rvg->recov_CleanShutDown,
                                sizeof(rvg->recov_CleanShutDown));
            CODA_ASSERT(ret == RVM_SUCCESS);
            rvg->recov_CleanShutDown = 1;
            ret = rvm_end_transaction(&tid, rvm_mode_t::flush);
            CODA_ASSERT(ret == RVM_SUCCESS);
        }

        eprint("RecovTerminate: clean shutdown");
    } else {
        eprint("RecovTerminate: dirty shutdown (%d uncommitted transactions)",
               n_uncommit);
    }

    rvm_return_t ret = rvm_terminate();
    switch (ret) {
    case RVM_SUCCESS:
        CODA_ASSERT(n_uncommit == 0);
        break;

    case RVM_EUNCOMMIT:
        CODA_ASSERT(n_uncommit != 0);
        break;

    default:
        CHOKE("RecovTerminate: rvm_terminate failed (%d)", ret);
    }
}

void RvmRecov::print(int fd)
{
    if (RvmType == VM)
        return;
    if (!RecovInited)
        return;

    fdprint(fd, "Recoverable Storage: (%s, %x)\n", VenusDataDevice,
            VenusDataDeviceSize);
    fdprint(fd, "\tTransactions = (%d, %3.1f)\n", TransCount,
            (TransCount > 0 ? TransElapsed / TransCount : 0.0));
    fdprint(fd, "\tHeap: chunks = %d, nlists = %d, bytes = (%d, %d)\n",
            RdsChunkSize, RdsNlists, 0, 0);
    fdprint(fd, "\tLast initialized %s\n",
            ctime((time_t *)&rvg->recov_LastInit));

    fdprint(fd, "***RVM Statistics***\n");
    getStatistics();
    rvm_return_t ret = rvm_print_statistics(&Recov_Statistics, GetLogFile());
    fflush(GetLogFile());
    if (ret != RVM_SUCCESS)
        CHOKE("Recov_PrintStatistics: rvm_print_statistics failed (%d)", ret);

    fdprint(fd, "***RDS Statistics***\n");
    rds_stats_t rdsstats;
    if (rds_get_stats(&rdsstats) != 0)
        fdprint(fd, "rds_get_stats failed\n\n");
    else
        fdprint(
            fd,
            "RecovPrint:  Free bytes in heap = %d; Malloc'd bytes in heap = %d\n\n",
            rdsstats.freebytes, rdsstats.mallocbytes);
    // We wish there were a way to find out if heap_header_t.maxlist < heap_header_t.nlists.
    // If there were, that would be a sign that fragmentation is becoming a problem.
    // Unfortunately, heap_header_t is in rds_private.h
}

/*  *****  recov_daemon.c  *****  */

static const int RecovDaemonInterval = 5;
static const int RecovDaemonStackSize =
    262144; /* MUST be big to handle rvm_trucates! */

static char recovdaemon_sync;

/* MUST be called from within a transaction */
void RvmRecov::generateStoreId(ViceStoreId *sid)
{
    /* VenusGenID, is randomly chosen whenever rvm is reinitialized, it
     * should be a 128-bit UUID (re-generated whenever rvm is reinitialized).
     * But that would require changing in the venus-vice protocol to either
     * add this UUID to every operation, or send it once per (volume-)
     * connection setup with ViceNewConnectFS. -JH */
    sid->HostId     = (RPC2_Unsigned)VenusGenID;
    sid->Uniquifier = rvg->recov_StoreId;

    /* Avoid overflow past UINT_MAX, server stopped replay detection once
     * we passed INT_MAX so we stop incrementing */
    if (rvg->recov_StoreId == UINT_MAX)
        return;

    RVMLIB_REC_OBJECT(rvg->recov_StoreId);
    rvg->recov_StoreId++;
}

void *RvmRecov::malloc(size_t size)
{
    return rvmlib_rec_malloc(size);
}

void RvmRecov::free(void *ptr)
{
    rvmlib_rec_free(ptr);
}

void RvmRecov::recordRange(void *ptr, size_t size)
{
    rvmlib_set_range(ptr, size);
}

rvm_type_t GetRvmType()
{
    return RvmType;
}

/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <rpc2/codatunnel.h>
#include <getopt.h>

#include "archive.h"

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "sighand.h"
#include "user.h"
#include "venus.private.h"
#include "venuscb.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "coda_assert.h"
#include "codaconf.h"
#include "realmdb.h"
#include "daemonizer.h"
#include "venusmux.h"

#include "nt_util.h"
#ifdef __CYGWIN32__
//  Not right now ... should go #define main venus_main
uid_t V_UID; 
#endif

/* FreeBSD 2.2.5 defines this in rpc/types.h, all others in netinet/in.h */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

/* *****  Exported variables  ***** */
/* globals in the .bss are implicitly initialized to 0 according to ANSI-C standards */
vproc *Main;
VenusFid rootfid;
long rootnodeid;
int CleanShutDown;
int SearchForNOreFind;  // Look for better detection method for iterrupted hoard walks. mre 1/18/93
int ASRallowed = 1;

/* Command-line/venus.conf parameters. */
const char *consoleFile;
const char *venusRoot;
const char *kernDevice;
const char *realmtab;
const char *CacheDir;
const char *CachePrefix;
unsigned int CacheBlocks;
uid_t PrimaryUser = UNSET_PRIMARYUSER;
const char *SpoolDir;
const char *CheckpointFormat;
const char *VenusPidFile;
const char *VenusControlFile;
const char *VenusLogFile;
const char *ASRLauncherFile;
const char *ASRPolicyFile;
const char *MarinerSocketPath;
int masquerade_port;
int PiggyValidations;
pid_t ASRpid;
VenusFid ASRfid;
uid_t ASRuid;
int detect_reintegration_retry;
int option_isr;
extern char PeriodicWalksAllowed;
/* exit codes (http://refspecs.linuxbase.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/iniscrptact.html) */
// EXIT_SUCCESS             0   /* stdlib.h - success */
// EXIT_FAILURE             1   /* stdlib.h - generic or unspecified error */
#define EXIT_INVALID_ARG    2   /* invalid or excess argument(s) */
#define EXIT_UNIMPLEMENTED  3   /* unimplemented feature */
#define EXIT_PRIVILEGE_ERR  4   /* user had insufficient privilege */
#define EXIT_UNINSTALLED    5   /* program is not installed */
#define EXIT_UNCONFIGURED   6   /* program is not configured */
#define EXIT_NOT_RUNNING    7   /* program is not running */

#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
int mariner_tcp_enable = 0;
#else
int mariner_tcp_enable = 1;
#endif
static int codafs_enabled;
int plan9server_enabled;
int nofork;

/* Global red and yellow zone limits on CML length; default is infinite */
int redzone_limit = -1, yellowzone_limit = -1;

static int codatunnel_enabled;
static int codatunnel_onlytcp;

/* *****  Private constants  ***** */

struct timeval DaemonExpiry = {TIMERINTERVAL, 0};

/* *****  Private routines  ***** */

static void ParseCmdline(int, char **);
static void DefaultCmdlineParms();
static void CdToCacheDir();
static void CheckInitFile();
static void UnsetInitFile();
static void SetRlimits();
/* **** defined in worker.c **** */
extern int testKernDevice();

struct in_addr venus_relay_addr = { INADDR_LOOPBACK };

#define NO_SHORT_OPT(c) c + 256

/* *****  venus.c  ***** */

/* socket connecting us back to our parent */
int parent_fd = -1;

/* Bytes units convertion */
static const char * KBYTES_UNIT[] = { "KB", "kb", "Kb", "kB", "K", "k"};
static const unsigned int KBYTE_UNIT_SCALE = 1;
static const char * MBYTES_UNIT[] = { "MB", "mb", "Mb", "mB", "M", "m"};
static const unsigned int MBYTE_UNIT_SCALE = 1024 * KBYTE_UNIT_SCALE;
static const char * GBYTES_UNIT[] = { "GB", "gb", "Gb", "gB", "G", "g"};
static const unsigned int GBYTE_UNIT_SCALE = 1024 * MBYTE_UNIT_SCALE;
static const char * TBYTES_UNIT[] = { "TB", "tb", "Tb", "tB", "T", "t"};
static const unsigned int TBYTE_UNIT_SCALE = 1024 * GBYTE_UNIT_SCALE;

/*
 * Parse size value and converts into amount of 1K-Blocks
 */
static unsigned int ParseSizeWithUnits(const char * SizeWUnits)
{
    const char * units = NULL;
    int scale_factor = 1;
    char SizeWOUnits[256];
    size_t size_len = 0;
    unsigned int size_int = 0;

    /* Locate the units and determine the scale factor */
    for (int i = 0; i < 6; i++) {
        if ((units = strstr(SizeWUnits, KBYTES_UNIT[i]))) {
            scale_factor = KBYTE_UNIT_SCALE;
            break;
        }
        
        if ((units = strstr(SizeWUnits, MBYTES_UNIT[i]))) {
            scale_factor = MBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, GBYTES_UNIT[i]))) {
            scale_factor = GBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, TBYTES_UNIT[i]))) {
            scale_factor = TBYTE_UNIT_SCALE;
            break;
        }
    }

    /* Strip the units from string */
    if (units) {
        size_len = (size_t)((units - SizeWUnits) / sizeof(char));
        strncpy(SizeWOUnits, SizeWUnits, size_len);
        SizeWOUnits[size_len] = 0;  // Make it null-terminated
    } else {
        snprintf(SizeWOUnits, sizeof(SizeWOUnits), "%s", SizeWUnits);
    }

    /* Scale the value */
    size_int = scale_factor * atof(SizeWOUnits);

    return size_int;
}

static int power_of_2(uint64_t num)
{
    int power = 0;
    
    if (!num) return -1;
    
    /*Find the first 1 */
    while (!(num & 0x1)) {
        num = num >> 1; 
        power++;
    }
    
    /* Shift the first 1 */
    num = num >> 1;

    /* Any other 1 means not power of 2 */
    if (num) return -1;

    return power; 
}

void ParseCacheChunkBlockSize(const char * ccblocksize) {
    uint64_t TmpCacheChunkBlockSize = ParseSizeWithUnits(ccblocksize) * 1024;
    int TmpCacheChunkBlockSizeBit = power_of_2(TmpCacheChunkBlockSize);
    
    if (TmpCacheChunkBlockSizeBit < 0) {
        /* Not a power of 2 FAIL!!! */
        eprint("Cannot start: provided cache chunk block size is not a power of 2");
        exit(EXIT_UNCONFIGURED);
    }
    
    if (TmpCacheChunkBlockSizeBit < 12) {
        /* Smaller than minimum FAIL*/
        eprint("Cannot start: minimum cache chunk block size is 4KB");
        exit(EXIT_UNCONFIGURED);
    }
        
    CacheChunkBlockSizeBits = TmpCacheChunkBlockSizeBit;
    CacheChunkBlockSize = 1 << CacheChunkBlockSizeBits;
    CacheChunkBlockSizeMax = CacheChunkBlockSize - 1;
    CacheChunkBlockBitmapSize = (UINT32_MAX >> CacheChunkBlockSizeBits) + 1;
}


/* local-repair modification */
int main(int argc, char **argv)
{
    coda_assert_action = CODA_ASSERT_SLEEP;
    coda_assert_cleanup = VFSUnmount;

    ParseCmdline(argc, argv);
    DefaultCmdlineParms();   /* read /etc/coda/venus.conf */

    // Cygwin runs as a service and doesn't need to daemonize.
#ifndef __CYGWIN__
    if (!nofork && LogLevel == 0)
	parent_fd = daemonize();
#endif

    update_pidfile(VenusPidFile);

    /* open the console file and print vital info */
    if (!nofork) /* only redirect stderr when daemonizing */
        freopen(consoleFile, "a+", stderr);
    eprint("Coda Venus, version " PACKAGE_VERSION);

    CdToCacheDir(); 
    CheckInitFile();
    SetRlimits();
    /* Initialize.  N.B. order of execution is very important here! */
    /* RecovInit < VSGInit < VolInit < FSOInit < HDB_Init */

#ifdef __CYGWIN32__
    /* MapPrivate does not work on Cygwin */
    if (MapPrivate) {
      eprint ("Private mapping turned off, does not work on CYGWIN.");
      MapPrivate = 0;
    }
    V_UID = getuid();
#endif    

    /* test mismatch with kernel before doing real work */
    testKernDevice();

    if (codatunnel_enabled) {
        int rc;
        /* masquerade_port is the UDP portnum specified via venus.conf */
        char service[6];
        sprintf(service, "%hu", masquerade_port);
        rc = codatunnel_fork(argc, argv, NULL, "0.0.0.0", service, codatunnel_onlytcp);
        if (rc < 0){
            perror("codatunnel_fork: ");
            exit(-1);
        }
        printf("codatunneld started\n");
    }

    /* 
     * VprocInit MUST precede LogInit. Log messages are stamped
     * with the id of the vproc that writes them, so log messages
     * can't be properly stamped until the vproc class is initialized.
     * The logging routines return without doing anything if LogInit 
     * hasn't yet been called. 
     */
    VprocInit();    /* init LWP/IOMGR support */
    LogInit();      /* move old Venus log and create a new one */
   
    LWP_SetLog(logFile, lwp_debug);
    RPC2_SetLog(logFile, RPC2_DebugLevel);
    DaemonInit();   /* before any Daemons initialize and after LogInit */
    StatsInit();
    SigInit();      /* set up signal handlers */

    DIR_Init(RvmType == VM ? DIR_DATA_IN_VM : DIR_DATA_IN_RVM);
    RecovInit();    /* set up RVM and recov daemon */
    CommInit();     /* set up RPC2, {connection,server,mgroup} lists, probe daemon */
    UserInit();     /* fire up user daemon */
    VSGDBInit();    /* init VSGDB */
    RealmDBInit();
    VolInit();	    /* init VDB, daemon */
    FSOInit();      /* allocate FSDB if necessary, recover FSOs, start FSO daemon */
    VolInitPost();  /* drop extra volume refcounts */
    HDB_Init();     /* allocate HDB if necessary, scan entries, start the HDB daemon */
    MarinerInit();  /* set up mariner socket */
    WorkerInit();   /* open kernel device */
    CallBackInit(); /* set up callback subsystem and create callback server threads */

    /* Get the Root Volume. */
    if (codafs_enabled) {
        eprint("Mounting root volume...");
        VFSMount();
    }

    UnsetInitFile();
    eprint("Venus starting...");

    freopen("/dev/null", "w", stdout);

    /* allow the daemonization to complete */
    if (!codafs_enabled)
        kill(getpid(), SIGUSR1);

    /* Act as message-multiplexor/daemon-dispatcher. */
    for (;;) {
	/* Wait for a message or daemon expiry. */
	fd_set rfds;
	int maxfd;

	FD_ZERO(&rfds);
        maxfd = _MUX_FD_SET(&rfds);

	if (VprocSelect(maxfd+1, &rfds, 0, 0, &DaemonExpiry) > 0)
            _MUX_Dispatch(&rfds);

	/* set in sighand.cc whenever we want to perform a clean shutdown */
	if (TerminateVenus)
	    break;

	/* Fire daemons that are ready to run. */
	DispatchDaemons();
    }

    LOG(0, ("Venus exiting\n"));

    RecovFlush(1);
    RecovTerminate();
    VFSUnmount();
    fflush(logFile);
    fflush(stderr);

    MarinerLog("shutdown in progress\n");

    LWP_TerminateProcessSupport();

#if defined(__CYGWIN32__)
    nt_stop_ipc();
    return 0;
#endif

    exit(EXIT_SUCCESS);
}

#define NO_SHORT(name) venus_no_short_##name

enum venus_no_short_e {
    venus_no_short_short_last = 255, // DO NOT USE
    NO_SHORT(primaryuser),
    NO_SHORT(rpc2_debug_level),
    NO_SHORT(lwp_debug_level),
    NO_SHORT(cache_files),
    NO_SHORT(cache_size),
    NO_SHORT(cache_chunk_size),
    NO_SHORT(console),
    NO_SHORT(ws),
    NO_SHORT(sa),
    NO_SHORT(ap),
    NO_SHORT(ps),
    NO_SHORT(rvmt),
    NO_SHORT(vld),
    NO_SHORT(vlds),
    NO_SHORT(vdd),
    NO_SHORT(vdds),
    NO_SHORT(rdscs),
    NO_SHORT(rdsnl),
    NO_SHORT(logopts),
    NO_SHORT(swt),
    NO_SHORT(mwt),
    NO_SHORT(ssf),
    NO_SHORT(alpha),
    NO_SHORT(beta),
    NO_SHORT(gamma),
    NO_SHORT(novcb),
    NO_SHORT(nowalk),
    NO_SHORT(spooldir),
    NO_SHORT(mles),
    NO_SHORT(hdbes),
    NO_SHORT(rdstrace),
    NO_SHORT(maxworkers),
    NO_SHORT(maxcbservers),
    NO_SHORT(maxprefetchers),
    NO_SHORT(retries),
    NO_SHORT(timeout),
    NO_SHORT(whole_file_max)
};

struct venus_option {
    struct option long_option;
    const char * description;
    const char * value_name;
};

static struct venus_option venus_options[] = {
    {
        .long_option = {"help", no_argument, 0, 'h'},
        .description = "Print venus command usage",
        .value_name = NULL
    },
    {
        .long_option = {"version", no_argument, 0, 'V'},
        .description = "Print venus version",
        .value_name = NULL
    },
    {
        .long_option = {"init", no_argument, 0, 'i'},
        .description = "Wipe and reinitialize persistent state",
        .value_name = NULL
    },
    {
        .long_option = {"primaryuser", required_argument, 0, NO_SHORT(primaryuser)},
        .description = "Primary user",
        .value_name = "uid"
    },
    {
        .long_option = {"mapprivate", no_argument, &MapPrivate, 1},
        .description = "Use private mmap for RVM",
        .value_name = NULL
    },
    {
        .long_option = {"newinstance", no_argument, &InitNewInstance, 1},
        .description = "Fake a 'reinit'",
        .value_name = NULL
    },
    {
        .long_option = {"debug-level", required_argument, 0, 'd'},
        .description = "Set the logging level",
        .value_name = "debug level"
    },
    {
        .long_option = {"rpc2-debug-level", required_argument, 0, NO_SHORT(rpc2_debug_level)},
        .description = "Set RPC2's logging level",
        .value_name = "debug level"
    },
    {
        .long_option = {"lwp-debug-level", required_argument, 0, NO_SHORT(lwp_debug_level)},
        .description = "Set LWP's logging level",
        .value_name = "debug level"
    },
    {
        .long_option = {"cache-files", required_argument, 0, NO_SHORT(cache_files)},
        .description = "Number of cache files",
        .value_name = "n"
    },
    {
        .long_option = {"cache-size", required_argument, 0, NO_SHORT(cache_size)},
        .description = "Cache size in the given units (e.g. 10MB)",
        .value_name = "n[KB|MB|GB|TB]"
    },
    {
        .long_option = {"cache-chunk-size", required_argument, 0, NO_SHORT(cache_chunk_size)},
        .description = "Cache chunk block size (shall be power of 2)",
        .value_name = "n[KB|MB|GB|TB]"
    },
    {
        .long_option = {"mles", required_argument, 0, NO_SHORT(mles)},
        .description = "Number of CML entries",
        .value_name = "n"
    },
    {
        .long_option = {"hdbes", required_argument, 0, NO_SHORT(hdbes)},
        .description = "Number of hoard database entries",
        .value_name = "n"
    },
    {
        .long_option = {"rdstrace", no_argument, 0, NO_SHORT(rdstrace)},
        .description = "enable RDS heap tracing",
        .value_name = NULL
    },
    {
        .long_option = {"kernel-dev", required_argument, 0, 'k'},
        .description = "Take kernel device to be the device for",
        .value_name = "kernel device"
    },
    {
        .long_option = {"cache-dir", required_argument, 0, 'f'},
        .description = "location of cache files",
        .value_name = "cache dir"
    },
    {
        .long_option = {"console", required_argument, 0, NO_SHORT(console)},
        .description = "Location of error log file",
        .value_name = "log file"
    },
    {
        .long_option = {"cop-modes", required_argument, 0, 'm'},
        .description = "COP modes",
        .value_name = "n"
    },
    {
        .long_option = {"maxworkers", required_argument, 0, NO_SHORT(maxworkers)},
        .description = "Number of worker threads",
        .value_name = "n"
    },
    {
        .long_option = {"maxcbservers", required_argument, 0, NO_SHORT(maxcbservers)},
        .description = "Number of callback server threads",
        .value_name = "n"
    },
    {
        .long_option = {"maxprefetchers", required_argument, 0, NO_SHORT(maxprefetchers)},
        .description = "Number of threads servicing pre-fetch ioctl",
        .value_name = "n"
    },
    {
        .long_option = {"retries", required_argument, 0, NO_SHORT(retries)},
        .description = "# of rpc2 retries",
        .value_name = "n"
    },
    {
        .long_option = {"timeout", required_argument, 0, NO_SHORT(timeout)},
        .description = "rpc2 timeout",
        .value_name = "n"
    },
    {
        .long_option = {"ws", required_argument, 0, NO_SHORT(ws)},
        .description = "SFTP window size",
        .value_name = "n"
    },
    {
        .long_option = {"sa", required_argument, 0, NO_SHORT(sa)},
        .description = "SFTP send ahead",
        .value_name = "n"
    },
    {
        .long_option = {"ap", required_argument, 0, NO_SHORT(ap)},
        .description = "SFTP ack point",
        .value_name = "n"
    },
    {
        .long_option = {"ps", required_argument, 0, NO_SHORT(ps)},
        .description = "SFTP packet size",
        .value_name = "n"
    },
    {
        .long_option = {"rvmt", required_argument, 0, NO_SHORT(rvmt)},
        .description = "RVM type",
        .value_name = "type"
    },
    {
        .long_option = {"vld", required_argument, 0, NO_SHORT(vld)},
        .description = "Location of RVM log",
        .value_name = "RVM log"
    },
    {
        .long_option = {"vlds", required_argument, 0, NO_SHORT(vlds)},
        .description = "Size of RVM log",
        .value_name = "n"
    },
    {
        .long_option = {"vdd", required_argument, 0, NO_SHORT(vdd)},
        .description = "Location of RVM data",
        .value_name = "RVM data"
    },
    {
        .long_option = {"vdds", required_argument, 0, NO_SHORT(vdds)},
        .description = "Size of RVM data",
        .value_name = "n"
    },
    {
        .long_option = {"rdscs", required_argument, 0, NO_SHORT(rdscs)},
        .description = "RDS chunk size",
        .value_name = "n"
    },
    {
        .long_option = {"rdsnl", required_argument, 0, NO_SHORT(rdsnl)},
        .description = "RDS # lists",
        .value_name = "n"
    },
    {
        .long_option = {"logopts", required_argument, 0, NO_SHORT(logopts)},
        .description = "RVM log optimizations",
        .value_name = "n"
    },
    {
        .long_option = {"swt", required_argument, 0, NO_SHORT(swt)},
        .description = "Short term weight",
        .value_name = "n"
    },
    {
        .long_option = {"mwt", required_argument, 0, NO_SHORT(mwt)},
        .description = "Medium term weight",
        .value_name = "n"
    },
    {
        .long_option = {"ssf", required_argument, 0, NO_SHORT(ssf)},
        .description = "Short term scale factor",
        .value_name = "n"
    },
    {
        .long_option = {"alpha", required_argument, 0, NO_SHORT(alpha)},
        .description = "Patience ALPHA value",
        .value_name = "n"
    },
    {
        .long_option = {"beta", required_argument, 0, NO_SHORT(beta)},
        .description = "Patience BETA valu",
        .value_name = "n"
    },
    {
        .long_option = {"gamma", required_argument, 0, NO_SHORT(gamma)},
        .description = "Patience GAMMA value",
        .value_name = "n"
    },
    {
        .long_option = {"von", no_argument, &rpc2_timeflag, 1},
        .description = "Enable RPC2 timing",
        .value_name = NULL
    },
    {
        .long_option = {"voff", no_argument, &rpc2_timeflag, 0},
        .description = "Disable RPC2 timing",
        .value_name = NULL
    },
    {
        .long_option = {"vmon", no_argument, &mrpc2_timeflag, 1},
        .description = "Enable multi-RPC2 timing",
        .value_name = NULL
    },
    {
        .long_option = {"vmoff", no_argument, &mrpc2_timeflag, 0},
        .description = "Disable multi-RPC2 timing",
        .value_name = NULL
    },
//****************REMOVE??********** " -SearchForNOreFind\t\tsomething, forgot what\n"
//**********REMOVE?**************************// " -noskk\t\t\t\tdisable venus sidekick\n"
    {
        .long_option = {"noasr", no_argument, &ASRallowed, 0},
        .description = "Disable application specific resolvers",
        .value_name = NULL
    },
    {
        .long_option = {"novcb", no_argument, 0, NO_SHORT(novcb)},
        .description = "Disable volume callbacks",
        .value_name = NULL
    },
    {
        .long_option = {"nowalk", no_argument, 0, NO_SHORT(nowalk)},
        .description = "Disable hoard walks",
        .value_name = NULL
    },
    {
        .long_option = {"spooldir", required_argument, 0, NO_SHORT(spooldir)},
        .description = "Spooldir to hold CML snapshots",
        .value_name = "spool directory"
    },
    {
        .long_option = {"mariner-tcp", no_argument, &mariner_tcp_enable, 1},
        .description = "Disable mariner TCP port",
        .value_name = NULL
    },
    {
        .long_option = {"no-mariner-tcp", no_argument, &mariner_tcp_enable, 0},
        .description = "Disable mariner TCP port",
        .value_name = NULL
    },
    {
        .long_option = {"allow-reattach", no_argument, &allow_reattach, 1},
        .description = "Allow reattach to already mounted tree",
        .value_name = NULL
    },
//*****REMOVE?*************// " -relay <addr>\t\t\trelay socket address (windows only)\n"
    {
        .long_option = {"codatunnel", no_argument, &codatunnel_enabled, 1},
        .description = "Enable codatunneld helper",
        .value_name = NULL
    },
    {
        .long_option = {"no-codatunnel", no_argument, &codatunnel_enabled, -1},
        .description = "Disable codatunneld helper",
        .value_name = NULL
    },
    {
        .long_option = {"onlytcp", no_argument, &codatunnel_onlytcp, 1},
        .description = "Only use TCP tunnel connections to servers",
        .value_name = NULL
    },
    {
        .long_option = {"no-9pfs", no_argument, &plan9server_enabled, -1},
        .description = "Disable embedded 9pfs server",
        .value_name = NULL
    },
    {
        .long_option = {"9pfs", no_argument, &plan9server_enabled, 1},
        .description = "Enable embedded 9pfs server (experimental, INSECURE!)",
        .value_name = NULL
    },
    {
        .long_option = {"no-codafs", no_argument, &codafs_enabled, -1},
        .description = "Do not automatically mount /coda",
        .value_name = NULL
    },
    {
        .long_option = {"codafs", no_argument, &codafs_enabled, 1},
        .description = "Automatically mount /coda",
        .value_name = NULL
    },
    {
        .long_option = {"nofork", no_argument, &nofork, 1},
        .description = "Do not daemonize the process",
        .value_name = NULL
    },
    {
        .long_option = {"whole-file-max", required_argument, 0, NO_SHORT(whole_file_max)},
        .description = "Maximum file size to allow whole-file caching",
        .value_name = "file size"
    }
};

static void Usage(char *argv0)
{
    const char * value_name = "";
    int noptions = sizeof(venus_options) / sizeof(struct venus_option);

    printf("Usage: %s [OPTION]...\n\nOptions:\n\n", argv0);

    for (int i = 0; i < noptions; i++) {

        printf("  ");

        if (venus_options[i].long_option.flag) {
            printf("--%s \n\t%s\n", 
                   venus_options[i].long_option.name, 
                   venus_options[i].description);
            continue;
        }

        if (venus_options[i].long_option.val && venus_options[i].long_option.val < 256) {
            printf("-%c", venus_options[i].long_option.val);

            if (venus_options[i].value_name) {
                printf(" <%s>", venus_options[i].value_name);
            }

            if (venus_options[i].long_option.name) {
                printf(", ");
            }
        }

        if (venus_options[i].long_option.name) {
            printf("--%s", venus_options[i].long_option.name);

            if (venus_options[i].value_name) {
                printf(" <%s>", venus_options[i].value_name);
            }
        }

        printf("\n\t%s\n", venus_options[i].description);
    }

    printf("\nFor more information see http://www.coda.cs.cmu.edu/\n"
           "Report bugs to <bugs@coda.cs.cmu.edu>.\n");
}

static void ParseCmdline(int argc, char **argv)
{
    int i, done = 0;
    int option_index = 0;
    int c = 0;
    int noptions = sizeof(venus_options) / sizeof(struct venus_option);
    char short_options[noptions*2 + 1] = "";
    char single_short_opt[3] = "";


    struct option * long_options = (struct option *)malloc(sizeof(struct option) * (noptions + 1));

    /* Create short and long options arrays */
    for (int i = 0; i < noptions; i++) {
        /* Add long option */
        long_options[i] = venus_options[i].long_option;

        /* Flags are handled by getopts_long */
        if (long_options[i].flag) continue;

        /* Concat short options */
        if (long_options[i].val < 256 && long_options[i].val != 0) {
            sprintf(single_short_opt, "%c%c", 
                    long_options[i].val, 
                    long_options[i].has_arg == no_argument ? '\0' : ':');
            strcat(short_options, single_short_opt);
        }
    }

    long_options[noptions] = {0,0,0,0};

    /* Iterate over the parameters */
    while (true) {
 
        c = getopt_long(argc, argv, short_options,
                        long_options, &option_index);

        if (c == -1) break;

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0)
                    break;
            case 'h':
                Usage(argv[0]);
                break;
            case 'V':
                printf("Venus " PACKAGE_VERSION "\n");
                exit(0);
                break;
            case 'i':
                InitMetaData = 1;
                break;
            case NO_SHORT(primaryuser):
                PrimaryUser = atoi(optarg);
                break;
            case 'd':
                LogLevel = atoi(optarg);
                break;
            case NO_SHORT(rpc2_debug_level):
                RPC2_DebugLevel = atoi(optarg);
                RPC2_Trace = 1;
                break;
            case NO_SHORT(lwp_debug_level):
                lwp_debug =atoi(optarg);
                break;
            case NO_SHORT(cache_files):
                CacheFiles = atoi(optarg);
                break;
            case NO_SHORT(cache_size):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(cache_chunk_size):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(mles):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(hdbes):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(rdstrace):
                //UNIMPLEMENTED
                break;
            case 'k':
                //UNIMPLEMENTED
                break;
            case 'f':
                //UNIMPLEMENTED
                break;
            case NO_SHORT(console):
                //UNIMPLEMENTED
                break;
            case 'm':
                //UNIMPLEMENTED
                break;
            case NO_SHORT(maxworkers):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(maxcbservers):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(maxprefetchers):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(retries):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(timeout):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(ws):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(sa):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(ap):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(ps):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(rvmt):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(vld):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(vdd):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(vdds):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(rdscs):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(rdsnl):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(logopts):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(swt):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(mwt):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(ssf):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(alpha):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(beta):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(gamma):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(novcb):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(nowalk):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(spooldir):
                //UNIMPLEMENTED
                break;
            case NO_SHORT(whole_file_max):
                //UNIMPLEMENTED
                break;
            default:
                exit(EXIT_INVALID_ARG);
        }
    }
}

/*
 * Use an adjusted logarithmic function experimentally linearlized around
 * the following points;
 * 2MB -> 85 cache files
 * 100MB -> 4166 cache files
 * 200MB -> 8333 cache files
 * With the logarithmic function the following values are obtained
 * 2MB -> 98 cache files
 * 100MB -> 4412 cache files
 * 200MB -> 8142 cache files
 */
static unsigned int CalculateCacheFiles(unsigned int CacheBlocks)
{
    static const int y_scale = 24200;
    static const double x_scale_down = 500000;

    return (unsigned int)(y_scale * log(CacheBlocks / x_scale_down + 1));
}


/* Initialize "general" unset command-line parameters to user specified values
 * or hard-wired defaults. */
/* Note that individual modules initialize their own unset command-line
 * parameters as appropriate. */
static void DefaultCmdlineParms()
{
    int DontUseRVM = 0;
    const char *CacheSize = NULL;
    const char *TmpCacheChunkBlockSize = NULL;
    const char *TmpWFMax = NULL;

    /* Load the "venus.conf" configuration file */
    codaconf_init("venus.conf");

    /* we will prefer the deprecated "cacheblocks" over "cachesize" */
    if (!CacheBlocks) {
        CODACONF_INT(CacheBlocks, "cacheblocks", 0);
        if (CacheBlocks)
            eprint("Using deprecated config 'cacheblocks', try the more flexible 'cachesize'");
    }

    if (!CacheBlocks) {
        CODACONF_STR(CacheSize, "cachesize", MIN_CS);
        CacheBlocks = ParseSizeWithUnits(CacheSize);
    }

    /* In case of user misconfiguration */
    if (CacheBlocks < MIN_CB) {
        eprint("Cannot start: minimum cache size is %s", "2MB");
        exit(EXIT_UNCONFIGURED);
    }

    CODACONF_INT(CacheFiles, "cachefiles", (int)CalculateCacheFiles(CacheBlocks));
    if (CacheFiles < MIN_CF) {
        eprint("Cannot start: minimum number of cache files is %d", CalculateCacheFiles(CacheBlocks));
        eprint("Cannot start: minimum number of cache files is %d", MIN_CF);
        exit(EXIT_UNCONFIGURED);
    }
    
    if (!CacheChunkBlockSize) {
        CODACONF_STR(TmpCacheChunkBlockSize, "cachechunkblocksize", "32KB");
        ParseCacheChunkBlockSize(TmpCacheChunkBlockSize);
    }
        
    if (!WholeFileMaxSize) {
        CODACONF_STR(TmpWFMax, "wholefilemaxsize", "50MB");
        WholeFileMaxSize = ParseSizeWithUnits(TmpWFMax);
    }
    
    CODACONF_STR(CacheDir,	    "cachedir",      DFLT_CD);
    CODACONF_STR(SpoolDir,	    "checkpointdir", "/usr/coda/spool");
    CODACONF_STR(VenusLogFile,	    "logfile",	     DFLT_LOGFILE);
    CODACONF_STR(consoleFile,	    "errorlog",      DFLT_ERRLOG);
    CODACONF_STR(kernDevice,	    "kerneldevice",  "/dev/cfs0,/dev/coda/0");
    CODACONF_INT(MapPrivate,	    "mapprivate",     0);
    CODACONF_STR(MarinerSocketPath, "marinersocket", "/usr/coda/spool/mariner");
    CODACONF_INT(masquerade_port,   "masquerade_port", 0);
    CODACONF_INT(allow_backfetch,   "allow_backfetch", 0);
    CODACONF_STR(venusRoot,	    "mountpoint",     DFLT_VR);
    CODACONF_INT(PrimaryUser,	    "primaryuser",    UNSET_PRIMARYUSER);
    CODACONF_STR(realmtab,	    "realmtab",	      "/etc/coda/realms");
    CODACONF_STR(VenusLogDevice,    "rvm_log",        "/usr/coda/LOG");
    CODACONF_STR(VenusDataDevice,   "rvm_data",       "/usr/coda/DATA");

    CODACONF_INT(rpc2_timeout,	    "RPC2_timeout",   DFLT_TO);
    CODACONF_INT(rpc2_retries,	    "RPC2_retries",   DFLT_RT);

    CODACONF_INT(T1Interval, "serverprobe", 150); // used to be 12 minutes

    CODACONF_INT(default_reintegration_age,  "reintegration_age",  0);
    CODACONF_INT(default_reintegration_time, "reintegration_time", 15);
    default_reintegration_time *= 1000; /* reintegration time is in msec */

#if defined(__CYGWIN32__)
    CODACONF_STR(CachePrefix, "cache_prefix", "/?" "?/C:/cygwin");
#else
    CachePrefix = "";
#endif

    CODACONF_INT(DontUseRVM, "dontuservm", 0);
    {
	if (DontUseRVM)
	    RvmType = VM;
    }

    CODACONF_INT(MLEs, "cml_entries", 0);
    {
	if (!MLEs) MLEs = CacheFiles * MLES_PER_FILE;

	if (MLEs < MIN_MLE) {
	    eprint("Cannot start: minimum number of cml entries is %d",MIN_MLE);
	    exit(EXIT_UNCONFIGURED);
	}
    }

    CODACONF_INT(HDBEs, "hoard_entries", 0);
    {
	if (!HDBEs) HDBEs = CacheFiles / FILES_PER_HDBE;

	if (HDBEs < MIN_HDBE) {
	    eprint("Cannot start: minimum number of hoard entries is %d",
		   MIN_HDBE);
	    exit(EXIT_UNCONFIGURED);
	}
    }

    CODACONF_STR(VenusPidFile, "pid_file", DFLT_PIDFILE);
    if (*VenusPidFile != '/') {
	char *tmp = (char *)malloc(strlen(CacheDir) + strlen(VenusPidFile) + 2);
	CODA_ASSERT(tmp);
	sprintf(tmp, "%s/%s", CacheDir, VenusPidFile);
	VenusPidFile = tmp;
    }

    CODACONF_STR(VenusControlFile, "run_control_file", DFLT_CTRLFILE);
    if (*VenusControlFile != '/') {
	char *tmp = (char *)malloc(strlen(CacheDir) + strlen(VenusControlFile) + 2);
	CODA_ASSERT(tmp);
	sprintf(tmp, "%s/%s", CacheDir, VenusControlFile);
	VenusControlFile = tmp;
    }

    CODACONF_STR(ASRLauncherFile, "asrlauncher_path", NULL);

    CODACONF_STR(ASRPolicyFile, "asrpolicy_path", NULL);

    CODACONF_INT(PiggyValidations, "validateattrs", 15);
    {
	if (PiggyValidations > MAX_PIGGY_VALIDATIONS)
	    PiggyValidations = MAX_PIGGY_VALIDATIONS;
    }

    /* Enable special tweaks for running in a VM
     * - Write zeros to container file contents before truncation.
     * - Disable reintegration replay detection. */
    CODACONF_INT(option_isr, "isr", 0);

    /* Kernel filesystem support */
    CODACONF_INT(codafs_enabled, "codafs", 1);
    CODACONF_INT(plan9server_enabled, "9pfs", 0);

    /* Allow overriding of the default setting from command line */
    if (codafs_enabled == -1)       codafs_enabled = false;
    if (plan9server_enabled == -1)  plan9server_enabled = false;

    /* Enable client-server communication helper process */
    CODACONF_INT(codatunnel_enabled, "codatunnel", 1);
    CODACONF_INT(codatunnel_onlytcp, "onlytcp", 0);

    if (codatunnel_onlytcp && codatunnel_enabled != -1)
        codatunnel_enabled = 1;
    if (codatunnel_enabled == -1) {
        codatunnel_onlytcp = 0;
        codatunnel_enabled = 0;
    }

    CODACONF_INT(detect_reintegration_retry, "detect_reintegration_retry", 1);
    if (option_isr) {
	detect_reintegration_retry = 0;
    }

    CODACONF_STR(CheckpointFormat,  "checkpointformat", "newc");
    if (strcmp(CheckpointFormat, "tar") == 0)	archive_type = TAR_TAR;
    if (strcmp(CheckpointFormat, "ustar") == 0) archive_type = TAR_USTAR;
    if (strcmp(CheckpointFormat, "odc") == 0)   archive_type = CPIO_ODC;
    if (strcmp(CheckpointFormat, "newc") == 0)  archive_type = CPIO_NEWC;

#ifdef moremoremore
    char *x = NULL;
    CODACONF_STR(x, "relay", NULL, "127.0.0.1");
    inet_aton(x, &venus_relay_addr);
#endif
}


static const char CACHEDIR_TAG[] =
"Signature: 8a477f597d28d172789f06886806bc55\n"
"# This file is a cache directory tag created by the Coda client (venus).\n"
"# For information about cache directory tags, see:\n"
"#   http://www.brynosaurus.com/cachedir/";

static void CdToCacheDir()
{
    struct stat statbuf;
    int fd;

    if (stat(CacheDir, &statbuf) != 0)
    {
        if (errno != ENOENT) {
            perror("CacheDir stat");
            exit(EXIT_FAILURE);
        }

        if (mkdir(CacheDir, 0700)) {
            perror("CacheDir mkdir");
            exit(EXIT_FAILURE);
        }
    }
    if (chdir(CacheDir)) {
        perror("CacheDir chdir");
        exit(EXIT_FAILURE);
    }

    /* create CACHEDIR.TAG as hint for backup programs */
    if (stat("CACHEDIR.TAG", &statbuf) == 0)
        return;

    fd = open("CACHEDIR.TAG", O_CREAT | O_WRONLY | O_EXCL | O_NOFOLLOW, 0444);
    if (fd == -1) return;
    write(fd, CACHEDIR_TAG, sizeof(CACHEDIR_TAG)-1);
    close(fd);
}

static void CheckInitFile() {
    char initPath[MAXPATHLEN];
    struct stat tstat;

    /* Construct name for INIT file */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);

    /* See if it's there */ 
    if (stat(initPath, &tstat) == 0) {
        /* If so, set InitMetaData */
	InitMetaData = 1;
    } else if ((errno == ENOENT) && (InitMetaData == 1)) {
        int initFD;

	/* If not and it should be, create it. */
        initFD = open(initPath, O_CREAT, S_IRUSR);
        if (initFD) {
	    write(initFD, initPath, strlen(initPath));
            close(initFD);
        }
    }
}

static void UnsetInitFile() {
    char initPath[MAXPATHLEN];

    /* Create the file, if it doesn't already exist */
    snprintf(initPath, MAXPATHLEN, "%s/INIT", CacheDir);
    unlink(initPath);
}

static void SetRlimits() {
#ifndef __CYGWIN32__
    /* Set DATA segment limit to maximum allowable. */
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("getrlimit");
        exit(EXIT_FAILURE);
    }

    rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_DATA, &rl) < 0) {
        perror("setrlimit");
        exit(EXIT_FAILURE);
    }
#endif
}

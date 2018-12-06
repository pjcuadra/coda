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
 *     Utility routines used by Venus.
 *
 *    ToDo:
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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <rpc2/rpc2.h>
/* interfaces */
// #include <vice.h>

#ifdef __cplusplus
}
#endif

/* from util */
#include <util.h>

/* from vv */
#include <inconsist.h>

/* from venus */
#include "venuslog.h"
#include "venuslog.subsystem.h"
#include "vproc.h"

struct logging_subsystem_instance_t {
    bool initialized;
    struct log_config config;
    FILE * log_file;
};

static struct logging_subsystem_instance_t log_sub_inst;

/* Venus Logging Subsystem */
int LoggingSetup(struct log_config config) {
    log_sub_inst.initialized = false;
    log_sub_inst.config = config;
    log_sub_inst.log_file = NULL;
}

int LoggingInit() {
    int ret = 0;

    log_sub_inst.log_file = fopen(log_sub_inst.config.log_file_path, "a+");

    if (log_sub_inst.log_file == NULL)   { 
        eprint("LogInit failed");
        return EPERM;
    }

    struct timeval now;
    ret = gettimeofday(&now, 0);
    if (ret) return ret;

    log_sub_inst.initialized = true;

    LOG(0, ("Coda Venus, version " PACKAGE_VERSION "\n"));

    LOG(0, ("Logfile initialized with LogLevel = %d at %s\n",
        log_sub_inst.config.logging_level, ctime((time_t *)&now.tv_sec)));

    return 0;
}

int LoggingUninit() {
    
}

struct log_config LoggingGetConf() {
    return (log_sub_inst.config);
}

bool LoggingIsInitialized() {
    return log_sub_inst.initialized;
}

/* Venus Logging API */
int GetLoggingLevel() {
    return log_sub_inst.config.logging_level;
}

void SetLoggingLevel(int logging_level) {
    log_sub_inst.config.logging_level = logging_level;
}

FILE * GetLogFile() {
    return log_sub_inst.log_file;
}

void SwapLog()
{
    struct timeval now;
    gettimeofday(&now, 0);

    freopen(log_sub_inst.config.log_file_path, "a+", log_sub_inst.log_file);
    if (!log_sub_inst.config.nofork) /* only redirect stderr when daemonizing */
        freopen(log_sub_inst.config.console_file_path, "a+", stderr);

    LOG(0, ("New Logfile started at %s", ctime((time_t *)&now.tv_sec)));
}

/* Print a debugging message to the log file. */
void dprint(const char *fmt, ...) {
    va_list ap;

    if (!log_sub_inst.initialized) return;

    char msg[240];
    (VprocSelf())->GetStamp(msg);

    /* Output a newline if we are starting a new block. */
    static int last_vpid = -1;
    static int last_seq = -1;
    int this_vpid;
    int this_seq;

    if (sscanf(msg, "[ %*c(%d) : %d : %*02d:%*02d:%*02d ] ", &this_vpid, &this_seq) != 2) {
        fprintf(stderr, "Choking in dprint\n");
        exit(EXIT_FAILURE);
    }

    if ((this_vpid != last_vpid || this_seq != last_seq) && (this_vpid != -1)) {
        fprintf(log_sub_inst.log_file, "\n");
        last_vpid = this_vpid;
        last_seq = this_seq;
    }

    va_start(ap, fmt);
    vsnprintf(msg + strlen(msg), 240-strlen(msg), fmt, ap);
    va_end(ap);

    fwrite(msg, (int)sizeof(char), (int) strlen(msg), log_sub_inst.log_file);
    fflush(log_sub_inst.log_file);
}

void DebugOn() {
    int logging_level = log_sub_inst.config.logging_level;

    logging_level = ((logging_level == 0) ? 1 : logging_level * 10);

    log_sub_inst.config.logging_level = logging_level;
}

void DebugOff() {
    log_sub_inst.config.logging_level = 0;
}


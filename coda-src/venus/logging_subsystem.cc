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
#include "logging.h"
#include "logging_subsystem.h"
#include "vproc.h"

/* Venus Logging Subsystem */
int LoggingSubsystem::init() {
    int ret = 0;

    log_file = fopen(config.log_file_path, "a+");

    if (log_file == NULL)   { 
        eprint("LogInit failed");
        return EPERM;
    }

    struct timeval now;
    ret = gettimeofday(&now, 0);
    if (ret) return ret;

    initialized = true;

    LOG(0, ("Coda Venus, version " PACKAGE_VERSION "\n"));

    LOG(0, ("Logfile initialized with LogLevel = %d at %s\n",
        config.logging_level, ctime((time_t *)&now.tv_sec)));

    return 0;
}

int LoggingSubsystem::uninit() {
    
}

/* Get singleton class */
LoggingSubsystem * LoggingSubsystem::GetInstance() {
    static LoggingSubsystem * sub = new LoggingSubsystem();
    return sub;
}

struct log_config * LoggingSubsystem::GetInstanceConf() {
    return &(LoggingSubsystem::GetInstance()->config);
}

int LoggingSubsystem::setup(struct log_config config) {
    static LoggingSubsystem * sub = LoggingSubsystem::GetInstance();
    sub->config = config;

    SubsystemManager::RegisterSubsystem(sub);
}

bool LoggingSubsystem::isInitialized() {
    return LoggingSubsystem::GetInstance()->_isInitialized();
}

/* Venus Logging API */
int GetLoggingLevel() {
    return LoggingSubsystem::GetInstanceConf()->logging_level;
}

void SetLoggingLevel(int logging_level) {
    LoggingSubsystem::GetInstanceConf()->logging_level = logging_level;
}

FILE * GetLogFile() {
    return LoggingSubsystem::GetInstance()->log_file;
}

void SwapLog()
{
    LoggingSubsystem * sub = LoggingSubsystem::GetInstance();
    struct timeval now;
    gettimeofday(&now, 0);

    freopen(sub->config.log_file_path, "a+", sub->log_file);
    if (!sub->config.nofork) /* only redirect stderr when daemonizing */
        freopen(sub->config.console_file_path, "a+", stderr);

    LOG(0, ("New Logfile started at %s", ctime((time_t *)&now.tv_sec)));
}

/* Print a debugging message to the log file. */
void dprint(const char *fmt, ...) {
    va_list ap;
    LoggingSubsystem * sub = LoggingSubsystem::GetInstance();

    if (!sub->isInitialized()) return;

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
        fprintf(sub->log_file, "\n");
        last_vpid = this_vpid;
        last_seq = this_seq;
    }

    va_start(ap, fmt);
    vsnprintf(msg + strlen(msg), 240-strlen(msg), fmt, ap);
    va_end(ap);

    fwrite(msg, (int)sizeof(char), (int) strlen(msg), sub->log_file);
    fflush(sub->log_file);
}

void DebugOn() {
    int logging_level = LoggingSubsystem::GetInstanceConf()->logging_level;

    logging_level = ((logging_level == 0) ? 1 : logging_level * 10);

    LoggingSubsystem::GetInstanceConf()->logging_level = logging_level;
}

void DebugOff() {
    LoggingSubsystem::GetInstanceConf()->logging_level = 0;
}


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
#include <stdarg.h>

#ifdef __cplusplus
}
#endif

#include <venusconf.h>
#include <vproc.h>
#include <venus/logging.h>

static FILE *logFile;
static int LogLevel  = 0;
static int LogInited = 0;

/* Print a debugging message to the log file. */
void dprint(const char *fmt...)
{
    va_list ap;

    if (!LogInited)
        return;

    char msg[240];
    (VprocSelf())->GetStamp(msg);

    /* Output a newline if we are starting a new block. */
    static int last_vpid = -1;
    static int last_seq  = -1;
    int this_vpid;
    int this_seq;
    if (sscanf(msg, "[ %*c(%d) : %d : %*02d:%*02d:%*02d ] ", &this_vpid,
               &this_seq) != 2) {
        fprintf(stderr, "Choking in dprint\n");
        exit(EXIT_FAILURE);
    }
    if ((this_vpid != last_vpid || this_seq != last_seq) && (this_vpid != -1)) {
        fprintf(logFile, "\n");
        last_vpid = this_vpid;
        last_seq  = this_seq;
    }

    va_start(ap, fmt);
    vsnprintf(msg + strlen(msg), 240 - strlen(msg), fmt, ap);
    va_end(ap);

    fwrite(msg, (int)sizeof(char), (int)strlen(msg), logFile);
    fflush(logFile);
}

void LogInit()
{
    logFile = fopen(GetVenusConf().get_value("logfile"), "a+");
    if (logFile == NULL) {
        eprint("LogInit failed");
        exit(EXIT_FAILURE);
    }
    LogInited = 1;
    LOG(0, ("Coda Venus, version " PACKAGE_VERSION "\n"));

    LogLevel = GetVenusConf().get_int_value("loglevel");

    struct timeval now;
    gettimeofday(&now, 0);
    LOG(0, ("Logfile initialized with LogLevel = %d at %s\n", LogLevel,
            ctime((time_t *)&now.tv_sec)));
}

void DebugOn()
{
    LogLevel = ((LogLevel == 0) ? 1 : LogLevel * 10);
    LOG(0, ("LogLevel is now %d.\n", LogLevel));
}

void DebugOff()
{
    LogLevel = 0;
    LOG(0, ("LogLevel is now %d.\n", LogLevel));
}

void SwapLog()
{
    struct timeval now;
    gettimeofday(&now, 0);

    freopen(GetVenusConf().get_value("logfile"), "a+", logFile);
    if (!GetVenusConf().get_bool_value(
            "nofork")) /* only redirect stderr when daemonizing */
        freopen(GetVenusConf().get_value("errorlog"), "a+", stderr);

    LOG(0, ("New Logfile started at %s", ctime((time_t *)&now.tv_sec)));
}

FILE *GetLogFile()
{
    return logFile;
}

int GetLogLevel()
{
    return LogLevel;
}

void SetLogLevel(int loglevel)
{
    LogLevel = loglevel;
    GetVenusConf().set_int("loglevel", loglevel);
}

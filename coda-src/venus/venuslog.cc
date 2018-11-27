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
#include "vproc.h"

int LoggingSubsystem::init() {
    int ret = 0;
    printf("-> %s\n", config.log_file_path);
    printf("-> %s\n", config.console_file_path);
    printf("-> %d\n", config.nofork);
    printf("-> %d\n", config.logging_level);
    log_file = fopen(config.log_file_path, "a+");

    if (log_file == NULL)   { 
        eprint("LogInit failed");
        return EPERM;
    }

    printf("TP: %s %d\n", __FILE__, __LINE__);

   

    struct timeval now;
    // ret = gettimeofday(&now, 0);
    if (ret) return ret;

    // LOG(0, ("TP: %s %d\n", __FILE__, __LINE__));

    initialized = true;

    LOG(0, ("Coda Venus, version " PACKAGE_VERSION "\n"));

    // LOG(0, ("Logfile initialized with LogLevel = %d at %s\n",
    //     config.logging_level, ctime((time_t *)&now.tv_sec)));

    printf("TP: %s %d\n", __FILE__, __LINE__);

    return 0;
}

int LoggingSubsystem::uninit() {
    
}

/* Get singleton class */
LoggingSubsystem * LoggingSubsystem::GetInstance() {
    static LoggingSubsystem * sub = new LoggingSubsystem();
    return sub;
}

int LoggingSubsystem::setup(struct log_config config) {
    static LoggingSubsystem * sub = LoggingSubsystem::GetInstance();
    sub->config = config;

    SubsystemManager::RegisterSubsystem(sub);
}

int LoggingSubsystem::_GetLoggingLevel() {
    return config.logging_level;
}

int LoggingSubsystem::GetLoggingLevel() {
    return LoggingSubsystem::GetInstance()->_GetLoggingLevel();
}

void LoggingSubsystem::_SetLoggingLevel(int logging_level) {
    config.logging_level = logging_level;
}

void LoggingSubsystem::SetLoggingLevel(int logging_level) {
    return LoggingSubsystem::GetInstance()->_SetLoggingLevel(logging_level);
}

FILE * LoggingSubsystem::_GetLogFile() {
    return log_file;
}

FILE * LoggingSubsystem::GetLogFile() {
    return LoggingSubsystem::GetInstance()->_GetLogFile();
}

void LoggingSubsystem::SwapLog()
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
void LoggingSubsystem::dprint(const char *fmt, ...) {
    va_list ap;
    LoggingSubsystem * sub = LoggingSubsystem::GetInstance();

    printf("TP: %s %d\n", __FILE__, __LINE__);

    if (!sub->isInitialized()) return;

    printf("TP: %s %d\n", __FILE__, __LINE__);

    char msg[240];
    (VprocSelf())->GetStamp(msg);

    printf("TP: %s %d\n", __FILE__, __LINE__);

    /* Output a newline if we are starting a new block. */
    static int last_vpid = -1;
    static int last_seq = -1;
    int this_vpid;
    int this_seq;

    printf("TP: %s %d\n", __FILE__, __LINE__);
    if (sscanf(msg, "[ %*c(%d) : %d : %*02d:%*02d:%*02d ] ", &this_vpid, &this_seq) != 2) {
        fprintf(stderr, "Choking in dprint\n");
        exit(EXIT_FAILURE);
    }

    printf("TP: %s %d\n", __FILE__, __LINE__);
    if ((this_vpid != last_vpid || this_seq != last_seq) && (this_vpid != -1)) {
        fprintf(sub->log_file, "\n");
        last_vpid = this_vpid;
        last_seq = this_seq;
    }

    printf("TP: %s %d\n", __FILE__, __LINE__);

    va_start(ap, fmt);
    vsnprintf(msg + strlen(msg), 240-strlen(msg), fmt, ap);
    va_end(ap);

    printf("TP: %s %d\n", __FILE__, __LINE__);

    fwrite(msg, (int)sizeof(char), (int) strlen(msg), sub->log_file);
    fflush(sub->log_file);
}

void LoggingSubsystem::_DebugOn() {
    config.logging_level = ((config.logging_level == 0) ? 1 : config.logging_level * 10);
    LOG(0, ("LogLevel is now %d.\n", config.logging_level));
}

void LoggingSubsystem::DebugOn() {
    return LoggingSubsystem::GetInstance()->_DebugOn();
}

void LoggingSubsystem::_DebugOff() {
    config.logging_level = 0;
    LOG(0, ("LogLevel is now %d.\n", config.logging_level));
}

void LoggingSubsystem::DebugOff() {
    return LoggingSubsystem::GetInstance()->_DebugOff();
}

void LoggingSubsystem::_DumpState() {
    if (!initialized) return;

    const char *argv[1];
    argv[0] = "all";
    VenusPrint(log_file, 1, argv);
    fflush(log_file);
}

void LoggingSubsystem::DumpState() {
    return LoggingSubsystem::GetInstance()->_DumpState();
}

bool LoggingSubsystem::isInitialized() {
    return LoggingSubsystem::GetInstance()->_isInitialized();
}


void VenusPrint(int argc, const char **argv) {
    VenusPrint(stdout, argc, argv);
}


void VenusPrint(FILE *fp, int argc, const char **argv) {
    fflush(fp);
    VenusPrint(fileno(fp), argc, argv);
}

/* local-repair modification */
void VenusPrint(int fd, int argc, const char **argv)
{
//     int allp = 0;
//     int rusagep = 0;
//     int recovp = 0;
//     int vprocp = 0;
//     int userp = 0;
//     int serverp = 0;
//     int connp = 0;
//     int vsgp = 0;
//     int mgrpp = 0;
//     int volumep = 0;
//     int fsop = 0;
//     int	fsosump	= 0;	    /* summary only */
//     int vfsp = 0;
//     int rpcp = 0;
//     int hdbp = 0;
//     int vmonp = 0;
//     int mallocp = 0;

//     /* Parse the argv to see what modules should be printed. */
//     for (int i = 0; i < argc; i++) {
// 	if (STREQ(argv[i], "all")) { allp++; break; }
// 	else if (STREQ(argv[i], "rusage")) rusagep++;
// 	else if (STREQ(argv[i], "recov")) recovp++;
// 	else if (STREQ(argv[i], "vproc")) vprocp++;
// 	else if (STREQ(argv[i], "user")) userp++;
// 	else if (STREQ(argv[i], "server")) serverp++;
// 	else if (STREQ(argv[i], "conn")) connp++;
// 	else if (STREQ(argv[i], "vsg")) vsgp++;
// 	else if (STREQ(argv[i], "mgrp")) mgrpp++;
// 	else if (STREQ(argv[i], "volume")) volumep++;
// 	else if (STREQ(argv[i], "fso")) fsop++;
// 	else if (STREQ(argv[i], "fsosum")) fsosump++;
// 	else if (STREQ(argv[i], "vfs")) vfsp++;
// 	else if (STREQ(argv[i], "rpc")) rpcp++;
// 	else if (STREQ(argv[i], "hdb")) hdbp++;
// 	else if (STREQ(argv[i], "vmon")) vmonp++;
// 	else if (STREQ(argv[i], "malloc")) mallocp++;
//     }

//     fdprint(fd, "*****  VenusPrint  *****\n\n");
//     FILE *f = fdopen(dup(fd), "a");
//     if (allp) REALMDB->print(f);
//     if (serverp || allp)  ServerPrint(f);
//     if ((mgrpp || allp) && VSGDB) VSGDB->print(f);
//     fclose(f);

//     if (rusagep || allp)  RusagePrint(fd);
//     if (recovp || allp)   if (RecovInited) RecovPrint(fd);
//     if (vprocp || allp)   PrintVprocs(fd);
//     if (userp || allp)    UserPrint(fd);
//     if (connp || allp)    ConnPrint(fd);
//     if (volumep || allp)  if (RecovInited && VDB) VDB->print(fd);
//     if (fsop || allp)     if (RecovInited && FSDB) FSDB->print(fd);
//     if (fsosump && !allp) if (RecovInited && FSDB) FSDB->print(fd, 1);
//     if (vfsp || allp)     VFSPrint(fd);
//     if (rpcp || allp)     RPCPrint(fd);
//     if (hdbp || allp)     if (RecovInited && HDB) HDB->print(fd);
//     if (mallocp || allp)  MallocPrint(fd);
//     fdprint(fd, "************************\n\n");
}


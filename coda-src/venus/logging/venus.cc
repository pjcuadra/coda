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

#include <venus/conf.h>
#include <venus/logging/venus.h>
#include <venus/logging/filelogger.h>

void LogInit(logging_stamp_callback_t stamp_cb)
{
    FileLogger *logger = NULL;
    int LogLevel       = GetVenusConf().get_int_value("loglevel");

    FILE *logFile = fopen(GetVenusConf().get_value("logfile"), "a+");
    if (logFile == NULL) {
        eprint("LogInit failed");
        exit(EXIT_FAILURE);
    }

    Logging::SetDefaultLogger();
    logger = (FileLogger *)Logging::GetLogger();

    logger->SetLogFile(logFile);
    logger->SetStampCallback(stamp_cb);

    Logging::SetEnable(true);
    Logging::SetLogLevel(LogLevel);
    Logging::SetLogger(logger);

    LOG(0, "Coda Venus, version " PACKAGE_VERSION "\n");

    struct timeval now;
    gettimeofday(&now, 0);
    LOG(0, "Logfile initialized with LogLevel = %d at %s\n", LogLevel,
        ctime((const time_t *)&(now.tv_sec)));
}

void SwapLog()
{
    FILE *logFile = GetLogFile();
    struct timeval now;
    gettimeofday(&now, 0);

    freopen(GetVenusConf().get_value("logfile"), "a+", logFile);
    if (!GetVenusConf().get_bool_value(
            "nofork")) /* only redirect stderr when daemonizing */
        freopen(GetVenusConf().get_value("errorlog"), "a+", stderr);

    LOG(0, "New Logfile started at %s", ctime((time_t *)&now.tv_sec));
}

FILE *GetLogFile()
{
    FileLogger *logger = (FileLogger *)Logging::GetLogger();
    return logger->GetLogFile();
}

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
#include <venus/logging/logging.h>
#include <venus/logging/filelogger.h>

static Logger *defaultLogger = new FileLogger();
Logger *Logging::logger      = defaultLogger;
uint Logging::LogLevel       = 0;
bool Logging::LogEnable      = false;

void Logging::DebugOn()
{
    LogLevel = ((LogLevel == 0) ? 1 : LogLevel * 10);
    LOG(0, "LogLevel is now %d.\n", LogLevel);
}

void Logging::DebugOff()
{
    LogLevel = 0;
    LOG(0, "LogLevel is now %d.\n", LogLevel);
}

void Logging::SetDefaultLogger()
{
    logger = defaultLogger;
}

int Logging::GetLogLevel()
{
    return LogLevel;
}

void Logging::SetLogLevel(uint loglevel)
{
    LogLevel = loglevel;
    GetVenusConf().set_int("loglevel", loglevel);
}

void Logging::log(uint level, ...)
{
    const char *fmt;
    if (!LogEnable)
        return;

    if (LogLevel < level)
        return;

    va_list args;
    va_start(args, level);
    fmt = va_arg(args, const char *);
    logger->log(fmt, args);
    va_end(args);
}

void Logging::log(uint level, logging_callback_with_args_t log_cb, ...)
{
    va_list args;

    if (LogLevel < level)
        return;

    va_start(args, log_cb);
    logger->LoggingCallBackArgs(log_cb, args);
    va_end(args);
}

void Logging::log(uint level, logging_callback_t log_cb)
{
    if (LogLevel < level)
        return;

    logger->LoggingCallBack(log_cb);
}

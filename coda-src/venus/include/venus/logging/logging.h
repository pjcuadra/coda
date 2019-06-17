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

#ifndef _LOGGING_H_
#define _LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include <venus/logging/logger.h>

#define LOG(...) Logging::log(__VA_ARGS__)

class Logging {
    static Logger *logger;
    static logging_stamp_callback_t stamp_fn;
    static uint LogLevel;
    static bool LogEnable;

public:
    static void log(uint level, ...);
    static void log(uint level, logging_callback_with_args_t log_cb, ...);
    static void log(uint level, logging_callback_t log_cb);
    static void SetStampCallback(logging_stamp_callback_t stamp_cb);
    static int GetLogLevel();
    static void SetLogLevel(uint loglevel);
    static void SetEnable(bool enable) { Logging::LogEnable = enable; }

    static void DebugOn();
    static void DebugOff();

    static void SetLogger(Logger *logger) { Logging::logger = logger; }
    static void SetDefaultLogger();
    static Logger *GetLogger() { return logger; }
};

#endif /* _LOGGING_H_ */
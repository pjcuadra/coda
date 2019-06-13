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
#include <assert.h>

#ifdef __cplusplus
}
#endif

#include <venus/conf.h>
#include <venus/logging/logging.h>
#include <venus/logging/filelogger.h>

void Logger::GetStamp(char *stamp)
{
    if (in_callback)
        return;

    if (stamp_fn == NULL) {
        stamp[0] = '\0';
        return;
    }

    in_callback = true;
    stamp_fn(stamp);
    in_callback = false;
}

void Logger::log(const char *fmt, va_list args)
{
    char stamp[240] = "";

    GetStamp(stamp);
    stamped_log(stamp, fmt, args);
}

void Logger::LoggingCallBackArgs(logging_callback_with_args_t log_cb,
                                 va_list args)
{
    if (log_cb == NULL)
        return;

    log_cb(args);
}

void Logger::LoggingCallBack(logging_callback_t log_cb)
{
    if (log_cb == NULL)
        return;

    log_cb();
}

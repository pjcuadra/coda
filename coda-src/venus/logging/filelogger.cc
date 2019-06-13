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

void FileLogger::stamped_log(const char *stamp, const char *fmt, va_list args)
{
    char msg[240];

    vsnprintf(msg + strlen(stamp), 240 - strlen(stamp), fmt, args);

    fwrite(msg, (int)sizeof(char), (int)strlen(msg), logFile);
    fflush(logFile);
}

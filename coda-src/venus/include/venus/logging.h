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
 * Manifest constants for Venus, plus declarations for source files without
 * their own headers.
 */

#ifndef _VENUS_LOGGING_H_
#define _VENUS_LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

/*  *****  Debugging macros.  *****  */
#ifndef VENUSDEBUG
#define LOG(...)
#endif /* !VENUSDEBUG */

typedef void (*stamp_callback_t)(FILE *logFile, char *msg);
typedef void (*logging_args_callback_t)(FILE *logFile, ...);
typedef void (*logging_callback_t)(FILE *logFile);

void LOG(int level, const char *fmt, ...);
void LOG(int level, logging_args_callback_t log_cb, ...);
void LOG(int level, logging_callback_t log_cb);
void dprint(const char *...);
void SetLoggingStampCallback(stamp_callback_t stamp_cb);
void LogInit();
void DebugOn();
void DebugOff();
void SwapLog();
FILE *GetLogFile();
int GetLogLevel();
void SetLogLevel(int loglevel);

#endif /* _VENUS_LOGGING_H_ */
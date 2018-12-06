/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _VENUS_LOG_
#define _VENUS_LOG_

#include <stdio.h>

/*  *****  Debugging macros.  *****  */
#ifdef	VENUSDEBUG
#define	LOG(level, stmt) do { if (GetLoggingLevel() >= (level)) dprint stmt; } while(0)
#else
#define	LOG(level, stmt)
#endif /* !VENUSDEBUG */

int GetLoggingLevel();

void SetLoggingLevel(int logging_level);

FILE * GetLogFile();

void SwapLog();

void dprint(const char *fmt, ...);

void DebugOn();

void DebugOff();

#endif /* _VENUS_LOG_ */
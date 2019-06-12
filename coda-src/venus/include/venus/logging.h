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
#ifdef VENUSDEBUG
#define LOG(level, stmt)              \
    do {                              \
        if (GetLogLevel() >= (level)) \
            dprint stmt;              \
    } while (0)
#else
#define LOG(level, stmt)
#endif /* !VENUSDEBUG */

/*  *****  Misc stuff  *****  */
#define TRANSLATE_TO_LOWER(s)      \
    {                              \
        for (char *c = s; *c; c++) \
            if (isupper(*c))       \
                *c = tolower(*c);  \
    }
#define TRANSLATE_TO_UPPER(s)      \
    {                              \
        for (char *c = s; *c; c++) \
            if (islower(*c))       \
                *c = toupper(*c);  \
    }

/*  *****  Declarations for source files without their own headers.  ***** */
void dprint(const char *...);
void rds_printer(char *...);
void VenusPrint(int argc, const char **argv);
void VenusPrint(FILE *, int argc, const char **argv);
void VenusPrint(int, int argc, const char **argv);
const char *VenusOpStr(int);
const char *IoctlOpStr(unsigned char nr);
const char *VenusRetStr(int);
int binaryfloor(int);
void LogInit();
void DebugOn();
void DebugOff();
void Terminate();
void DumpState();
void RusagePrint(int);
void VFSPrint(int);
void RPCPrint(int);
void MallocPrint(int);
void StatsInit();
void SwapLog();

FILE *GetLogFile();
int GetLogLevel();
void SetLogLevel(int loglevel);

#endif /* _VENUS_LOGGING_H_ */
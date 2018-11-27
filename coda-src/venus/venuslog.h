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

#include "subsystem.h"

/*  *****  Debugging macros.  *****  */
#ifdef	VENUSDEBUG
#define	LOG(level, stmt) do { if (LoggingSubsystem::GetLoggingLevel() >= (level)) LoggingSubsystem::dprint stmt; } while(0)
#else
#define	LOG(level, stmt)
#endif /* !VENUSDEBUG */

struct log_config {
    int logging_level;
    const char * log_file_path;
    const char * console_file_path;
    int nofork;
};

/*  *****  Declarations for source files without their own headers.  ***** */
void VenusPrint(int argc, const char **argv);
void VenusPrint(FILE *, int argc, const char **argv);
void VenusPrint(int, int argc, const char **argv);
void VFSPrint(int);

class LoggingSubsystem : public Subsystem {
private:
    struct log_config config;
    FILE * log_file = NULL;
    LoggingSubsystem() : Subsystem("Logging"), log_file(NULL) {
        printf("CREEE\n");
    }
    int init();
    int uninit();

    int _GetLoggingLevel();

    void _SetLoggingLevel(int logging_level);

    FILE * _GetLogFile();

    void _DebugOn();

    void _DebugOff();

    void _DumpState();

public:
    /* Get singleton class */
    static LoggingSubsystem * GetInstance();

    static int setup(struct log_config config);

    static int GetLoggingLevel();

    static void SetLoggingLevel(int logging_level);

    static FILE * GetLogFile();

    static void SwapLog();

    static void dprint(const char *fmt, ...);

    static void DebugOn();
    static void DebugOff();

    static void DumpState();
    static bool isInitialized();
};



#endif /* _VENUS_LOG_ */
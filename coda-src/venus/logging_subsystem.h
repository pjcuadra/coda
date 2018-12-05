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

#ifndef _VENUS_LOG_SUBSYSTEM_
#define _VENUS_LOG_SUBSYSTEM_

#include "subsystem.h"
#include "logging.h"

struct log_config {
    int logging_level;
    const char * log_file_path;
    const char * console_file_path;
    int nofork;
};

class LoggingSubsystem : public Subsystem {
/* Allow Logging API to interact with the subsystem configuration */
friend int GetLoggingLevel();
friend void SetLoggingLevel(int logging_level);
friend FILE * GetLogFile();
friend void SwapLog();
friend void dprint(const char *fmt, ...);
friend void DebugOn();
friend void DebugOff();
friend void DumpState();

private:
    struct log_config config;
    FILE * log_file = NULL;
    LoggingSubsystem() : Subsystem("Logging"), log_file(NULL) {}

    int init();

    int uninit();

public:
    /* Get singleton class */
    static LoggingSubsystem * GetInstance();

    static struct log_config * GetInstanceConf();

    static int setup(struct log_config config);

    static bool isInitialized();
};



#endif /* _VENUS_LOG_SUBSYSTEM_ */
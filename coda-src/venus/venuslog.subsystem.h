/* BLURB gpl

                           Coda File System
                              Release 7

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

/*
 * Venus Logging Subsystem managing API
 */

struct log_config {
    int logging_level;
    const char * log_file_path;
    const char * console_file_path;
    int nofork;
};

int LoggingSetup(struct log_config config);
int LoggingInit();
int LoggingUninit();
bool LoggingIsInitialized();

struct log_config LoggingGetConf();

#endif /* _VENUS_LOG_SUBSYSTEM_ */
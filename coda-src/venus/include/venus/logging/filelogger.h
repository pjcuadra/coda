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

#ifndef _VENUS_FILE_LOGGER_H_
#define _VENUS_FILE_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include <venus/logging/logging.h>

class FileLogger : public Logger {
private:
    FILE *logFile;
    void stamped_log(const char *stamp, const char *fmt, va_list args);

public:
    FileLogger()
        : Logger()
        , logFile(stdout)
    {
    }

    void SetLogFile(FILE *logFile) { this->logFile = logFile; }
    FILE *GetLogFile() { return logFile; }
    void LoggingCallBackArgs(logging_callback_with_args_t log_cb, va_list args);
    void LoggingCallBack(logging_callback_t log_cb);
};

#endif /* _VENUS_FILE_LOGGER_H_ */

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

#ifndef _VENUS_LOGGER_H_
#define _VENUS_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#define LOG(...) Logging::log(__VA_ARGS__)

class Logger;

typedef void (*logging_stamp_callback_t)(char *stamp);
typedef void (*logging_callback_with_args_t)(va_list args);
typedef void (*logging_callback_t)();

class Logger {
private:
    bool in_callback;
    void GetStamp(char *stamp);
    logging_stamp_callback_t stamp_fn = NULL;

protected:
    virtual void stamped_log(const char *stamp, const char *fmt,
                             va_list args) = 0;

public:
    Logger()
        : in_callback(false)
        , stamp_fn(NULL)
    {
    }
    virtual ~Logger() {}

    void SetStampCallback(logging_stamp_callback_t stamp_cb)
    {
        stamp_fn = stamp_cb;
    }
    void LoggingCallBackArgs(logging_callback_with_args_t log_cb, va_list args);
    void LoggingCallBack(logging_callback_t log_cb);
    void log(const char *fmt, va_list args);
};

#endif /* _VENUS_LOGGER_H_ */

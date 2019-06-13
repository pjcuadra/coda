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
/* system */
#include <sys/stat.h>
#include <stdarg.h>

/* external */
#include <gtest/gtest.h>

/* from coda-src */
#include <venus/logging/logging.h>
#include <venus/conf.h>

#include <fstream>
#include <iostream>

using namespace std;

static bool stamping_triggered = false;
static bool log_triggered      = false;
static const char *logFileName = "dummy.log";

namespace
{
class TestLogger : public Logger {
    char last_entry[500];
    char last_entry_stamp[500];

public:
    void stamped_log(const char *stamp, const char *fmt, va_list args)
    {
        va_list args_cpy;

        log_triggered = true;
        strcpy(last_entry_stamp, stamp);

        va_copy(args_cpy, args);
        vsnprintf(last_entry, 500, fmt, args_cpy);
    }

    char *getLastEntry() { return last_entry; }

    char *getLastEntryStamp() { return last_entry_stamp; }
};

class LoggingTest : public ::testing::Test {
protected:
    TestLogger *logger;
    void SetUp() override
    {
        logger = new TestLogger();
        Logging::SetLogger(logger);
        Logging::SetEnable(true);
        Logging::SetLogLevel(10);
    }

    void TearDown() override
    {
        Logging::SetDefaultLogger();
        delete logger;
    }

    void assertLastLoggedMessage(const char *stamp, const char *entry)
    {
        EXPECT_STREQ(logger->getLastEntryStamp(), stamp);
        EXPECT_STREQ(logger->getLastEntry(), entry);
    }
};

static void dummy_stamping_cb(char *stamp)
{
    const char *_stamp = "DUMMY_STAMP: ";
    strncpy(stamp, _stamp, strlen(_stamp) + 1);
    stamping_triggered = true;
}

TEST_F(LoggingTest, simple_log)
{
    va_list va;
    log_triggered = false;
    logger->log("DUMMY\n", va);
    EXPECT_TRUE(log_triggered);
    assertLastLoggedMessage("", "DUMMY\n");
}

TEST_F(LoggingTest, stamping_cb)
{
    va_list va;

    logger->SetStampCallback(dummy_stamping_cb);
    stamping_triggered = false;
    log_triggered      = false;
    logger->log("DUMMY\n", va);
    EXPECT_TRUE(stamping_triggered);
    EXPECT_TRUE(log_triggered);
    assertLastLoggedMessage("DUMMY_STAMP: ", "DUMMY\n");
}

TEST_F(LoggingTest, no_stamping_cb)
{
    va_list va;

    stamping_triggered = false;
    log_triggered      = false;
    logger->log("DUMMY\n", va);
    EXPECT_FALSE(stamping_triggered);
    EXPECT_TRUE(log_triggered);
    assertLastLoggedMessage("", "DUMMY\n");
}

TEST_F(LoggingTest, logged_cb)
{
    log_triggered = false;
    LOG(1, "DUMMY\n");
    EXPECT_TRUE(log_triggered);
}

TEST_F(LoggingTest, log_init_log)
{
    GetVenusConf().set("logfile", logFileName);
    LogInit(dummy_stamping_cb);
    log_triggered = false;
    LOG(1, "DUMMY\n");
    EXPECT_FALSE(log_triggered);
}

TEST(NullLoggingTest, null_logger)
{
    log_triggered = false;
    LOG(1, "DUMMY\n");
    LOG(1, "DUMMY %s %s\n", "A", "A");
    EXPECT_FALSE(log_triggered);
}

TEST_F(LoggingTest, stamped_message_cb)
{
    logger->SetStampCallback(dummy_stamping_cb);

    stamping_triggered = false;
    LOG(1, "DUMMY\n");
    EXPECT_TRUE(stamping_triggered);
    assertLastLoggedMessage("DUMMY_STAMP: ", "DUMMY\n");
}

static void stamping_cb_with_logging_call(char *stamp)
{
    const char *_stamp = "DUMMY_STAMP: ";
    strncpy(stamp, _stamp, strlen(_stamp) + 1);
    stamping_triggered = true;
    Logging::log(1, "logging inside a stamping callback");
}

TEST_F(LoggingTest, log_in_stamping_cb)
{
    logger->SetStampCallback(stamping_cb_with_logging_call);
    LOG(1, "DUMMY\n");
    assertLastLoggedMessage("DUMMY_STAMP: ", "DUMMY\n");
}

static void logging_cb()
{
    LOG(1, "Logging Callback log call\n");
}

TEST_F(LoggingTest, log_in_logging_cb)
{
    LOG(1, logging_cb);
    assertLastLoggedMessage("", "Logging Callback log call\n");
}

static void logging_with_args_cb(va_list args)
{
    LOG(1, va_arg(args, char *));
}

TEST_F(LoggingTest, log_in_logging_with_args_cb)
{
    LOG(1, logging_with_args_cb, "PASSED DUMMY TEXT\n");
    assertLastLoggedMessage("", "PASSED DUMMY TEXT\n");
}

TEST_F(LoggingTest, log_formated_text)
{
    LOG(1, "%d %s\n", 10, "PASSED DUMMY TEXT");
    assertLastLoggedMessage("", "10 PASSED DUMMY TEXT\n");
}

} // namespace

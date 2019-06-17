/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2019 Carnegie Mellon University
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
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#ifdef __cplusplus
}
#endif

#include <venus/conf.h>
#include "dlist.h"

static VenusConf global_conf;

static const unsigned int max_int64_string_length = 23;

/* Bytes units convertion */
static const char *KBYTES_UNIT[] = { "KB", "kb", "Kb", "kB", "K", "k" };
static const unsigned int KBYTE_UNIT_SCALE = 1;
static const char *MBYTES_UNIT[] = { "MB", "mb", "Mb", "mB", "M", "m" };
static const unsigned int MBYTE_UNIT_SCALE = 1024 * KBYTE_UNIT_SCALE;
static const char *GBYTES_UNIT[] = { "GB", "gb", "Gb", "gB", "G", "g" };
static const unsigned int GBYTE_UNIT_SCALE = 1024 * MBYTE_UNIT_SCALE;
static const char *TBYTES_UNIT[] = { "TB", "tb", "Tb", "tB", "T", "t" };
static const unsigned int TBYTE_UNIT_SCALE = 1024 * GBYTE_UNIT_SCALE;

/*
 * Use an adjusted logarithmic function experimentally linearlized around
 * the following points;
 * 2MB -> 85 cache files
 * 100MB -> 4166 cache files
 * 200MB -> 8333 cache files
 * With the logarithmic function the following values are obtained
 * 2MB -> 98 cache files
 * 100MB -> 4412 cache files
 * 200MB -> 8142 cache files
 */
static unsigned int CalculateCacheFiles(unsigned int CacheBlocks)
{
    static const int y_scale         = 24200;
    static const double x_scale_down = 500000;

    return (unsigned int)(y_scale * log(CacheBlocks / x_scale_down + 1));
}

/*
 * Parse size value and converts into amount of 1K-Blocks
 */
uint64_t ParseSizeWithUnits(const char *SizeWUnits)
{
    const char *units = NULL;
    int scale_factor  = 1;
    char SizeWOUnits[256];
    size_t size_len   = 0;
    uint64_t size_int = 0;

    /* Locate the units and determine the scale factor */
    for (int i = 0; i < 6; i++) {
        if ((units = strstr(SizeWUnits, KBYTES_UNIT[i]))) {
            scale_factor = KBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, MBYTES_UNIT[i]))) {
            scale_factor = MBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, GBYTES_UNIT[i]))) {
            scale_factor = GBYTE_UNIT_SCALE;
            break;
        }

        if ((units = strstr(SizeWUnits, TBYTES_UNIT[i]))) {
            scale_factor = TBYTE_UNIT_SCALE;
            break;
        }
    }

    /* Strip the units from string */
    if (units) {
        size_len = (size_t)((units - SizeWUnits) / sizeof(char));
        strncpy(SizeWOUnits, SizeWUnits, size_len);
        SizeWOUnits[size_len] = 0; // Make it null-terminated
    } else {
        snprintf(SizeWOUnits, sizeof(SizeWOUnits), "%s", SizeWUnits);
    }

    /* Scale the value */
    size_int = scale_factor * atof(SizeWOUnits);

    return size_int;
}

static int power_of_2(uint64_t num)
{
    int power = 0;

    if (!num)
        return -1;

    /*Find the first 1 */
    while (!(num & 0x1)) {
        num = num >> 1;
        power++;
    }

    /* Shift the first 1 */
    num = num >> 1;

    /* Any other 1 means not power of 2 */
    if (num)
        return -1;

    return power;
}

VenusConf::~VenusConf()
{
    on_off_pair *curr_pair = NULL;

    while ((curr_pair = (on_off_pair *)on_off_pairs_list.first())) {
        on_off_pairs_list.remove((dlink *)curr_pair);
        delete curr_pair;
    }
}

bool VenusConf::check_if_value_is_int(const char *key)
{
    const char *value = get_value(key);
    int64_t ret_val   = 0;
    int read_vals     = 0;

    if (!value)
        return true;

    read_vals = sscanf(value, "%" PRId64, &ret_val);
    if (read_vals != 1)
        eprint("Cannot start: %s is not an integer, it's set to %s", key,
               value);
    return (read_vals == 1);
}

int64_t VenusConf::get_int_value(const char *key)
{
    const char *value = get_value(key);
    int64_t ret_val   = 0;
    int read_vals     = 0;
    CODA_ASSERT(value);
    read_vals = sscanf(value, "%" PRId64, &ret_val);
    CODA_ASSERT(read_vals == 1);
    return ret_val;
}

const char *VenusConf::get_string_value(const char *key)
{
    return get_value(key);
}

bool VenusConf::get_bool_value(const char *key)
{
    return (get_int_value(key) ? 1 : 0);
}

int VenusConf::add_on_off_pair(const char *on_key, const char *off_key,
                               bool on_value)
{
    if (has_key(unalias_key(on_key)) || has_key(unalias_key(off_key))) {
        return EEXIST;
    }

    add(on_key, on_value ? "1" : "0");
    add(off_key, on_value ? "0" : "1");

    on_off_pairs_list.insert((dlink *)new on_off_pair(on_key, off_key));

    return 0;
}

void VenusConf::set(const char *key, const char *value)
{
    VenusConf::on_off_pair *pair = find_on_off_pair(key);

    if (pair) {
        bool bool_val = atoi(value) ? true : false;
        StringKeyValueStore::set(pair->on_val, bool_val ? "1" : "0");
        StringKeyValueStore::set(pair->off_val, bool_val ? "0" : "1");
    } else {
        StringKeyValueStore::set(key, value);
    }
}

void VenusConf::load_default_config()
{
    if (already_loaded)
        return;
    already_loaded = true;

    add("cachesize", MIN_CS);
    add_int("cacheblocks", 0);
    add_int("cachefiles", 0);
    add("cachechunkblocksize", "32KB");
    add("wholefilemaxsize", "50MB");
    add("wholefileminsize", "4MB");
    add("wholefilemaxstall", "10");
    add("partialcachefilesratio", "1");
    add("cachedir", DFLT_CD);
    add("checkpointdir", "/usr/coda/spool");
    add("logfile", DFLT_LOGFILE);
    add("errorlog", DFLT_ERRLOG);
    add("kerneldevice", "/dev/cfs0,/dev/coda/0");
    add_int("mapprivate", 0);
    add("marinersocket", "/usr/coda/spool/mariner");
    add_int("masquerade_port", 0);
    add_int("allow_backfetch", 0);
    add("mountpoint", DFLT_VR);
    add_int("primaryuser", UNSET_PRIMARYUSER);
    add("realmtab", "/etc/coda/realms");
    add("rvm_log", "/usr/coda/LOG");
    add("rvm_data", "/usr/coda/DATA");
    add_int("RPC2_timeout", DFLT_TO);
    add_int("RPC2_retries", DFLT_RT);
    add_int("serverprobe", 150);
    add_int("reintegration_age", 0);
    add_int("reintegration_time", 15);
    add_int("dontuservm", 0);
    add_int("cml_entries", 0);
    add_int("hoard_entries", 0);
    add("pid_file", DFLT_PIDFILE);
    add("run_control_file", DFLT_CTRLFILE);
    add("asrlauncher_path", "");
    add("asrpolicy_path", "");
    add_int("validateattrs", 15);
    add_int("isr", 0);
    add_on_off_pair("codafs", "no-codafs", true);
    add_on_off_pair("9pfs", "no-9pfs", true);
    add_on_off_pair("codatunnel", "no-codatunnel", true);
    add_int("onlytcp", 0);
    add_int("detect_reintegration_retry", 1);
    add("checkpointformat", "newc");

    //Newly added
    add("initmetadata", "0");
    add("loglevel", "0");
    add("rpc2loglevel", "0");
    add("lwploglevel", "0");
    add("rdstrace", "0");
    add("copmodes", "6");
    add_int("maxworkers", DFLT_MAXWORKERS);
    add_int("maxcbservers", DFLT_MAXCBSERVERS);
    add_int("maxprefetchers", DFLT_MAXPREFETCHERS);
    add_int("sftp_windowsize", DFLT_WS);
    add_int("sftp_sendahead", DFLT_SA);
    add_int("sftp_ackpoint", DFLT_AP);
    add_int("sftp_packetsize", DFLT_PS);
    add_int("rvmtype", UNSET);
    add_int("rvm_log_size", UNSET_VLDS);
    add_int("rvm_data_size", UNSET_VDDS);
    add_int("rds_chunk_size", UNSET_RDSCS);
    add_int("rds_list_size", UNSET_RDSNL);
    add("log_optimization", "1");

    add_int("swt", DFLT_SWT);
    add_int("mwt", DFLT_MWT);
    add_int("ssf", DFLT_SSF);
    add_on_off_pair("von", "no-voff", DFLT_ST);
    add_on_off_pair("vmon", "vmoff", DFLT_MT);
    add("SearchForNOreFind", "0");
    add_on_off_pair("asr", "noasr", true);
    add_on_off_pair("vcb", "novcb", true);
    add("nowalk", "0");
#if defined(HAVE_SYS_UN_H) && !defined(__CYGWIN32__)
    add_on_off_pair("MarinerTcp", "noMarinerTcp", false);
#else
    add_on_off_pair("MarinerTcp", "noMarinerTcp", true);
#endif
    add("allow-reattach", "0");
    add("nofork", "0");
}

void VenusConf::configure_cmdline_options()
{
    if (already_configured)
        return;
    already_configured = true;

    add_key_alias("cachesize", "-c");
    add_key_alias("cachefiles", "-cf");
    add_key_alias("cachechunkblocksize", "-ccbs");
    add_key_alias("wholefilemaxsize", "-wfmax");
    add_key_alias("wholefileminsize", "-wfmin");
    add_key_alias("wholefilemaxstall", "-wfstall");
    add_key_alias("partialcachefilesratio", "-pcfr");
    add_key_alias("initmetadata", "-init");
    add_key_alias("cachedir", "-f");
    add_key_alias("checkpointdir", "-spooldir");
    add_key_alias("errorlog", "-console");
    add_key_alias("kerneldevice", "-k");
    add_key_alias("mapprivate", "-mapprivate");
    add_key_alias("primaryuser", "-primaryuser");
    add_key_alias("rvm_log", "-vld");
    add_key_alias("rvm_log_size", "-vlds");
    add_key_alias("rvm_data", "-vdd");
    add_key_alias("rvm_data_size", "-vdds");
    add_key_alias("RPC2_timeout", "-timeout");
    add_key_alias("RPC2_retries", "-retries");
    add_key_alias("cml_entries", "-mles");
    add_key_alias("hoard_entries", "-hdbes");
    add_key_alias("codafs", "-codafs");
    add_key_alias("no-codafs", "-no-codafs");
    add_key_alias("9pfs", "-9pfs");
    add_key_alias("no-9pfs", "-no-9pfs");
    add_key_alias("onlytcp", "-onlytcp");
    add_key_alias("loglevel", "-d");
    add_key_alias("rpc2loglevel", "-rpcdebug");
    add_key_alias("lwploglevel", "-lwpdebug");
    add_key_alias("rdstrace", "-rdstrace");
    add_key_alias("copmodes", "-m");
    add_key_alias("maxworkers", "-maxworkers");
    add_key_alias("maxcbservers", "-maxcbservers");
    add_key_alias("maxprefetchers", "-maxprefetchers");
    add_key_alias("sftp_windowsize", "-ws");
    add_key_alias("sftp_sendahead", "-sa");
    add_key_alias("sftp_ackpoint", "-ap");
    add_key_alias("sftp_packetsize", "-ps");
    add_key_alias("rvmtype", "-rvmt");
    add_key_alias("rds_chunk_size", "-rdscs");
    add_key_alias("rds_list_size", "-rdsnl");
    add_key_alias("log_optimization", "-logopts");

    add_key_alias("swt", "-swt");
    add_key_alias("mwt", "-mwt");
    add_key_alias("ssf", "-ssf");
    add_key_alias("von", "-von");
    add_key_alias("voff", "-voff");
    add_key_alias("vmon", "-vmon");
    add_key_alias("vmoff", "-vmoff");
    add_key_alias("SearchForNOreFind", "-SearchForNOreFind");
    add_key_alias("noasr", "-noasr");
    add_key_alias("novcb", "-novcb");
    add_key_alias("nowalk", "-nowalk");
    add_key_alias("MarinerTcp", "-MarinerTcp");
    add_key_alias("noMarinerTcp", "-noMarinerTcp");
    add_key_alias("allow-reattach", "-allow-reattach");
    add_key_alias("masquerade", "-masquerade");
    add_key_alias("nomasquerade", "-nomasquerade");
    add_key_alias("nofork", "-nofork");
    add_on_off_pair("-codatunnel", "-no-codatunnel", true);
}

void VenusConf::apply_consistency_rules()
{
    uint64_t cacheblock = get_int_value("cacheblocks");
    int cachefiles      = get_int_value("cachefiles");
    uint64_t cachechunkblocksize =
        ParseSizeWithUnits(get_value("cachechunkblocksize")) * 1024;

    /* we will prefer the deprecated "cacheblocks" over "cachesize" */
    if (get_int_value("cacheblocks"))
        eprint(
            "Using deprecated config 'cacheblocks', try the more flexible 'cachesize'");
    else {
        cacheblock = ParseSizeWithUnits(get_value("cachesize"));
        set_int("cacheblocks", cacheblock);
    }

    if (cachefiles == 0) {
        cachefiles = (int)CalculateCacheFiles(cacheblock);
        set_int("cachefiles", cachefiles);
    }

    add_int("cachechunkblockbitsize", power_of_2(cachechunkblocksize));

    if (get_bool_value("dontuservm"))
        set_int("rvmtype", VM);

    /* reintegration time is in msec */
    set_int("reintegration_time", get_int_value("reintegration_time") * 1000);

    if (!get_int_value("cml_entries"))
        set_int("cml_entries", cachefiles * MLES_PER_FILE);

    if (!get_int_value("hoard_entries"))
        set_int("hoard_entries", cachefiles / FILES_PER_HDBE);

    handle_relative_path("pid_file");
    handle_relative_path("run_control_file");

    /* Enable special tweaks for running in a VM
     * - Write zeros to container file contents before truncation.
     * - Disable reintegration replay detection. */
    if (get_bool_value("isr")) {
        set_int("detect_reintegration_retry", 0);
    }

    if (get_int_value("validateattrs") > MAX_PIGGY_VALIDATIONS)
        set_int("validateattrs", MAX_PIGGY_VALIDATIONS);

    if (get_bool_value("onlytcp")) {
        set("codatunnel", "1");
    }

    // /* If explicitly disabled thru the command line */
    if (get_bool_value("-no-codatunnel")) {
        set("codatunnel", "0");
        set("onlytcp", "0");
    }
}

bool VenusConf::check_all_int_values()
{
    bool all_ok = true;

    all_ok &= check_if_value_is_int("codatunnel");
    all_ok &= check_if_value_is_int("cacheblocks");
    all_ok &= check_if_value_is_int("cachefiles");
    all_ok &= check_if_value_is_int("wholefilemaxstall");
    all_ok &= check_if_value_is_int("partialcachefilesratio");
    all_ok &= check_if_value_is_int("mapprivate");
    all_ok &= check_if_value_is_int("masquerade_port");
    all_ok &= check_if_value_is_int("allow_backfetch");
    all_ok &= check_if_value_is_int("primaryuser");
    all_ok &= check_if_value_is_int("RPC2_timeout");
    all_ok &= check_if_value_is_int("RPC2_retries");
    all_ok &= check_if_value_is_int("serverprobe");
    all_ok &= check_if_value_is_int("reintegration_age");
    all_ok &= check_if_value_is_int("reintegration_time");
    all_ok &= check_if_value_is_int("dontuservm");
    all_ok &= check_if_value_is_int("cml_entries");
    all_ok &= check_if_value_is_int("hoard_entries");
    all_ok &= check_if_value_is_int("validateattrs");
    all_ok &= check_if_value_is_int("isr");
    all_ok &= check_if_value_is_int("codafs");
    all_ok &= check_if_value_is_int("9pfs");
    all_ok &= check_if_value_is_int("codatunnel");
    all_ok &= check_if_value_is_int("onlytcp");
    all_ok &= check_if_value_is_int("detect_reintegration_retry");
    all_ok &= check_if_value_is_int("initmetadata");
    all_ok &= check_if_value_is_int("loglevel");
    all_ok &= check_if_value_is_int("rpc2loglevel");
    all_ok &= check_if_value_is_int("lwploglevel");
    all_ok &= check_if_value_is_int("rdstrace");
    all_ok &= check_if_value_is_int("copmodes");
    all_ok &= check_if_value_is_int("maxworkers");
    all_ok &= check_if_value_is_int("maxcbservers");
    all_ok &= check_if_value_is_int("maxprefetchers");
    all_ok &= check_if_value_is_int("sftp_windowsize");
    all_ok &= check_if_value_is_int("sftp_sendahead");
    all_ok &= check_if_value_is_int("sftp_ackpoint");
    all_ok &= check_if_value_is_int("sftp_packetsize");
    all_ok &= check_if_value_is_int("rvmtype");
    all_ok &= check_if_value_is_int("rvm_log_size");
    all_ok &= check_if_value_is_int("rvm_data_size");
    all_ok &= check_if_value_is_int("rds_chunk_size");
    all_ok &= check_if_value_is_int("rds_list_size");
    all_ok &= check_if_value_is_int("log_optimization");
    all_ok &= check_if_value_is_int("swt");
    all_ok &= check_if_value_is_int("mwt");
    all_ok &= check_if_value_is_int("ssf");
    all_ok &= check_if_value_is_int("von");
    all_ok &= check_if_value_is_int("vmon");
    all_ok &= check_if_value_is_int("SearchForNOreFind");
    all_ok &= check_if_value_is_int("asr");
    all_ok &= check_if_value_is_int("vcb");
    all_ok &= check_if_value_is_int("nowalk");
    all_ok &= check_if_value_is_int("MarinerTcp");
    all_ok &= check_if_value_is_int("allow-reattach");
    all_ok &= check_if_value_is_int("nofork");

    return all_ok;
}

int VenusConf::check()
{
    if (get_int_value("cacheblocks") < MIN_CB) {
        eprint("Cannot start: minimum cache size is %s", "2MB");
        return EINVAL;
    }

    if (get_int_value("cachefiles") < MIN_CF) {
        eprint("Cannot start: minimum number of cache files is %d", MIN_CF);
        return EINVAL;
    }

    if (get_int_value("cml_entries") < MIN_MLE) {
        eprint("Cannot start: minimum number of cml entries is %d", MIN_MLE);
        return EINVAL;
    }

    if (get_int_value("hoard_entries") < MIN_HDBE) {
        eprint("Cannot start: minimum number of hoard entries is %d", MIN_HDBE);
        return EINVAL;
    }

    if (get_int_value("cachechunkblockbitsize") < 0) {
        /* Not a power of 2 FAIL!!! */
        eprint(
            "Cannot start: provided cache chunk block size is not a power of 2");
        return EINVAL;
    }

    if (get_int_value("cachechunkblockbitsize") < 12) {
        /* Smaller than minimum FAIL*/
        eprint("Cannot start: minimum cache chunk block size is 4KB");
        return EINVAL;
    }

    if (!check_all_int_values()) {
        return EINVAL;
    }

    return 0;
}

void VenusConf::set_int(const char *key, int64_t value)
{
    char buffer[max_int64_string_length];
    snprintf(buffer, max_int64_string_length, "%" PRId64, value);
    set(key, buffer);
}

int VenusConf::add_int(const char *key, int64_t value)
{
    char buffer[max_int64_string_length];
    snprintf(buffer, max_int64_string_length, "%" PRId64, value);
    return add(key, buffer);
}

void VenusConf::handle_relative_path(const char *key)
{
    const char *TmpChar  = get_value(key);
    const char *cachedir = get_value("cachedir");
    if (*TmpChar != '/') {
        char *tmp = (char *)malloc(strlen(cachedir) + strlen(TmpChar) + 2);
        CODA_ASSERT(tmp);
        sprintf(tmp, "%s/%s", cachedir, TmpChar);
        set(key, tmp);
        free(tmp);
    }
}

VenusConf::on_off_pair *VenusConf::find_on_off_pair(const char *key)
{
    dlist_iterator next(on_off_pairs_list);
    on_off_pair *curr_pair = NULL;

    while ((curr_pair = (on_off_pair *)next())) {
        if (curr_pair->is_in_pair(key))
            return curr_pair;
    }

    return NULL;
}

bool VenusConf::on_off_pair::is_in_pair(const char *val)
{
    if (strcmp(val, on_val) == 0)
        return true;
    if (strcmp(val, off_val) == 0)
        return true;
    return false;
}

VenusConf::on_off_pair::on_off_pair(const char *on, const char *off)
    : dlink()
{
    on_val  = strdup(on);
    off_val = strdup(off);
}

VenusConf::on_off_pair::~on_off_pair()
{
    free((void *)on_val);
    free((void *)off_val);
}

VenusConf &GetVenusConf()
{
    return global_conf;
}
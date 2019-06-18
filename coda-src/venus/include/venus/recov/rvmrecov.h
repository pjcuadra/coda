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
 *
 *    Specification of the Venus Recoverable Storage manager.
 *
 */

#ifndef _VENUS_RVM_RECOV_H_
#define _VENUS_RVM_RECOV_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

#include <venus/recov/recov.h>

class RvmRecov : public Recov {
    int InitMetaData                  = UNSET_IMD;
    int InitNewInstance               = UNSET_IMD;
    const char *VenusLogDevice        = NULL;
    unsigned long VenusLogDeviceSize  = UNSET_VLDS;
    const char *VenusDataDevice       = NULL;
    unsigned long VenusDataDeviceSize = UNSET_VDDS;
    unsigned int CacheFiles           = 0;
    unsigned int MLEs                 = 0;
    unsigned int HDBEs                = 0;

    void loadConfiguration();
    void initMemorySpace();
    void checkConfigurationParms();
    void initRVM();
    void initRDS();
    void loadRDS();
    void createNewInstance();

public:
    RvmRecov();
    ~RvmRecov();
    void *malloc(size_t size);
    void free(void *ptr);
    void recordRange(void *ptr, size_t size);
    void beginTrans(const char file[], int line);
    void endTrans(int);
    void setBound(int bound);
    void flush(int = 0); /* XXX - parameter is now redundant! */
    void truncate(int = 0); /* XXX - parameter is now redundant! */
    void print(int);
    void generateStoreId(ViceStoreId *sid);
    void getStatistics();
};

rvm_type_t GetRvmType();

#endif /* _VENUS_RVM_RECOV_H_ */

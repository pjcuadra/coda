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

#ifndef _VENUS_RPC2_H_
#define _VENUS_RPC2_H_ 1

/*
 *
 * Specification of the Venus Volume abstraction.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

class RPC2 {
    static RPC2 *default_prototype;
    static RPC2 *prototype;

public:
    static RPC2 *getInstance() { return prototype; }

    static void setImplementation(RPC2 *prototype)
    {
        RPC2::prototype = prototype;
    }

    static void setDefaultImplementation()
    {
        RPC2::prototype = default_prototype;
    }

    virtual long bind(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
                      RPC2_SubsysIdent *Subsys, RPC2_BindParms *Bparms,
                      RPC2_Handle *ConnHandle);

    virtual long
    createMgrp(RPC2_Handle *MgroupHandle, RPC2_McastIdent *MulticastHost,
               RPC2_PortIdent *MulticastPort, RPC2_SubsysIdent *Subsys,
               RPC2_Integer SecurityLevel, RPC2_EncryptionKey SessionKey,
               RPC2_Integer EncryptionType, long SideEffectType);

    virtual long unbind(RPC2_Handle whichConn);
};

#endif /* _VENUS_RPC2_H_ */
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
 *   Implementation of the Venus CallBack server.
 *
 *
 *    ToDo:
 *        1. Allow for authenticated callback connections, and use them for CallBackFetch.
 *        2. Create/manage multiple worker threads (at least one per user).
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include <venus/rpc2.h>

RPC2 *RPC2::default_prototype = new RPC2();
RPC2 *RPC2::prototype         = default_prototype;

long RPC2::bind(RPC2_HostIdent *Host, RPC2_PortIdent *Port,
                RPC2_SubsysIdent *Subsys, RPC2_BindParms *Bparms,
                RPC2_Handle *ConnHandle)
{
    return RPC2_NewBinding(Host, Port, Subsys, Bparms, ConnHandle);
}

long RPC2::createMgrp(RPC2_Handle *MgroupHandle, RPC2_McastIdent *MulticastHost,
                      RPC2_PortIdent *MulticastPort, RPC2_SubsysIdent *Subsys,
                      RPC2_Integer SecurityLevel, RPC2_EncryptionKey SessionKey,
                      RPC2_Integer EncryptionType, long SideEffectType)
{
    return RPC2_CreateMgrp(MgroupHandle, MulticastHost, MulticastPort, Subsys,
                           SecurityLevel, SessionKey, EncryptionType,
                           SideEffectType);
}

long RPC2::unbind(RPC2_Handle whichConn)
{
    return RPC2_Unbind(whichConn);
}
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

#include <vice.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include <venus/vicerpc2.h>

ViceRPC2 *ViceRPC2::default_prototype = new ViceRPC2();
ViceRPC2 *ViceRPC2::prototype         = default_prototype;

long ViceRPC2::getACL(RPC2_Handle cid, ViceFid *Fid, RPC2_Unsigned InconOK,
                      RPC2_BoundedBS *AccessList, ViceStatus *Status,
                      RPC2_Unsigned Unused, RPC2_CountedBS *PiggyCOP2)
{
    return ViceGetACL(cid, Fid, InconOK, AccessList, Status, Unused, PiggyCOP2);
}

long ViceRPC2::fetch(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV,
                     RPC2_Unsigned InconOK, ViceStatus *Status,
                     RPC2_Unsigned PrimaryHost, RPC2_Unsigned Offset,
                     RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD)
{
    return ViceFetch(cid, Fid, VV, InconOK, Status, PrimaryHost, Offset,
                     PiggyCOP2, BD);
}

long ViceRPC2::setACL(RPC2_Handle cid, ViceFid *Fid, RPC2_CountedBS *AccessList,
                      ViceStatus *Status, RPC2_Unsigned Unused,
                      ViceStoreId *StoreId, RPC2_CountedBS *OldVS,
                      RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
                      RPC2_CountedBS *PiggyCOP2)
{
    return ViceSetACL(cid, Fid, AccessList, Status, Unused, StoreId, OldVS,
                      NewVS, VCBStatus, PiggyCOP2);
}

long ViceRPC2::getRootVolume(RPC2_Handle cid, RPC2_BoundedBS *Volume)
{
    return ViceGetRootVolume(cid, Volume);
}

long ViceRPC2::getVolumeStatus(RPC2_Handle cid, VolumeId Vid,
                               VolumeStatus *Status, RPC2_BoundedBS *Name,
                               RPC2_BoundedBS *OfflineMsg, RPC2_BoundedBS *MOTD,
                               RPC2_Unsigned IsReplicated)
{
    return ViceGetVolumeStatus(cid, Vid, Status, Name, OfflineMsg, MOTD,
                               IsReplicated);
}

long ViceRPC2::setVolumeStatus(RPC2_Handle cid, VolumeId Vid,
                               VolumeStatus *Status, RPC2_BoundedBS *Name,
                               RPC2_BoundedBS *OfflineMsg, RPC2_BoundedBS *MOTD,
                               RPC2_Unsigned Unused, ViceStoreId *StoreId,
                               RPC2_CountedBS *PiggyCOP2)
{
    return ViceSetVolumeStatus(cid, Vid, Status, Name, OfflineMsg, MOTD, Unused,
                               StoreId, PiggyCOP2);
}

long ViceRPC2::disconnectFS(RPC2_Handle cid)
{
    return ViceDisconnectFS(cid);
}

long ViceRPC2::getTime(RPC2_Handle cid, RPC2_Unsigned *Seconds,
                       RPC2_Integer *USeconds)
{
    return ViceGetTime(cid, Seconds, USeconds);
}

long ViceRPC2::tokenExpired(RPC2_Handle cid)
{
    return TokenExpired(cid);
}

long ViceRPC2::getStatistics(RPC2_Handle cid, ViceStatistics *Statistics)
{
    return ViceGetStatistics(cid, Statistics);
}

long ViceRPC2::getVolumeInfo(RPC2_Handle cid, RPC2_String Vid, VolumeInfo *Info)
{
    return ViceGetVolumeInfo(cid, Vid, Info);
}

long ViceRPC2::getVolumeLocation(RPC2_Handle cid, VolumeId Vid,
                                 RPC2_BoundedBS *HostPort)
{
    return ViceGetVolumeLocation(cid, Vid, HostPort);
}

long ViceRPC2::COP2(RPC2_Handle cid, RPC2_CountedBS *COP2BS)
{
    return ViceCOP2(cid, COP2BS);
}

long ViceRPC2::resolve(RPC2_Handle cid, ViceFid *Fid)
{
    return ViceResolve(cid, Fid);
}

long ViceRPC2::repair(RPC2_Handle cid, ViceFid *Fid, ViceStatus *status,
                      ViceStoreId *StoreId, SE_Descriptor *BD)
{
    return ViceRepair(cid, Fid, status, StoreId, BD);
}

long ViceRPC2::setVV(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV,
                     RPC2_CountedBS *PiggyCOP2)
{
    return ViceSetVV(cid, Fid, VV, PiggyCOP2);
}

long ViceRPC2::allocFids(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                         ViceFidRange *Range)
{
    return ViceAllocFids(cid, Vid, Type, Range);
}

long ViceRPC2::allocFidsOld(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                            ViceFidRange *Range, RPC2_Unsigned AllocHost,
                            RPC2_CountedBS *PiggyCOP2)
{
    return OldViceAllocFids(cid, Vid, Type, Range, AllocHost, PiggyCOP2);
}

long ViceRPC2::connectFS(RPC2_Handle cid, RPC2_Unsigned ViceVersion,
                         ViceClient *ClientId)
{
    return ViceNewConnectFS(cid, ViceVersion, ClientId);
}

long ViceRPC2::getVolVS(RPC2_Handle cid, VolumeId Vid, RPC2_Integer *VS,
                        CallBackStatus *CBStatus)
{
    return ViceGetVolVS(cid, Vid, VS, CBStatus);
}

long ViceRPC2::validateVols(RPC2_Handle cid, RPC2_Unsigned Vids_size_,
                            ViceVolumeIdStruct Vids[], RPC2_CountedBS *VS,
                            RPC2_BoundedBS *VFlagBS)
{
    return ViceValidateVols(cid, Vids_size_, Vids, VS, VFlagBS);
}

long ViceRPC2::openReintHandle(RPC2_Handle cid, ViceFid *Fid,
                               ViceReintHandle *RHandle)
{
    return ViceOpenReintHandle(cid, Fid, RHandle);
}

long ViceRPC2::queryReintHandle(RPC2_Handle cid, VolumeId Vid,
                                ViceReintHandle *RHandle, RPC2_Unsigned *Length)
{
    return ViceQueryReintHandle(cid, Vid, RHandle, Length);
}

long ViceRPC2::sendReintFragment(RPC2_Handle cid, VolumeId Vid,
                                 ViceReintHandle *RHandle, RPC2_Unsigned Length,
                                 SE_Descriptor *BD)
{
    return ViceSendReintFragment(cid, Vid, RHandle, Length, BD);
}

long ViceRPC2::closeReintHandle(RPC2_Handle cid, VolumeId Vid,
                                RPC2_Integer LogSize, ViceReintHandle *RHandle,
                                RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
                                CallBackStatus *VCBStatus,
                                RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD)
{
    return ViceCloseReintHandle(cid, Vid, LogSize, RHandle, OldVS, NewVS,
                                VCBStatus, PiggyCOP2, BD);
}

long ViceRPC2::reintegrate(RPC2_Handle cid, VolumeId Vid, RPC2_Integer LogSize,
                           RPC2_Integer *Index, RPC2_Integer OutOfOrder,
                           RPC2_Unsigned StaleDirs_size_max_,
                           RPC2_Unsigned *StaleDirs_size_, ViceFid StaleDirs[],
                           RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
                           CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyCOP2,
                           SE_Descriptor *BD)
{
    return ViceReintegrate(cid, Vid, LogSize, Index, OutOfOrder,
                           StaleDirs_size_max_, StaleDirs_size_, StaleDirs,
                           OldVS, NewVS, VCBStatus, PiggyCOP2, BD);
}

long ViceRPC2::getAttrPlusSHA(RPC2_Handle cid, ViceFid *Fid,
                              RPC2_Unsigned InconOK, ViceStatus *Status,
                              RPC2_BoundedBS *SHAval, RPC2_Unsigned Unused,
                              RPC2_CountedBS *PiggyCOP2)
{
    return ViceGetAttrPlusSHA(cid, Fid, InconOK, Status, SHAval, Unused,
                              PiggyCOP2);
}

long ViceRPC2::validateAttrsPlusSHA(
    RPC2_Handle cid, RPC2_Unsigned Unused, ViceFid *PrimaryFid,
    ViceStatus *PrimaryStatus, RPC2_BoundedBS *SHAval,
    RPC2_Unsigned Piggies_size_, ViceFidAndVV Piggies[],
    RPC2_BoundedBS *VFlagBS, RPC2_CountedBS *PiggyCOP2)
{
    return ViceValidateAttrsPlusSHA(cid, Unused, PrimaryFid, PrimaryStatus,
                                    SHAval, Piggies_size_, Piggies, VFlagBS,
                                    PiggyCOP2);
}

long ViceRPC2::fetchPartial(RPC2_Handle cid, ViceFid *Fid,
                            ViceVersionVector *VV, RPC2_Unsigned InconOK,
                            ViceStatus *Status, RPC2_Unsigned PrimaryHost,
                            RPC2_Unsigned Offset, RPC2_Unsigned Count,
                            RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD)
{
    return ViceFetchPartial(cid, Fid, VV, InconOK, Status, PrimaryHost, Offset,
                            Count, PiggyCOP2, BD);
}

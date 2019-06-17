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

#ifndef _VENUS_VICE_H_
#define _VENUS_VICE_H_ 1

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

class ViceRPC2 {
    static ViceRPC2 *default_prototype;
    static ViceRPC2 *prototype;

public:
    static ViceRPC2 *getInstance() { return prototype; }

    static void setImplementation(ViceRPC2 *prototype)
    {
        ViceRPC2::prototype = prototype;
    }

    static void setDefaultImplementation()
    {
        ViceRPC2::prototype = default_prototype;
    }

    virtual long getACL(RPC2_Handle cid, ViceFid *Fid, RPC2_Unsigned InconOK,
                        RPC2_BoundedBS *AccessList, ViceStatus *Status,
                        RPC2_Unsigned Unused, RPC2_CountedBS *PiggyCOP2);

    virtual long fetch(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV,
                       RPC2_Unsigned InconOK, ViceStatus *Status,
                       RPC2_Unsigned PrimaryHost, RPC2_Unsigned Offset,
                       RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD);

    virtual long setACL(RPC2_Handle cid, ViceFid *Fid,
                        RPC2_CountedBS *AccessList, ViceStatus *Status,
                        RPC2_Unsigned Unused, ViceStoreId *StoreId,
                        RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
                        CallBackStatus *VCBStatus, RPC2_CountedBS *PiggyCOP2);

    virtual long getRootVolume(RPC2_Handle cid, RPC2_BoundedBS *Volume);

    virtual long getVolumeStatus(RPC2_Handle cid, VolumeId Vid,
                                 VolumeStatus *Status, RPC2_BoundedBS *Name,
                                 RPC2_BoundedBS *OfflineMsg,
                                 RPC2_BoundedBS *MOTD,
                                 RPC2_Unsigned IsReplicated);

    virtual long setVolumeStatus(RPC2_Handle cid, VolumeId Vid,
                                 VolumeStatus *Status, RPC2_BoundedBS *Name,
                                 RPC2_BoundedBS *OfflineMsg,
                                 RPC2_BoundedBS *MOTD, RPC2_Unsigned Unused,
                                 ViceStoreId *StoreId,
                                 RPC2_CountedBS *PiggyCOP2);

    virtual long connectFS(RPC2_Handle cid, RPC2_Unsigned ViceVersion,
                           ViceClient *ClientId);

    virtual long disconnectFS(RPC2_Handle cid);

    virtual long getTime(RPC2_Handle cid, RPC2_Unsigned *Seconds,
                         RPC2_Integer *USeconds);

    virtual long tokenExpired(RPC2_Handle cid);

    virtual long getStatistics(RPC2_Handle cid, ViceStatistics *Statistics);

    virtual long getVolumeInfo(RPC2_Handle cid, RPC2_String Vid,
                               VolumeInfo *Info);

    virtual long getVolumeLocation(RPC2_Handle cid, VolumeId Vid,
                                   RPC2_BoundedBS *HostPort);

    virtual long COP2(RPC2_Handle cid, RPC2_CountedBS *COP2BS);

    virtual long resolve(RPC2_Handle cid, ViceFid *Fid);

    virtual long repair(RPC2_Handle cid, ViceFid *Fid, ViceStatus *status,
                        ViceStoreId *StoreId, SE_Descriptor *BD);

    virtual long setVV(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV,
                       RPC2_CountedBS *PiggyCOP2);

    virtual long allocFids(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                           ViceFidRange *Range);

    virtual long allocFidsOld(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                              ViceFidRange *Range, RPC2_Unsigned AllocHost,
                              RPC2_CountedBS *PiggyCOP2);

    virtual long getVolVS(RPC2_Handle cid, VolumeId Vid, RPC2_Integer *VS,
                          CallBackStatus *CBStatus);

    virtual long validateVols(RPC2_Handle cid, RPC2_Unsigned Vids_size_,
                              ViceVolumeIdStruct Vids[], RPC2_CountedBS *VS,
                              RPC2_BoundedBS *VFlagBS);

    virtual long openReintHandle(RPC2_Handle cid, ViceFid *Fid,
                                 ViceReintHandle *RHandle);

    virtual long queryReintHandle(RPC2_Handle cid, VolumeId Vid,
                                  ViceReintHandle *RHandle,
                                  RPC2_Unsigned *Length);

    virtual long sendReintFragment(RPC2_Handle cid, VolumeId Vid,
                                   ViceReintHandle *RHandle,
                                   RPC2_Unsigned Length, SE_Descriptor *BD);

    virtual long closeReintHandle(RPC2_Handle cid, VolumeId Vid,
                                  RPC2_Integer LogSize,
                                  ViceReintHandle *RHandle,
                                  RPC2_CountedBS *OldVS, RPC2_Integer *NewVS,
                                  CallBackStatus *VCBStatus,
                                  RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD);

    virtual long reintegrate(RPC2_Handle cid, VolumeId Vid,
                             RPC2_Integer LogSize, RPC2_Integer *Index,
                             RPC2_Integer OutOfOrder,
                             RPC2_Unsigned StaleDirs_size_max_,
                             RPC2_Unsigned *StaleDirs_size_,
                             ViceFid StaleDirs[], RPC2_CountedBS *OldVS,
                             RPC2_Integer *NewVS, CallBackStatus *VCBStatus,
                             RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD);

    virtual long getAttrPlusSHA(RPC2_Handle cid, ViceFid *Fid,
                                RPC2_Unsigned InconOK, ViceStatus *Status,
                                RPC2_BoundedBS *SHAval, RPC2_Unsigned Unused,
                                RPC2_CountedBS *PiggyCOP2);

    virtual long
    validateAttrsPlusSHA(RPC2_Handle cid, RPC2_Unsigned Unused,
                         ViceFid *PrimaryFid, ViceStatus *PrimaryStatus,
                         RPC2_BoundedBS *SHAval, RPC2_Unsigned Piggies_size_,
                         ViceFidAndVV Piggies[], RPC2_BoundedBS *VFlagBS,
                         RPC2_CountedBS *PiggyCOP2);

    virtual long fetchPartial(RPC2_Handle cid, ViceFid *Fid,
                              ViceVersionVector *VV, RPC2_Unsigned InconOK,
                              ViceStatus *Status, RPC2_Unsigned PrimaryHost,
                              RPC2_Unsigned Offset, RPC2_Unsigned Count,
                              RPC2_CountedBS *PiggyCOP2, SE_Descriptor *BD);
};

#endif /* _VENUS_VICE_H_ */
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

/************************************************************************/
/*									*/
/*  codaproc.c	- File Server Coda specific routines			*/
/*									*/
/*  Function	-							*/
/*									*/
/*									*/
/*									*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <struct.h>
#include <inodeops.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <util.h>
#include <rvmlib.h>

#include <prs.h>
#include <al.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include <vmindex.h>
#include <srv.h>
#include <volume.h>
#include <lockqueue.h>
#include <vldb.h>
#include <vrdb.h>
#include <vlist.h>
#include <callback.h>
#include "codaproc.h"
#include <codadir.h>
#include <nettohost.h>
#include <operations.h>
#include <timing.h>

#define EMPTYDIRBLOCKS 2

class TreeRmBlk;
class rmblk;

/* *****  Exported variables  ***** */

ViceVersionVector NullVV = { { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0 }, 0 };

/* globals */
int OngoingRepairs = 0;

/* external routines */
extern int CmpPlus(AL_AccessEntry *a, AL_AccessEntry *b);

extern int CmpMinus(AL_AccessEntry *a, AL_AccessEntry *b);

extern int CheckWriteMode(ClientEntry *, Vnode *);

extern int FetchFileByFD(RPC2_Handle, int fd, ClientEntry *);

int GetSubTree(ViceFid *, Volume *, dlist *);

int PerformTreeRemoval(struct DirEntry *, void *);

/* ***** Private routines  ***** */
void UpdateVVs(ViceVersionVector *, ViceVersionVector *, ViceVersionVector *);

static int CheckTreeRemoveSemantics(ClientEntry *, Volume *, ViceFid *,
                                    dlist *);

static int getFids(struct DirEntry *dirent, void *data);

const int MaxFidAlloc = 32;

/*
  ViceAllocFids: Allocated a range of fids
*/
long FS_ViceAllocFids(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                      ViceFidRange *Range)
{
    long errorCode      = 0;
    VolumeId VSGVolnum  = Vid;
    Volume *volptr      = 0;
    ClientEntry *client = 0;
    int stride, ix;
    char *rock;

    START_TIMING(AllocFids_Total);
    SLog(1, "ViceAllocFids: Vid = %x, Type = %d, Count = %d", Vid, Type,
         Range->Count);

    /* Validate parameters. */
    {
        /* Retrieve the client handle. */
        if ((errorCode = RPC2_GetPrivatePointer(cid, &rock)) != RPC2_SUCCESS)
            goto FreeLocks;
        client = (ClientEntry *)rock;
        if (!client) {
            errorCode = EINVAL;
            goto FreeLocks;
        }

        /* Translate the GroupVol into this host's RWVol. */
        /* This host's {stride, ix} are returned as side effects; they will be needed in the Alloc below. */
        if (!XlateVid(&Vid, &stride, &ix)) {
            SLog(0, "ViceAllocFids: XlateVid (%x) failed", VSGVolnum);
            errorCode = EINVAL;
            goto FreeLocks;
        }

        switch (Type) {
        case File:
        case Directory:
        case SymbolicLink:
            break;

        case Invalid:
        default:
            errorCode = EINVAL;
            goto FreeLocks;
        }
    }

    /* Get the volume. */
    if ((errorCode = GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0))) {
        SLog(0, "ViceAllocFids: GetVolObj error %s",
             ViceErrorMsg((int)errorCode));
        goto FreeLocks;
    }

    /* Allocate a contiguous range of fids. */
    if (Range->Count > MaxFidAlloc)
        Range->Count = MaxFidAlloc;

    /* Always allocating fids using a stride of VSG_MEMBERS. This way we
     * can potentially grow or shrink the replication group without
     * affecting previously issued fids which are being used by
     * disconnected clients --JH */
    if ((errorCode = VAllocFid(volptr, Type, Range, stride, ix))) {
        SLog(0, "ViceAllocFids: VAllocVnodes error %s",
             ViceErrorMsg((int)errorCode));
        goto FreeLocks;
    }

FreeLocks:
    PutVolObj(&volptr, VOL_NO_LOCK);

    SLog(2, "ViceAllocFids returns %s (count = %d)",
         ViceErrorMsg((int)errorCode), Range->Count);
    END_TIMING(AllocFids_Total);
    return (errorCode);
}

long FS_OldViceAllocFids(RPC2_Handle cid, VolumeId Vid, ViceDataType Type,
                         ViceFidRange *Range, RPC2_Unsigned AllocHost,
                         RPC2_CountedBS *PiggyBS)
{
    long errorCode = 0;

    SLog(1, "OldViceAllocFids: Vid = %x, Type = %d, Count = %d, AllocHost = %x",
         Vid, Type, Range->Count, AllocHost);

    /* Only AllocHost actually allocates the fids. */
    if (ThisHostAddr == AllocHost) {
        errorCode = FS_ViceAllocFids(cid, Vid, Type, Range);
    } else {
        Range->Count = 0;
    }

FreeLocks:
    SLog(2, "OldViceAllocFids returns %s (count = %d)",
         ViceErrorMsg((int)errorCode), Range->Count);
    return (errorCode);
}

/*
  ViceCOP2: Update the VV that was modified during the first phase (COP1)
*/
long FS_ViceCOP2(RPC2_Handle cid, RPC2_CountedBS *PiggyBS)
{
    return RPC2_INVALIDOPCODE;
}

/*
  ViceResolve: Resolve the diverging replicas of an object
*/
long FS_ViceResolve(RPC2_Handle cid, ViceFid *Fid)
{
    return RPC2_INVALIDOPCODE;
}

/*
  BEGIN_HTML
  <a name="ViceSetVV"><strong>Sets the version vector for an object</strong></a>
  END_HTML
*/
// THIS CODE IS NEEDED TO DO A CLEARINC FROM THE REPAIR TOOL FOR QUOTA REPAIRS
long FS_ViceSetVV(RPC2_Handle cid, ViceFid *Fid, ViceVersionVector *VV,
                  RPC2_CountedBS *PiggyBS)
{
    Vnode *vptr            = 0;
    Volume *volptr         = 0;
    ClientEntry *client    = 0;
    long errorCode         = 0;
    rvm_return_t camstatus = RVM_SUCCESS;
    char *rock;

    SLog(9, "Entering ViceSetVV(%s", FID_(Fid));

    /* translate replicated fid to rw fid */
    XlateVid(&Fid->Volume); /* dont bother checking for errors */
    if (RPC2_GetPrivatePointer(cid, &rock) != RPC2_SUCCESS)
        return (EINVAL);
    client = (ClientEntry *)rock;

    if (!client)
        return EINVAL;

    /* if volume is being repaired check if repairer is same as client */
    if (V_VolLock(volptr).IPAddress) {
        if (V_VolLock(volptr).IPAddress != client->VenusId->host.s_addr) {
            SLog(0, "ViceSetVV: Volume Repairer != Locker");
            errorCode = EINVAL;
            goto FreeLocks;
        }
    }
    memcpy(&(Vnode_vv(vptr)), VV, sizeof(ViceVersionVector));

FreeLocks:
    rvmlib_begin_transaction(restore);
    int fileCode = 0;
    if (vptr) {
        VPutVnode((Error *)&fileCode, vptr);
        CODA_ASSERT(fileCode == 0);
    }
    if (volptr)
        PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(camstatus));
    if (camstatus) {
        SLog(0, "ViceSetVV: Error during transaction");
        return (camstatus);
    }
    SLog(9, "ViceSetVV finished with errorcode %d", errorCode);
    return (errorCode);
}

/*
  ViceRepair: Manually resolve the object
*/
long FS_ViceRepair(RPC2_Handle cid, ViceFid *Fid, ViceStatus *status,
                   ViceStoreId *StoreId, SE_Descriptor *BD)
{
    return RPC2_INVALIDOPCODE;
}

/* Set positive rights for user "name"; zero means delete */
int SetRights(Vnode *vptr, char *name, int rights)
{
    int Id;
    AL_AccessList *aCL = 0;
    // int aCLSize        = 0;

    SLog(9, "Entering SetRights(%s %d)", name, rights);
    if (AL_NameToId(name, &Id) < 0) {
        SLog(0, "SetRights: couldnt get id for %s ", name);
        return -1;
    }
    /* set the ACL */
    aCL = VVnodeACL(vptr);
    // aCLSize = VAclSize(vptr);

    /* find the entry */
    for (int i = 0; i < aCL->PlusEntriesInUse; i++) {
        if (aCL->ActualEntries[i].Id == Id) {
            if (rights)
                aCL->ActualEntries[i].Rights = rights;
            else {
                /* remove this entry since rights are zero */
                for (int j = i; j < (aCL->PlusEntriesInUse - 1); j++)
                    memcpy(&(aCL->ActualEntries[j]),
                           &(aCL->ActualEntries[j + 1]),
                           sizeof(AL_AccessEntry));
                aCL->PlusEntriesInUse--;
                aCL->TotalNoOfEntries--;
                aCL->MySize -= (int)sizeof(AL_AccessEntry);
            }
            return (0);
        }
    }

    /* didnt find the entry - create one */
    if (aCL->PlusEntriesInUse + aCL->MinusEntriesInUse ==
        aCL->TotalNoOfEntries) {
        /* allocate some more entries */
        for (int i = aCL->TotalNoOfEntries - 1;
             i > (aCL->TotalNoOfEntries - aCL->MinusEntriesInUse - 1); i--)
            memcpy(&(aCL->ActualEntries[i + 1]), &(aCL->ActualEntries[i]),
                   sizeof(AL_AccessEntry));
        aCL->TotalNoOfEntries++;
        aCL->MySize += (int)sizeof(AL_AccessEntry);
    }

    aCL->ActualEntries[aCL->PlusEntriesInUse].Id     = Id;
    aCL->ActualEntries[aCL->PlusEntriesInUse].Rights = rights;
    aCL->PlusEntriesInUse++;

    /* sort the entries */
    qsort((char *)&(aCL->ActualEntries[0]), aCL->PlusEntriesInUse,
          sizeof(AL_AccessEntry), (int (*)(const void *, const void *))CmpPlus);
    printf("The accessList after setting rights is \n");
    AL_PrintAlist(aCL);
    return (0);
}

/* set negative rights for user "name"; zero means delete */
int SetNRights(Vnode *vptr, char *name, int rights)
{
    int Id;
    AL_AccessList *aCL = 0;
    // int aCLSize        = 0;
    int p, m, t;

    SLog(9, "Entering SetNRights(%s %d)", name, rights);
    if (AL_NameToId(name, &Id) < 0) {
        SLog(0, "SetRights: couldnt get id for %s ", name);
        return -1;
    }
    /* set the ACL */
    aCL = VVnodeACL(vptr);
    // aCLSize = VAclSize(vptr);

    p = aCL->PlusEntriesInUse;
    m = aCL->MinusEntriesInUse;
    t = aCL->TotalNoOfEntries;

    /* find the entry */
    for (int i = t - 1; i >= t - m; i--) {
        if (aCL->ActualEntries[i].Id == Id) {
            if (rights)
                aCL->ActualEntries[i].Rights = rights;
            else {
                /* remove this entry since rights are zero */
                for (int j = i; j > t - m; j--)
                    memcpy(&(aCL->ActualEntries[j]),
                           &(aCL->ActualEntries[j - 1]),
                           sizeof(AL_AccessEntry));
                aCL->MinusEntriesInUse--;
                aCL->TotalNoOfEntries--;
                aCL->MySize -= (int)sizeof(AL_AccessEntry);
            }
            return (0);
        }
    }
    /* didnt find the entry - create one */
    if ((m + p) == t) {
        /* all entries are taken - create one */
        for (int i = t - 1; i > t - m - 1; i--)
            memcpy(&(aCL->ActualEntries[i + 1]), &(aCL->ActualEntries[i]),
                   sizeof(AL_AccessEntry));
        t = ++aCL->TotalNoOfEntries;
        aCL->MySize += (int)sizeof(AL_AccessEntry);
    }
    aCL->ActualEntries[t - m - 1].Id     = Id;
    aCL->ActualEntries[t - m - 1].Rights = rights;
    aCL->MinusEntriesInUse++;

    /* sort the entry */
    qsort((char *)&(aCL->ActualEntries[t - m - 1]), aCL->MinusEntriesInUse,
          sizeof(AL_AccessEntry),
          (int (*)(const void *, const void *))CmpMinus);
    printf("The accessList after setting rights is \n");
    AL_PrintAlist(aCL);
    return 0;
}

/*
 * data structure used to pass arguments
 * for the recursive tree removal routines
 */
#include "treeremove.h"

/*
    Get all the fids in a subtree - deadlock free solution
    add the fids to the vlist
*/
int GetSubTree(ViceFid *fid, Volume *volptr, dlist *vlist)
{
    Vnode *vptr = 0;
    vle *v;
    dlist *tmplist = new dlist((CFN)VLECmp);
    int errorCode  = 0;

    CODA_ASSERT(volptr != 0);

    /* get root vnode */
    {
        if ((errorCode =
                 GetFsObj(fid, &volptr, &vptr, READ_LOCK, NO_LOCK, 1, 0, 1)))
            goto Exit;

        CODA_ASSERT(vptr->disk.type == vDirectory);
    }

    /* obtain fids of immediate children */
    {
        PDirHandle dh;
        dh = VN_SetDirHandle(vptr);

        if (!DH_IsEmpty(dh))
            DH_EnumerateDir(dh, (int (*)(PDirEntry, void *))getFids,
                            (void *)tmplist);
        VN_PutDirHandle(vptr);
    }

    /* put root's vnode */
    {
        Error error = 0;
        VPutVnode(&error, vptr);
        CODA_ASSERT(error == 0);
        vptr = 0;
    }

    /* put fids of sub-subtrees in list */
    {
        vle *v;
        while ((v = (vle *)tmplist->get())) {
            ViceFid cFid = v->fid;
            delete v;
            cFid.Volume = fid->Volume;
            if (!ISDIR(cFid))
                AddVLE(*vlist, &cFid);
            else {
                errorCode = GetSubTree(&cFid, volptr, vlist);
                if (errorCode)
                    goto Exit;
            }
        }
    }

    /* add object's fid into list */
    v             = AddVLE(*vlist, fid);
    v->d_inodemod = 1;
Exit : {
    vle *v;
    while ((v = (vle *)tmplist->get()))
        delete v;
    delete tmplist;
}
    if (vptr) {
        Error error = 0;
        VPutVnode(&error, vptr);
        CODA_ASSERT(error = 0);
    }
    return (errorCode);
}
static int getFids(struct DirEntry *de, void *data)
{
    dlist *flist = (dlist *)data;
    char *name   = de->name;
    VnodeId vnode;
    Unique_t unique;
    ViceFid fid;

    SLog(9, "Entering GetFid for %s", name);

    FID_NFid2Int(&de->fid, &vnode, &unique);
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return 0;

    fid.Volume = 0;
    fid.Vnode  = vnode;
    fid.Unique = unique;
    AddVLE(*flist, &fid);

    SLog(9, "Leaving GetFid for %s ", name);
    return (0);
}

class rmblk {
public:
    dlist *vlist;
    Volume *volptr;
    ClientEntry *client;
    int result;

    rmblk(dlist *vl, Volume *vp, ClientEntry *cl)
    {
        vlist  = vl;
        volptr = vp;
        client = cl;
        result = 0;
    }
};

static int RecursiveCheckRemoveSemantics(PDirEntry de, void *data)
{
    rmblk *rb = (rmblk *)data;
    VnodeId vnode;
    Unique_t unique;
    char *name = de->name;
    FID_NFid2Int(&de->fid, &vnode, &unique);

    if (rb->result)
        return (rb->result);
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return (0);

    int errorCode = 0;
    ViceFid fid;
    vle *pv = 0;
    vle *ov = 0;
    /* form the fid */
    {
        fid.Volume = V_id(rb->volptr);
        fid.Vnode  = vnode;
        fid.Unique = unique;
    }

    /* get the object and its parent */
    {
        ov = FindVLE(*(rb->vlist), &fid);
        CODA_ASSERT(ov != NULL);

        ViceFid pfid;
        pfid.Volume = fid.Volume;
        pfid.Vnode  = ov->vptr->disk.vparent;
        pfid.Unique = ov->vptr->disk.uparent;

        pv = FindVLE(*(rb->vlist), &pfid);
        CODA_ASSERT(pv != NULL);
    }
    /* Check Semantics for the object's removal */
    {
        if (ov->vptr->disk.type == vFile || ov->vptr->disk.type == vSymlink) {
            errorCode = CheckRemoveSemantics(rb->client, &(pv->vptr),
                                             &(ov->vptr), name, &(rb->volptr),
                                             0, NULL, NULL, NULL, NULL, NULL);
            if (errorCode) {
                rb->result = errorCode;
                return (errorCode);
            }
            return (0);
        } else {
            /* child is a directory */
            errorCode = CheckRmdirSemantics(rb->client, &(pv->vptr),
                                            &(ov->vptr), name, &(rb->volptr), 0,
                                            NULL, NULL, NULL, NULL, NULL);
            if (errorCode) {
                rb->result = errorCode;
                return (errorCode);
            }

            PDirHandle td;
            td = VN_SetDirHandle(ov->vptr);
            if (!DH_IsEmpty(td))
                DH_EnumerateDir(td, RecursiveCheckRemoveSemantics, (void *)rb);
            VN_PutDirHandle(ov->vptr);
        }
    }

    return (rb->result);
}

/*
  CheckTreeRemoveSemantics: Check the semantic constraints for
  removing a subtree
*/
static int CheckTreeRemoveSemantics(ClientEntry *client, Volume *volptr,
                                    ViceFid *tFid, dlist *vlist)
{
    vle *tv = 0;

    /* get the root's vnode */
    {
        tv = FindVLE(*vlist, tFid);
        CODA_ASSERT(tv != 0);
    }

    /* recursively check semantics */
    {
        PDirHandle td;
        td = VN_SetDirHandle(tv->vptr);
        rmblk enumparm(vlist, volptr, client);
        if (!DH_IsEmpty(td))
            DH_EnumerateDir(td, RecursiveCheckRemoveSemantics,
                            (void *)&enumparm);
        VN_PutDirHandle(tv->vptr);
        return (enumparm.result);
    }
}

/*
 *  PerformTreeRemove: Perform the actions for removing a subtree
 */

int PerformTreeRemoval(PDirEntry de, void *data)
{
    TreeRmBlk *pkdparm = (TreeRmBlk *)data;
    VnodeId vnode;
    Unique_t unique;
    char *name = de->name;
    ViceFid cFid;
    ViceFid pFid;
    vle *cv, *pv;

    FID_NFid2Int(&de->fid, &vnode, &unique);

    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return (0);
    /* get vnode of object */
    {
        cFid.Volume = V_id(pkdparm->volptr);
        cFid.Vnode  = vnode;
        cFid.Unique = unique;

        cv = FindVLE(*(pkdparm->vlist), &cFid);
        CODA_ASSERT(cv != 0);
    }
    /* get vnode of parent */
    {
        pFid.Volume = cFid.Volume;
        pFid.Vnode  = cv->vptr->disk.vparent;
        pFid.Unique = cv->vptr->disk.uparent;

        pv = FindVLE(*(pkdparm->vlist), &pFid);
        CODA_ASSERT(pv != 0);
    }

    /* delete children first */
    {
        if (cv->vptr->disk.type == vDirectory) {
            PDirHandle cDir;
            cDir = VN_SetDirHandle(cv->vptr);
            if (!DH_IsEmpty(cDir)) {
                DH_EnumerateDir(cDir, PerformTreeRemoval, (void *)pkdparm);
                CODA_ASSERT(DC_Dirty(cv->vptr->dh));
            }
            VN_PutDirHandle(cv->vptr);
        }
    }

    /* delete the object */
    {
        int nblocks = 0;
        if (cv->vptr->disk.type == vDirectory) {
            PerformRmdir(pkdparm->client, pkdparm->VSGVnum, pkdparm->volptr,
                         pv->vptr, cv->vptr, name,
                         pkdparm->status ? pkdparm->status->Date :
                                           pv->vptr->disk.unixModifyTime,
                         0, pkdparm->storeid, &pv->d_cinode, &nblocks);
            *(pkdparm->blocks) += nblocks;
            CODA_ASSERT(cv->vptr->delete_me);
            nblocks = (int)-nBlocks(cv->vptr->disk.length);
            CODA_ASSERT(AdjustDiskUsage(pkdparm->volptr, nblocks) == 0);
            *(pkdparm->blocks) += nblocks;

        } else {
            PerformRemove(pkdparm->client, pkdparm->VSGVnum, pkdparm->volptr,
                          pv->vptr, cv->vptr, name,
                          pkdparm->status ? pkdparm->status->Date :
                                            pv->vptr->disk.unixModifyTime,
                          0, pkdparm->storeid, &pv->d_cinode, &nblocks);
            *(pkdparm->blocks) += nblocks;
            if (cv->vptr->delete_me) {
                nblocks = (int)-nBlocks(cv->vptr->disk.length);
                CODA_ASSERT(AdjustDiskUsage(pkdparm->volptr, nblocks) == 0);
                *(pkdparm->blocks) += nblocks;
                cv->f_sinode = cv->vptr->disk.node.inodeNumber;
                cv->vptr->disk.node.inodeNumber = 0;
            }
        }
    }
    return 0;
}

/*
  NewCOP1Update: Increment the version number and update the
  storeid of an object.

  Only the version number of this replica is incremented.  The
  other replicas's version numbers are incremented by COP2Update
*/
void NewCOP1Update(Volume *volptr, Vnode *vptr, ViceStoreId *StoreId,
                   RPC2_Integer *vsptr, bool isReplicated)
{
    int ix     = 0;
    vrent *vre = NULL;

    SLog(2, "COP1Update: Fid = (%x.%x.%x), StoreId = (%x.%x)", V_id(volptr),
         vptr->vnodeNumber, vptr->disk.uniquifier, StoreId->HostId,
         StoreId->Uniquifier);

    /* If a volume version stamp was sent in, and if it matches, update it. */
    if (vsptr) {
        SLog(2, "COP1Update: client VS %d", *vsptr);
        if (*vsptr == (&(V_versionvector(volptr).Versions.Site0))[ix])
            (*vsptr)++;
        else
            *vsptr = 0;
    }

    /* Fashion an UpdateSet using just ThisHost. */
    ViceVersionVector UpdateSet       = NullVV;
    (&(UpdateSet.Versions.Site0))[ix] = 1;

    /* Install the new StoreId in the Vnode. */
    Vnode_vv(vptr).StoreId = *StoreId;

    /* Update the Volume and Vnode VVs. */
    UpdateVVs(&V_versionvector(volptr), &Vnode_vv(vptr), &UpdateSet);
}

void UpdateVVs(ViceVersionVector *VVV, ViceVersionVector *VV,
               ViceVersionVector *US)
{
    if (SrvDebugLevel >= 2) {
        SLog(2, "\tVVV = [%d %d %d %d %d %d %d %d]", VVV->Versions.Site0,
             VVV->Versions.Site1, VVV->Versions.Site2, VVV->Versions.Site3,
             VVV->Versions.Site4, VVV->Versions.Site5, VVV->Versions.Site6,
             VVV->Versions.Site7);
        SLog(2, "\tVV = [%d %d %d %d %d %d %d %d]", VV->Versions.Site0,
             VV->Versions.Site1, VV->Versions.Site2, VV->Versions.Site3,
             VV->Versions.Site4, VV->Versions.Site5, VV->Versions.Site6,
             VV->Versions.Site7);
        SLog(2, "\tUS = [%d %d %d %d %d %d %d %d]", US->Versions.Site0,
             US->Versions.Site1, US->Versions.Site2, US->Versions.Site3,
             US->Versions.Site4, US->Versions.Site5, US->Versions.Site6,
             US->Versions.Site7);
    }

    AddVVs(VVV, US);
    AddVVs(VV, US);
}

void PollAndYield()
{
    if (!pollandyield)
        return;

    /* Do a polling select first to make threads with pending I/O runnable. */
    (void)IOMGR_Poll();

    SLog(100, "Thread Yielding");
    int lwprc = LWP_DispatchProcess();
    if (lwprc != LWP_SUCCESS)
        SLog(0, "PollAndYield: LWP_DispatchProcess failed (%d)", lwprc);
    SLog(100, "Thread Yield Returning");
}

/*
  BEGIN_HTML
  <a name="ViceGetVolVS"><strong>Return the volume version vector for the specified
  volume, and establish a volume callback on it</strong></a>
  END_HTML
*/
long FS_ViceGetVolVS(RPC2_Handle cid, VolumeId Vid, RPC2_Integer *VS,
                     CallBackStatus *CBStatus)
{
    long errorCode = 0;
    Volume *volptr;
    VolumeId rwVid;
    ViceFid fid;
    ClientEntry *client = 0;
    int ix, count;
    char *rock;

    SLog(1, "ViceGetVolVS for volume 0x%x", Vid);

    errorCode = RPC2_GetPrivatePointer(cid, &rock);
    client    = (ClientEntry *)rock;
    if (!client || errorCode) {
        SLog(0, "No client structure built by ViceConnectFS");
        return (errorCode);
    }

    /* now get the version stamp for Vid */
    rwVid = Vid;
    if (!XlateVid(&rwVid, &count, &ix)) {
        SLog(1, "GetVolVV: Couldn't translate VSG %u", Vid);
        errorCode = EINVAL;
        goto Exit;
    }

    SLog(9, "GetVolVS: Going to get volume %u pointer", rwVid);
    volptr = VGetVolume((Error *)&errorCode, rwVid);
    SLog(1, "GetVolVS: Got volume %u: error = %d", rwVid, errorCode);
    if (errorCode) {
        SLog(0, "ViceGetVolVV, VgetVolume error %s",
             ViceErrorMsg((int)errorCode));
        /* should we check to see if we must do a putvolume here */
        goto Exit;
    }

    *VS = (&(V_versionvector(volptr).Versions.Site0))[ix];
    VPutVolume(volptr);

    /*
     * add a volume callback. don't need to use CodaAddCallBack
     * because we always send in the VSG volume id.
     */
    *CBStatus = NoCallBack;

    fid.Volume = Vid;
    fid.Vnode = fid.Unique = 0;
    if (AddCallBack(client->VenusId, &fid))
        *CBStatus = CallBackSet;

Exit:
    SLog(2, "ViceGetVolVS returns %s\n", ViceErrorMsg((int)errorCode));

    return (errorCode);
}

void GetMyVS(Volume *volptr, RPC2_CountedBS *VSList, RPC2_Integer *MyVS,
             int voltype)
{
    vrent *vre;
    int ix = 0;

    *MyVS = 0;
    if (VSList->SeqLen == 0)
        return;

    switch (voltype) {
    case REPVOL:
        /* Look up the VRDB entry. */
        vre = VRDB.find(V_groupId(volptr));
        if (!vre)
            Die("GetMyVS: VSG not found!");

        /* Look up the index of this host. */
        ix = vre->index();
        if (ix < 0)
            Die("GetMyVS: this host not found!");
        break;
    case NONREPVOL:
    case RWVOL:
        break;
    default:
        return;
    }

    /* get the version stamp from our slot in the vector */
    *MyVS = ((RPC2_Unsigned *)VSList->SeqBody)[ix];

    SLog(1, "GetMyVS: 0x%x, incoming stamp %d", V_id(volptr), *MyVS);

    return;
}

void SetVSStatus(ClientEntry *client, Volume *volptr, RPC2_Integer *NewVS,
                 CallBackStatus *VCBStatus, int voltype)
{
    *VCBStatus = NoCallBack;
    *NewVS     = 0;

    int ix = 0;
    switch (voltype) {
    case REPVOL: {
        /* Look up the VRDB entry. */
        vrent *vre = VRDB.find(V_groupId(volptr));
        if (!vre)
            Die("SetVSStatus: VSG not found!");

        /* Look up the index of this host. */
        ix = vre->index();
        if (ix < 0)
            Die("SetVSStatus: this host not found!");
    } break;
    case NONREPVOL:
    case RWVOL:
        break;
    default:
        return;
    }

    SLog(1, "SetVSStatus: 0x%x, client %d, server %d", V_id(volptr), *NewVS,
         (&(V_versionvector(volptr).Versions.Site0))[ix]);

    /* check the version stamp in our slot in the vector */
    if (*NewVS == (&(V_versionvector(volptr).Versions.Site0))[ix]) {
        /*
	 * add a volume callback. don't need to use CodaAddCallBack because
	 * we always send in the VSG volume id.
	 */
        ViceFid fid;
        fid.Volume = V_id(volptr);
        fid.Vnode = fid.Unique = 0;
        *VCBStatus             = AddCallBack(client->VenusId, &fid);
    } else {
        *NewVS = 0;
    }

    SLog(1, "SetVSStatus: 0x%x, NewVS %d, CBstatus %d", V_id(volptr), *NewVS,
         *VCBStatus);
    return;
}

/*
  BEGIN_HTML
  <a name="ViceValidateVols"><strong>Takes a list of volumes and
  corresponding version stamps from a client, and returns a vector indicating
  for each volume whether or the version stamp supplied is current, and whether or not
  a callback was established for it.</strong></a>
  END_HTML
*/
long FS_ViceValidateVols(RPC2_Handle cid, RPC2_Unsigned numVids,
                         ViceVolumeIdStruct Vids[], RPC2_CountedBS *VSBS,
                         RPC2_BoundedBS *VFlagBS)
{
    long errorCode      = 0;
    ClientEntry *client = 0;
    char *rock;

    SLog(1, "ViceValidateVols, (%d volumes)", numVids);

    VFlagBS->SeqLen = 0;
    memset(VFlagBS->SeqBody, 0, VFlagBS->MaxSeqLen);

    if (VFlagBS->MaxSeqLen < (RPC2_Unsigned)numVids) {
        SLog(0,
             "Wrong output buffer while validating volumes: "
             "MaxSeqLen %d, should be %d",
             VFlagBS->MaxSeqLen, numVids);
        return (EINVAL);
    }

    errorCode = RPC2_GetPrivatePointer(cid, &rock);
    client    = (ClientEntry *)rock;
    if (!client || errorCode) {
        SLog(0, "No client structure built by ViceConnectFS");
        return (errorCode);
    }

    /* check the piggybacked volumes */

    for (unsigned int i = 0; i < numVids; i++) {
        int error, index, ix, count;
        Volume *volptr;
        VolumeId rwVid;
        RPC2_Integer myVS;

        rwVid = Vids[i].Vid;
        if (!XlateVid(&rwVid, &count, &ix)) {
            SLog(1, "ValidateVolumes: Couldn't translate VSG %x", Vids[i].Vid);
            goto InvalidVolume;
        }

        SLog(9, "ValidateVolumes: Going to get volume %x pointer", rwVid);
        volptr = VGetVolume((Error *)&error, rwVid);
        SLog(1, "ValidateVolumes: Got volume %x: error = %d ", rwVid, error);
        if (error) {
            SLog(0, "ViceValidateVolumes, VgetVolume error %s",
                 ViceErrorMsg((int)error));
            /* should we check to see if we must do a putvolume here */
            goto InvalidVolume;
        }

        myVS = (&(V_versionvector(volptr).Versions.Site0))[ix];
        VPutVolume(volptr);

        /* check the version stamp in our slot in the vector */
        index = i * count + ix;
        if (VSBS->SeqLen < ((index + 1) * sizeof(RPC2_Unsigned))) {
            SLog(9, "ValidateVolumes: short input");
            goto InvalidVolume;
        }

        if ((long)ntohl(((RPC2_Unsigned *)VSBS->SeqBody)[index]) == myVS) {
            SLog(8, "ValidateVolumes: 0x%x ok, adding callback", Vids[i].Vid);
            /*
	     * add a volume callback. don't need to use CodaAddCallBack because
	     * we always send in the VSG volume id.
	     */
            ViceFid fid;
            fid.Volume = Vids[i].Vid;
            fid.Vnode = fid.Unique = 0;
            VFlagBS->SeqBody[i]    = AddCallBack(client->VenusId, &fid);

            continue;
        }

    InvalidVolume:
        SLog(0, "ValidateVolumes: 0x%x failed!", Vids[i].Vid);
        VFlagBS->SeqBody[i] = 255;
    }
    VFlagBS->SeqLen = numVids;

    SLog(2, "ValidateVolumes returns %s\n", ViceErrorMsg((int)errorCode));

    return (errorCode);
}

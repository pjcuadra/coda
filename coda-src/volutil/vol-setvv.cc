/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


/* vol-setvv.c */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <inodeops.h>
#include <util.h>
#include <rvmlib.h>

#include <util.h>
#include <vice.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <vrdb.h>
#include <partition.h>
#include <viceinode.h>
#include <vutil.h>
#include <srv.h>



/*
  S_VolSetVV: Set the version vector for a given object
*/
long S_VolSetVV(RPC2_Handle rpcid, RPC2_Unsigned formal_volid, 
		RPC2_Unsigned vnodeid, RPC2_Unsigned unique,  
		ViceVersionVector *vv)
{

    Volume *vp;
    Vnode *vnp;
    Error error;
    int status;	    // transaction status variable
    long rc = 0;
    ProgramType *pt;
    ViceFid fid; 
    int ix;
    vrent *vre;
    /* To keep C++ 2.0 happy */
    VolumeId volid = (VolumeId)formal_volid;

    VLog(9, "Checking lwp rock in S_VolSetVV");
    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    VLog(9, "Entering VolSetVV(%d, %u, %u)", rpcid, volid, vnodeid);
    VolumeId tmpvolid = volid;
    if (!XlateVid(&tmpvolid)){
	VLog(0, "S_VolSetVV Couldn't translate VSG ");
	tmpvolid = volid;
    }

    RVMLIB_BEGIN_TRANSACTION(restore)
    VInitVolUtil(volumeUtility);
    /*    vp = VAttachVolume(&error, volid, V_READONLY); */
    /* Ignoring the volume lock for now - assume this will 
       be used in bad situations only*/
    vp = VGetVolume(&error, tmpvolid);
    if (error) {
	VLog(0, "S_VolSetVV: failure attaching volume %d", tmpvolid);
	if (error != VNOVOL) {
	    VPutVolume(vp);
	}
        rvmlib_abort(error);
	goto exit;
    }
    /* VGetVnode moved from after VOffline to here 11/88 ***/
    vnp = VGetVnode(&error, vp, vnodeid, unique, WRITE_LOCK, 1);
    if (error && error != EIO) {
	VLog(0, "S_VolSetVV: VGetVnode failed with %d", error);
	VPutVolume(vp);
	rvmlib_abort(VFAIL);
	goto exit;
    }

    if (error && error == EIO) {
	/* barren object - debarrenize it - setvv is overloaded here */
	vnp = VGetVnode(&error, vp, vnodeid, unique, WRITE_LOCK, 1, 1);
	CODA_ASSERT(IsBarren(vnp->disk.versionvector));
	
	VLog(0, SrvDebugLevel, stdout, "%x.%x.%x is barren - Debarrenizing it", 
		V_id(vp), vnp->vnodeNumber, vnp->disk.uniquifier);
	VLog(0, SrvDebugLevel, stdout, "Object will be inconsistent and input vector is ignored");

	/* clear the barren flag - make sure object will be marked 
	   inconsistent; create a new inode so salvager will not complain */
	ClearBarren(vnp->disk.versionvector);
	SetIncon(vnp->disk.versionvector);

	/* Clear the cloned flag since we're changing the inodeNumber. */
	vnp->disk.cloned = 0;
	
	vnp->disk.dataVersion++;
	vnp->disk.inodeNumber = icreate((int)V_device(vp), 0, (int)V_id(vp), 
					(int)vnp->vnodeNumber,
					(int)vnp->disk.uniquifier, 
					(int)vnp->disk.dataVersion);
    } else 
	bcopy((const void *)vv, (void *)&(Vnode_vv(vnp)), sizeof(ViceVersionVector));

    /* update volume version vector,  break callbacks */
    vre = VRDB.find(V_groupId(vp));    /* Look up the VRDB entry. */
    if (!vre) Die("S_VolSetVV: VSG not found!");

    ix = vre->index(ThisHostAddr);	    /* Look up the index of this host. */
    if (ix < 0) Die("S_VolSetVV: this host not found!");

    /* Fashion an UpdateSet using just ThisHost. */
    ViceVersionVector UpdateSet = NullVV; 
    (&(UpdateSet.Versions.Site0))[ix] = 1;
    AddVVs(&V_versionvector(vp), &UpdateSet);

    fid.Volume = formal_volid; fid.Vnode = vnodeid; fid.Unique = unique;
    CodaBreakCallBack(0, &fid, formal_volid);

    VPutVnode((Error *)&error, vnp);
    if (error){
	VLog(0, "S_VolSetVV: VPutVnode failed with %d", error);
	VPutVolume(vp);
	rvmlib_abort(VFAIL);
	goto exit;
    }

    VPutVolume(vp);
    RVMLIB_END_TRANSACTION(flush, &(status));

 exit:

    VDisconnectFS();
    if (status)
	VLog(0, "S_VolSetVV failed with %d", status);
    return (status?status:rc);
}


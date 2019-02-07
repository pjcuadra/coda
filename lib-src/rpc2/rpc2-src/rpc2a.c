/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/multi.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/secure.h>

#include "cbuf.h"
#include "rpc2.private.h"
#include "trace.h"

/*
Contains the hard core of the major RPC runtime routines.

Protocol
========
    Client sends request to server.
    Retries from client provoke BUSY until server finishes servicing request.
    Server sends reply to client and holds the reply packet for a fixed amount
    of time after that. Server LWP is released immediately.
    Retries from client cause saved reply packet to be sent out.
    Reply packet is released on next request or on a timeout.  Further client
    retries are ignored.


Connection Invariants:
=====================
        1. State:
                          Modified in GetRequest, SendResponse,
                          MakeRPC, MultiRPC, Bind and SocketListener.
                          Always C_THINK in client code.  In
                          S_AWAITREQUEST when not servicing a request,
                          in server code.  In S_PROCESS between a
                          GetRequest and a SendResponse in server
                          code.  Other values used only during bind
                          sequence.  Set to S_HARDERROR or C_HARDERROR
                          on a permanent error.

        2. NextSeqNumber: Initialized by connection creation code in
                        GetRequest.  ALWAYS updated by SocketListener,
                        except in the RPC2_MultiRPC case.  Updated in
                        RPC2_MultiRPC() if SocketListener return code
                        is WAITING.  On client side, in state C_THINK,
                        this value is the next outgoing request's
                        sequence number.  On server side, in state
                        S_AWAITREQUEST, this value is the next
                        incoming request's sequence number.

NOTE 1
======

    The code works with the LWP preemption package.  All user-callable
    RPC2 routines are in critical sections.  The independent LWPs such
    as SocketListener, side-effect LWPs, etc. are totally
    non-preemptible, since they do not do a PRE_PreemptMe() call.  The
    only lower-level RPC routines that have to be explicitly bracketed
    by critical sections are the randomize and encryption routines
    which are useful independent of RPC2.
*/

#define HAVE_SE_FUNC(xxx) (ce->SEProcs && ce->SEProcs->xxx)

size_t RPC2_Preferred_Keysize;
int RPC2_secure_only;

void SavePacketForRetry();
static int InvokeSE();
static void SendOKInit2();
static int ServerHandShake(struct CEntry *ce, int32_t xrand,
                           RPC2_EncryptionKey SharedSecret,
                           uint32_t rpc2sec_version, size_t keysize,
                           int new_binding);
static RPC2_PacketBuffer *Send2Get3(struct CEntry *ce, RPC2_EncryptionKey key,
                                    int32_t xrand, int32_t *yrand,
                                    uint32_t rpc2sec_version, size_t keysize,
                                    int new_binding);
static long Test3(RPC2_PacketBuffer *pb, struct CEntry *ce, int32_t yrand,
                  RPC2_EncryptionKey ekey, int new_binding);
static void Send4AndSave(struct CEntry *ce, int32_t xrand,
                         RPC2_EncryptionKey ekey, int new_binding);
static void RejectBind(struct CEntry *ce, size_t bodysize, RPC2_Integer opcode);
static RPC2_PacketBuffer *HeldReq(RPC2_RequestFilter *filter,
                                  struct CEntry **ce);
static int GetFilter(RPC2_RequestFilter *inf, RPC2_RequestFilter *outf);
static long GetNewRequest(IN RPC2_RequestFilter *filter,
                          IN struct timeval *timeout,
                          OUT struct RPC2_PacketBuffer **pb,
                          OUT struct CEntry **ce);
static long MakeFake(INOUT RPC2_PacketBuffer *pb, IN struct CEntry *ce,
                     RPC2_Integer *AuthenticationType, OUT RPC2_Integer *xrand,
                     OUT RPC2_CountedBS *cident, OUT uint32_t *rpc2sec_version,
                     OUT size_t *keysize);

FILE *rpc2_logfile;
FILE *rpc2_tracefile;
extern struct timeval SaveResponse;

void RPC2_SetLog(FILE *file, int level)
{
    if (file) {
        rpc2_logfile   = file;
        rpc2_tracefile = file;
    }
    RPC2_DebugLevel = level;
}

static void rpc2_StampPacket(struct CEntry *ce, struct RPC2_PacketBuffer *pb)
{
    unsigned int delta;

    assert(ce->RequestTime);

    delta                = TSDELTA(rpc2_MakeTimeStamp(), ce->RequestTime);
    pb->Header.TimeStamp = (unsigned int)ce->TimeStampEcho + delta;

    say(15, RPC2_DebugLevel, "TSin %u delta %u TSout %u\n", ce->TimeStampEcho,
        delta, pb->Header.TimeStamp);
}

long RPC2_SendResponse(IN RPC2_Handle ConnHandle, IN RPC2_PacketBuffer *Reply)
{
    RPC2_PacketBuffer *preply, *pretry;
    struct CEntry *ce;
    long rc;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_SendResponse()\n");
    assert(!Reply || Reply->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* Perform sanity checks */
    ce = rpc2_GetConn(ConnHandle);
    if (!ce)
        rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ce, SERVER, S_PROCESS))
        rpc2_Quit(RPC2_NOTWORKER);

    /* set connection state */
    SetState(ce, S_AWAITREQUEST);
    if (ce->Mgrp)
        SetState(ce->Mgrp, S_AWAITREQUEST);

    /* return if we have no reply to send */
    if (!Reply)
        rpc2_Quit(RPC2_FAIL);

    TR_SENDRESPONSE();

    { /* Cancel possibly pending delayed ack response, the error code is
       * ignored, but RPC2_ABANDONED just looked nice :) */
        if (ce->MySl) {
            rpc2_DeactivateSle(ce->MySl, RPC2_ABANDONED);
            rpc2_FreeSle(&ce->MySl);
        }
    }

    preply = Reply; /* side effect routine usually does not reallocate
                     * packet. preply will be the packet actually sent
                     * over the wire */

    rc = preply->Header.ReturnCode; /* InitPacket clobbers it */
    rpc2_InitPacket(preply, ce, preply->Header.BodyLength);
    /* convert system to on the wire error value */
    preply->Header.ReturnCode = RPC2_S2RError(rc);
    preply->Header.Opcode     = RPC2_REPLY;
    preply->Header.SeqNumber  = ce->NextSeqNumber - 1;
    /* SocketListener has already updated NextSeqNumber */

    rc = RPC2_SUCCESS; /* tentative, for sendresponse */
    /* Notify side effect routine, if any */
    if (HAVE_SE_FUNC(SE_SendResponse))
        rc = (*ce->SEProcs->SE_SendResponse)(ConnHandle, &preply);

    /* Allocate retry packet before encrypting Bodylength */
    RPC2_AllocBuffer(preply->Header.BodyLength, &pretry);

    if (ce->TimeStampEcho) /* service time is now-requesttime */
        rpc2_StampPacket(ce, preply);

    /* Sanitize packet */
    rpc2_htonp(preply);
    rpc2_ApplyE(preply, ce);

    /* Send reply */
    say(9, RPC2_DebugLevel, "Sending reply\n");
    rpc2_XmitPacket(preply, ce->HostInfo->Addr, 1);

    /* Save reply for retransmission */
    memcpy(&pretry->Header, &preply->Header, preply->Prefix.LengthOfPacket);
    pretry->Prefix.LengthOfPacket = preply->Prefix.LengthOfPacket;
    pretry->Prefix.sa             = preply->Prefix.sa;
    SavePacketForRetry(pretry, ce);

    if (preply != Reply)
        RPC2_FreeBuffer(&preply); /* allocated by SE routine */
    rpc2_Quit(rc);
}

/* Helpers to set up keys and pack/unpack init2/init3 packets */
static int setup_init1_key(int (*init)(uint32_t secure_version,
                                       struct security_association *sa,
                                       const struct secure_auth *auth,
                                       const struct secure_encr *encr,
                                       const uint8_t *key, size_t len),
                           struct security_association *sa, uint32_t xrandom,
                           uint32_t unique, RPC2_EncryptionKey secret)
{
    const struct secure_auth *auth;
    const struct secure_encr *encr;
    uint32_t salt[2];
    uint8_t key[48]; /* 256-bits for encryption + 128-bits for authentication */
    int rc;

    auth = secure_get_auth_byid(SECURE_AUTH_AES_XCBC_96);
    encr = secure_get_encr_byid(SECURE_ENCR_AES_CBC);
    if (!auth || !encr)
        return -1;

    salt[0] = xrandom;
    salt[1] = htonl(unique);

    rc = secure_pbkdf(secret, sizeof(RPC2_EncryptionKey), (uint8_t *)salt,
                      sizeof(salt), SECURE_PBKDF_ITERATIONS, key, sizeof(key));
    if (rc)
        return -1;

    rc = init(0, sa, auth, encr, key, sizeof(key));
    memset(key, 0, sizeof(key));
    return rc;
}

static int pack_initX_body(struct security_association *sa,
                           const struct secure_auth *auth,
                           const struct secure_encr *encr,
                           uint32_t rpc2sec_version, void *packet_body,
                           size_t len)
{
    uint32_t *body = (uint32_t *)packet_body;

    if (!auth || !encr)
        return -1;

    body[0] = htonl(rpc2sec_version); /* version */
    body[1] = htonl(auth->id);
    body[2] = htonl(encr->id);
    secure_random_bytes(&body[3], len);

    return secure_setup_decrypt(rpc2sec_version, sa, auth, encr,
                                (uint8_t *)&body[3], len);
}

static int unpack_initX_body(struct CEntry *ce, struct RPC2_PacketBuffer *pb,
                             const struct secure_auth **auth,
                             const struct secure_encr **encr,
                             uint32_t *rpc2sec_version, size_t *keylen)
{
    const struct secure_auth *a;
    const struct secure_encr *e;
    uint32_t *body = (uint32_t *)pb->Body;
    uint32_t version;
    size_t len, min_keylen;
    int rc;

    /* Make sure the nonce in the decrypted packet is correct */
    if (pb->Header.Uniquefier != ce->PeerUnique)
        return RPC2_NOTAUTHENTICATED;

    /* check for sufficient length of the incoming packet */
    len = pb->Prefix.LengthOfPacket - sizeof(struct RPC2_PacketHeader);
    if (len < 3 * sizeof(uint32_t))
        return RPC2_NOTAUTHENTICATED;

    /* lookup authentication and encryption functions */
    version = ntohl(body[0]);

    /* Our peer should have switched to the lowest common version. If it hasn't
     * that is likely a good indication that something is very wrong. */
    if (version > SECURE_VERSION)
        return RPC2_NOTAUTHENTICATED;

    a = secure_get_auth_byid(ntohl(body[1]));
    e = secure_get_encr_byid(ntohl(body[2]));
    if (!a || !e)
        return RPC2_NOTAUTHENTICATED;

    /* check for sufficient keying material in the incoming packet */
    len -= 3 * sizeof(uint32_t);
    min_keylen = e->min_keysize;
    if (min_keylen < a->keysize)
        min_keylen = a->keysize;

    if (len < min_keylen)
        return RPC2_NOTAUTHENTICATED;

    /* initialize keys */
    rc = secure_setup_encrypt(version, &ce->sa, a, e, (uint8_t *)&body[3], len);
    if (rc)
        return RPC2_NOTAUTHENTICATED;

    if (rpc2sec_version)
        *rpc2sec_version = version;
    if (auth)
        *auth = a;
    if (encr)
        *encr = e;
    if (keylen)
        *keylen = len;
    return RPC2_SUCCESS;
}

long RPC2_GetRequest(IN RPC2_RequestFilter *Filter, OUT RPC2_Handle *ConnHandle,
                     OUT RPC2_PacketBuffer **Request,
                     IN struct timeval *BreathOfLife,
                     IN RPC2_GetKeys_func *GetKeys, IN long EncryptionTypeMask,
                     IN RPC2_AuthFail_func *AuthFail)
{
    struct CEntry *ce = NULL;
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *pb;
    RPC2_Integer AuthenticationType;
    RPC2_CountedBS cident, *clientIdent;
    RPC2_Integer XRandom;
    RPC2_EncryptionKey SharedSecret;
    uint32_t rpc2sec_version = 0;
    size_t keysize           = 0;
    long rc;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_GetRequest()\n");

    TR_GETREQUEST();

/* worthless request */
#define DROPIT()                        \
    do {                                \
        rpc2_SetConnError(ce);          \
        RPC2_FreeBuffer(Request);       \
        (void)RPC2_Unbind(*ConnHandle); \
        goto ScanWorkList;              \
    } while (0)

    if (!GetFilter(Filter, &myfilter))
        rpc2_Quit(RPC2_BADFILTER);

ScanWorkList:
    pb = HeldReq(&myfilter, &ce);
    if (!pb) {
        /* await a proper request */
        rc = GetNewRequest(&myfilter, BreathOfLife, &pb, &ce);
        if (rc != RPC2_SUCCESS)
            rpc2_Quit(rc);
    }

    if (!TestState(ce, SERVER, S_STARTBIND))
        SetState(ce, S_PROCESS);
    /* Invariants here:
         (1) pb points to a request packet, decrypted and nettohosted
         (2) ce is the connection associated with pb
         (3) ce's state is S_STARTBIND if this is a new bind,
         S_PROCESS otherwise
    */

    *Request    = pb;
    *ConnHandle = ce->UniqueCID;

    if (!TestState(ce, SERVER, S_STARTBIND)) { /* not a bind request */
        say(9, RPC2_DebugLevel, "Request on existing connection\n");

        rc = RPC2_SUCCESS;

        /* Notify side effect routine, if any */
        if (HAVE_SE_FUNC(SE_GetRequest)) {
            rc = (*ce->SEProcs->SE_GetRequest)(*ConnHandle, *Request);
            if (rc != RPC2_SUCCESS) {
                RPC2_FreeBuffer(Request);
                if (rc < RPC2_FLIMIT)
                    rpc2_SetConnError(ce);
            }
        }
        rpc2_Quit(rc);
    }

    /* Bind packet on a brand new connection */

    /* If we don't want to fall back on the old handshake, drop the packet */
    if (RPC2_secure_only && !(pb->Header.Flags & RPC2SEC_CAPABLE)) {
        rc = RPC2_NOTAUTHENTICATED;
        DROPIT();
    }

    /* Extract relevant fields from Init1 packet and then make it a fake
     * NEWCONNECTION packet */
    rc = MakeFake(pb, ce, &XRandom, &AuthenticationType, &cident,
                  &rpc2sec_version, &keysize);
    if (rc < RPC2_WLIMIT) {
        DROPIT();
    }

    memset(SharedSecret, 0, sizeof(RPC2_EncryptionKey));
    clientIdent = (ce->SecurityLevel != RPC2_OPENKIMONO) ? &cident : NULL;

    /* Abort if we cannot get keys for client */
    if (GetKeys && GetKeys(&AuthenticationType, clientIdent, SharedSecret,
                           ce->SessionKey) != 0) {
        RejectBind(ce, sizeof(struct Init2Body), RPC2_INIT2);

        if (AuthFail) { /* Client could be iterating through keys; log this */
            RPC2_HostIdent Host;
            RPC2_PortIdent Port;
            rpc2_splitaddrinfo(&Host, &Port, ce->HostInfo->Addr);
            (*AuthFail)(AuthenticationType, &cident, ce->EncryptionType, &Host,
                        &Port);
            if (Host.Tag == RPC2_HOSTBYADDRINFO)
                RPC2_freeaddrinfo(Host.Value.AddrInfo);
        }
        rc = RPC2_NOTAUTHENTICATED;
        DROPIT();
    }

    /* new bind sequence */
    if (pb->Header.Flags & RPC2SEC_CAPABLE) {
        /* setup xmit key so we can reply with encrypted key material */
        rc = setup_init1_key(secure_setup_encrypt, &ce->sa, XRandom,
                             ce->PeerUnique, SharedSecret);
        if (rc) {
            RejectBind(ce, sizeof(struct Init2Body), RPC2_INIT2);
            rc = RPC2_FAIL;
            DROPIT();
        }

        /* use the lowest common denominator */
        if (rpc2sec_version > SECURE_VERSION)
            rpc2sec_version = SECURE_VERSION;

        /* why reimplement something that is already working... */
        /* we just need some minor tweaks for the existing INIT2/INIT3/INIT4
         * sequence, but this is should not be a problem, the packets in the
         * new handshake are easily recognized by having a non-NULL
         * pb->Prefix.sa field */
        rc = ServerHandShake(ce, XRandom, SharedSecret, rpc2sec_version,
                             keysize, 1);
        if (rc != RPC2_SUCCESS)
            DROPIT();
    }

    /* old bind sequence, authenticated connections, 4-way handshake */
    else if (ce->SecurityLevel != RPC2_OPENKIMONO) {
        say(-1, RPC2_DebugLevel,
            "Server doesn't support RPC2SEC, using XOR based handshake\n");

        if (!GetKeys || (ce->EncryptionType & EncryptionTypeMask) == 0 ||
            MORETHANONEBITSET(ce->EncryptionType)) {
            RejectBind(ce, sizeof(struct Init2Body), RPC2_INIT2);
            rc = RPC2_NOTAUTHENTICATED;
            DROPIT();
        }

        /* It is probably safer to ignore the SessionKey returned by GetKeys. */
        secure_random_bytes(ce->SessionKey, sizeof(RPC2_EncryptionKey));

        rc = ServerHandShake(ce, XRandom, SharedSecret, 0, 0, 0);
        if (rc != RPC2_SUCCESS)
            DROPIT();
    }

    /* old bind sequence, unauthenticated connections, 2-way handshake */
    else {
        say(-1, RPC2_DebugLevel,
            "Server doesn't support RPC2SEC, establishing "
            "unauthenticated connection\n");
        SendOKInit2(ce);
    }

    /* Do final processing: we need is RPC2_Enable() */
    SetState(ce, S_AWAITENABLE);

    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_NewConnection)) {
        rc = (*ce->SEProcs->SE_NewConnection)(*ConnHandle, &cident);
        if (rc < RPC2_FLIMIT) {
            DROPIT();
        }
    }

    /* And now we are really done */
    if (ce->Flags & CE_OLDV) {
        char addr[RPC2_ADDRSTRLEN];
        RPC2_formataddrinfo(ce->HostInfo->Addr, addr, RPC2_ADDRSTRLEN);
        say(-1, RPC2_DebugLevel, "Request from %s, Old rpc2 version\n", addr);

        /* Get rid of allocated connection entry. */
        DROPIT();
    } else
        rpc2_Quit(RPC2_SUCCESS);

#undef DROPIT
}

long RPC2_MakeRPC(RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request,
                  SE_Descriptor *SDesc, RPC2_PacketBuffer **Reply,
                  struct timeval *BreathOfLife, long EnqueueRequest)
/* 'RPC2_PacketBuffer *Request' Gets clobbered during call: BEWARE */
{
    struct CEntry *ce;
    struct SL_Entry *sl;
    RPC2_PacketBuffer *preply = NULL;
    RPC2_PacketBuffer *preq;
    long rc, secode = RPC2_SUCCESS, finalrc, opcode;

    ProfileEnableSet(false);

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_MakeRPC()\n");

    TR_MAKERPC();

    /* Perform sanity checks */
    assert(Request->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* Zero out reply pointer */
    *Reply = NULL;

    /* verify and set connection state, with possible enqueueing;
       cannot factor out the verification, since other LWPs may grab
       ConnHandle in race after wakeup */
    while (TRUE) {
        ce = rpc2_GetConn(ConnHandle);
        if (!ce)
            rpc2_Quit(RPC2_NOCONNECTION);
        if (TestState(ce, CLIENT, C_HARDERROR))
            rpc2_Quit(RPC2_FAIL);
        if (TestState(ce, CLIENT, C_THINK))
            break;
        if (SDesc && ce->sebroken)
            rpc2_Quit(RPC2_SEFAIL2);

        if (!EnqueueRequest)
            rpc2_Quit(RPC2_CONNBUSY);
        say(1, RPC2_DebugLevel, "Enqueuing on connection %#x\n", ConnHandle);
        LWP_WaitProcess((char *)ce);
        say(1, RPC2_DebugLevel, "Dequeueing on connection %#x\n", ConnHandle);
    }
    /* XXXXXX race condition with preemptive threads */
    SetState(ce, C_AWAITREPLY);

    preq = Request;
    /* side effect routine usually does not reallocate packet */
    /* preq will be the packet actually sent over the wire */

    /* Complete  header fields and sanitize */
    opcode = preq->Header.Opcode; /* InitPacket clobbers it */
    rpc2_InitPacket(preq, ce, preq->Header.BodyLength);
    preq->Header.SeqNumber = ce->NextSeqNumber;
    preq->Header.Opcode    = opcode;
    preq->Header.BindTime  = 0;

    /* Notify side effect routine, if any */
    if (SDesc && HAVE_SE_FUNC(SE_MakeRPC1))
        if ((secode = (*ce->SEProcs->SE_MakeRPC1)(ConnHandle, SDesc, &preq)) !=
            RPC2_SUCCESS) {
            if (secode > RPC2_FLIMIT) {
                rpc2_Quit(RPC2_SEFAIL1);
            }
            rpc2_SetConnError(ce); /* does signal on ConnHandle also */
            rpc2_Quit(RPC2_SEFAIL2);
        }

    rpc2_htonp(preq);
    rpc2_ApplyE(preq, ce);

    /* send packet and await reply*/

    say(9, RPC2_DebugLevel, "Sending request on  %#x\n", ConnHandle);
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rc = rpc2_SendReliably(ce, sl, preq, BreathOfLife);

    switch ((int)rc) {
    case RPC2_SUCCESS:
        break;

    case RPC2_TIMEOUT: /* don't destroy the connection */
        say(9, RPC2_DebugLevel, "rpc2_SendReliably()--> %s on %#x\n",
            RPC2_ErrorMsg(rc), ConnHandle);
        rpc2_FreeSle(&sl);
        /* release packet allocated by SE routine */
        if (preq != Request)
            RPC2_FreeBuffer(&preq);
        goto SendReliablyError;

    default:
        assert(FALSE);
    }

    switch (sl->ReturnCode) {
    case ARRIVED:
        say(9, RPC2_DebugLevel, "Request reliably sent on %#x\n", ConnHandle);
        *Reply = preply = (RPC2_PacketBuffer *)sl->data;
        rpc2_FreeSle(&sl);
        /* release packet allocated by SE routine */
        if (preq != Request)
            RPC2_FreeBuffer(&preq);
        rc = RPC2_SUCCESS;
        break;

    case TIMEOUT:
        say(9, RPC2_DebugLevel, "Request failed on %#x\n", ConnHandle);
        rpc2_FreeSle(&sl);
        rpc2_SetConnError(ce); /* does signal on ConnHandle also */
        /* release packet allocated by SE routine */
        if (preq != Request)
            RPC2_FreeBuffer(&preq);
        rc = RPC2_DEAD;
        break;

    case NAKED:
        say(9, RPC2_DebugLevel, "Request NAK'ed on %#x\n", ConnHandle);
        rpc2_FreeSle(&sl);
        rpc2_SetConnError(ce); /* does signal on ConnHandle also */
        /* release packet allocated by SE routine */
        if (preq != Request)
            RPC2_FreeBuffer(&preq);
        rc = RPC2_NAKED;
        break;

    default:
        assert(FALSE);
    }

    /* At this point, if rc == RPC2_SUCCESS, the final reply has been received.
	SocketListener has already decrypted it */

    /* Notify side effect routine, if any.  It may modify the received packet. */
SendReliablyError:
    if (SDesc && HAVE_SE_FUNC(SE_MakeRPC2)) {
        secode = (*ce->SEProcs->SE_MakeRPC2)(
            ConnHandle, SDesc, (rc == RPC2_SUCCESS) ? preply : NULL);

        if (secode < RPC2_FLIMIT) {
            ce->sebroken = TRUE;
            finalrc      = secode;
            goto QuitMRPC;
        }
    }

    if (rc == RPC2_SUCCESS) {
        if (SDesc != NULL &&
            (secode != RPC2_SUCCESS || SDesc->LocalStatus == SE_FAILURE ||
             SDesc->RemoteStatus == SE_FAILURE)) {
            finalrc = RPC2_SEFAIL1;
        } else
            finalrc = RPC2_SUCCESS;
    } else
        finalrc = rc;

QuitMRPC: /* finalrc has been correctly set by now */

    /* wake up any enqueue'd threads */
    LWP_NoYieldSignal((char *)ce);

    ProfileEnableSet(true);

    rpc2_Quit(finalrc);
}

long RPC2_NewBinding(IN RPC2_HostIdent *Host, IN RPC2_PortIdent *Port,
                     IN RPC2_SubsysIdent *Subsys, IN RPC2_BindParms *Bparms,
                     IN RPC2_Handle *ConnHandle)
{
    ProfileEnableSet(false);
    struct CEntry *ce; /* equal to *ConnHandle after allocation */
    RPC2_PacketBuffer *pb;
    long i;
    struct Init1Body *ib;
    struct Init2Body *ib2;
    struct Init3Body *ib3;
    struct Init4Body *ib4;
    struct SL_Entry *sl;
    long rc, init2rc, init4rc;
    RPC2_Integer xrandom = 0, yrandom = 0;
    RPC2_Unsigned bsize;
    struct RPC2_addrinfo *addr, *peeraddrs;
    RPC2_EncryptionKey rpc2key;
    size_t bodylen;

    /* the following will be set when the received INIT2 packet was encrypted */
    int new_binding                = 0;
    const struct secure_auth *auth = NULL;
    const struct secure_encr *encr = NULL;
    uint32_t rpc2sec_version       = 0;
    size_t keylen                  = 0;

#define DROPCONN()                      \
    {                                   \
        rpc2_SetConnError(ce);          \
        (void)RPC2_Unbind(*ConnHandle); \
        *ConnHandle = 0;                \
    }

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "In RPC2_NewBinding()\n");

    TR_BIND();

    switch ((int)Bparms->SecurityLevel) {
    case RPC2_OPENKIMONO:
        Bparms->EncryptionType = 0;
        break;

    case RPC2_AUTHONLY:
    case RPC2_HEADERSONLY:
    case RPC2_SECURE:
        /* unknown encryption type */
        if ((Bparms->EncryptionType & RPC2_ENCRYPTIONTYPES) == 0)
            rpc2_Quit(RPC2_FAIL);
        /* tell me just one */
        if (MORETHANONEBITSET(Bparms->EncryptionType))
            rpc2_Quit(RPC2_FAIL);
        break;

    default:
        rpc2_Quit(RPC2_FAIL); /* bogus security level */
    }

    /* Step 0: Resolve bind parameters */
    peeraddrs = rpc2_resolve(Host, Port);
    if (!peeraddrs)
        rpc2_Quit(RPC2_NOBINDING);

    say(9, RPC2_DebugLevel, "Bind parameters successfully resolved\n");

try_next_addr:
    addr          = peeraddrs;
    peeraddrs     = addr->ai_next;
    addr->ai_next = NULL;

    /* Step 1: Obtain and initialize a new connection */
    ce          = rpc2_AllocConn(addr);
    *ConnHandle = ce->UniqueCID;
    say(9, RPC2_DebugLevel, "Allocating connection %#x\n", *ConnHandle);
    SetRole(ce, CLIENT);
    SetState(ce, C_AWAITINIT2);
    secure_random_bytes(&ce->PeerUnique, sizeof(ce->PeerUnique));
    secure_random_bytes(&xrandom, sizeof(xrandom));

    if (Bparms->SecurityLevel != RPC2_OPENKIMONO && Bparms->SharedSecret)
        memcpy(rpc2key, *Bparms->SharedSecret, sizeof(RPC2_EncryptionKey));
    else
        memset(rpc2key, 0, sizeof(RPC2_EncryptionKey));

    /* set up decryption/validation context */
    rc = setup_init1_key(secure_setup_decrypt, &ce->sa, xrandom, ce->PeerUnique,
                         rpc2key);
    if (rc) {
        say(1, RPC2_DebugLevel, "Failed to initialize security context\n");
        rpc2_Quit(RPC2_FAIL);
    }

    ce->SecurityLevel  = Bparms->SecurityLevel;
    ce->EncryptionType = Bparms->EncryptionType;

    /* Obtain pointer to appropriate set of side effect routines */
    if (Bparms->SideEffectType) {
        for (i = 0; i < SE_DefCount; i++)
            if (SE_DefSpecs[i].SideEffectType == Bparms->SideEffectType)
                break;
        if (i >= SE_DefCount) {
            DROPCONN();
            rpc2_Quit(RPC2_FAIL); /* bogus side effect */
        }
        ce->SEProcs = &SE_DefSpecs[i];
    }

    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_Bind1)) {
        rc = (*ce->SEProcs->SE_Bind1)(*ConnHandle, Bparms->ClientIdent);
        if (rc != RPC2_SUCCESS) {
            DROPCONN();
            rpc2_Quit(rc);
        }
    }

    assert(Subsys->Tag == RPC2_SUBSYSBYID);
    ce->SubsysId = Subsys->Value.SubsysId;

    /* Step 2: Construct Init1 packet */
    bsize = sizeof(struct Init1Body) - sizeof(ib->Text) +
            (Bparms->ClientIdent == NULL ? 0 : Bparms->ClientIdent->SeqLen);
    RPC2_AllocBuffer(bsize, &pb);
    rpc2_InitPacket(pb, ce, bsize);
    SetPktColor(pb, Bparms->Color);

    switch ((int)Bparms->SecurityLevel) {
    case RPC2_OPENKIMONO:
        pb->Header.Opcode = RPC2_INIT1OPENKIMONO;
        break;

    case RPC2_AUTHONLY:
        pb->Header.Opcode = RPC2_INIT1AUTHONLY;
        break;

    case RPC2_HEADERSONLY:
        pb->Header.Opcode = RPC2_INIT1HEADERSONLY;
        break;

    case RPC2_SECURE:
        pb->Header.Opcode = RPC2_INIT1SECURE;
        break;

    default:
        RPC2_FreeBuffer(&pb);
        DROPCONN();
        rpc2_Quit(RPC2_FAIL); /* bogus security level  specified */
    }

    /* Fill in the body */
    ib = (struct Init1Body *)pb->Body;
    memset(ib, 0, sizeof(struct Init1Body));
    ib->FakeBody_SideEffectType      = htonl(Bparms->SideEffectType);
    ib->FakeBody_SecurityLevel       = htonl(Bparms->SecurityLevel);
    ib->FakeBody_EncryptionType      = htonl(Bparms->EncryptionType);
    ib->FakeBody_AuthenticationType  = htonl(Bparms->AuthenticationType);
    ib->FakeBody_ClientIdent_SeqBody = 0;
    if (Bparms->ClientIdent == NULL)
        ib->FakeBody_ClientIdent_SeqLen = 0;
    else {
        ib->FakeBody_ClientIdent_SeqLen = htonl(Bparms->ClientIdent->SeqLen);
        /* ib->FakeBody_ClientIdent_SeqBody = ib->Text; // not really meaningful:
         * this is pointer has to be reset on other side */
        memcpy(ib->Text, Bparms->ClientIdent->SeqBody,
               Bparms->ClientIdent->SeqLen);
    }
    assert(sizeof(RPC2_VERSION) < sizeof(ib->Version));
    strcpy((char *)ib->Version, RPC2_VERSION);

    /* hint for the other side that we support the new encryption layer */
    if (ce->sa.decrypt) {
        pb->Header.Flags |= RPC2SEC_CAPABLE;
        ib->RPC2SEC_version   = htonl(SECURE_VERSION);
        ib->Preferred_Keysize = htonl(RPC2_Preferred_Keysize);
    }

    rpc2_htonp(pb); /* convert header to network order */

    ib->XRandom = xrandom;
    if (Bparms->SecurityLevel != RPC2_OPENKIMONO) {
        /* Same decryption step as used on the server. */
        rpc2_Decrypt((char *)&ib->XRandom, (char *)&xrandom, sizeof(xrandom),
                     *Bparms->SharedSecret, Bparms->EncryptionType);
        xrandom = ntohl(xrandom);
        say(9, RPC2_DebugLevel, "XRandom = %d\n", xrandom);
    }

    /* Step3: Send INIT1 packet  and wait for reply */

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT2 packet) */

    say(9, RPC2_DebugLevel, "Sending INIT1 packet on %#x\n", *ConnHandle);
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, &ce->TimeBomb);

    switch (sl->ReturnCode) {
    case ARRIVED:
        say(9, RPC2_DebugLevel, "Received INIT2 packet on %#x\n", *ConnHandle);
        RPC2_FreeBuffer(&pb); /* release the Init1 Packet */
        pb = (RPC2_PacketBuffer *)sl->data; /* get the Init2 Packet */
        rpc2_FreeSle(&sl);
        break;

    case NAKED:
    case TIMEOUT:
    case KEPTALIVE:
        /* free connection, buffers, and quit */
        say(9, RPC2_DebugLevel, "Failed to send INIT1 packet on %#x\n",
            *ConnHandle);
        RPC2_FreeBuffer(&pb); /* release the Init1 Packet */
        rc = sl->ReturnCode == NAKED ? RPC2_NAKED : RPC2_NOBINDING;
        rpc2_FreeSle(&sl);
        DROPCONN();
        if (rc == RPC2_NOBINDING && peeraddrs)
            goto try_next_addr;
        rpc2_Quit(rc);

    default:
        assert(FALSE);
    }

    /* At this point, pb points to the Init2 packet */

    /* Step3: Examine Init2 packet, get bind info (at least
       PeerHandle) and continue with handshake sequence */
    /* is usually RPC2_SUCCESS or RPC2_OLDVERSION */
    init2rc = pb->Header.ReturnCode;

    /* Authentication failure typically */
    if (init2rc < RPC2_ELIMIT) {
        DROPCONN();
        RPC2_FreeBuffer(&pb);
        rpc2_Quit(init2rc);
    }

    /* We have a good INIT2 packet in pb */
    /* If the reply was over a secure connection, we interpret the INIT2 body
     * differently (uint32_t auth, uint32_t encr, uint8_t key[]) */
    if (pb->Prefix.sa) {
        rc = unpack_initX_body(ce, pb, &auth, &encr, &rpc2sec_version, &keylen);
        if (rc != RPC2_SUCCESS) {
            DROPCONN();
            RPC2_FreeBuffer(&pb);
            rpc2_Quit(rc);
        }
        /* When we know that the server is trying to use an incorrectly
         * initialized AES-CCM counter, we can't do much about that. However
         * we can at least force it to use AES-CBC encryption for any packets
         * it sends back to us. */
        if (rpc2sec_version == 0) {
            auth = secure_get_auth_byid(SECURE_AUTH_AES_XCBC_96);
            encr = secure_get_encr_byid(SECURE_ENCR_AES_CBC);
            keylen += auth->keysize;
        }

        new_binding = 1;
    } else if (RPC2_secure_only) {
        /* We don't want to fall back on the old handshake */
        DROPCONN();
        RPC2_FreeBuffer(&pb);
        rpc2_Quit(RPC2_OLDVERSION);
    } else {
        say(-1, RPC2_DebugLevel,
            "Client doesn't support RPC2SEC, using old handshake\n");
        /* The INIT2 packet came over insecure connection, remove decryption
         * context and fall back on the old handshake */
        secure_setup_decrypt(0, &ce->sa, NULL, NULL, NULL, 0);
        say(1, RPC2_DebugLevel, "Got INIT2, proceeding with old binding\n");
        new_binding = 0;

        /* old authenticated binding */
        if (Bparms->SecurityLevel != RPC2_OPENKIMONO) {
            if (pb->Prefix.LengthOfPacket <
                (ssize_t)(sizeof(struct RPC2_PacketHeader) +
                          sizeof(struct Init2Body))) {
                DROPCONN();
                RPC2_FreeBuffer(&pb);
                rpc2_Quit(RPC2_NOTAUTHENTICATED);
            }

            ib2 = (struct Init2Body *)pb->Body;
            rpc2_Decrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body),
                         *Bparms->SharedSecret, Bparms->EncryptionType);
            ib2->XRandomPlusOne = ntohl(ib2->XRandomPlusOne);
            say(9, RPC2_DebugLevel, "XRandomPlusOne = %d\n",
                ib2->XRandomPlusOne);
            if (xrandom + 1 != ib2->XRandomPlusOne) {
                DROPCONN();
                RPC2_FreeBuffer(&pb);
                rpc2_Quit(RPC2_NOTAUTHENTICATED);
            }
            yrandom = ntohl(ib2->YRandom);
            say(9, RPC2_DebugLevel, "YRandom = %d\n", yrandom);
        }
    }

    ce->PeerHandle  = pb->Header.LocalHandle;
    ce->sa.peer_spi = pb->Header.LocalHandle;
    say(9, RPC2_DebugLevel, "PeerHandle for local %#x is %#x\n", *ConnHandle,
        ce->PeerHandle);
    RPC2_FreeBuffer(&pb); /* Release INIT2 packet */

    /* old bind sequence, 2-way handshake, skip remaining phases */
    if (!new_binding && Bparms->SecurityLevel == RPC2_OPENKIMONO)
        goto BindOver;

    /* Step4: Construct Init3 packet and send it */
    bodylen = new_binding ? (3 * sizeof(uint32_t) + keylen) :
                            sizeof(struct Init3Body);

    RPC2_AllocBuffer(bodylen, &pb);
    rpc2_InitPacket(pb, ce, bodylen);
    pb->Header.Opcode = RPC2_INIT3;

    rpc2_htonp(pb);

    if (new_binding) {
        /* keylen, auth and encr were initialized when we handled the secure
         * INIT2 packet. We could choose different algorithms and keylengths,
         * but that doesn't seem particularily useful.
         * - A good server will already have picked the 'optimal strategy'
         * - MITM shouldn't have been able to send a decryptable INIT2 packet
         * - A rogue server won't care how we encrypt outgoing data, it clearly
         *   will be getting any required key material with this INIT3 packet.
         */
        rc = pack_initX_body(&ce->sa, auth, encr, rpc2sec_version, &pb->Body,
                             keylen);
        if (rc) {
            DROPCONN();
            RPC2_FreeBuffer(&pb);
            rpc2_Quit(RPC2_FAIL);
        }
    } else {
        ib3                 = (struct Init3Body *)pb->Body;
        ib3->YRandomPlusOne = htonl(yrandom + 1);
        rpc2_Encrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body),
                     *Bparms->SharedSecret,
                     Bparms->EncryptionType); /* in-place encryption */
    }

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT4 packet) */

    say(9, RPC2_DebugLevel, "Sending INIT3 packet on %#x\n", *ConnHandle);

    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, &ce->TimeBomb);

    switch (sl->ReturnCode) {
    case ARRIVED:
        say(9, RPC2_DebugLevel, "Received INIT4 packet on %#x\n", *ConnHandle);
        RPC2_FreeBuffer(&pb); /* release the Init3 Packet */
        pb = (RPC2_PacketBuffer *)sl->data; /* get the Init4 Packet */
        rpc2_FreeSle(&sl);
        break;

    case NAKED:
    case TIMEOUT:
        /* free connection, buffers, and quit */
        say(9, RPC2_DebugLevel, "Failed to send INIT3 packet on %#x\n",
            *ConnHandle);
        RPC2_FreeBuffer(&pb); /* release the Init3 Packet */
        rpc2_FreeSle(&sl);
        DROPCONN();
        if (peeraddrs)
            goto try_next_addr;
        rpc2_Quit(RPC2_NOBINDING);

    default:
        assert(FALSE);
    }

    /* Step5: Verify Init4 packet; pb points to it. */
    init4rc = pb->Header.ReturnCode; /* should be RPC2_SUCCESS */

    if (init4rc != RPC2_SUCCESS) { /* Authentication failure typically */
        DROPCONN();
        RPC2_FreeBuffer(&pb);
        rpc2_Quit(init4rc);
    }

    if (pb->Prefix.LengthOfPacket < (ssize_t)(sizeof(struct RPC2_PacketHeader) +
                                              sizeof(struct Init4Body))) {
        DROPCONN();
        RPC2_FreeBuffer(&pb);
        rpc2_Quit(RPC2_NOTAUTHENTICATED);
    }

    /* check nonce */
    if (new_binding && pb->Header.Uniquefier != ce->PeerUnique) {
        DROPCONN();
        RPC2_FreeBuffer(&pb);
        rpc2_Quit(RPC2_NOTAUTHENTICATED);
    }

    /* We have a good INIT4 packet in pb */
    ib4 = (struct Init4Body *)pb->Body;
    if (!new_binding) {
        rpc2_Decrypt((char *)ib4, (char *)ib4, sizeof(struct Init4Body),
                     *Bparms->SharedSecret, Bparms->EncryptionType);
        ib4->XRandomPlusTwo = ntohl(ib4->XRandomPlusTwo);
        // say(9, RPC2_DebugLevel, "XRandomPlusTwo = %l\n", ib4->XRandomPlusTwo);
        if (xrandom + 2 != ib4->XRandomPlusTwo) {
            DROPCONN();
            RPC2_FreeBuffer(&pb);
            rpc2_Quit(RPC2_NOTAUTHENTICATED);
        }
    }
    memcpy(ce->SessionKey, ib4->SessionKey, sizeof(RPC2_EncryptionKey));
    ce->NextSeqNumber = ntohl(ib4->InitialSeqNumber);

    RPC2_FreeBuffer(&pb); /* Release Init4 Packet */

    /* The security handshake is now over */

BindOver:
    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_Bind2)) {
        rc = (*ce->SEProcs->SE_Bind2)(*ConnHandle, 0);
        if (rc != RPC2_SUCCESS) {
            DROPCONN();
            rpc2_Quit(rc);
        }
    }

    SetState(ce, C_THINK);
    LWP_NoYieldSignal(ce);

    say(9, RPC2_DebugLevel, "Bind complete for %#x\n", *ConnHandle);
    ProfileEnableSet(true);
    rpc2_Quit(RPC2_SUCCESS);
    /* quit */
}

long RPC2_InitSideEffect(IN RPC2_Handle ConnHandle, IN SE_Descriptor *SDesc)
{
    say(1, RPC2_DebugLevel, "RPC2_InitSideEffect()\n");

    TR_INITSE();

    rpc2_Enter();
    rpc2_Quit(InvokeSE(1, ConnHandle, SDesc, 0));
}

long RPC2_CheckSideEffect(IN RPC2_Handle ConnHandle, INOUT SE_Descriptor *SDesc,
                          IN long Flags)
{
    say(1, RPC2_DebugLevel, "RPC2_CheckSideEffect()\n");

    TR_CHECKSE();

    rpc2_Enter();
    rpc2_Quit(InvokeSE(2, ConnHandle, SDesc, Flags));
}

/* CallType: 1 ==> Init, 2==> Check */
static int InvokeSE(long CallType, RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                    long Flags)
{
    long rc;
    struct CEntry *ce;

    ce = rpc2_GetConn(ConnHandle);
    if (!ce)
        rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ce, SERVER, S_PROCESS))
        return (RPC2_FAIL);
    if (ce->sebroken)
        return (RPC2_SEFAIL2);

    if (!SDesc || !ce->SEProcs)
        return (RPC2_FAIL);

    if (CallType == 1) {
        if (!ce->SEProcs->SE_InitSideEffect)
            return (RPC2_FAIL);
        SetState(ce, S_INSE);
        rc = (*ce->SEProcs->SE_InitSideEffect)(ConnHandle, SDesc);
    } else {
        if (!ce->SEProcs->SE_CheckSideEffect)
            return (RPC2_FAIL);
        SetState(ce, S_INSE);
        rc = (*ce->SEProcs->SE_CheckSideEffect)(ConnHandle, SDesc, Flags);
    }
    if (rc < RPC2_FLIMIT)
        ce->sebroken = TRUE;
    SetState(ce, S_PROCESS);
    return (rc);
}

long RPC2_Unbind(RPC2_Handle whichConn)
{
    struct CEntry *ce;
    struct MEntry *me;

    say(1, RPC2_DebugLevel, "RPC2_Unbind(%x)\n", whichConn);

    TR_UNBIND();

    rpc2_Enter();
    rpc2_Unbinds++;

    ce = __rpc2_GetConn(whichConn);
    if (ce == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    if (TestState(ce, CLIENT, ~(C_THINK | C_HARDERROR)) ||
        TestState(ce, SERVER,
                  ~(S_AWAITREQUEST | S_REQINQUEUE | S_PROCESS | S_HARDERROR)) ||
        (ce->MySl != NULL && ce->MySl->ReturnCode != WAITING)) {
        rpc2_Quit(RPC2_CONNBUSY);
    }

    /* Call side effect routine and ignore return code */
    if (HAVE_SE_FUNC(SE_Unbind))
        (*ce->SEProcs->SE_Unbind)(whichConn);

    /* Remove ourselves from our Mgrp if we have one. */
    me = ce->Mgrp;
    if (me != NULL)
        rpc2_RemoveFromMgrp(me, ce);

    rpc2_FreeConn(whichConn);
    rpc2_Quit(RPC2_SUCCESS);
}

time_t rpc2_time()
{
    return FT_ApproxTime();
}

void SavePacketForRetry(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
    struct SL_Entry *sl;

    pb->Header.Flags = htonl((ntohl(pb->Header.Flags) | RPC2_RETRY));
    ce->HeldPacket   = pb;

    if (ce->MySl)
        say(-1, RPC2_DebugLevel,
            "BUG: Pending DELAYED ACK response still queued!?");

    sl = rpc2_AllocSle(REPLY, ce);
    rpc2_ActivateSle(sl, &ce->SaveResponse);
}

static int GetFilter(RPC2_RequestFilter *inf, RPC2_RequestFilter *outf)
{
    struct SubsysEntry *ss;
    struct CEntry *ce;
    long i;

    if (inf == NULL) {
        outf->FromWhom = ANY;
        outf->OldOrNew = OLDORNEW;
    } else {
        *outf = *inf; /* structure assignment */
    }

    switch (outf->FromWhom) {
    case ANY:
        break;

    case ONESUBSYS:
        for (i = 0, ss = rpc2_SSList; i < rpc2_SSCount; i++, ss = ss->Next)
            if (ss->Id == outf->ConnOrSubsys.SubsysId)
                break;
        if (i >= rpc2_SSCount)
            return (FALSE); /* no such subsystem */
        break;

    case ONECONN:
        ce = rpc2_GetConn(outf->ConnOrSubsys.WhichConn);
        if (ce == NULL)
            return (FALSE);
        if (!TestState(ce, SERVER, S_AWAITREQUEST | S_REQINQUEUE))
            return (FALSE);
        break;
    }

    return (TRUE);
}

static RPC2_PacketBuffer *HeldReq(RPC2_RequestFilter *filter,
                                  struct CEntry **ce)
{
    RPC2_PacketBuffer *pb;
    long i;

    do {
        say(9, RPC2_DebugLevel, "Scanning hold queue\n");
        pb = rpc2_PBHoldList;
        for (i = 0; i < rpc2_PBHoldCount; i++) {
            if (rpc2_FilterMatch(filter, pb))
                break;
            else
                pb = (RPC2_PacketBuffer *)pb->Prefix.Next;
        }
        if (i >= rpc2_PBHoldCount)
            return (NULL);

        rpc2_UnholdPacket(pb);

        *ce = rpc2_GetConn(pb->Header.RemoteHandle);
        /* conn nuked; throw away and rescan */
        if (*ce == NULL) {
            say(9, RPC2_DebugLevel, "Conn missing, punting request\n");
            RPC2_FreeBuffer(&pb);
        }
    } while (!(*ce));

    return (pb);
}

static long GetNewRequest(IN RPC2_RequestFilter *filter,
                          IN struct timeval *timeout,
                          OUT struct RPC2_PacketBuffer **pb,
                          OUT struct CEntry **ce)
{
    struct SL_Entry *sl;

    say(9, RPC2_DebugLevel, "GetNewRequest()\n");

TryAnother:
    sl         = rpc2_AllocSle(REQ, NULL);
    sl->Filter = *filter; /* structure assignment */
    rpc2_ActivateSle(sl, timeout);

    LWP_WaitProcess((char *)sl);

    /* SocketListener will wake us up */

    switch (sl->ReturnCode) {
    case TIMEOUT: /* timeout */
        say(9, RPC2_DebugLevel, "Request wait timed out\n");
        rpc2_FreeSle(&sl);
        return (RPC2_TIMEOUT);

    case ARRIVED: /* a request that matches my filter was received */
        say(9, RPC2_DebugLevel, "Request wait succeeded\n");
        *pb = (RPC2_PacketBuffer *)sl->data; /* save the buffer */
        rpc2_FreeSle(&sl);

        *ce = rpc2_GetConn((*pb)->Header.RemoteHandle);
        if (*ce ==
            NULL) { /* a connection was nuked by someone while we slept */
            say(9, RPC2_DebugLevel, "Conn gone, punting packet\n");
            RPC2_FreeBuffer(pb);
            goto TryAnother;
        }
        return (RPC2_SUCCESS);

    default:
        assert(FALSE);
    }
    return RPC2_FAIL;
    /*NOTREACHED*/
}

static long MakeFake(RPC2_PacketBuffer *pb, struct CEntry *ce,
                     RPC2_Integer *xrand, RPC2_Integer *authenticationtype,
                     RPC2_CountedBS *cident, uint32_t *rpc2sec_version,
                     size_t *keysize)
{
    /* Synthesize fake packet after extracting encrypted XRandom and
     * clientident. It is really pretty ugly if you think about it, we
     * transform the Init1 packet to look like a valid RPC2_NEWCONNECTION
     * rpc so that it can be unpacked by the stub generated code and passed
     * back to the application. */
    long i;
    struct Init1Body *ib1;
    RPC2_NewConnectionBody *ncb;
    RPC2_Integer SideEffectType;

    if (pb->Prefix.LengthOfPacket <
        (ssize_t)(sizeof(struct RPC2_PacketHeader) + sizeof(struct Init1Body) -
                  sizeof(ib1->Text))) {
        return RPC2_FAIL;
    }

    ib1 = (struct Init1Body *)(pb->Body);
    ncb = (RPC2_NewConnectionBody *)&ib1->FakeBody_SideEffectType;

    if (strcmp((char *)ib1->Version, RPC2_VERSION) != 0) {
        say(9, RPC2_DebugLevel, "Old Version  Mine: \"%s\"  His: \"%s\"\n",
            RPC2_VERSION, (char *)ib1->Version);
        ce->Flags |= CE_OLDV;
    }

    *xrand              = ib1->XRandom; /* Still encrypted */
    *authenticationtype = ntohl(ncb->AuthenticationType);
    *rpc2sec_version    = ntohl(ib1->RPC2SEC_version);
    *keysize            = ntohl(ib1->Preferred_Keysize);

    cident->SeqLen  = ntohl(ncb->ClientIdent_SeqLen);
    cident->SeqBody = (RPC2_ByteSeq)&ncb->ClientIdent_SeqBody;

    /* check ClientIdent length since we're pointing the cident body back into
     * the received packet buffer */
    if ((char *)&ib1->Text + cident->SeqLen >
        (char *)&pb->Header + pb->Prefix.LengthOfPacket) {
        return (RPC2_FAIL);
    }
    memmove(cident->SeqBody, &ib1->Text, cident->SeqLen);

    /* Obtain pointer to appropriate set of SE routines */
    ce->SEProcs    = NULL;
    SideEffectType = ntohl(ncb->SideEffectType);
    if (SideEffectType) {
        for (i = 0; i < SE_DefCount; i++)
            if (SE_DefSpecs[i].SideEffectType == SideEffectType)
                break;
        if (i >= SE_DefCount)
            return (RPC2_SEFAIL2);
        ce->SEProcs = &SE_DefSpecs[i];
    }

    pb->Header.Opcode = RPC2_NEWCONNECTION;
    return (RPC2_SUCCESS);
}

static void SendOKInit2(IN struct CEntry *ce)
{
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, "SendOKInit2()\n");

    RPC2_AllocBuffer(sizeof(struct Init2Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init2Body));
    pb->Header.Opcode = RPC2_INIT2;
    if (ce->Flags & CE_OLDV)
        pb->Header.ReturnCode = RPC2_OLDVERSION;
    else
        pb->Header.ReturnCode = RPC2_SUCCESS;

    if (ce->TimeStampEcho) /* service time is now-requesttime */
        rpc2_StampPacket(ce, pb);

    rpc2_htonp(pb); /* convert to network order */
    rpc2_XmitPacket(pb, ce->HostInfo->Addr, 1);
    SavePacketForRetry(pb, ce);
}

static int ServerHandShake(struct CEntry *ce, int32_t xrand,
                           RPC2_EncryptionKey SharedSecret,
                           uint32_t rpc2sec_version, size_t keysize,
                           int new_binding)
{
    RPC2_PacketBuffer *pb;
    int32_t saveYRandom;
    long rc;

    if (!new_binding) {
        rpc2_Decrypt((char *)&xrand, (char *)&xrand, sizeof(xrand),
                     SharedSecret, ce->EncryptionType);
        xrand = ntohl(xrand);
    }

    /* Send Init2 packet and await Init3 */
    pb = Send2Get3(ce, SharedSecret, xrand, &saveYRandom, rpc2sec_version,
                   keysize, new_binding);
    if (!pb)
        return (RPC2_NOTAUTHENTICATED);

    /* Validate Init3 */
    rc = Test3(pb, ce, saveYRandom, SharedSecret, new_binding);
    RPC2_FreeBuffer(&pb);
    if (rc == RPC2_NOTAUTHENTICATED) {
        RejectBind(ce, sizeof(struct Init4Body), RPC2_INIT4);
        return (RPC2_NOTAUTHENTICATED);
    }

    /* Send Init4 */
    Send4AndSave(ce, xrand, SharedSecret, new_binding);
    return (RPC2_SUCCESS);
}

static void RejectBind(struct CEntry *ce, size_t bodysize, RPC2_Integer opcode)
{
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, "Rejecting  bind request\n");

    RPC2_AllocBuffer(bodysize, &pb);
    rpc2_InitPacket(pb, ce, bodysize);
    pb->Header.Opcode     = opcode;
    pb->Header.ReturnCode = RPC2_NOTAUTHENTICATED;

    rpc2_htonp(pb);
    rpc2_XmitPacket(pb, ce->HostInfo->Addr, 1);
    RPC2_FreeBuffer(&pb);
}

static RPC2_PacketBuffer *Send2Get3(struct CEntry *ce, RPC2_EncryptionKey key,
                                    int32_t xrand, int32_t *yrand,
                                    uint32_t rpc2sec_version, size_t keysize,
                                    int new_binding)
{
    RPC2_PacketBuffer *pb2, *pb3 = NULL;
    struct Init2Body *ib2;
    struct SL_Entry *sl;

    const struct secure_auth *auth = NULL;
    const struct secure_encr *encr = NULL;
    size_t bodylen                 = sizeof(struct Init2Body);

    if (new_binding) {
        if (rpc2sec_version == 0) { /* version 0, broken AES-CCM counter */
            auth = secure_get_auth_byid(SECURE_AUTH_AES_XCBC_96);
            encr = secure_get_encr_byid(SECURE_ENCR_AES_CBC);
        } else {
            auth = secure_get_auth_byid(SECURE_AUTH_NONE);
            encr = secure_get_encr_byid(SECURE_ENCR_AES_CCM_8);
        }
        if (!auth || !encr)
            return NULL;

        /* find the longest key that both the client and the server agree on */
        if (RPC2_Preferred_Keysize > keysize)
            keysize = RPC2_Preferred_Keysize;
        if (keysize < encr->min_keysize)
            keysize = encr->min_keysize;
        else if (keysize > encr->max_keysize)
            keysize = encr->max_keysize;

        keysize += auth->keysize;
        bodylen = 3 * sizeof(uint32_t) + keysize;
    }

    /* Allocate Init2 packet and initialize the header */
    RPC2_AllocBuffer(bodylen, &pb2);
    rpc2_InitPacket(pb2, ce, bodylen);

    pb2->Header.Opcode     = RPC2_INIT2;
    pb2->Header.ReturnCode = (ce->Flags & CE_OLDV) ? RPC2_OLDVERSION :
                                                     RPC2_SUCCESS;

    if (ce->TimeStampEcho) /* service time is now-requesttime */
        rpc2_StampPacket(ce, pb2);

    rpc2_htonp(pb2);

    /* and pack the body */
    if (new_binding) {
        if (pack_initX_body(&ce->sa, auth, encr, rpc2sec_version, &pb2->Body,
                            keysize)) {
            RPC2_FreeBuffer(&pb2);
            return NULL;
        }
    } else {
        /* Do xrand, yrand munging */
        say(9, RPC2_DebugLevel, "XRandom = %d\n", xrand);
        ib2                 = (struct Init2Body *)pb2->Body;
        ib2->XRandomPlusOne = htonl(xrand + 1);
        secure_random_bytes(yrand, sizeof(*yrand));
        ib2->YRandom = htonl(*yrand);
        say(9, RPC2_DebugLevel, "YRandom = %d\n", *yrand);
        rpc2_Encrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body), key,
                     ce->EncryptionType);
    }

    /* Send Init2 packet and await Init3 packet */
    SetState(ce, S_AWAITINIT3);
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb2, &ce->TimeBomb);

    switch (sl->ReturnCode) {
    case ARRIVED:
        pb3 = (RPC2_PacketBuffer *)sl->data; /* get the Init3 Packet */
        break;

    case NAKED:
    case TIMEOUT:
        /* free connection, buffers, and quit */
        say(9, RPC2_DebugLevel, "Failed to send INIT2\n");
        break;

    default:
        assert(FALSE);
    }

    /* Clean up and quit */
    rpc2_FreeSle(&sl);
    RPC2_FreeBuffer(&pb2); /* release the Init2 Packet */
    return (pb3);
}

static long Test3(RPC2_PacketBuffer *pb, struct CEntry *ce, int32_t yrand,
                  RPC2_EncryptionKey ekey, int new_binding)
{
    struct Init3Body *ib3;

    if (new_binding)
        return unpack_initX_body(ce, pb, NULL, NULL, NULL, NULL);

    if (pb->Prefix.LengthOfPacket < (ssize_t)(sizeof(struct RPC2_PacketHeader) +
                                              sizeof(struct Init3Body))) {
        say(9, RPC2_DebugLevel, "Runt Init3 packet\n");
        return RPC2_NOTAUTHENTICATED;
    }

    ib3 = (struct Init3Body *)pb->Body;
    rpc2_Decrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body), ekey,
                 ce->EncryptionType);
    ib3->YRandomPlusOne = ntohl(ib3->YRandomPlusOne);
    say(9, RPC2_DebugLevel, "YRandomPlusOne = %d\n", ib3->YRandomPlusOne);
    if (ib3->YRandomPlusOne != yrand + 1)
        return RPC2_NOTAUTHENTICATED;

    return RPC2_SUCCESS;
}

static void Send4AndSave(struct CEntry *ce, int32_t xrand,
                         RPC2_EncryptionKey ekey, int new_binding)
{
    RPC2_PacketBuffer *pb;
    struct Init4Body *ib4;

    say(9, RPC2_DebugLevel, "Send4AndSave()\n");

    RPC2_AllocBuffer(sizeof(struct Init4Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init4Body));
    pb->Header.Opcode     = RPC2_INIT4;
    pb->Header.ReturnCode = RPC2_SUCCESS;

    ib4 = (struct Init4Body *)pb->Body;
    memcpy(ib4->SessionKey, ce->SessionKey, sizeof(RPC2_EncryptionKey));
    ib4->InitialSeqNumber = htonl(ce->NextSeqNumber);

    if (!new_binding) {
        ib4->XRandomPlusTwo = htonl(xrand + 2);
        rpc2_Encrypt((char *)ib4, (char *)ib4, sizeof(struct Init4Body), ekey,
                     ce->EncryptionType);
    }

    if (ce->TimeStampEcho) /* service time is now-requesttime */
        rpc2_StampPacket(ce, pb);

    rpc2_htonp(pb);

    /* Send packet; don't bother waiting for acknowledgement */
    rpc2_XmitPacket(pb, ce->HostInfo->Addr, 1);

    SavePacketForRetry(pb, ce);
}

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


#define DEBUG
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "sftp.h"
#include "test.h"

#define SUBSYS_SRV 1001

#ifdef FAKESOCKETS
extern int fake;
#endif FAKESOCKETS


void DoBinding(RPC2_Handle *cid);
void PrintHelp(void);
void PrintStats(void);
void ClearStats(void);
void GetPort(    RPC2_PortIdent *p);
void GetWho(RPC2_CountedBS *w, long s, RPC2_EncryptionKey e);

extern struct SStats rpc2_Sent;
extern struct RStats rpc2_Recvd;
extern long RPC2_Perror;
extern long RPC2_DebugLevel;
#ifndef FAKESOCKETS
extern long SFTP_DebugLevel;
#endif FAKESOCKETS
FILE *ErrorLogFile;
static char ShortText[200];
static char LongText[3000];
PROCESS mypid;			/* Pid of main process */

long VMMaxFileSize; /* length of VMFileBuf, initially 0 */
long VMCurrFileSize; /* amount of useful data in VMFileBuf */
char *VMFileBuf;    /* for FILEINVM transfers */

long rpc2rc;
#define WhatHappened(X,Y) ((rpc2rc = X), printf("%s: %s\n", Y, RPC2_ErrorMsg(rpc2rc)), rpc2rc)



int main(int arg, char **argv)
{
	RPC2_Handle cid;
	long i, j, tt;
	long opcode;
	
	char *nextc;
	RPC2_PacketBuffer *Buff1;
	RPC2_PacketBuffer *Buff2;
	SFTP_Initializer sftpi;

	long rpctime;
	struct timeval t1, t2;
	
	SE_Descriptor sed;

	ErrorLogFile = stderr;
	CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mypid) == LWP_SUCCESS);


	for (i='a'; i < 'z' + 1; i++) {
		j = 6*(i-'a');
		ShortText[j] = ShortText[j+1] = ShortText[j+2] = 
			ShortText[j+3] = ShortText[j+4]
			= ShortText[j+5] = i;
	}
	LongText[0] = 0;
	for (i = 0; i < 10; i++)
		(void) strcat(LongText, ShortText);


	printf("Debug level? ");
	(void) scanf("%ld", &RPC2_DebugLevel);

#ifndef FAKESOCKETS
	SFTP_SetDefaults(&sftpi);
	sftpi.WindowSize = 32;
	sftpi.SendAhead = 8;
	sftpi.AckPoint = 8;
	sftpi.PacketSize = 2800;
	SFTP_Activate(&sftpi);
	SFTP_EnforceQuota = 1;
#endif FAKESOCKETS

#ifdef PROFILE
	InitProfiling();
#endif PROFILE

	if(WhatHappened(RPC2_Init(RPC2_VERSION, (RPC2_Options *)NULL, 
				  (RPC2_PortIdent *)NULL, 0, (struct timeval *)NULL), "Init") != RPC2_SUCCESS)
	exit(-1);

#ifdef OLDLWP
    lwp_stackUseEnabled = 0;
#endif OLDLWP

    CODA_ASSERT(RPC2_AllocBuffer(RPC2_MAXPACKETSIZE - 500, &Buff1) == RPC2_SUCCESS);
                       /* 500 is a fudge factor */

    Buff2 = NULL; /* only for safety; RPC2_MakeRPC() will set it */


    DoBinding(&cid);

    while (TRUE)
	{
	printf("RPC operation (%d for help)? ", HELP);
	(void) scanf("%ld", &opcode);

	Buff1->Header.Opcode = opcode;
	switch((int) opcode)
	    {
	    case HELP: 
		    FT_GetTimeOfDay(&t1, NULL);
		    
		    PrintHelp();
		    FT_GetTimeOfDay(&t2, NULL);
		    break;

	    case REMOTESTATS:
	    case BEGINREMOTEPROFILING:
	    case ENDREMOTEPROFILING:
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		Buff1->Header.BodyLength = 0;
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, 
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		if (tt != RPC2_SUCCESS)
		    continue;
		FT_GetTimeOfDay(&t2, NULL);
		break;

	    case STATS:
		PrintStats();
		break;

	    case SETVMFILESIZE:
		printf("Local buffer size? ");  (void) scanf("%ld", &VMMaxFileSize);
		if (VMFileBuf) free(VMFileBuf);
		CODA_ASSERT(VMFileBuf = (char *)malloc((unsigned)VMMaxFileSize));
		break;
		
	    case SETREMOTEVMFILESIZE:
		{
		printf("Remote buffer size? ");
		(void) scanf("%ld", &tt);
		tt = (long) htonl((unsigned long)tt);
		bcopy((char *)&tt, (char *)Buff1->Body, sizeof(long));
		Buff1->Header.BodyLength = sizeof(long);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL,  
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)continue;
		break;
		}

		


	    case DUMPTRACE:
		{
		long count;
		printf("How many elements? "); (void) scanf("%ld", &count);
		(void) RPC2_DumpTrace(stdout, count);
		break;
		}

	    case FETCHFILE:
	    case STOREFILE:
		bzero((char *)&sed, sizeof(sed));  /* initialize */
		sed.Tag = SMARTFTP;
		sed.Value.SmartFTPD.Tag = FILEBYNAME;
		sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0644;
	
		if (opcode  == (long) STOREFILE)
			sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
		else sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	
		printf("Request body length (0 unless testing piggybacking): ");
		(void) scanf("%lu", &Buff1->Header.BodyLength);		

		printf("Local seek offset? (0): ");
		(void) scanf("%ld", &sed.Value.SmartFTPD.SeekOffset);

		printf("Local byte quota? (-1): ");
		(void) scanf("%ld", &sed.Value.SmartFTPD.ByteQuota);

		printf("Local file name ('-' for stdin/stdout, '/dev/mem' for VM file): ");
		(void) scanf("%s", sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName);
		if (strcmp(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "-") == 0)
		    {
		    sed.Value.SmartFTPD.Tag = FILEBYFD;
		    sed.Value.SmartFTPD.FileInfo.ByFD.fd = (opcode == FETCHFILE) ? fileno(stdout) : 
								fileno(stdin);
		    
		    }

		if (strcmp(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, "/dev/mem") == 0)
		    {
		    sed.Value.SmartFTPD.Tag = FILEINVM;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody= (RPC2_ByteSeq)VMFileBuf;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = VMMaxFileSize;
		    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = VMCurrFileSize; /* ignored for fetch */
		    }
		


		/* Request packet contains: reply length, remote seek offset, remote byte quota, 
			hash mark, remote name*/
		printf("Reply body length (0 unless testing piggybacking): ");
		(void) scanf("%ld", &tt);
		tt = (long) htonl((unsigned long)tt);
		bcopy((char *)&tt, (char *)Buff1->Body, sizeof(long));

		printf("Remote seek offset (0) : ");
		(void) scanf("%ld", &tt);
		tt = (long) htonl((unsigned long)tt);
		bcopy((char *)&tt, (char *)Buff1->Body + sizeof(long), sizeof(long));

		printf("Remote byte quota (-1): ");
		(void) scanf("%ld", &tt);
		tt = (long) htonl((unsigned long)tt);
		bcopy((char *)&tt, (char *)Buff1->Body + 2*sizeof(long), sizeof(long));

		printf("Remote file name ('-' for stdin/stdout, '/dev/mem' for VM file): ");
		(void) scanf("%s", (char *)Buff1->Body+1+3*sizeof(long));
		Buff1->Header.BodyLength += 3*sizeof(long)+2+strlen((char *)(Buff1->Body+1+3*sizeof(long)));

		printf("Hash mark: ");
#ifndef	__linux__
		CODA_ASSERT(fseek(stdin, (long) 0, 2) == 0);
#endif
		(void) scanf("%c", &sed.Value.SmartFTPD.hashmark);
		if (sed.Value.SmartFTPD.hashmark == '0')
			sed.Value.SmartFTPD.hashmark = 0;
		*(Buff1->Body + 3*sizeof(long)) = sed.Value.SmartFTPD.hashmark;

                


		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
#ifdef PROFILE
		ProfilingOn();
#endif PROFILE
		tt = RPC2_MakeRPC(cid, Buff1, &sed, &Buff2, (struct timeval *)NULL, (long) 0);
#ifdef PROFILE
		ProfilingOff();
#endif PROFILE
		FT_GetTimeOfDay(&t2, NULL);


		if (tt != RPC2_SUCCESS)
		    {
		    WhatHappened(tt, "MakeRPC");
		    continue;
		    }
		tt = Buff2->Header.ReturnCode;
		if (tt == (long)SE_SUCCESS && sed.LocalStatus == SE_SUCCESS)
		    {
		    rpctime = ((t2.tv_sec - t1.tv_sec)*1000) + ((t2.tv_usec - t1.tv_usec)/1000);
		    printf("%ld bytes transferred in %ld milliseconds (%ld kbytes/second) \n", sed.Value.SmartFTPD.BytesTransferred,
		    	rpctime, sed.Value.SmartFTPD.BytesTransferred/rpctime);
		    printf("QuotaExceeded = %ld\n", sed.Value.SmartFTPD.QuotaExceeded);
		    if (opcode == (long)FETCHFILE &&
			    (sed.Value.SmartFTPD.Tag == FILEINVM))
			{
			VMCurrFileSize = sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen;
			printf("VMCurrFileSize = %ld\n", VMCurrFileSize);
			}
		    }
		else printf("RemoteStatus: %s\tLocalStatus: %s\n",
			SE_ErrorMsg(tt), SE_ErrorMsg((long) sed.LocalStatus));

	    break;

	    
	    case QUIT:
		goto Finish;
		
	    case ONEPING:
		printf("1 ping:\n");
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		Buff1->Header.BodyLength = 0;
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, &Buff2, 
					       (struct timeval *)NULL, (long)0), "MakeRPC");
		if (tt != RPC2_SUCCESS)
		    continue;
		FT_GetTimeOfDay(&t2, NULL);
	    break;

	    case MANYPINGS:
		printf("How many pings?: ");
		(void) scanf("%ld", &i);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
#ifdef PROFILE
		ProfilingOn();
#endif PROFILE
		while(i--)
		    {
		    Buff1->Header.BodyLength = 0;
		    Buff1->Header.Opcode = opcode;
		    tt = RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL, &Buff2, 
				      (struct timeval *)NULL, (long)0);
		    if (tt != RPC2_SUCCESS)break;
		    CODA_ASSERT(RPC2_FreeBuffer(&Buff2) == RPC2_SUCCESS);
		    }
#ifdef PROFILE
		ProfilingOff();
#endif PROFILE
		if (tt != RPC2_SUCCESS)
		    {
		    WhatHappened(tt, "MakeRPC");
		    continue;
		    }
		FT_GetTimeOfDay(&t2, NULL);
	    break;

	    case LENGTHTEST:
		printf("Length? ");
		(void) scanf("%ld", &tt);
		tt = (long) htonl((unsigned long)tt);
		bcopy((char *)&tt, (char *)Buff1->Body, sizeof(long));
		tt = (long) ntohl((unsigned long)tt);
		Buff1->Header.BodyLength = sizeof(long) + tt;
		bcopy((char *)LongText, (char *)Buff1->Body+sizeof(long), (int)tt);
		ClearStats();
		FT_GetTimeOfDay(&t1, NULL);
		tt = WhatHappened(RPC2_MakeRPC(cid, Buff1, (SE_Descriptor *)NULL,  
					       &Buff2, (struct timeval *)NULL, 
					       (long) 0), "MakeRPC");
		FT_GetTimeOfDay(&t2, NULL);
		if (tt != RPC2_SUCCESS)continue;
	    break;


	    case REBIND:
		WhatHappened(RPC2_Unbind(cid), "Unbind");
                DoBinding(&cid);
		continue;
	    }
		

	if(Buff2 != NULL)
	    {
#ifdef RPC2DEBUG
	    if (RPC2_DebugLevel > 0) {
		rpc2_htonp(Buff2);  /* rpc2_PrintPacketHeader assumes net order */
		rpc2_PrintPacketHeader(Buff2, rpc2_tracefile);
		rpc2_ntohp(Buff2);
	    }
	    (void) fflush(rpc2_tracefile);
#endif RPC2DEBUG
	    printf("Response Body: ``");
	    nextc = (char *)Buff2->Body;
	    for (i=0; i < Buff2->Header.BodyLength; i++)
		(void) putchar(*nextc++);
	    printf("''\n");
	    CODA_ASSERT(RPC2_FreeBuffer(&Buff2) == RPC2_SUCCESS);
	    }

	rpctime = ((t2.tv_sec - t1.tv_sec)*1000) + ((t2.tv_usec - t1.tv_usec)/1000);

	printf("RPC2_MakeRPC(): %ld milliseconds elapsed time\n", rpctime);
	}
Finish:
    (void) RPC2_Unbind(cid);

#ifdef PROFILE
    DoneProfiling();
#endif PROFILE
    return 0;

    }



int iopen()
{
	printf("In iopen");
    CODA_ASSERT(1 == 0);
    return 1;
}



void PrintStats()
{
    printf("RPC2:\n");
    printf("Packets Sent = %lu\tPacket Retries = %lu (of %lu)\tPackets Received = %lu\n",
	   rpc2_Sent.Total, rpc2_Sent.Retries, 
	   rpc2_Sent.Retries + rpc2_Sent.Cancelled, rpc2_Recvd.Total);
    printf("Bytes sent = %lu\tBytes received = %lu\n", rpc2_Sent.Bytes, rpc2_Recvd.Bytes);
    printf("Received Packet Distribution:\n");
    printf("\tRequests = %lu\tGoodRequests = %lu\n",
	   rpc2_Recvd.Requests, rpc2_Recvd.GoodRequests);
    printf("\tReplies = %lu\tGoodReplies = %lu\n",
	   rpc2_Recvd.Replies, rpc2_Recvd.GoodReplies);
    printf("\tBusies = %lu\tGoodBusies = %lu\n",
	    rpc2_Recvd.Busies, rpc2_Recvd.GoodBusies);
    printf("SFTP:\n");
    printf("Packets Sent = %lu\t\tStarts Sent = %lu\t\tDatas Sent = %lu\n",
	   sftp_Sent.Total, sftp_Sent.Starts, sftp_Sent.Datas);
    printf("Data Retries Sent = %lu\t\tAcks Sent = %lu\t\tNaks Sent = %lu\n",
	   sftp_Sent.DataRetries, sftp_Sent.Acks, sftp_Sent.Naks);
    printf("Busies Sent = %lu\t\t\tBytes Sent = %lu\n",
	   sftp_Sent.Busies, sftp_Sent.Bytes);
    printf("Packets Received = %lu\t\tStarts Received = %lu\tDatas Received = %lu\n",
	   sftp_Recvd.Total, sftp_Recvd.Starts, sftp_Recvd.Datas);
    printf("Data Retries Received = %lu\tAcks Received = %lu\tNaks Received = %lu\n",
	   sftp_Recvd.DataRetries, sftp_Recvd.Acks, sftp_Recvd.Naks);
    printf("Busies Received = %lu\t\tBytes Received = %lu\n",
	   sftp_Recvd.Busies, sftp_Recvd.Bytes);
    (void) fflush(stdout);
    }


void ClearStats()
    {
    bzero((char *)&rpc2_Sent, sizeof(struct SStats));
    bzero((char *)&rpc2_Recvd, sizeof(struct RStats));
    bzero((char *)&sftp_Sent, sizeof(struct sftpStats));
    bzero((char *)&sftp_Recvd, sizeof(struct sftpStats));
    }



void GetHost(RPC2_HostIdent *h)
{
    char buff[100];

    do {
	h->Tag = RPC2_HOSTBYINETADDR;
	printf("Host id? ");
	(void) scanf("%s", buff);
    } while (inet_aton(buff, &h->Value.InetAddress) != 0);
}

void GetPort(    RPC2_PortIdent *p)
    {
    long i;

    p->Tag = RPC2_PORTBYINETNUMBER;
    printf("Port number? "); (void) scanf("%ld", &i);
    p->Value.InetPortNumber = i;
    p->Value.InetPortNumber = htons(p->Value.InetPortNumber);
    }

void GetWho(RPC2_CountedBS *w, long s, RPC2_EncryptionKey e)
{
    if (s != RPC2_OPENKIMONO)
	{
	printf("Identity? ");
	(void) scanf("%s", (char *)w->SeqBody);
	w->SeqLen = 1+strlen((char *)w->SeqBody);
	printf("Encryption Key (7 chars)? ");
	(void) sprintf((char *)e, "       ");
	(void) scanf("%s", (char *)e);
	}
    else
	{
	printf("Identity [.]? ");
	(void) scanf("%s", (char *)w->SeqBody);
	w->SeqLen = 1+strlen((char *)w->SeqBody);
	}
    }


void PrintHelp(void)
{
    int i, n;
    n = sizeof(Opnames)/sizeof(char *);
    
    for (i = 0; i < n; i++) printf("%s = %d   ", Opnames[i], i);
    printf("\n");
}


void DoBinding(RPC2_Handle *cid)
{
    RPC2_HostIdent hid;
    RPC2_PortIdent sid;
    RPC2_SubsysIdent ssid;
    RPC2_BindParms bparms;
    RPC2_EncryptionKey ekey;
    RPC2_CountedBS who;
    char whobuff[100];

    hid.Tag = RPC2_HOSTBYNAME;
    printf("Host? "); (void) scanf("%s", hid.Value.Name);
    GetPort(&sid);
    ssid.Tag = RPC2_SUBSYSBYID;
    ssid.Value.SubsysId = SUBSYS_SRV;

    printf("Side Effect Type (%d or %d)? ", 0, SMARTFTP);
    (void) scanf("%ld", &bparms.SideEffectType);

    printf("Security [98(OK), 12(AO), 73(HO), 66(S)]? ");
    (void) scanf("%ld", &bparms.SecurityLevel);

    who.SeqLen = 0;
    who.SeqBody = (RPC2_Byte *)whobuff;
    GetWho(&who, bparms.SecurityLevel, ekey);


    bparms.ClientIdent = &who;
    if (bparms.SecurityLevel == RPC2_OPENKIMONO)
	{
	if (strcmp((char *)who.SeqBody, ".") == 0)
	    bparms.ClientIdent = NULL;
	}
    else
	{
	bparms.EncryptionType = RPC2_XOR;
	bparms.SharedSecret = (RPC2_EncryptionKey *)ekey;
	}

    bparms.AuthenticationType = 0;	/* server doesn't care */

    if (WhatHappened(RPC2_NewBinding(&hid, &sid, &ssid, &bparms, cid),
            "NewBinding") < RPC2_ELIMIT)
	exit(-1);

    }

















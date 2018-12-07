/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
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
 *    Venus Stats API and subsystem
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "venusstats.h"
#include "venus.private.h"
#include "venuslog.h"
#include "venusstats.subsystem.h"

struct stats_subsystem_t {
    bool initialized;
    VFSStatistics VFSStats;
    RPCOpStatistics RPCOpStats;
};

/* *****  Private variables  ***** */
static struct stats_subsystem_t stats_sub_inst;

static const char *VFSOpsNameTemplate[NVFSOPS] = {
    "No-Op",
    "No-Op",
    "Root",
    "OpenByFD",
    "Open",
    "Close",
    "Ioctl",
    "Getattr",
    "Setattr",
    "Access",
    "Lookup",
    "Create",
    "Remove",
    "Link",
    "Rename",
    "Mkdir",
    "Rmdir",
    "No-Op",
    "Symlink",
    "Readlink",
    "Fsync",
    "No-Op",
    "Vget",
    "Signal",
    "Replace",
    "Flush",
    "PurgeUser",
    "ZapFile",
    "ZapDir",
    "No-Op",
    "PurgeFid",
    "OpenByPath",
    "Resolve",
    "Reintegrate",
    "Statfs",
    "Store",
    "Release",
    "AccessIntent",
    "No-Op",
    "No-Op"
};

void StatsSetup() {
    stats_sub_inst.initialized = true;
}

void StatsReset() {
    StatsInit();
}

void StatsInit() {
    int i;

    LOG(0, ("E StatsInit()\n"));
    memset((void *)&stats_sub_inst.VFSStats, 0, (int)sizeof(VFSStatistics));
    for (i = 0; i < NVFSOPS; i++)
        strncpy(stats_sub_inst.VFSStats.VFSOps[i].name, VFSOpsNameTemplate[i],
                VFSSTATNAMELEN);

    memset((void *)&stats_sub_inst.RPCOpStats, 0, (int)sizeof(RPCOpStatistics));
    for (i = 0; i < srvOPARRAYSIZE; i++) {
       strncpy(stats_sub_inst.RPCOpStats.RPCOps[i].name, 
               (char *) srv_CallCount[i].name+4, 
               RPCOPSTATNAMELEN);
    }
    LOG(0, ("L StatsInit()\n"));
}


void VFSPrint(int afd) {
    fdprint(afd, "VFS Operations\n");
    fdprint(afd, " Operation                 Counts                    Times\n");
    for (int i = 0; i < NVFSOPS; i++)
	if (!STREQ(stats_sub_inst.VFSStats.VFSOps[i].name, "No-Op")) {
	    VFSStat *t = &stats_sub_inst.VFSStats.VFSOps[i];

	    double mean = (t->success > 0 ? (t->time / (double)t->success) : 0.0);
	    double stddev = (t->success > 1 ? sqrt(((double)t->success * t->time2 - t->time * t->time) / ((double)t->success * (double)(t->success - 1))) : 0.0);

	    fdprint(afd, "%-12s  :  %5d  [%5d %5d %5d]  :  %5.1f (%5.1f)\n",
		    t->name, t->success, t->retry, t->timeout, t->failure, mean, stddev);
	}
    fdprint(afd, "\n");
}


void RPCPrint(int afd) {
    /* Operation statistics. */
    fdprint(afd, "RPC Operations:\n");
    fdprint(afd, " Operation    \tGood  Bad   Time MGood  MBad MTime   RPCR MRPCR\n");
    for (int i = 1; i < srvOPARRAYSIZE; i++) {
	RPCOpStat *t = &stats_sub_inst.RPCOpStats.RPCOps[i];
	fdprint(afd, "%-16s %5d %5d %5.1f %5d %5d %5.1f %5d %5d\n",
		t->name, t->good, t->bad, (t->time > 0 ? t->time / t->good : 0),
		t->Mgood, t->Mbad, (t->Mtime > 0 ? t->Mtime / t->Mgood : 0),
		t->rpc_retries, t->Mrpc_retries);
    }
    fdprint(afd, "\n");

    /* Communication statistics. */
    fdprint(afd, "RPC Packets:\n");
    RPCPktStatistics RPCPktStats;
    memset((void *)&RPCPktStats, 0, (int)sizeof(RPCPktStatistics));
    GetCSS(&RPCPktStats);
    struct SStats *rsu = &RPCPktStats.RPC2_SStats_Uni;
    struct SStats *rsm = &RPCPktStats.RPC2_SStats_Multi;
    struct RStats *rru = &RPCPktStats.RPC2_RStats_Uni;
    struct RStats *rrm = &RPCPktStats.RPC2_RStats_Multi;
    fdprint(afd, "RPC2:\n");
    fdprint(afd, "   Sent:           Total        Retrys  Busies   Naks\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d   %5d\n",
	     rsu->Total, rsu->Bytes, rsu->Retries, rsu->Busies, rsu->Naks);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d   %5d\n",
	     rsm->Total, rsm->Bytes, rsm->Retries, 0, 0);
    fdprint(afd, "   Received:       Total          Replys       Reqs       Busies    Bogus    Naks\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d : %-2d  %5d : %-2d  %5d : %-2d  %5d   %5d\n",
	     rru->Total, rru->Bytes, rru->GoodReplies, (rru->Replies - rru->GoodReplies),
	     rru->GoodRequests, (rru->Requests - rru->GoodRequests), rru->GoodBusies,
	     (rru->Busies - rru->GoodBusies), rru->Bogus, rru->Naks);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d : %-2d  %5d : %-2d  %5d : %-2d  %5d   %5d\n",
	     rrm->Total, rrm->Bytes, 0, 0,
	     rrm->GoodRequests, (rrm->Requests - rrm->GoodRequests), 0, 0, 0, 0);
    struct sftpStats *msu = &RPCPktStats.SFTP_SStats_Uni;
    struct sftpStats *msm = &RPCPktStats.SFTP_SStats_Multi;
    struct sftpStats *mru = &RPCPktStats.SFTP_RStats_Uni;
    struct sftpStats *mrm = &RPCPktStats.SFTP_RStats_Multi;
    fdprint(afd, "SFTP:\n");
    fdprint(afd, "   Sent:           Total        Starts     Datas       Acks    Naks   Busies\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     msu->Total, msu->Bytes, msu->Starts, msu->Datas,
	     msu->DataRetries, msu->Acks, msu->Naks, msu->Busies);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     msm->Total, msm->Bytes, msm->Starts, msm->Datas,
	     msm->DataRetries, msm->Acks, msm->Naks, msm->Busies);
    fdprint(afd, "   Received:       Total        Starts     Datas       Acks    Naks   Busies\n");
    fdprint(afd, "      Uni:    %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     mru->Total, mru->Bytes, mru->Starts, mru->Datas,
	     mru->DataRetries, mru->Acks, mru->Naks, mru->Busies);
    fdprint(afd, "      Multi:  %5d : %-8d  %5d   %5d : %-4d  %5d   %5d   %5d\n",
	     mrm->Total, mrm->Bytes, mrm->Starts, mrm->Datas,
	     mrm->DataRetries, mrm->Acks, mrm->Naks, mrm->Busies);
    fdprint(afd, "\n");
}

void GetCSS(RPCPktStatistics *cs) {
    cs->RPC2_SStats_Uni = rpc2_Sent;
    cs->RPC2_SStats_Multi = rpc2_MSent;
    cs->RPC2_RStats_Uni = rpc2_Recvd;
    cs->RPC2_RStats_Multi = rpc2_MRecvd;
    cs->SFTP_SStats_Uni = sftp_Sent;
    cs->SFTP_SStats_Multi = sftp_MSent;
    cs->SFTP_RStats_Uni = sftp_Recvd;
    cs->SFTP_RStats_Multi = sftp_MRecvd;
}

void SubCSSs(RPCPktStatistics *cs1, RPCPktStatistics *cs2) {
    cs1->RPC2_SStats_Uni.Total -= cs2->RPC2_SStats_Uni.Total;
    cs1->RPC2_SStats_Uni.Retries -= cs2->RPC2_SStats_Uni.Retries;
    cs1->RPC2_SStats_Uni.Multicasts -= cs2->RPC2_SStats_Uni.Multicasts;
    cs1->RPC2_SStats_Uni.Busies -= cs2->RPC2_SStats_Uni.Busies;
    cs1->RPC2_SStats_Uni.Naks -= cs2->RPC2_SStats_Uni.Naks;
    cs1->RPC2_SStats_Uni.Bytes -= cs2->RPC2_SStats_Uni.Bytes;

    cs1->RPC2_SStats_Multi.Total -= cs2->RPC2_SStats_Multi.Total;
    cs1->RPC2_SStats_Multi.Retries -= cs2->RPC2_SStats_Multi.Retries;
    cs1->RPC2_SStats_Multi.Multicasts -= cs2->RPC2_SStats_Multi.Multicasts;
    cs1->RPC2_SStats_Multi.Busies -= cs2->RPC2_SStats_Multi.Busies;
    cs1->RPC2_SStats_Multi.Naks -= cs2->RPC2_SStats_Multi.Naks;
    cs1->RPC2_SStats_Multi.Bytes -= cs2->RPC2_SStats_Multi.Bytes;

    cs1->RPC2_RStats_Uni.Total -= cs2->RPC2_RStats_Uni.Total;
    cs1->RPC2_RStats_Uni.Giant -= cs2->RPC2_RStats_Uni.Giant;
    cs1->RPC2_RStats_Uni.Replies -= cs2->RPC2_RStats_Uni.Replies;
    cs1->RPC2_RStats_Uni.Requests -= cs2->RPC2_RStats_Uni.Requests;
    cs1->RPC2_RStats_Uni.GoodReplies -= cs2->RPC2_RStats_Uni.GoodReplies;
    cs1->RPC2_RStats_Uni.GoodRequests -= cs2->RPC2_RStats_Uni.GoodRequests;
    cs1->RPC2_RStats_Uni.Multicasts -= cs2->RPC2_RStats_Uni.Multicasts;
    cs1->RPC2_RStats_Uni.GoodMulticasts -= cs2->RPC2_RStats_Uni.GoodMulticasts;
    cs1->RPC2_RStats_Uni.Busies -= cs2->RPC2_RStats_Uni.Busies;
    cs1->RPC2_RStats_Uni.GoodBusies -= cs2->RPC2_RStats_Uni.GoodBusies;
    cs1->RPC2_RStats_Uni.Bogus -= cs2->RPC2_RStats_Uni.Bogus;
    cs1->RPC2_RStats_Uni.Naks -= cs2->RPC2_RStats_Uni.Naks;
    cs1->RPC2_RStats_Uni.Bytes -= cs2->RPC2_RStats_Uni.Bytes;

    cs1->RPC2_RStats_Multi.Total -= cs2->RPC2_RStats_Multi.Total;
    cs1->RPC2_RStats_Multi.Giant -= cs2->RPC2_RStats_Multi.Giant;
    cs1->RPC2_RStats_Multi.Replies -= cs2->RPC2_RStats_Multi.Replies;
    cs1->RPC2_RStats_Multi.Requests -= cs2->RPC2_RStats_Multi.Requests;
    cs1->RPC2_RStats_Multi.GoodReplies -= cs2->RPC2_RStats_Multi.GoodReplies;
    cs1->RPC2_RStats_Multi.GoodRequests -= cs2->RPC2_RStats_Multi.GoodRequests;
    cs1->RPC2_RStats_Multi.Multicasts -= cs2->RPC2_RStats_Multi.Multicasts;
    cs1->RPC2_RStats_Multi.GoodMulticasts -= cs2->RPC2_RStats_Multi.GoodMulticasts;
    cs1->RPC2_RStats_Multi.Busies -= cs2->RPC2_RStats_Multi.Busies;
    cs1->RPC2_RStats_Multi.GoodBusies -= cs2->RPC2_RStats_Multi.GoodBusies;
    cs1->RPC2_RStats_Multi.Bogus -= cs2->RPC2_RStats_Multi.Bogus;
    cs1->RPC2_RStats_Multi.Naks -= cs2->RPC2_RStats_Multi.Naks;
    cs1->RPC2_RStats_Multi.Bytes -= cs2->RPC2_RStats_Multi.Bytes;

    cs1->SFTP_SStats_Uni.Total -= cs2->SFTP_SStats_Uni.Total;
    cs1->SFTP_SStats_Uni.Starts -= cs2->SFTP_SStats_Uni.Starts;
    cs1->SFTP_SStats_Uni.Datas -= cs2->SFTP_SStats_Uni.Datas;
    cs1->SFTP_SStats_Uni.DataRetries -= cs2->SFTP_SStats_Uni.DataRetries;
    cs1->SFTP_SStats_Uni.Acks -= cs2->SFTP_SStats_Uni.Acks;
    cs1->SFTP_SStats_Uni.Naks -= cs2->SFTP_SStats_Uni.Naks;
    cs1->SFTP_SStats_Uni.Busies -= cs2->SFTP_SStats_Uni.Busies;
    cs1->SFTP_SStats_Uni.Bytes -= cs2->SFTP_SStats_Uni.Bytes;

    cs1->SFTP_SStats_Multi.Total -= cs2->SFTP_SStats_Multi.Total;
    cs1->SFTP_SStats_Multi.Starts -= cs2->SFTP_SStats_Multi.Starts;
    cs1->SFTP_SStats_Multi.Datas -= cs2->SFTP_SStats_Multi.Datas;
    cs1->SFTP_SStats_Multi.DataRetries -= cs2->SFTP_SStats_Multi.DataRetries;
    cs1->SFTP_SStats_Multi.Acks -= cs2->SFTP_SStats_Multi.Acks;
    cs1->SFTP_SStats_Multi.Naks -= cs2->SFTP_SStats_Multi.Naks;
    cs1->SFTP_SStats_Multi.Busies -= cs2->SFTP_SStats_Multi.Busies;
    cs1->SFTP_SStats_Multi.Bytes -= cs2->SFTP_SStats_Multi.Bytes;

    cs1->SFTP_RStats_Uni.Total -= cs2->SFTP_RStats_Uni.Total;
    cs1->SFTP_RStats_Uni.Starts -= cs2->SFTP_RStats_Uni.Starts;
    cs1->SFTP_RStats_Uni.Datas -= cs2->SFTP_RStats_Uni.Datas;
    cs1->SFTP_RStats_Uni.DataRetries -= cs2->SFTP_RStats_Uni.DataRetries;
    cs1->SFTP_RStats_Uni.Acks -= cs2->SFTP_RStats_Uni.Acks;
    cs1->SFTP_RStats_Uni.Naks -= cs2->SFTP_RStats_Uni.Naks;
    cs1->SFTP_RStats_Uni.Busies -= cs2->SFTP_RStats_Uni.Busies;
    cs1->SFTP_RStats_Uni.Bytes -= cs2->SFTP_RStats_Uni.Bytes;

    cs1->SFTP_RStats_Multi.Total -= cs2->SFTP_RStats_Multi.Total;
    cs1->SFTP_RStats_Multi.Starts -= cs2->SFTP_RStats_Multi.Starts;
    cs1->SFTP_RStats_Multi.Datas -= cs2->SFTP_RStats_Multi.Datas;
    cs1->SFTP_RStats_Multi.DataRetries -= cs2->SFTP_RStats_Multi.DataRetries;
    cs1->SFTP_RStats_Multi.Acks -= cs2->SFTP_RStats_Multi.Acks;
    cs1->SFTP_RStats_Multi.Naks -= cs2->SFTP_RStats_Multi.Naks;
    cs1->SFTP_RStats_Multi.Busies -= cs2->SFTP_RStats_Multi.Busies;
    cs1->SFTP_RStats_Multi.Bytes -= cs2->SFTP_RStats_Multi.Bytes;
}

RPCOpStat &GetRPCOpt(int opcode) {
    return stats_sub_inst.RPCOpStats.RPCOps[opcode];
}

const char *VenusOpStr(int opcode)
{
    static char	buf[12];    /* This is shaky. */

    if (opcode >= 0 && opcode < NVFSOPS)
        return(stats_sub_inst.VFSStats.VFSOps[opcode].name);

    snprintf(buf, 12, "%d", opcode);
    return(buf);
}

VFSStatistics & GetVFSStats() {
    return stats_sub_inst.VFSStats;
}

RPCOpStatistics & GetRPCOpStats() {
    return stats_sub_inst.RPCOpStats;
}
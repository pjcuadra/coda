/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_

typedef int (*VCP)(VnodeType, void *, void *);

int ValidateParms(RPC2_Handle, ClientEntry **, VolumeId *, RPC2_CountedBS *);
int AllocVnode(Vnode **, Volume *, ViceDataType, ViceFid *, ViceFid *, UserId,
               int *);
int CheckFetchSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, Rights *,
                        Rights *);
int CheckGetAttrSemantics(ClientEntry *, Vnode **, Vnode **, Volume **,
                          Rights *, Rights *);
int CheckGetACLSemantics(ClientEntry *, Vnode **, Volume **, Rights *, Rights *,
                         RPC2_BoundedBS *, RPC2_String *);
int CheckStoreSemantics(ClientEntry *, Vnode **, Vnode **, Volume **, VCP,
                        FileVersion, Rights *, Rights *);
int CheckSetAttrSemantics(ClientEntry *, Vnode **, Vnode **, Volume **,
                          VCP, RPC2_Integer, Date_t, UserId, RPC2_Unsigned,
                          RPC2_Integer, FileVersion,
                          Rights *, Rights *);
int CheckSetACLSemantics(ClientEntry *, Vnode **, Volume **, VCP,
                         ViceVersionVector *, FileVersion, Rights *, Rights *,
                         RPC2_CountedBS *, AL_AccessList **);
int CheckCreateSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                         VCP, FileVersion, FileVersion, Rights *, Rights *, int = 1);
int CheckRemoveSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                         VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckLinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                        VCP, FileVersion, FileVersion, Rights *, Rights *, int = 1);
int CheckRenameSemantics(ClientEntry *, Vnode **, Vnode **, Vnode **, char *,
                         Vnode **, char *, Volume **, VCP, void *, void *,
                         void *, void *, Rights *, Rights *, Rights *, Rights *,
                         Rights *, Rights *, int = 1, int = 0, dlist * = NULL);
int CheckMkdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                        VCP, FileVersion, FileVersion, Rights *, Rights *, int = 1);
int CheckRmdirSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                        VCP, void *, void *, Rights *, Rights *, int = 1);
int CheckSymlinkSemantics(ClientEntry *, Vnode **, Vnode **, char *, Volume **,
                          VCP, FileVersion, FileVersion, Rights *, Rights *,
                          int = 1);
void PerformFetch(ClientEntry *, Volume *, Vnode *);
int FetchBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *,
                      RPC2_Unsigned Offset, RPC2_Integer Count,
                      ViceVersionVector *VV);
void PerformGetAttr(ClientEntry *, Volume *, Vnode *);
void PerformGetACL(ClientEntry *, Volume *, Vnode *, RPC2_BoundedBS *,
                   RPC2_String);
void PerformStore(ClientEntry *, Volume *, Vnode *, Inode,
                  RPC2_Integer, Date_t, ViceStoreId *);
int StoreBulkTransfer(RPC2_Handle, ClientEntry *, Volume *, Vnode *, Inode,
                      RPC2_Integer);
void PerformSetAttr(ClientEntry *, Volume *, Vnode *,
                    RPC2_Integer, Date_t, UserId, RPC2_Unsigned, RPC2_Integer,
                    ViceStoreId *, Inode *);
void PerformSetACL(ClientEntry *, Volume *, Vnode *,
                   AL_AccessList *);
int PerformCreate(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                  Date_t, RPC2_Unsigned, ViceStoreId *, DirInode **, int *);
void PerformRemove(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                   Date_t, ViceStoreId *, DirInode **, int *);
int PerformLink(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                Date_t, ViceStoreId *, DirInode **, int *);
void PerformRename(ClientEntry *, Volume *, Vnode *, Vnode *, Vnode *,
                   Vnode *, char *, char *, Date_t, ViceStoreId *,
                   DirInode **, DirInode **, DirInode **, int * = NULL);
int PerformMkdir(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                 Date_t, RPC2_Unsigned, ViceStoreId *, DirInode **, int *);
void PerformRmdir(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                  Date_t, ViceStoreId *, DirInode **, int *);
int PerformSymlink(ClientEntry *, Volume *, Vnode *, Vnode *, char *,
                   Inode, RPC2_Unsigned, Date_t, RPC2_Unsigned,
                   ViceStoreId *, DirInode **, int *);
void PerformSetQuota(ClientEntry *, Volume *, Vnode *, ViceFid *, int, ViceStoreId *);

void PutObjects(int, Volume *, int, dlist *, int, int, int = 0);

void SpoolRenameLogRecord(int, vle *, vle *, vle *, vle *, Volume *, char *,
                          char *, ViceStoreId *);

#endif /* _OPERATIONS_H_ */

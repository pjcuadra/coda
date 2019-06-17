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
 * Specification of the Venus process abstraction
 *
 */

#ifndef _VENUS_VFS_H_
#define _VENUS_VFS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
}
#endif

/* Replica Control Rights. */
/* Note that we presently do not distinguish between read and write rights. */
/* We may well do so in the future, however. */
#define RC_STATUSREAD 1
#define RC_STATUSWRITE 2
#define RC_STATUS (RC_STATUSREAD | RC_STATUSWRITE)
#define RC_DATAREAD 4
#define RC_DATAWRITE 8
#define RC_DATA (RC_DATAREAD | RC_DATAWRITE)

struct venus_cnode {
    u_short c_flags; /* flags (see below) */
    VenusFid c_fid; /* file handle */
    CacheFile *c_cf; /* container file object */
    int c_type;
};

class vfs {
public:
    /* The vproc interface: mostly matching kernel requests.  */
    static int root(struct venus_cnode *);
    static int statfs(struct coda_statfs *);
    static int sync();
    static int vget(struct venus_cnode *, VenusFid *, int what = RC_STATUS);
    static int open(struct venus_cnode *, int);
    static int close(struct venus_cnode *, int);
    static int ioctl(struct venus_cnode *, unsigned char nr, struct ViceIoctl *,
                     int);
    static int select(struct venus_cnode *, int);
    static int getattr(struct venus_cnode *, struct coda_vattr *);
    static int setattr(struct venus_cnode *, struct coda_vattr *);
    static int access(struct venus_cnode *, int);
    static int lookup(struct venus_cnode *, const char *, struct venus_cnode *,
                      int);
    static int create(struct venus_cnode *, char *, struct coda_vattr *, int,
                      int, struct venus_cnode *);
    static int remove(struct venus_cnode *, char *);
    static int link(struct venus_cnode *, struct venus_cnode *, char *);
    static int rename(struct venus_cnode *, char *, struct venus_cnode *,
                      char *);
    static int mkdir(struct venus_cnode *, char *, struct coda_vattr *,
                     struct venus_cnode *);
    static int rmdir(struct venus_cnode *, char *);
    static int symlink(struct venus_cnode *, char *, struct coda_vattr *,
                       char *);
    static int readlink(struct venus_cnode *, struct coda_string *);
    static int fsync(struct venus_cnode *);

    /**
     * Read file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes to be read from the file
     *
     */
    static int read(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Write file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes to be written to the file
     *
     */
    static int write(struct venus_cnode *node, uint64_t pos, int64_t count);

    /**
     * Signal the end of a synchronous read file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes read from the file
     *
     */
    static int read_finish(struct venus_cnode *node, uint64_t pos,
                           int64_t count);

    /**
     * Signal the end of a synchronous write file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes written to the file
     *
     */
    static int write_finish(struct venus_cnode *node, uint64_t pos,
                            int64_t count);

    /**
     * Memory map file operation
     *
     * @param node     Venus cnode pointer holding file's metadata
     * @param pos      Offset within the file
     * @param count    Number of bytes mapped into memory
     *
     */
    static int mmap(struct venus_cnode *node, uint64_t pos, int64_t count);

    static int verifyname(char *name, int flags);
};

int k_Purge();
int k_Purge(VenusFid *, int = 0);
int k_Purge(uid_t);
int k_Replace(VenusFid *, VenusFid *);

#endif /* _VENUS_VFS_H_ */

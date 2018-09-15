/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2008 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    Specification of the Venus File-System Object (fso) abstraction.
 *
 *    ToDo:
 *
 */


#ifndef _VENUS_FSO_CACHEFILE_H_
#define _VENUS_FSO_CACHEFILE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
// #include <sys/stat.h>

// #include <sys/uio.h>

// #include <rpc2/rpc2.h>
// 
// #include <codadir.h>

extern int global_kernfd;
#ifdef __cplusplus
}
#endif

/* interfaces */
// #include <user.h>
// #include <vice.h>

/* from util */
#include <bitmap.h>
#include <dlist.h>

/* from lka */
// #include <lka.h>
#include <lwp/lock.h>

/* from venus */


/* from coda include again, must appear AFTER venus.private.h */


/* Representation of UFS file in cache directory. */
/* Currently, CacheFiles and fsobjs are statically bound to each
   other, one-to-one, by embedding */
/* a single CacheFile in each fsobj.  An fsobj may use its CacheFile
   in several ways (or not at all). */
/* We guarantee that these uses are mutually exclusive (in time,
   per-fsobj), hence the static allocation */
/* policy works.  In the future we may choose not to make the uses
   mutually exclusive, and will then */
/* have to implement some sort of dynamic allocation/binding
   scheme. */
/* The different uses of CacheFile are: */
/*    1. Copy of plain file */
/*    2. Unix-format copy of directory */

#define CACHEFILENAMELEN 12 + 4

#define CBLOCK_BITS_SIZE 15 /* 32768 = 2^15 */
#define CBLOCK_SIZE (1 << 15)
#define CBLOCK_SIZE_MAX (CBLOCK_SIZE - 1)

#define LARGEST_SUPPORTED_FILE_SIZE (2^32)
#define LARGEST_BITMAP_SIZE 131072 /* 2^32 / CBLOCK_SIZE */

static inline uint64_t cblocks_to_bytes(uint64_t cblocks) {
    return cblocks << CBLOCK_BITS_SIZE;
}

static inline uint64_t bytes_to_cblocks(uint64_t bytes) {
    return bytes >> CBLOCK_BITS_SIZE;
}

static inline uint64_t bytes_to_cblocks_floor(uint64_t bytes) {
    return bytes_to_cblocks(bytes);
}

static inline uint64_t bytes_to_cblocks_ceil(uint64_t bytes) {
    return bytes_to_cblocks(bytes + CBLOCK_SIZE_MAX);
}

static inline uint64_t align_to_cblock_ceil(uint64_t bytes)
{
    return cblocks_to_bytes(bytes_to_cblocks_ceil(bytes));
}

static inline uint64_t align_to_cblock_floor(uint64_t bytes)
{
    return cblocks_to_bytes(bytes_to_cblocks_floor(bytes));
}

static inline uint64_t cblock_start(uint64_t b_pos)
{
    return bytes_to_cblocks_floor(b_pos);
}

static inline uint64_t cblock_end(uint64_t b_pos, int64_t b_count)
{
    return bytes_to_cblocks_ceil(b_pos + b_count);
}

static inline uint64_t cblock_length(uint64_t b_pos, int64_t b_count)
{
    return cblock_end(b_pos, b_count) - cblock_start(b_pos);
}

static inline uint64_t pos_align_to_cblock(uint64_t b_pos)
{
    return cblocks_to_bytes(bytes_to_cblocks_floor(b_pos));
}

static inline uint64_t length_align_to_cblock(uint64_t b_pos, int64_t b_count)
{
    return cblocks_to_bytes(cblock_length(b_pos, b_count));
}

class CacheChunck : private dlink {
    friend class fsobj;
    friend class vproc;
 protected:
    uint64_t start;
    int64_t len;
    bool valid;

 public:
    CacheChunck(uint64_t start, int64_t len) : start(start), len(len),
        valid(true) {}
    CacheChunck() : start(0), len(0), valid(false) {}
    uint64_t GetStart() {return start;}
    int64_t GetLength() {return len;}
    bool isValid() {return valid;}
};

class CacheChunckList : private dlist {
    Lock rd_wr_lock;
 public:
    CacheChunckList();
    ~CacheChunckList();

    void AddChunck(uint64_t start, int64_t len);

    bool ReverseCheck(uint64_t start, int64_t len);
    void ReverseRemove(uint64_t start, int64_t len);
    
    void ForEach(void (*foreachcb)(uint64_t start, int64_t len, 
        void * usr_data_cb), void * usr_data = NULL);
    
    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();

    CacheChunck pop();
};

class CacheFile {
    friend class CacheChunck;
    friend class CacheSegmentFile;
    
protected:
    long length;
    long validdata; /* amount of successfully fetched data */
    int  refcnt;
    char name[CACHEFILENAMELEN];		/* "xx/xx/xx/xx" */
    int numopens;
    bitmap *cached_chuncks;
    int recoverable;
    Lock rw_lock;

    int ValidContainer();
    
    CacheChunck GetNextHole(uint64_t start_b, uint64_t end_b);
    int UpdateValidData();

 public:
    CacheFile(int i, int recoverable = 1);
    CacheFile();
    ~CacheFile();

    /* for safely obtaining access to container files, USE THESE!!! */
    void Create(int newlength = 0);
    int Open(int flags);
    int Close(int fd);

    void Validate();
    void Reset();
    int  Copy(CacheFile *destination);
    int  Copy(char *destname, int recovering = 0);

    void IncRef() { refcnt++; } /* creation already does an implicit incref */
    int  DecRef();             /* returns refcnt, unlinks if refcnt becomes 0 */

    void Stat(struct stat *);
    void Utimes(const struct timeval times[2]);
    void Truncate(long);
    void SetLength(uint64_t len);
    void SetValidData(uint64_t len);
    void SetValidData(uint64_t start, int64_t len);
    CacheChunckList * GetHoles(uint64_t start, int64_t len);

    char *Name()         { return(name); }
    long Length()        { return(length); }
    uint64_t ValidData(void) { return(validdata); }
    uint64_t ConsecutiveValidData(void);
    int  IsPartial(void) { return(length != validdata); }

    void print() { print (stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

class CacheSegmentFile : public CacheFile {
    CacheFile * cf;
    
public:
    CacheSegmentFile(int i);
    ~CacheSegmentFile();

    void Create(CacheFile *cf);
    int64_t ExtractSegment(uint64_t pos, int64_t count);
    void InjectSegment(uint64_t pos, int64_t count);
};

#endif	/* _VENUS_FSO_CACHEFILE_H_ */

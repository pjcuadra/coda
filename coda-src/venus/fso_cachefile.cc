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
                           none currently

#*/

/*
 *    Cache file management
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>
#include <mkpath.h>
#include <copyfile.h>

/* from util */
#include <rvmlib.h>

/* from venus */
#include "fso_cachefile.h"
#include "venusrecov.h"
#include "venus.private.h"

#ifndef fdatasync
#define fdatasync(fd) fsync(fd)
#endif

/* always useful to have a page of zero's ready */
static char zeropage[4096];

/*  *****  CacheFile Members  *****  */

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
CacheFile::CacheFile(int i, int recoverable)
{
    /* Assume caller has done RVMLIB_REC_OBJECT! */
    /* RVMLIB_REC_OBJECT(*this); */
    sprintf(name, "%02X/%02X/%02X/%02X",
	    (i>>24) & 0xff, (i>>16) & 0xff, (i>>8) & 0xff, i & 0xff);
            
    length = validdata = 0;
    refcnt = 1;
    numopens = 0;
    this->recoverable = recoverable;
    cached_chuncks = new (recoverable) bitmap(LARGEST_BITMAP_SIZE, recoverable);
    /* Container reset will be done by eventually by FSOInit()! */
    LOG(100, ("CacheFile::CacheFile(%d): %s (this=0x%x)\n", i, name, this));
}


CacheFile::CacheFile()
{
    CODA_ASSERT(length == 0);
    refcnt = 1;
    numopens = 0;
    this->recoverable = 1;
    cached_chuncks = new (recoverable) bitmap(LARGEST_BITMAP_SIZE, recoverable);
}


CacheFile::~CacheFile()
{
    LOG(10, ("CacheFile::~CacheFile: %s (this=0x%x)\n", name, this));
    CODA_ASSERT(length == 0);
    delete(cached_chuncks);
}


/* MUST NOT be called from within transaction! */
void CacheFile::Validate()
{
    if (!ValidContainer())
	Reset();
}


/* MUST NOT be called from within transaction! */
void CacheFile::Reset()
{
    if (access(name, F_OK) == 0 && length != 0) {
	Recov_BeginTrans();
	Truncate(0);
	Recov_EndTrans(MAXFP);
    }
}

int CacheFile::ValidContainer()
{
    struct stat tstat;
    int rc;
    
    rc = ::stat(name, &tstat);
    if (rc) return 0;

    int valid =
#ifndef __CYGWIN32__
      tstat.st_uid == (uid_t)V_UID &&
      tstat.st_gid == (gid_t)V_GID &&
      (tstat.st_mode & ~S_IFMT) == V_MODE &&
#endif
      tstat.st_size == (off_t)length;

    if (!valid && LogLevel >= 0) {
	dprint("CacheFile::ValidContainer: %s invalid\n", name);
	dprint("\t(%u, %u), (%u, %u), (%o, %o), (%d, %d)\n",
	       tstat.st_uid, (uid_t)V_UID, tstat.st_gid, (gid_t)V_GID,
	       (tstat.st_mode & ~S_IFMT), V_MODE,
	       tstat.st_size, length);
    }
    return(valid);
}

/* Must be called from within a transaction!  Assume caller has done
   RVMLIB_REC_OBJECT() */

void CacheFile::Create(int newlength)
{
    LOG(10, ("CacheFile::Create: %s, %d\n", name, newlength));

    int tfd;
    struct stat tstat;
    if (mkpath(name, V_MODE | 0100)<0)
	CHOKE("CacheFile::Create: could not make path for %s", name);
    if ((tfd = ::open(name, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::Create: open failed (%d)", errno);

#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif
    if (::ftruncate(tfd, newlength) < 0)
      CHOKE("CacheFile::Create: ftruncate failed (%d)", errno);
    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::ResetContainer: fstat failed (%d)", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::ResetContainer: close failed (%d)", errno);

    validdata = 0;
    length = newlength;
    refcnt = 1;
    numopens = 0;
}


/*
 * copies a cache file, data and attributes, to a new one.
 */
int CacheFile::Copy(CacheFile *destination)
{
    Copy(destination->name);

    destination->length = length;
    destination->validdata = validdata;
    ReadLock();
    destination->WriteLock();
    *(destination->cached_chuncks) = *cached_chuncks;
    destination->WriteUnlock();
    ReadUnlock();
    return 0;
}

int CacheFile::Copy(char *destname, int recovering)
{
    LOG(10, ("CacheFile::Copy: from %s, %d/%d, to %s\n",
	     name, validdata, length, destname));

    int tfd, ffd;
    struct stat tstat;

    if (mkpath(destname, V_MODE | 0100) < 0) {
	LOG(0, ("CacheFile::Copy: could not make path for %s\n", name));
	return -1;
    }
    if ((tfd = ::open(destname, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, V_MODE)) < 0) {
	LOG(0, ("CacheFile::Copy: open failed (%d)\n", errno));
	return -1;
    }
    ::fchmod(tfd, V_MODE);
#ifdef __CYGWIN32__
    ::chown(destname, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif
    if ((ffd = ::open(name, O_RDONLY| O_BINARY, V_MODE)) < 0)
	CHOKE("CacheFile::Copy: source open failed (%d)\n", errno);

    if (copyfile(ffd, tfd) < 0) {
	LOG(0, ("CacheFile::Copy failed! (%d)\n", errno));
	::close(ffd);
	::close(tfd);
	return -1;
    }
    if (::close(ffd) < 0)
	CHOKE("CacheFile::Copy: source close failed (%d)\n", errno);

    if (::fstat(tfd, &tstat) < 0)
	CHOKE("CacheFile::Copy: fstat failed (%d)\n", errno);
    if (::close(tfd) < 0)
	CHOKE("CacheFile::Copy: close failed (%d)\n", errno);
    
    CODA_ASSERT(recovering || (off_t)length == tstat.st_size);

    return 0;
}


int CacheFile::DecRef()
{
    if (--refcnt == 0)
    {
	length = validdata = 0;
	if (::unlink(name) < 0)
	    CHOKE("CacheFile::DecRef: unlink failed (%d)", errno);
    }
    return refcnt;
}


/* N.B. length member is NOT updated as side-effect! */
void CacheFile::Stat(struct stat *tstat)
{
    CODA_ASSERT(::stat(name, tstat) == 0);
}


void CacheFile::Utimes(const struct timeval times[2])
{
    CODA_ASSERT(::utimes(name, times) == 0);
}


/* MUST be called from within transaction! */
void CacheFile::Truncate(long newlen)
{
    int fd;

    fd = open(name, O_WRONLY | O_BINARY);
    CODA_ASSERT(fd >= 0 && "fatal error opening container file");

    /* ISR tweak, write zeros to data area before truncation */
    if (option_isr && newlen < length) {
	size_t len = sizeof(zeropage), n = length - newlen;

	lseek(fd, newlen, SEEK_SET);

	while (n) {
	    if (n < len) len = n;
	    write(fd, zeropage, len);
	    n -= len;
	}
	/* we have to force these writes to disk, otherwise the following
	 * truncate would simply drop any unwritten data */
	fdatasync(fd);
    }

    if (length != newlen) {
        if (recoverable) RVMLIB_REC_OBJECT(*this);
    
        

        if (newlen < length) {
            WriteLock();
            
            cached_chuncks->FreeRange(bytes_to_cblocks_floor(newlen), 
                bytes_to_cblocks_ceil(length - newlen));
                
            WriteUnlock();
        } 

        length = newlen;

        UpdateValidData();
    }

    CODA_ASSERT(::ftruncate(fd, length) == 0);

    close(fd);
}

/* Update the valid data*/
int CacheFile::UpdateValidData() {
    uint64_t length_cb = bytes_to_cblocks_ceil(length); /* Floor length in blocks */
    
    ReadLock();

    validdata = cblocks_to_bytes(cached_chuncks->Count());

    /* In case the the last block is set */
    if (cached_chuncks->Value(length_cb - 1)) {
        validdata -= cblocks_to_bytes(length_cb) - length;
    }
    
    ReadUnlock();
}

/* MUST be called from within transaction! */
void CacheFile::SetLength(uint64_t newlen)
{    
    if (length != newlen) {
        if (recoverable) RVMLIB_REC_OBJECT(*this);

        if (newlen < length) {
            WriteLock();
            
            cached_chuncks->FreeRange(bytes_to_cblocks_floor(newlen), 
                bytes_to_cblocks_ceil(length - newlen));
                
            WriteUnlock();
        }

        UpdateValidData();

        length = newlen;
    }

    LOG(60, ("CacheFile::SetLength: New Length: %d, Validata %d\n", newlen, validdata));
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t len)
{
    SetValidData(0, len);
}

/* MUST be called from within transaction! */
void CacheFile::SetValidData(uint64_t start, int64_t len)
{
    uint64_t start_cb = cblock_start(start);
    uint64_t end_cb = cblock_end(start, len);
    uint64_t newvaliddata = 0;
    uint64_t length_cb = bytes_to_cblocks_ceil(length);

    if (len < 0) {
        end_cb = length_cb;
    }

    if (end_cb > length_cb) {
        end_cb = length_cb;
    }

    if (recoverable) RVMLIB_REC_OBJECT(validdata);
    
    WriteLock();

    for (uint64_t i = start_cb; i < end_cb; i++) {
        if (cached_chuncks->Value(i)) {
            continue;
        }

        cached_chuncks->SetIndex(i);

        /* Add a full block */
        newvaliddata += CBLOCK_SIZE;

        /* The last block might not be full */
        if (i + 1 == length_cb) {
            newvaliddata -= cblocks_to_bytes(length_cb) - length;
            continue;
        }
    }
    
    WriteUnlock();

    validdata += newvaliddata;

    LOG(60, ("CacheFile::SetValidData: { validdata: %d }\n", validdata));
    LOG(60, ("CacheFile::SetValidData: { fetchedblocks: %d, totalblocks: %d }\n",
            cached_chuncks->Count(), length_cb));
}

void CacheFile::print(int fdes)
{
    fdprint(fdes, "[ %s, %d/%d ]\n", name, validdata, length);
}

int CacheFile::Open(int flags)
{
    int fd = ::open(name, flags | O_BINARY, V_MODE);

    CODA_ASSERT (fd != -1);
    numopens++;
    
    return fd;
}

int CacheFile::Close(int fd)
{
    CODA_ASSERT(fd != -1 && numopens);
    numopens--;
    return ::close(fd);
}

CacheChunck CacheFile::GetNextHole(uint64_t start_cb, uint64_t end_cb) 
{
    /* Ceil length in blocks */
    uint64_t length_cb = bytes_to_cblocks_ceil(length);
    uint64_t holestart = start_cb;
    uint64_t holesize = 0;

    for (uint64_t i = start_cb; i < end_cb; i++) {
        if (cached_chuncks->Value(i)) {
            holesize = 0;
            holestart = i + 1;
            continue;
        }

        /* Add a full block */
        holesize += CBLOCK_SIZE;

        /* The last block might not be full */
        if (i + 1 == length_cb) {
            holesize -= cblocks_to_bytes(length_cb) - length;
            return (CacheChunck(cblocks_to_bytes(holestart), holesize));
        }

        if ((i + 1 == end_cb) || cached_chuncks->Value(i + 1)) {
            return (CacheChunck(cblocks_to_bytes(holestart), holesize));
        }
    }

    return (CacheChunck());
}


CacheChunckList * CacheFile::GetHoles(uint64_t start, int64_t len) 
{
    uint64_t start_cb = cblock_start(start);
    uint64_t end_cb = cblock_end(start, len);
    uint64_t length_cb = bytes_to_cblocks_ceil(length);  // Ceil length in blocks
    uint64_t fstart = 0;
    uint64_t fend = 0;
    CacheChunckList * clist = new CacheChunckList();
    CacheChunck currc = {};

    if (len < 0) {
        end_cb = length_cb;
    }

    if (end_cb > length_cb) {
        end_cb = length_cb;
    }

    LOG(50, ("CacheFile::GetHoles Range [%d - %d]\n", start_cb, end_cb - 1));
    
    ReadLock();

    for (uint64_t i = start_cb; i < end_cb; i++) {
        currc = GetNextHole(i, end_cb);

        if (!currc.isValid()) break;

        fstart = currc.GetStart();
        fend = currc.GetStart() + currc.GetLength();

        LOG(50, ("CacheFile::GetHoles Found [%d - %d]\n",
                cblock_start(fstart),
                cblock_end(fstart, currc.GetLength()) - 1));

        clist->AddChunck(currc.GetStart(), currc.GetLength());

        /* Jump the hole */
        i = bytes_to_cblocks_ceil(currc.GetStart() + currc.GetLength());
    }
    
    ReadUnlock();

    return clist;
}

uint64_t CacheFile::ConsecutiveValidData(void)
{
    /* Use the start of the first hole */
    uint64_t start = GetNextHole(0, bytes_to_cblocks_ceil(length)).GetStart();

    if (start != 0)
        start--;

    return start;
}

CacheChunckList::~CacheChunckList()
{
    CacheChunck * curr = NULL;

    while ((curr = (CacheChunck *)this->first())) {
        this->remove((dlink *)curr);
    }
}

void CacheChunckList::AddChunck(uint64_t start, int64_t len)
{
    WriteLock();
    CacheChunck * new_chunck = new CacheChunck(start, len);
    this->insert((dlink *) new_chunck);
    WriteUnlock();
}

void rd_rw_lockable::ReadLock() 
{
    ObtainReadLock(&rd_wr_lock);
}

void rd_rw_lockable::WriteLock() 
{
    ObtainWriteLock(&rd_wr_lock);
}


void rd_rw_lockable::ReadUnlock() 
{
    ReleaseReadLock(&rd_wr_lock);
}

void rd_rw_lockable::WriteUnlock() 
{
    ReleaseWriteLock(&rd_wr_lock);
}

bool CacheChunckList::ReverseCheck(uint64_t start, int64_t len) {
    dlink * curr = NULL;
    CacheChunck * curr_cc = NULL;
    
    ReadLock();
    
    dlist_iterator previous(*this, DlDescending);
    
    while (curr = previous()) {
        curr_cc = (CacheChunck *)curr;
        
        if (!curr_cc->isValid()) continue;
        
        if (start != curr_cc->GetStart()) continue;
        
        if (len != curr_cc->GetLength()) continue;
        
        ReadUnlock();
        
        return true;
    }
    
    ReadUnlock();
    
    return false;
}

void CacheChunckList::ReverseRemove(uint64_t start, int64_t len)
{
    dlink * curr = NULL;
    CacheChunck * curr_cc = NULL;
    
    WriteLock();
    
    dlist_iterator previous(*this, DlDescending);
    
    while (curr = previous()) {
        curr_cc = (CacheChunck *)curr;
        
        if (!curr_cc->isValid()) continue;
        
        if (start != curr_cc->GetStart()) continue;
        
        if (len != curr_cc->GetLength()) continue;
        
        this->remove(curr);
        break;
    }
    
    WriteUnlock();
}

void CacheChunckList::ForEach(void (*foreachcb)(uint64_t start, int64_t len, 
    void * usr_data_cb), void * usr_data)
{
    dlink * curr = NULL;
    CacheChunck * curr_cc = NULL;
    
    if (!foreachcb) return;
    
    ReadLock();
    
    dlist_iterator next(*this);
    
    while (curr = next()) {
        curr_cc = (CacheChunck *)curr;
        foreachcb(curr_cc->GetStart(), curr_cc->GetLength(), usr_data);
    }
    
    ReadUnlock();
}


CacheChunck CacheChunckList::pop() {
    dlink * curr_first = NULL;
    CacheChunck * tmp = NULL;
    
    WriteLock();
    
    curr_first = this->first();
    tmp = (CacheChunck *)curr_first; 

    if (!curr_first) {
        WriteUnlock();
        return CacheChunck();
    }

    CacheChunck cp = CacheChunck(*tmp);
    this->remove(curr_first);
    delete tmp;
    
    WriteUnlock();

    return CacheChunck(cp);
}

/* MUST be called from within transaction! */
CacheSegmentFile::CacheSegmentFile(int i) : CacheFile(i, 0) 
{
    sprintf(name, "%02X/%02X/%02X/%02X.seg",
	    (i>>24) & 0xff, (i>>16) & 0xff, (i>>8) & 0xff, i & 0xff);
}

CacheSegmentFile::~CacheSegmentFile() {
    this->Truncate(0);
    DecRef();
}

void CacheSegmentFile::Create(CacheFile *cf)
{
    CacheFile::Create(cf->length);
    this->cf = cf;
}

int64_t CacheSegmentFile::ExtractSegment(uint64_t pos, int64_t count)
{
    uint32_t byte_start = pos_align_to_cblock(pos);
    uint32_t block_start = bytes_to_cblocks(byte_start);
    uint32_t byte_len = length_align_to_cblock(pos, count);
    uint32_t block_end = bytes_to_cblocks(byte_start + byte_len);
    int tfd, ffd;
    struct stat tstat;
    
    LOG(10, ("CacheSegmentFile::ExtractSegment: from %s [%d, %d], to %s\n",
             cf->name, byte_start, byte_len, name));

    if (mkpath(name, V_MODE | 0100) < 0) {
        LOG(0, ("CacheSegmentFile::ExtractSegment: could not make path for %s\n", name));
        return -1;
    }
    
    tfd = Open(O_RDWR|O_CREAT);
    
    ::fchmod(tfd, V_MODE);
    
#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif

    ffd = cf->Open(O_RDONLY);

    if (copyfile_seg(ffd, tfd, byte_start, byte_len) < 0) {
    	LOG(0, ("CacheSegmentFile::ExtractSegment failed! (%d)\n", errno));
    	cf->Close(ffd);
    	Close(tfd);
    	return -1;
    }

    if (cf->Close(ffd) < 0) {
        CHOKE("CacheSegmentFile::ExtractSegment: source close failed (%d)\n", 
              errno);
    }

    if (::fstat(tfd, &tstat) < 0) {
        CHOKE("CacheSegmentFile::ExtractSegment: fstat failed (%d)\n", errno);
    }

    if (Close(tfd) < 0){
        CHOKE("CacheSegmentFile::ExtractSegment: close failed (%d)\n", errno);
    }

    CODA_ASSERT((off_t)length == tstat.st_size);

    cf->ReadLock();
    WriteLock();

    cf->cached_chuncks->CopyRange(block_start, block_end + 1, *cached_chuncks);

    WriteUnlock();
    cf->ReadUnlock();

    UpdateValidData();

    return byte_len;
}

void CacheSegmentFile::InjectSegment(uint64_t pos, int64_t count)
{
    uint32_t byte_start = pos_align_to_cblock(pos);
    uint32_t block_start = bytes_to_cblocks(byte_start);
    uint32_t byte_len = length_align_to_cblock(pos, count);
    uint32_t block_end = bytes_to_cblocks(byte_start + byte_len);
    int tfd, ffd;
    struct stat tstat;
    
    LOG(10, ("CacheSegmentFile::InjectSegment: from %s [%d, %d], to %s\n",
	     name, byte_start, byte_len, cf->name));

    tfd = cf->Open(O_RDWR|O_CREAT);
    
    ::fchmod(tfd, V_MODE);
    
#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif

    ffd = Open(O_RDONLY);

    if (copyfile_seg(ffd, tfd, byte_start, byte_len) < 0) {
        LOG(0, ("CacheSegmentFile::ExtractSegment failed! (%d)\n", errno));
        cf->Close(ffd);
        Close(tfd);
        return;
    }

    if (Close(ffd) < 0) {
        CHOKE("CacheSegmentFile::InjectSegment: source close failed (%d)\n", 
              errno);
    }

    if (::fstat(tfd, &tstat) < 0) {
        CHOKE("CacheSegmentFile::InjectSegment: fstat failed (%d)\n", errno);
    }

    if (cf->Close(tfd) < 0) {
        CHOKE("CacheSegmentFile::InjectSegment: close failed (%d)\n", errno);
    }

    CODA_ASSERT((off_t)length == tstat.st_size);
    
    ReadLock();
    cf->WriteLock();

    cached_chuncks->CopyRange(block_start, block_end + 1, *cf->cached_chuncks);
    
    cf->WriteUnlock();
    ReadUnlock();

    cf->UpdateValidData();

}

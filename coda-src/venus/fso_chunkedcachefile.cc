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

/* Pre-allocation routine. */
/* MUST be called from within transaction! */
ChunkedCacheFile::ChunkedCacheFile(int i, int recoverable) : CacheFile(i, recoverable)
{
    cached_chunks = new (recoverable)
            bitmap7(CacheChunkBlockBitmapSize, recoverable);
        Lock_Init(&rw_lock);
     /* Container reset will be done by eventually by FSOInit()! */
    LOG(100, ("ChunkedCacheFile::ChunkedCacheFile(%d): %s (this=0x%x)\n", i, name, this));
}

ChunkedCacheFile::~ChunkedCacheFile()
{
    LOG(10, ("ChunkedCacheFile::~ChunkedCacheFile: %s (this=0x%x)\n", name, this));
    CODA_ASSERT(length == 0);

    delete cached_chunks;
}

/* Must be called from within a transaction!  Assume caller has done
   RVMLIB_REC_OBJECT() */

void ChunkedCacheFile::Create(int newlength)
{
    // clear bitmap
    cached_chunks->FreeRange(0, -1);
    CacheFile::Create(newlength);
}

/*
 * copies a cache file, data and attributes, to a new one.
 */
int ChunkedCacheFile::Copy(ChunkedCacheFile *destination)
{
    CacheFile::Copy(destination);

    ObtainDualLock(&rw_lock, READ_LOCK, &destination->rw_lock, WRITE_LOCK);

    *(destination->cached_chunks) = *cached_chunks;

    ReleaseDualLock(&rw_lock, READ_LOCK, &destination->rw_lock, WRITE_LOCK);

    return 0;
}

int ChunkedCacheFile::DecRef()
{
    if (refcnt == 1) {
        cached_chunks->FreeRange(0, -1);
    }

    return CacheFile::DecRef();
}

void ChunkedCacheFile::Truncate(uint64_t newlen)
{
    CacheFile::Truncate(newlen);

    if (length != newlen) {
        if (newlen < length) {
            ObtainWriteLock(&rw_lock);

            cached_chunks->FreeRange(bytes_to_ccblocks_ceil(newlen),
                                        bytes_to_ccblocks_ceil(length) -
                                            bytes_to_ccblocks_ceil(newlen));

            ReleaseWriteLock(&rw_lock);
        }

        length = newlen;
        UpdateValidData();
    }
}

/* Update the valid data*/
void ChunkedCacheFile::UpdateValidData()
{
    uint64_t length_cb = bytes_to_ccblocks_ceil(length);

    ObtainReadLock(&rw_lock);

    validdata = ccblocks_to_bytes(cached_chunks->Count());

    /* In case the the last block is set */
    if (length_cb && cached_chunks->Value(length_cb - 1)) {
        uint64_t empty_tail = ccblocks_to_bytes(length_cb) - length;
        validdata -= empty_tail;
    }

    ReleaseReadLock(&rw_lock);
}

/* MUST be called from within transaction! */
void ChunkedCacheFile::SetLength(uint64_t newlen)
{
    if (length != newlen) {
        if (recoverable)
            RVMLIB_REC_OBJECT(*this);

        if (newlen < length) {
            ObtainWriteLock(&rw_lock);

            cached_chunks->FreeRange(
                bytes_to_ccblocks_floor(newlen),
                bytes_to_ccblocks_ceil(length - newlen));

            ReleaseWriteLock(&rw_lock);
        }

        length = newlen;

        UpdateValidData();
    }

    LOG(60, ("ChunkedCacheFile::SetLength: New Length: %d, Validdata %d\n", newlen,
             validdata));
}

/* MUST be called from within transaction! */
void ChunkedCacheFile::SetValidData(uint64_t start, int64_t len)
{
    CODA_ASSERT(start <= length);

    // Negative length indicates we wanted (or got) to end of file
    if (len < 0)
        len = length - start;

    CODA_ASSERT(start + len <= length);

    // Empty received range implies we won't need to mark anything as cached
    if (len == 0) // this implicitly covers 'length == 0' as well
        return;

    // Skip partial blocks at the start or end
    uint64_t start_cb     = bytes_to_ccblocks_ceil(start);
    uint64_t end_cb       = bytes_to_ccblocks_floor(start + len);
    uint64_t length_cb    = bytes_to_ccblocks_ceil(length);
    uint64_t newvaliddata = 0;

    if (recoverable)
        RVMLIB_REC_OBJECT(validdata);

    ObtainWriteLock(&rw_lock);

    for (uint64_t i = start_cb; i < end_cb; i++) {
        if (cached_chunks->Value(i))
            continue;

        /* Account for a full block */
        cached_chunks->SetIndex(i);
        newvaliddata += CacheChunkBlockSize;
    }

    /* End of file? The last block is allowed to not be full */
    CODA_ASSERT(length_cb > 0);
    if (start + len == length && !cached_chunks->Value(length_cb - 1)) {
        uint64_t tail = length - ccblocks_to_bytes(length_cb - 1);

        /* Account for a last partial block */
        cached_chunks->SetIndex(length_cb - 1);
        newvaliddata += tail;
    }

    validdata += newvaliddata;

    LOG(60,
        ("CacheFile::SetValidData: { cachedblocks: %d, totalblocks: %d }\n",
            cached_chunks->Count(), length_cb));

    ReleaseWriteLock(&rw_lock);

    LOG(60, ("ChunkedCacheFile::SetValidData: { validdata: %d }\n", validdata));
}

uint64_t ChunkedCacheFile::ConsecutiveValidData()
{
    /* Find the start of the first hole */
    uint64_t index      = 0;
    uint64_t length_ccb = bytes_to_ccblocks_ceil(length);

    /* Find the first 0 in the bitmap */
    for (index = 0; index < length_ccb; index++)
        if (!cached_chunks->Value(index))
            break;

    /* In case we reached the last cache block */
    if (index == length_ccb)
        return length;

    return ccblocks_to_bytes(index);
}

int64_t ChunkedCacheFile::CopySegment(ChunkedCacheFile *from, ChunkedCacheFile *to, uint64_t pos,
                               int64_t count)
{
    uint32_t byte_start  = pos_align_to_ccblock(pos);
    uint32_t block_start = bytes_to_ccblocks(byte_start);
    uint32_t byte_len    = length_align_to_ccblock(pos, count);
    int tfd, ffd;
    struct stat tstat;
    CacheChunkList *c_list;
    CacheChunk chunk;

    LOG(300, ("ChunkedCacheFile::CopySegment: from %s [%d, %d], to %s\n", from->Name(),
              byte_start, byte_len, to->Name()));

    if (mkpath(to->Name(), V_MODE | 0100) < 0) {
        LOG(0,
            ("ChunkedCacheFile::CopySegment: could not make path for %s\n", to->Name()));
        return -1;
    }

    tfd = to->Open(O_RDWR | O_CREAT);

    ::fchmod(tfd, V_MODE);

#ifdef __CYGWIN32__
    ::chown(name, (uid_t)V_UID, (gid_t)V_GID);
#else
    ::fchown(tfd, (uid_t)V_UID, (gid_t)V_GID);
#endif

    ffd = from->Open(O_RDONLY);

    /* Skip the holes */
    c_list = from->GetValidChunks(byte_start, byte_len);

    for (chunk = c_list->pop(); chunk.isValid(); chunk = c_list->pop()) {
        if (copyfile_seg(ffd, tfd, chunk.GetStart(), chunk.GetLength()) < 0) {
            LOG(0, ("ChunkedCacheFile::CopySegment failed! (%d)\n", errno));
            from->Close(ffd);
            to->Close(tfd);
            return -1;
        }
    }

    delete c_list;

    if (from->Close(ffd) < 0) {
        CHOKE("ChunkedCacheFile::CopySegment: source close failed (%d)\n", errno);
    }

    if (::fstat(tfd, &tstat) < 0) {
        CHOKE("ChunkedCacheFile::CopySegment: fstat failed (%d)\n", errno);
    }

    if (to->Close(tfd) < 0) {
        CHOKE("ChunkedCacheFile::CopySegment: close failed (%d)\n", errno);
    }

    CODA_ASSERT((off_t)from->Length() == tstat.st_size);

    ObtainDualLock(&from->rw_lock, READ_LOCK, &to->rw_lock, WRITE_LOCK);

    from->cached_chunks->CopyRange(block_start, bytes_to_ccblocks(byte_len),
                                   *(to->cached_chunks));

    ReleaseDualLock(&from->rw_lock, READ_LOCK, &to->rw_lock, WRITE_LOCK);

    to->UpdateValidData();

    return byte_len;
}

CacheChunk ChunkedCacheFile::GetNextHole(uint64_t start_b, uint64_t end_b)
{
    CODA_ASSERT(start_b <= end_b);
    /* Number of blocks of the cache file */
    uint64_t nblocks = bytes_to_ccblocks_ceil(length);
    /* Number of full blocks */
    uint64_t nblocks_full   = bytes_to_ccblocks_floor(length);
    uint64_t hole_start_idx = 0;
    uint64_t hole_end_idx   = 0;
    uint64_t hole_size      = 0;

    CODA_ASSERT(start_b <= nblocks);

    if (end_b > nblocks) {
        end_b = nblocks;
    }

    /* Find the start of the hole */
    for (hole_start_idx = start_b; hole_start_idx < end_b; hole_start_idx++) {
        if (!cached_chunks->Value(hole_start_idx)) {
            break;
        }
    }

    /* No hole */
    if (hole_start_idx == end_b)
        return CacheChunk();

    /* Find the end of the hole */
    for (hole_end_idx = hole_start_idx; hole_end_idx < end_b; hole_end_idx++) {
        if (cached_chunks->Value(hole_end_idx)) {
            break;
        }
    }

    CODA_ASSERT(hole_end_idx > hole_start_idx);

    hole_size = ccblocks_to_bytes(hole_end_idx - hole_start_idx - 1);

    /* If the hole ends at EOF and it has a tail */
    if (hole_end_idx == nblocks && nblocks_full != nblocks) {
        /* Add the tail */
        uint64_t tail = length - ccblocks_to_bytes(nblocks - 1);
        hole_size += tail;
    } else {
        /* Add the last accounted block as a whole block*/
        hole_size += CacheChunkBlockSize;
    }

    return (CacheChunk(ccblocks_to_bytes(hole_start_idx), hole_size));
}

CacheChunkList *ChunkedCacheFile::GetHoles(uint64_t start, int64_t len)
{
    uint64_t start_b  = ccblock_start(start);
    uint64_t end_b    = ccblock_end(start, len);
    uint64_t length_b = bytes_to_ccblocks_ceil(length); // Ceil length in blocks
    CacheChunkList *clist = new CacheChunkList();
    CacheChunk currc;

    if (len < 0 || end_b > length_b) {
        end_b = length_b;
    }

    LOG(100, ("ChunkedCacheFile::GetHoles Range [%lu - %lu]\n",
              ccblocks_to_bytes(start_b), ccblocks_to_bytes(end_b) - 1));

    for (uint64_t i = start_b; i < end_b; i++) {
        currc = GetNextHole(i, end_b);

        if (!currc.isValid())
            break;

        LOG(100, ("ChunkedCacheFile::GetHoles Found [%d, %d]\n", currc.GetStart(),
                  currc.GetLength()));

        clist->AddChunk(currc.GetStart(), currc.GetLength());
        i = bytes_to_ccblocks(currc.GetStart() + currc.GetLength() - 1);
    }

    return clist;
}

CacheChunkList *ChunkedCacheFile::GetValidChunks(uint64_t start, int64_t len)
{
    uint64_t start_b     = ccblock_start(start);
    uint64_t start_bytes = 0;
    uint64_t end_b       = ccblock_end(start, len);
    uint64_t length_b = bytes_to_ccblocks_ceil(length); // Ceil length in blocks
    CacheChunkList *clist = new CacheChunkList();
    CacheChunk currc;
    uint64_t i = start_b;

    if (len < 0) {
        end_b = length_b;
    }

    LOG(100, ("ChunkedCacheFile::GetValidChunks Range [%lu - %lu]\n",
              ccblocks_to_bytes(start_b), ccblocks_to_bytes(end_b) - 1));

    for (i = start_b; i < end_b; i++) {
        currc = GetNextHole(i, end_b);

        if (!currc.isValid())
            break;

        LOG(100, ("ChunkedCacheFile::GetValidChunks Found [%d, %d]\n",
                  currc.GetStart(), currc.GetLength()));

        start_bytes = ccblocks_to_bytes(i);

        if (start_bytes != currc.GetStart()) {
            clist->AddChunk(start_bytes, currc.GetStart() - start_bytes + 1);
        }

        i = bytes_to_ccblocks(currc.GetStart() + currc.GetLength() - 1);
    }

    /* Is case de range ends with valid data */
    if (i < end_b) {
        start_bytes = ccblocks_to_bytes(i);
        clist->AddChunk(i, end_b - i + 1);
    }

    return clist;
}

CacheChunkList::CacheChunkList()
{
    Lock_Init(&rd_wr_lock);
}

CacheChunkList::~CacheChunkList()
{
    CacheChunk *curr = NULL;

    while ((curr = (CacheChunk *)this->first())) {
        this->remove((dlink *)curr);
    }
}

void CacheChunkList::AddChunk(uint64_t start, int64_t len)
{
    WriteLock();
    CacheChunk *new_chunk = new CacheChunk(start, len);
    this->insert((dlink *)new_chunk);
    WriteUnlock();
}

void CacheChunkList::ReadLock()
{
    ObtainReadLock(&rd_wr_lock);
}

void CacheChunkList::WriteLock()
{
    ObtainWriteLock(&rd_wr_lock);
}

void CacheChunkList::ReadUnlock()
{
    ReleaseReadLock(&rd_wr_lock);
}

void CacheChunkList::WriteUnlock()
{
    ReleaseWriteLock(&rd_wr_lock);
}

bool CacheChunkList::ReverseCheck(uint64_t start, int64_t len)
{
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    ReadLock();

    dlist_iterator previous(*this, DlDescending);

    while ((curr = previous())) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid())
            continue;

        if (start != curr_cc->GetStart())
            continue;

        if (len != curr_cc->GetLength())
            continue;

        ReadUnlock();

        return true;
    }

    ReadUnlock();

    return false;
}

void CacheChunkList::ReverseRemove(uint64_t start, int64_t len)
{
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    WriteLock();

    dlist_iterator previous(*this, DlDescending);

    while ((curr = previous())) {
        curr_cc = (CacheChunk *)curr;

        if (!curr_cc->isValid())
            continue;

        if (start != curr_cc->GetStart())
            continue;

        if (len != curr_cc->GetLength())
            continue;

        this->remove(curr);
        break;
    }

    WriteUnlock();
}

void CacheChunkList::ForEach(void (*foreachcb)(uint64_t start, int64_t len,
                                               void *usr_data_cb),
                             void *usr_data)
{
    dlink *curr         = NULL;
    CacheChunk *curr_cc = NULL;

    if (!foreachcb)
        return;

    ReadLock();

    dlist_iterator next(*this);

    while ((curr = next())) {
        curr_cc = (CacheChunk *)curr;
        foreachcb(curr_cc->GetStart(), curr_cc->GetLength(), usr_data);
    }

    ReadUnlock();
}

CacheChunk CacheChunkList::pop()
{
    dlink *curr_first = NULL;
    CacheChunk *tmp   = NULL;

    WriteLock();

    curr_first = this->first();
    tmp        = (CacheChunk *)curr_first;

    if (!curr_first) {
        WriteUnlock();
        return CacheChunk();
    }

    CacheChunk cp = CacheChunk(*tmp);
    this->remove(curr_first);
    delete tmp;

    WriteUnlock();

    return CacheChunk(cp);
}

/* MUST be called from within transaction! */
CacheFileDiscarterDecorator::CacheFileDiscarterDecorator(int i)
    : ChunkedCacheFile(i, 0)
{
    sprintf(name, "%02X/%02X/%02X/%02X.seg", (i >> 24) & 0xff, (i >> 16) & 0xff,
            (i >> 8) & 0xff, i & 0xff);
}

CacheFileDiscarterDecorator::~CacheFileDiscarterDecorator()
{
    this->Truncate(0);
    DecRef();
}

void CacheFileDiscarterDecorator::SetComponent(ChunkedCacheFile *cf)
{
    CacheFile::Create(cf->Length());
    this->cf = cf;
}

int64_t CacheFileDiscarterDecorator::BackupSegment(uint64_t pos, int64_t count)
{
    return CopySegment(cf, this, pos, count);
}

int64_t CacheFileDiscarterDecorator::RestoreSegment(uint64_t pos, int64_t count)
{
    return CopySegment(this, cf, pos, count);
}

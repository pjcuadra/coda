/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
 * ohash.h -- Specification of hash-table type where each bucket is a singly-linked
 * list (an olist).
 *
 */

#ifndef _UTIL_HTAB_H_
#define _UTIL_HTAB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "olist.h"

class ohashtab;
class ohashtab_iterator;

class ohashtab {
    friend class ohashtab_iterator;
    int sz; // size of the array
    olist *a; // array of olists
    intptr_t (*hfn)(void *); // the hash function
    int cnt;

public:
    ohashtab(int size, intptr_t (*hashfn)(void *));
    ohashtab(ohashtab &); // not supported!
    int operator=(ohashtab &); // not supported!
    virtual ~ohashtab();
    void insert(void *, olink *); // add at head of list
    void append(void *, olink *); // add at tail of list
    olink *remove(void *, olink *); // remove specified entry
    olink *first(); // return first element of table
    olink *last(); // return last element of table
    olink *get(void *); // return and remove head of list
    void clear(); // remove all entries
    int count();
    int IsMember(void *, olink *);
    int bucket(void *); // returns bucket number of key
    virtual void print();
    virtual void print(FILE *);
    virtual void print(int);

    /* find object with matching tag in a hash table;
       return NULL if no such object; key and tag are separate because
       hash buckets may use different field from tag (Satya, May04) */
    olink *FindObject(void *key, void *tag, otagcompare_t cmpfn);
};

class ohashtab_iterator {
    ohashtab *chashtab; // current ohashtab
    int allbuckets; // iterate over all or single bucket
    int cbucket; // current bucket
    olist_iterator *nextlink; // current olist iterator
public:
    ohashtab_iterator(ohashtab &, void * = (void *)-1);
    ~ohashtab_iterator();
    olink *operator()(); // return next object or 0
};

#endif /* _UTIL_HTAB_H_ */

/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODACONFPARSER_H_
#define _CODACONFPARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include "coda_config.h"

#ifdef __cplusplus
}
#endif

#include "stringkeyvaluestore.h"

class CodaConfParser {
protected:
    StringKeyValueStore &store;

public:
    CodaConfParser(StringKeyValueStore &s)
        : store(s)
    {
    }

    int parse() {}
};

#endif /* _CODACONFPARSER_H_ */
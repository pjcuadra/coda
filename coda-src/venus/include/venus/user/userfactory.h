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
 * Specification of the Venus User abstraction.
 *
 */

#ifndef _VENUS_USER_FACTORY_H_
#define _VENUS_USER_FACTORY_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/* from venus */
#include <venus/user/user.h>

class UserFactory : private userent {
    static userent *prototype;

public:
    UserFactory()
        : userent(0, 0)
    {
    }
    static userent *create(RealmId realmid, uid_t userid)
    {
        if (prototype)
            return prototype->instanceOf(realmid, userid);
        return UserFactory().create(realmid, userid);
    }

    static void setPrototype(userent *proto) { prototype = proto; }

    static void setDefaultPrototype() { prototype = NULL; }
};

#endif /* _VENUS_USER_FACTORY_H_ */

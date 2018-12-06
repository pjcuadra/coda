/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _SUBSYSTEM_H_
#define _SUBSYSTEM_H_

#include <dlist.h>
#include <errno.h>

class SubsystemManager;

class Subsystem : private dlink {
    friend class SubsystemManager;
private:
    const char * name;
protected:
    bool initialized;
    Subsystem(const char * name) : name(name), initialized(false) {} /* Make class abstract */

    virtual int init() {}
    virtual int uninit() {}

public:
    bool _isInitialized() {return initialized;}

};

class SubsystemManager {
private:
    dlist uninitialized_subsystems;
    dlist initialized_subsystems;
    bool initialized;
    SubsystemManager();

    void setup();

    void teardown();

public:

    static SubsystemManager * GetInstance();

    static int InitializeSubsystems();

    static int RegisterSubsystem(Subsystem * sub);

    static int UninitializeSubsystems();

    static void KillInstance();
};

#endif /* _SUBSYSTEM_H_ */

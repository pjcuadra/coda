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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "subsystem.h"

SubsystemManager::SubsystemManager(): initialized(true) {
    uninitialized_subsystems = dlist();
    initialized_subsystems = dlist();
}; 

void SubsystemManager::setup() {
    if (initialized) {
        return;
    }

    /* Init registry */
}

void SubsystemManager::teardown() {
    if (!initialized) {
        return;
    }

    /* Free registry */
}

SubsystemManager * SubsystemManager::GetInstance() {
    static SubsystemManager * man = new SubsystemManager();

    return man;
}

int SubsystemManager::InitializeSubsystems() {
    SubsystemManager * man = SubsystemManager::GetInstance();
    int list_len = 0;
    Subsystem * sub;
    int ret = 0;
    /* Initialize all subsystems */
    if (!man->initialized) {
        return ENODEV;
    }

    list_len = man->uninitialized_subsystems.count();

    printf("Subsystem 0x%x - OK\n", man);

    for (int i = 0;  i < list_len; i++) {
        sub = (Subsystem *) man->uninitialized_subsystems.last();

        printf("Initializing Subsystem %s\n", sub->name);

        ret = sub->init();
        if (ret) {
            break;
        }

        printf("Subsystem %s - OK\n", sub->name);

        man->uninitialized_subsystems.remove((Subsystem *) sub);
        man->initialized_subsystems.insert((Subsystem *) sub);

    }

    printf("Subsystem OK\n");

    return ret;

}

int SubsystemManager::RegisterSubsystem(Subsystem * sub) {
    SubsystemManager * man = SubsystemManager::GetInstance();
    if (!man->initialized) {
        return ENODEV;
    }

    man->uninitialized_subsystems.insert((dlink*)sub);
}

int SubsystemManager::UninitializeSubsystems() {
    SubsystemManager * man = SubsystemManager::GetInstance();
    int list_len = 0;
    Subsystem * sub;
    int ret = 0;
    /* Uninitialize all subsystems */
    if (!man->initialized) {
        return ENODEV;
    }

    /* Just uninitialize the ones that were successfully initialized */
    list_len = man->initialized_subsystems.count();

    for (int i = 0;  i < list_len; i++) {
        sub = (Subsystem *) man->initialized_subsystems.last();

        ret = sub->uninit();
        if (ret) {
            break;
        }

        man->initialized_subsystems.remove((dlink *) sub);

    }

    return ret;
}

void SubsystemManager::KillInstance() {
    SubsystemManager * man = SubsystemManager::GetInstance();
    /* Uninit all registered subsystems */
    man->UninitializeSubsystems();

    /* Free registry*/
    man->teardown();
}
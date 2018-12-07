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
 *    Generic associative data structure.
 */

#ifndef _DAEMON_SUBSYSTEM_H_
#define _DAEMON_SUBSYSTEM_H_ 1


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

void DaemonSetup();
void DaemonInit();

#endif /* _DAEMON_SUBSYSTEM_H_ */

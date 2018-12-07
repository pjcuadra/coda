/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * Venus process abstraction subsystem API
 */

#ifndef _VENUS_PROC_SUBSYSTEM_H_
#define _VENUS_PROC_SUBSYSTEM_H_ 1


#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
}
#endif

void VprocInit();
void VprocSetup();

#endif /* _VENUS_PROC_SUBSYSTEM_H_ */

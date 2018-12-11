/* BLURB gpl

                           Coda File System
                              Release 7

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
 * Venus Callback MUX API
 */

#ifndef _VENUS_MUX_H_
#define _VENUS_MUX_H_ 1


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/select.h>

#ifdef __cplusplus
}
#endif

int _MUX_FD_SET(fd_set *fds);

/* Dispatch callbacks for file descriptors in the fd_set */
void _MUX_Dispatch(fd_set *fds);

/* Helper to add a file descriptor with callback to main select loop.
 *
 * Call with cb == NULL to remove existing callback.
 * cb is called with fd == -1 when an existing callback is removed or updated.
 */
void MUX_add_callback(int fd, void (*cb)(int fd, void *udata), void *udata);



#endif /* _VENUS_MUX_H_ */

#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /home/braam/src/coda-src/res/RCS/rescomm.private.h,v 1.1 1996/11/22 19:12:44 braam Exp braam $";
#endif /*_BLURB_*/







/* rescomm.private.h
 * Created Puneet Kumar, June 1990
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <cthreads.h>
#else
#include <dummy_cthreads.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

extern void ResProcWait(char *);
extern void ResProcSignal(char *, int = 0);

#define	CONDITION_INIT(c)   
#define	CONDITION_WAIT(c, m)	ResProcWait((char *)(c))
#define	CONDITION_SIGNAL(c)	ResProcSignal((char *)(c))

#define TRANSLATE_TO_LOWER(s)\
{\
    for (char *c = s; *c; c++)\
	if (isupper(*c)) *c = tolower(*c);\
}

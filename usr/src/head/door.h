/*	Copyright (c) 1995,1996,1997 by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_DOOR_H
#define	_DOOR_H

#pragma ident	"@(#)door.h	1.1	95/12/11 SMI"

#include <sys/types.h>
#include <sys/door.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

/*
 * Doors API
 */
int	door_create(void (*)(), void *, u_int);
int	door_revoke(int);
int	door_info(int, door_info_t *);
int	door_call(int, door_arg_t *);
int	door_return(char *, size_t, door_desc_t *, size_t);
int	door_cred(door_cred_t *);
int	door_bind(int);
int	door_unbind(void);
void	(*door_server_create(void (*)())) (void (*)());

#endif /* _ASM */

#ifdef __cplusplus
}
#endif

#endif	/* _DOOR_H */

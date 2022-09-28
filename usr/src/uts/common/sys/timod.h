/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TIMOD_H
#define	_SYS_TIMOD_H

#pragma ident	"@(#)timod.h	1.25	96/10/17 SMI"	/* SVr4.0 11.4 */

#include <sys/stream.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Timod ioctls */
#define		TIMOD 		('T'<<8)
#define		TI_GETINFO	(TIMOD|140)
#define		TI_OPTMGMT	(TIMOD|141)
#define		TI_BIND		(TIMOD|142)
#define		TI_UNBIND	(TIMOD|143)
#define		TI_GETMYNAME	(TIMOD|144)
#define		TI_GETPEERNAME	(TIMOD|145)
#define		TI_SETMYNAME	(TIMOD|146)
#define		TI_SETPEERNAME	(TIMOD|147)
#define		TI_SYNC		(TIMOD|148)
#define		TI_GETADDRS	(TIMOD|149)

/* returned by TI_SYNC  */
struct ti_sync_ack {
	/* initial part derived from and matches T_info_ack */
	int32_t	PRIM_type;
	int32_t	TSDU_size;
	int32_t	ETSDU_size;
	int32_t	CDATA_size;
	int32_t	DDATA_size;
	int32_t	ADDR_size;
	int32_t	OPT_size;
	int32_t TIDU_size;
	int32_t SERV_type;
	int32_t CURRENT_state;
	int32_t PROVIDER_flag;

	uint32_t qlen;
	/* can grow at the end */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIMOD_H */

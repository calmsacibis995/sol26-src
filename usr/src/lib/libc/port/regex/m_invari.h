/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 *
 * This code is MKS code ported to Solaris with minimum modifications so that
 * upgrades from MKS will readily integrate. Avoid modification if possible!
 */
#ident	"@(#)m_invari.h 1.2	96/07/31 SMI"

/*
 * Copyright 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 * 
 * $Header: /u/rd/h/RCS/m_invari.h,v 1.10 1994/01/24 18:18:07 mark Exp $
 */

/*
 * m_invari.h: 
 *    Configuration and definitions for support of on systems (e.g EBCDIC)
 *    where the POSIX.2 portable characters are not invariant
 */

/*
 * This file has been updated to remove all MKS specific code or setttings.
 * As all regex funcitons of MKS are removed, we are no longer in need of
 * the whole header file.
 */


#ifndef __M_M_INVARI_H__
#define	__M_M_INVARI_H__

/* Normal system */
#define	M_INVARIANTINIT()	/* NULL */
#define	M_INVARIANT(c)		(c)
#define	M_UNVARIANT(c)		(c)
#define	M_UNVARIANTSTR(s)	(s)
#define	M_WUNVARIANTSTR(ws)	(ws)

#endif /*__M_M_INVARI_H__*/

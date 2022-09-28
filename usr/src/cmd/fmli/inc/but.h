/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright  (c) 1985 AT&T
 *	All Rights Reserved
 */

#ident	"@(#)but.h	1.4	92/07/14 SMI"       /* SVr4.0 1.1 */

#include	"mio.h"

#define ANY	99	/* same as in io.h, but don't want to include it all */

#define RESTART	(LBUT)
#define DONE	(LBUT-1)
#define MORE	(LBUT-2)
#define PRE	(LBUT-3)
#define BUT1	(FBUT)
#define BUT2	(FBUT+1)
#define BUT3	(FBUT+2)
#define BUT4	(FBUT+3)
#define BUT5	(FBUT+4)
#define BUT6	(FBUT+5)
#define BUT7	(FBUT+6)
#define BUT1R	(LBUT-6)
#define BUT2R	(LBUT-5)
#define BUT3R	(LBUT-4)
#define BUT4R	(LBUT-3)
#define BUT5R	(LBUT-2)
#define BUT6R	(LBUT-1)
#define BUT7R	(LBUT)

#define LABLEN_1	(11)
#define LABLEN_2	(14)

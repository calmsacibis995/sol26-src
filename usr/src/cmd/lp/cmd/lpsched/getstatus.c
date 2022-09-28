/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getstatus.c	1.26	96/04/10 SMI"	/* SVr4.0 1.9	*/

#include "stdlib.h"
#include "unistd.h"
#include "stdarg.h"

#include "lpsched.h"

int			Redispatch	= 0;

RSTATUS *		Status_List	= 0;

static SUSPENDED	*Suspend_List	= 0;

#define SHOULD_NOTIFY(PRS) \
	( \
		(PRS)->request->actions & (ACT_MAIL|ACT_WRITE|ACT_NOTIFY)\
	     || (PRS)->request->alert \
	)

/**
 ** mesgdup()
 **/

static char *
mesgdup(char *m)
{
	char *			p;
	unsigned long		size	= msize(m);

	p = Malloc(size);
	memcpy (p, m, size);
	return (p);
}

/**
 ** update_req()
 **/

void
update_req(char *req_id, long rank)
{
	RSTATUS		*prs;

	if (!(prs = request_by_id(req_id)))
		return;
	
	prs->status |= RSS_RANK;
	prs->rank = rank;

	return;
}

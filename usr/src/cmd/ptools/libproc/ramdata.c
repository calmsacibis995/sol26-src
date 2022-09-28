/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ramdata.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include "pcontrol.h"
#include "ramdata.h"

/* ramdata.c -- read/write data definitions for process management */

char	*command = NULL;	/* name of command ("truss") */
int	interrupt = FALSE;	/* interrupt signal was received */

int	Fflag = FALSE;		/* option flags from getopt() */
int	qflag = FALSE;

char	*str_buffer = NULL;	/* fetchstring() buffer */
unsigned str_bsize = 0;		/* sizeof(*str_buffer) */

process_t	Proc;		/* the process structure */
process_t	*PR = NULL;	/* pointer to same (for abend()) */

char	*procdir = "/proc";	/* default PROC directory */

int	timeout = FALSE;	/* set TRUE by SIGALRM catchers */

int	debugflag = FALSE;	/* enable debugging printfs */

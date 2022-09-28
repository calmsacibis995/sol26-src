/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)exitto.c	1.3	96/03/13 SMI"

void
exitto(void *entrypoint, void *fw_ptr)
{
	(*(void (*)(void *))entrypoint)(fw_ptr);
	/*NOTREACHED*/
}

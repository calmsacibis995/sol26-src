/*
 * Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)initsrc.c	1.3	95/10/14 SMI"

/* initial source executed at startup */

unsigned char config_source[] =
	"source /platform/i86pc/boot/solaris/boot.rc\n";

unsigned char boot_source[] =
	"source /etc/bootrc\n";

#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)chexec.sh	1.2	89/12/04 SMI"	/* SVr4.0 1.3	*/

if [ -z "$1" ]
then
	exit 1
elif [ -x "$1" ]
then
	if [ -f "$1" ]
	then
		exit 0
	fi
else 	
	cmd="$1"
	not_found=`type "$cmd" | grep "not found"`
	if [ -z "$not_found" ]
	then
		exit 0
	fi
fi
exit 1

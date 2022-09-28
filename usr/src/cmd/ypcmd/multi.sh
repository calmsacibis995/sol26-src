#!/bin/sh
#
#ident	"@(#)multi.sh	1.4	96/04/25 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# Script to examine hosts file and make "magic" entries for
# those hosts that have multiple IP addresses.
#
#

MAKEDBM=/usr/sbin/makedbm
STDHOSTS=/usr/lib/netsvc/yp/stdhosts
MULTIAWK=/usr/lib/netsvc/yp/multi.awk

USAGE="Usage: $0 [-b] [-l] [-s] [hosts file]
Where:
	-b	Add YP_INTERDOMAIN flag to hosts map
	-l	Convert keys to lower case before creating map
	-s	Add YP_SECURE flag to hosts map

	hosts file defaults to /etc/hosts"

while getopts bls c
do
    case $c in
	b)	BFLAG=-b;;
	l)	LFLAG=-l;;
	s)	SFLAG=-s;;
	\?)	echo "$USAGE"
		exit 2;;
    esac
done

shift `expr $OPTIND - 1`

if [ "$1" ]
then
    HOSTS=$1
else
    HOSTS=/etc/hosts
fi

if [ "$HOSTS" = "-" ]
then
    unset HOSTS
fi

cd /var/yp/`domainname` && \
    sed -e '/^[ 	]*$/d' -e '/^#/d' -e 's/#.*$//' $HOSTS | \
    $STDHOSTS | \
    $MULTIAWK - | \
    $MAKEDBM $BFLAG $LFLAG $SFLAG - hosts.byname

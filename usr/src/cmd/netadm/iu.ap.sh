#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)iu.ap.sh	1.20	96/09/11 SMI"	/* SVr4.0 1.3	*/

case "$MACH" in
  "u3b2" )
	echo "# /dev/console and /dev/contty autopush setup
#
# major	minor	lastminor	modules

    0	  -1	    0		ldterm
" >iu.ap
	;;
  "i386" )
	echo "# /dev/console and /dev/contty autopush setup
#
#       major minor   lastminor       modules

	chanmux	0	255	char ansi emap ldterm ttcompat
	asy	-1	0	ldterm ttcompat
	rts	-1	0	rts
" > iu.ap
	;;
  "sparc" )
	echo "# /dev/console and /dev/contty autopush setup
#
#      major   minor lastminor	modules

	wc	0	0	ldterm ttcompat
	zs	0	1	ldterm ttcompat
	zs	131072	131073	ldterm ttcompat
	ptsl	0	47	ldterm ttcompat
	mcpzsa	0	127	ldterm ttcompat
	mcpzsa	256	383	ldterm ttcompat
	stc	0	255	ldterm ttcompat
	se	0	1	ldterm ttcompat
	se	131072	131073	ldterm ttcompat
	su	0	1	ldterm ttcompat
	rts	-1	0	rts
" >iu.ap
	;;
  "ppc" )
	echo "# /dev/console and /dev/contty autopush setup
#
#       major minor   lastminor       modules

	wc	0	0	ldterm ttcompat
	asy	-1	0	ldterm ttcompat
	rts	-1	0	rts
" > iu.ap
	;;
  * )
	echo "Unknown architecture."
	exit 1
	;;
esac

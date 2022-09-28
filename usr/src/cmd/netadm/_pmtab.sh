#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)_pmtab.sh	1.8	96/08/12 SMI"	/* SVr4.0 1.2	*/

case "$MACH" in
  "u3b2"|"sparc"|"ppc" )
	echo "# VERSION=1
ttya:u:root:reserved:reserved:reserved:/dev/term/a:I::/usr/bin/login::9600:ldterm,ttcompat:ttya login\: ::tvi925:y:# 
ttyb:u:root:reserved:reserved:reserved:/dev/term/b:I::/usr/bin/login::9600:ldterm,ttcompat:ttyb login\: ::tvi925:y:# " > _pmtab
	;;
  "i386" )
	echo "# VERSION=1
ttya:u:root:reserved:reserved:reserved:/dev/term/a:I::/usr/bin/login::9600:ldterm,ttcompat:ttya login\: ::tvi925:y:# 
ttyb:u:root:reserved:reserved:reserved:/dev/term/b:I::/usr/bin/login::9600:ldterm,ttcompat:ttyb login\: ::tvi925:y:#

#
# Uncomment the following line to get a getty to run on VT01.  The second line
# is for VT02.  Lines up to VT12 can be added.
#
#vt01:u:root:reserved:reserved:reserved:/dev/vt01:::/usr/bin/login::console::VT01 login\: ::AT386:y:# 
#vt02:u:root:reserved:reserved:reserved:/dev/vt02:::/usr/bin/login::console::VT02 login\: ::AT386:y:#
	" > _pmtab
	;;
  * )
	echo "Unknown architecture."
	exit 1
	;;
esac

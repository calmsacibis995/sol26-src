/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)Text.h	1.2	92/07/14 SMI"	/* SVr4.0 1.6	*/

title=HELP on $ARG1
lifetime=shortterm

init="$RETVAL"

framemsg="Press the CANCEL function key to cancel."

`test -r $VMSYS/HELP/$ARG2 && set -l RETVAL=true || set -l RETVAL=false; 
 regex -e -v "$RETVAL" 
	'^false$' '`message "No HELP text is available for this item."`'`

text="`readfile $VMSYS/HELP/$ARG2`"
columns=`longline | set -l LL;
if [ "${LL}" -gt "${DISPLAYW}" ];
then
	echo ${DISPLAYW};
else
	echo ${LL};
fi`

name=""
button=1
action=nop

name="CONTENTS"
button=8
action=OPEN MENU OBJECTS/Menu.h0.toc

#
# ident	"@(#)Makefile	1.10	96/03/11 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/printers/Makefile
#

LIBRARY	=	liblpprt.a

OBJECTS =	chkprinter.o	\
		default.o	\
		delprinter.o	\
		freeprinter.o	\
		getprinter.o	\
		getpentry.o	\
		p_head.o	\
		okprinter.o	\
		printwheels.o	\
		putprinter.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp


CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:
# for messaging catalog
#
POFILE= printers.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS +=-a -x $(LPROOT)/lp.xcl
 
catalog:        $(POFILE)
  
$(POFILE):      $(SRCS)
	  $(RM)		$@
	  $(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	  $(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	  sed "/^domain/d"        messages.po     > $@
	  $(RM) messages.po $(POFILE).i


all install :		$(LIBS)

strip lint :

include		../../../../lib/Makefile.targ

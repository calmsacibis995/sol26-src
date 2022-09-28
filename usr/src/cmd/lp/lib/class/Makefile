#
# ident	"@(#)Makefile	1.10	96/01/22 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/class/Makefile
#

LIBRARY	=	liblpcls.a

OBJECTS =	delclass.o	\
		freeclass.o	\
		getclass.o	\
		putclass.o


include		../../../../lib/Makefile.lib
include		../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:
# for messaging catalog
#
POFILE= class.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS += -a -x $(LPROOT)/lp.xcl
 
catalog:        $(POFILE)
  
$(POFILE):      $(SRCS)
	  $(RM)	$@
	  $(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	  $(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	  sed "/^domain/d"        messages.po     > $@
	  $(RM) messages.po $(POFILE).i

all install :	$(LIBS)

strip lint :

include		../../../../lib/Makefile.targ

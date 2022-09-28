#
#ident	"@(#)Makefile	1.3	96/09/10 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

include		../../../Makefile.master

SUBDIRS=	$(MACH) rdb_demo
 
all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: \
		$(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

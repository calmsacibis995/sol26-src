#
#ident	"@(#)Makefile	1.30	95/10/10 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

# build two versions, dynamic and static
PROG= su
STATPROG= su

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

DFLTD= $(ROOTETC)/default

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)

DLLIB = -ldl
LDLIBS_LIST = -lcrypt_i -lcmd -lbsm -lmp \
	-lnss_files -lnss_nis -lnss_nisplus -lnss_compat -lnss_dns \
	-lsocket -lnsl -lstraddr $(DLLIB)
LDLIBS += $(LDLIBS_LIST)
#only set /usr/bin/su to 4555
FILEMODE= 04555
$(ROOTSBINPROG) := FILEMODE = 0555
OWNER= root
GROUP= sys
CLOBBERFILES += $(STATIC)

# conditional assignments for the static version
#
# A partially static version is actually created, with just libdl
# linked dynamically.
#

$(STATIC) := DLLIB =

$(STATIC) := LDLIBS = -Bstatic $(LDLIBS_LIST) -lgen -lc -Bdynamic -ldl -Bstatic

LDLIBS += -lpam

$(STATIC) := LDFLAGS = $(LDLIBS.cmd) -dy -R/etc/lib $(LDFLAGS.cmd)

LDFLAGS += -DDYNAMIC_SU

$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)

# install rule for the default file
$(DFLTD)/%: %.dfl
	$(INS.rename)

.KEEP_STATE:

all: $(PROG) $(STATIC) $(PROG).dfl 

$(STATIC): $(PROG).c
	LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) $(PROG).c -o $@ `nm -hu $(ROOT)/etc/lib/nss_files.so.1 | sed 's/ */-u /' | egrep -v '\.'` $(LDLIBS)

install: all $(ROOTSBINPROG) $(ROOTPROG) $(ETCDFLTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ

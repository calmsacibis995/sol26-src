#
#ident	"@(#)Makefile.com	1.32	96/09/12 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/common/Makefile.com
#
GREP	=	egrep
WC	=	wc
TOPDIR	=	../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm
include $(TOPDIR)/psm/stand/lib/Makefile.lib

SYSDIR	=  	$(TOPDIR)/uts
BOOTDIR	= 	../../boot
COMDIR	=  	../../common
SPECDIR=	$(TOPDIR)/uts/common/cpr
ARCHDIR	= 	$(SYSDIR)/${ARCH}
MACHDIR	= 	$(SYSDIR)/$(MACH)
MMUDIR	=	$(SYSDIR)/$(MMU)
PROMLIBDIR=	$(TOPDIR)/psm/stand/lib/promif/$(ARCH_PROMDIR)
PROMLIB	=	$(PROMLIBDIR)/libprom.a
CPRLIB	=	libcpr.a

LINTLIBCPR =	llib-lcpr.ln
LINTFLAGS.lib =	-ysxmun

CPRLDLIB =	-L. -lcpr
PROMLDLIBS =	-L$(PROMLIBDIR) -lprom
LDLIBS +=	$(CPRLDLIB) $(PROMLDLIBS) $(PLATLDLIBS)
OBJSDIR=	.

PROG=		cprboot

CPRCOMLIBOBJ =	support.o strsubr.o
CPRSPECLIBOBJ = cpr_compress.o
CPRLIBOBJ =	$(CPRCOMLIBOBJ) $(CPRSPECLIBOBJ)

L_SRCS	=	$(CPRCOMLIBOBJ:%.o=$(COMDIR)/%.c)
L_SRCS +=	$(CPRSPECLIBOBJ:%.o=$(SPECDIR)/%.c)
L_COBJ	=	$(CPRBOOTOBJ:%.o=%.ln)
L_CEROBJ=	$(CPRBOOTEROBJ:%.o=%.ln)

CPPDEFS=	$(ARCHOPTS) -D$(ARCH) -D__$(ARCH) -D$(MACH) \
		-D__$(MACH) -D_KERNEL -D_MACHDEP -D__ELF
CPPINCS=	-I. -I${COMDIR} -I$(ARCHDIR) -I$(MMUDIR) -I$(MACHDIR) \
		-I$(MACHDIR)/$(ARCHVER)	\
		-I$(SYSDIR)/sun -I$(SYSDIR)/common -I$(TOPDIR)/head
CPPOPTS=  	$(CPPDEFS)
COPTS=		-v -O $(CPPINCS)
CFLAGS=		${COPTS} ${CPPOPTS}
ASFLAGS= 	$(EXTRA_ASM_FLAG) -P -D_ASM $(CPPOPTS) -DLOCORE -D_LOCORE -D__STDC__
CPPFLAGS=	$(CPPINCS) $(CCYFLAG)$(SYSDIR)/common $(CPPFLAGS.master)
AS_CPPFLAGS=	$(CPPINCS) $(CPPFLAGS.master)

CPRBOOT=	cprboot $(EXTRA_TARGET)

# install values
CPRFILES=	$(CPRBOOT:%=$(ROOT_PSM_DIR)/$(ARCH)/%)
FILEMODE=	644
OWNER=		root
GROUP=		sys

# lint stuff
LINTFLAGS += -Dlint
LOPTS = -hbxn

# install rule
$(ROOT_PSM_DIR)/$(ARCH)/%: %
	$(INS.file)

ALL=		cprboot $(EXTRA_TARGET)
all:	$(ALL)

install: all $(CPRFILES)


LINT.c=	$(LINT) $(LINTFLAGS.c) $(LINT_DEFS) $(CFLAGS) -c
LINT.s=	$(LINT) $(LINTFLAGS.s) $(LINT_DEFS) $(CFLAGS) -c

# build rule

$(OBJSDIR)/%.o: $(SPECDIR)/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: $(COMDIR)/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ./%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ../common/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.ln: ./%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)

$(OBJSDIR)/%.ln: ../common/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)

$(OBJSDIR)/%.ln: ./%.s
	@$(LHEAD) $(LINT.s) $< $(LTAIL)

.KEEP_STATE:

cprboot: ucpr.o $(MAPFILE) 
	${LD} -dn -M $(MAPFILE) $(MAP_FLAG) -o $@ ucpr.o

cprbooter: ucper.o $(EXTRA_MAPFILE) 
	${LD} -dn -M $(EXTRA_MAPFILE) $(MAP_FLAG) -o $@ ucper.o

ucpr.o: $(CPRBOOTOBJ) $(CPRLIB) $(PROMLIB) $(PLATLIB)
	${LD} -r -o $@ $(CPRBOOTOBJ) ${OBJ} $(LDLIBS)

ucper.o: $(CPRBOOTEROBJ) $(CPRLIB) $(PROMLIB) $(PLATLIB)
	${LD} -r -o $@ $(CPRBOOTEROBJ) ${OBJ} $(LDLIBS)

$(CPRLIB): $(CPRLIBOBJ)
	$(AR) $(ARFLAGS) $@ $(CPRLIBOBJ)

$(ROOTDIR):
	$(INS.dir)

lint: $(PROG)_lint $(EXTRA_LINT)

$(PROG)_lint: $(L_COBJ) $(LINTLIBCPR)
	@$(ECHO) "\nperforming global crosschecks: $@"
	@$(LINT.2) $(L_OBJS) $(L_COBJ) $(LDLIBS)

$(EXTRA_LINT): $(L_CEROBJ) $(LINTLIBCPR)
	@$(ECHO) "\nperforming global crosschecks: $@"
	@$(LINT.2) $(L_OBJS) $(L_CEROBJ) $(LDLIBS)

$(LINTLIBCPR): $(L_SRCS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LHEAD) $(LINT.lib) $(CPPOPTS) -o cpr $(L_SRCS) $(LTAIL)

clean:
	$(RM) $(OBJSDIR)/*.o $(OBJSDIR)/*.ln $(OBJSDIR)/*.a

clobber: clean
	$(RM) $(CPRBOOT)

FRC:

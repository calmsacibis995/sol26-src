/*
 * Copyright (c) 1992, 1993, 1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)codes.c	1.30	96/06/22 SMI"	/* SVr4.0 1.14	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include "pcontrol.h"
#include "ioc.h"

#include <ctype.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/fstyp.h>
#ifdef i386
#include <sys/sysi86.h>
#endif /* i386 */
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#include <sys/termiox.h>
#include <sys/jioctl.h>
#include <sys/filio.h>
#include <fcntl.h>
#include <sys/termio.h>
#include <sys/stermio.h>
#include <sys/ttold.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/utssys.h>
#include <sys/sysconfig.h>
#include <sys/statvfs.h>
#include <sys/kstat.h>

#include "ramdata.h"
#include "proto.h"

#define	FCNTLMIN	F_DUPFD
#define	FCNTLMAX	F_UNSHARE
static const char *const FCNTLname[] = {
	"F_DUPFD",
	"F_GETFD",
	"F_SETFD",
	"F_GETFL",
	"F_SETFL",
	"F_O_GETLK",
	"F_SETLK",
	"F_SETLKW",
	"F_CHKFL",
	"F_DUP2FD",
	"F_ALLOCSP",
	"F_FREESP",
	NULL,		/* 12 */
	NULL,		/* 13 */
	"F_GETLK",
	NULL,		/* 15 */
	NULL,		/* 16 */
	NULL,		/* 17 */
	NULL,		/* 18 */
	NULL,		/* 19 */
	"F_RSETLK",
	"F_RGETLK",
	"F_RSETLKW",
	"F_GETOWN",
	"F_SETOWN",
	"F_REVOKE",
	"F_HASREMOTELOCKS",
	"F_FREESP64",
	NULL,		/* 28 */
	NULL,		/* 29 */
	NULL,		/* 30 */
	NULL,		/* 31 */
	NULL,		/* 32 */
	"F_GETLK64",
	"F_SETLK64",
	"F_SETLKW64",
	NULL,		/* 36 */
	NULL,		/* 37 */
	NULL,		/* 38 */
	NULL,		/* 39 */
	"F_SHARE",
	"F_UNSHARE"
};

#define	SYSFSMIN	GETFSIND
#define	SYSFSMAX	GETNFSTYP
static const char *const SYSFSname[] = {
	"GETFSIND",
	"GETFSTYP",
	"GETNFSTYP"
};

#define	PLOCKMIN	UNLOCK
#define	PLOCKMAX	DATLOCK
static const char *const PLOCKname[] = {
	"UNLOCK",
	"PROCLOCK",
	"TXTLOCK",
	NULL,
	"DATLOCK"
};

#define	SCONFMIN	_CONFIG_NGROUPS
#define	SCONFMAX	_CONFIG_PPC_TB_RATEL
static const char *const SCONFname[] = {
	"_CONFIG_NGROUPS",		/*  2 */
	"_CONFIG_CHILD_MAX",		/*  3 */
	"_CONFIG_OPEN_FILES",		/*  4 */
	"_CONFIG_POSIX_VER",		/*  5 */
	"_CONFIG_PAGESIZE",		/*  6 */
	"_CONFIG_CLK_TCK",		/*  7 */
	"_CONFIG_XOPEN_VER",		/*  8 */
	"_CONFIG_HRESCLK_TCK",		/*  9 */
	"_CONFIG_PROF_TCK",		/* 10 */
	"_CONFIG_NPROC_CONF",		/* 11 */
	"_CONFIG_NPROC_ONLN",		/* 12 */
	"_CONFIG_AIO_LISTIO_MAX",	/* 13 */
	"_CONFIG_AIO_MAX",		/* 14 */
	"_CONFIG_AIO_PRIO_DELTA_MAX",	/* 15 */
	"_CONFIG_DELAYTIMER_MAX",	/* 16 */
	"_CONFIG_MQ_OPEN_MAX",		/* 17 */
	"_CONFIG_MQ_PRIO_MAX",		/* 18 */
	"_CONFIG_RTSIG_MAX",		/* 19 */
	"_CONFIG_SEM_NSEMS_MAX",	/* 20 */
	"_CONFIG_SEM_VALUE_MAX",	/* 21 */
	"_CONFIG_SIGQUEUE_MAX",		/* 22 */
	"_CONFIG_SIGRT_MIN",		/* 23 */
	"_CONFIG_SIGRT_MAX",		/* 24 */
	"_CONFIG_TIMER_MAX",		/* 25 */
	"_CONFIG_PHYS_PAGES",		/* 26 */
	"_CONFIG_AVPHYS_PAGES",		/* 27 */
	"_CONFIG_COHERENCY",		/* 28 */
	"_CONFIG_SPLIT_CACHE",		/* 29 */
	"_CONFIG_ICACHESZ",		/* 30 */
	"_CONFIG_DCACHESZ",		/* 31 */
	"_CONFIG_ICACHELINESZ",		/* 32 */
	"_CONFIG_DCACHELINESZ",		/* 33 */
	"_CONFIG_ICACHEBLKSZ",		/* 34 */
	"_CONFIG_DCACHEBLKSZ",		/* 35 */
	"_CONFIG_DCACHETBLKSZ",		/* 36 */
	"_CONFIG_ICACHE_ASSOC",		/* 37 */
	"_CONFIG_DCACHE_ASSOC",		/* 38 */
	"_CONFIG_PPC_GRANULE_SZ",	/* 39 */
	"_CONFIG_PPC_TB_RATEH",		/* 40 */
	"_CONFIG_PPC_TB_RATEL"		/* 41 */
};

#define	PATHCONFMIN	_PC_LINK_MAX
#define	PATHCONFMAX	_PC_CHOWN_RESTRICTED
static const char *const PATHCONFname[] = {
	"_PC_LINK_MAX",
	"_PC_MAX_CANON",
	"_PC_MAX_INPUT",
	"_PC_NAME_MAX",
	"_PC_PATH_MAX",
	"_PC_PIPE_BUF",
	"_PC_NO_TRUNC",
	"_PC_VDISABLE",
	"_PC_CHOWN_RESTRICTED"
};

static const struct ioc {
	u_int	code;
	const char *name;
} ioc[] = {
	{ (u_int)TCGETA,	"TCGETA"	},
	{ (u_int)TCSETA,	"TCSETA"	},
	{ (u_int)TCSETAW,	"TCSETAW"	},
	{ (u_int)TCSETAF,	"TCSETAF"	},
	{ (u_int)TCFLSH,	"TCFLSH"	},
	{ (u_int)TIOCKBON,	"TIOCKBON"	},
	{ (u_int)TIOCKBOF,	"TIOCKBOF"	},
	{ (u_int)KBENABLED,	"KBENABLED"	},
	{ (u_int)TCGETS,	"TCGETS"	},
	{ (u_int)TCSETS,	"TCSETS"	},
	{ (u_int)TCSETSW,	"TCSETSW"	},
	{ (u_int)TCSETSF,	"TCSETSF"	},
	{ (u_int)TCDSET,	"TCDSET"	},
	{ (u_int)RTS_TOG,	"RTS_TOG"	},
	{ (u_int)TIOCSWINSZ,	"TIOCSWINSZ"	},
	{ (u_int)TIOCGWINSZ,	"TIOCGWINSZ"	},
	{ (u_int)TIOCGETD,	"TIOCGETD"	},
	{ (u_int)TIOCSETD,	"TIOCSETD"	},
	{ (u_int)TIOCHPCL,	"TIOCHPCL"	},
	{ (u_int)TIOCGETP,	"TIOCGETP"	},
	{ (u_int)TIOCSETP,	"TIOCSETP"	},
	{ (u_int)TIOCSETN,	"TIOCSETN"	},
	{ (u_int)TIOCEXCL,	"TIOCEXCL"	},
	{ (u_int)TIOCNXCL,	"TIOCNXCL"	},
	{ (u_int)TIOCFLUSH,	"TIOCFLUSH"	},
	{ (u_int)TIOCSETC,	"TIOCSETC"	},
	{ (u_int)TIOCGETC,	"TIOCGETC"	},
	{ (u_int)TIOCGPGRP,	"TIOCGPGRP"	},
	{ (u_int)TIOCSPGRP,	"TIOCSPGRP"	},
	{ (u_int)TIOCGSID,	"TIOCGSID"	},
	{ (u_int)TIOCSTI,	"TIOCSTI"	},
	{ (u_int)TIOCSSID,	"TIOCSSID"	},
	{ (u_int)TIOCMSET,	"TIOCMSET"	},
	{ (u_int)TIOCMBIS,	"TIOCMBIS"	},
	{ (u_int)TIOCMBIC,	"TIOCMBIC"	},
	{ (u_int)TIOCMGET,	"TIOCMGET"	},
	{ (u_int)TIOCREMOTE,	"TIOCREMOTE"	},
	{ (u_int)TIOCSIGNAL,	"TIOCSIGNAL"	},
	{ (u_int)TIOCSTART,	"TIOCSTART"	},
	{ (u_int)TIOCSTOP,	"TIOCSTOP"	},
	{ (u_int)TIOCNOTTY,	"TIOCNOTTY"	},
	{ (u_int)TIOCOUTQ,	"TIOCOUTQ"	},
	{ (u_int)TIOCGLTC,	"TIOCGLTC"	},
	{ (u_int)TIOCSLTC,	"TIOCSLTC"	},
	{ (u_int)TIOCCDTR,	"TIOCCDTR"	},
	{ (u_int)TIOCSDTR,	"TIOCSDTR"	},
	{ (u_int)TIOCCBRK,	"TIOCCBRK"	},
	{ (u_int)TIOCSBRK,	"TIOCSBRK"	},
	{ (u_int)TIOCLGET,	"TIOCLGET"	},
	{ (u_int)TIOCLSET,	"TIOCLSET"	},
	{ (u_int)TIOCLBIC,	"TIOCLBIC"	},
	{ (u_int)TIOCLBIS,	"TIOCLBIS"	},
	{ (u_int)LDOPEN,	"LDOPEN"	},
	{ (u_int)LDCLOSE,	"LDCLOSE"	},
	{ (u_int)LDCHG,		"LDCHG"		},
	{ (u_int)LDGETT,	"LDGETT"	},
	{ (u_int)LDSETT,	"LDSETT"	},
	{ (u_int)LDSMAP,	"LDSMAP"	},
	{ (u_int)LDGMAP,	"LDGMAP"	},
	{ (u_int)LDNMAP,	"LDNMAP"	},
	{ (u_int)TCGETX,	"TCGETX"	},
	{ (u_int)TCSETX,	"TCSETX"	},
	{ (u_int)TCSETXW,	"TCSETXW"	},
	{ (u_int)TCSETXF,	"TCSETXF"	},
	{ (u_int)FIORDCHK,	"FIORDCHK"	},
	{ (u_int)FIOCLEX,	"FIOCLEX"	},
	{ (u_int)FIONCLEX,	"FIONCLEX"	},
	{ (u_int)FIONREAD,	"FIONREAD"	},
	{ (u_int)FIONBIO,	"FIONBIO"	},
	{ (u_int)FIOASYNC,	"FIOASYNC"	},
	{ (u_int)FIOSETOWN,	"FIOSETOWN"	},
	{ (u_int)FIOGETOWN,	"FIOGETOWN"	},
#ifdef DIOCGETP
	{ (u_int)DIOCGETP,	"DIOCGETP"	},
	{ (u_int)DIOCSETP,	"DIOCSETP"	},
#endif
#ifdef DIOCGETC
	{ (u_int)DIOCGETC,	"DIOCGETC"	},
	{ (u_int)DIOCGETB,	"DIOCGETB"	},
	{ (u_int)DIOCSETE,	"DIOCSETE"	},
#endif
#ifdef EI_RESET
	{ (u_int)EI_RESET,	"EI_RESET"	},
	{ (u_int)EI_LOAD,	"EI_LOAD"	},
	{ (u_int)EI_FCF,	"EI_FCF"	},
	{ (u_int)EI_SYSGEN,	"EI_SYSGEN"	},
	{ (u_int)EI_SETID,	"EI_SETID"	},
	{ (u_int)EI_TURNON,	"EI_TURNON"	},
	{ (u_int)EI_ALLOC,	"EI_ALLOC"	},
	{ (u_int)EI_TERM,	"EI_TERM"	},
	{ (u_int)EI_TURNOFF,	"EI_TURNOFF"	},
	{ (u_int)EI_SETA,	"EI_SETA"	},
	{ (u_int)EI_GETA,	"EI_GETA"	},
#endif
#ifdef IFFORMAT
	{ (u_int)IFFORMAT,	"IFFORMAT"	},
	{ (u_int)IFBCHECK,	"IFBCHECK"	},
	{ (u_int)IFCONFIRM,	"IFCONFIRM"	},
#endif
#ifdef LIOCGETP
	{ (u_int)LIOCGETP,	"LIOCGETP"	},
	{ (u_int)LIOCSETP,	"LIOCSETP"	},
	{ (u_int)LIOCGETS,	"LIOCGETS"	},
	{ (u_int)LIOCSETS,	"LIOCSETS"	},
#endif
#ifdef JBOOT
	{ (u_int)JBOOT,		"JBOOT"		},
	{ (u_int)JTERM,		"JTERM"		},
	{ (u_int)JMPX,		"JMPX"		},
#ifdef JTIMO
	{ (u_int)JTIMO,		"JTIMO"		},
#endif
	{ (u_int)JWINSIZE,	"JWINSIZE"	},
	{ (u_int)JTIMOM,	"JTIMOM"	},
	{ (u_int)JZOMBOOT,	"JZOMBOOT"	},
	{ (u_int)JAGENT,	"JAGENT"	},
	{ (u_int)JTRUN,		"JTRUN"		},
	{ (u_int)JXTPROTO,	"JXTPROTO"	},
#endif
	{ (u_int)KSTAT_IOC_CHAIN_ID,	"KSTAT_IOC_CHAIN_ID"	},
	{ (u_int)KSTAT_IOC_READ,	"KSTAT_IOC_READ"	},
	{ (u_int)KSTAT_IOC_WRITE,	"KSTAT_IOC_WRITE"	},
#ifdef NISETA
	{ (u_int)NISETA,	"NISETA"	},
	{ (u_int)NIGETA,	"NIGETA"	},
	{ (u_int)SUPBUF,	"SUPBUF"	},
	{ (u_int)RDBUF,		"RDBUF"		},
	{ (u_int)NIERRNO,	"NIERRNO"	},
	{ (u_int)STATGET,	"STATGET"	},
	{ (u_int)NISTATUS,	"NISTATUS"	},
	{ (u_int)NIPUMP,	"NIPUMP"	},
	{ (u_int)NIRESET,	"NIRESET"	},
	{ (u_int)NISELGRP,	"NISELGRP"	},
	{ (u_int)NISELECT,	"NISELECT"	},
#endif
	{ (u_int)STGET,		"STGET"		},
	{ (u_int)STSET,		"STSET"		},
	{ (u_int)STTHROW,	"STTHROW"	},
	{ (u_int)STWLINE,	"STWLINE"	},
	{ (u_int)STTSV,		"STTSV"		},
	{ (u_int)I_NREAD,	"I_NREAD"	},
	{ (u_int)I_PUSH,	"I_PUSH"	},
	{ (u_int)I_POP,		"I_POP"		},
	{ (u_int)I_LOOK,	"I_LOOK"	},
	{ (u_int)I_FLUSH,	"I_FLUSH"	},
	{ (u_int)I_SRDOPT,	"I_SRDOPT"	},
	{ (u_int)I_GRDOPT,	"I_GRDOPT"	},
	{ (u_int)I_STR,		"I_STR"		},
	{ (u_int)I_SETSIG,	"I_SETSIG"	},
	{ (u_int)I_GETSIG,	"I_GETSIG"	},
	{ (u_int)I_FIND,	"I_FIND"	},
	{ (u_int)I_LINK,	"I_LINK"	},
	{ (u_int)I_UNLINK,	"I_UNLINK"	},
	{ (u_int)I_PEEK,	"I_PEEK"	},
	{ (u_int)I_FDINSERT,	"I_FDINSERT"	},
	{ (u_int)I_SENDFD,	"I_SENDFD"	},
	{ (u_int)I_RECVFD,	"I_RECVFD"	},
	{ (u_int)I_SWROPT,	"I_SWROPT"	},
	{ (u_int)I_GWROPT,	"I_GWROPT"	},
	{ (u_int)I_LIST,	"I_LIST"	},
	{ (u_int)I_PLINK,	"I_PLINK"	},
	{ (u_int)I_PUNLINK,	"I_PUNLINK"	},
	{ (u_int)I_SETEV,	"I_SETEV"	},
	{ (u_int)I_GETEV,	"I_GETEV"	},
	{ (u_int)I_STREV,	"I_STREV"	},
	{ (u_int)I_UNSTREV,	"I_UNSTREV"	},
	{ (u_int)I_FLUSHBAND,	"I_FLUSHBAND"	},
	{ (u_int)I_CKBAND,	"I_CKBAND"	},
	{ (u_int)I_GETBAND,	"I_GETBAND"	},
	{ (u_int)I_ATMARK,	"I_ATMARK"	},
	{ (u_int)I_SETCLTIME,	"I_SETCLTIME"	},
	{ (u_int)I_GETCLTIME,	"I_GETCLTIME"	},
	{ (u_int)I_CANPUT,	"I_CANPUT"	},
#ifdef TI_GETINFO
	{ (u_int)TI_GETINFO,	"TI_GETINFO"	},
	{ (u_int)TI_OPTMGMT,	"TI_OPTMGMT"	},
	{ (u_int)TI_BIND,	"TI_BIND"	},
	{ (u_int)TI_UNBIND,	"TI_UNBIND"	},
#endif
#ifdef TI_GETMYNAME
	{ (u_int)TI_GETMYNAME,	"TI_GETMYNAME"},
	{ (u_int)TI_GETPEERNAME, "TI_GETPEERNAME"},
	{ (u_int)TI_SETMYNAME,	"TI_SETMYNAME"},
	{ (u_int)TI_SETPEERNAME, "TI_SETPEERNAME"},
#endif
#ifdef V_PREAD
	{ (u_int)V_PREAD,	"V_PREAD"	},
	{ (u_int)V_PWRITE,	"V_PWRITE"	},
	{ (u_int)V_PDREAD,	"V_PDREAD"	},
	{ (u_int)V_PDWRITE,	"V_PDWRITE"	},
#if !defined(i386)
	{ (u_int)V_GETSSZ,	"V_GETSSZ"	},
#endif /* !i386 */
#endif
	/* the old /proc ioctl() control codes */
#define	PIOC	('q'<<8)
	{ (u_int)(PIOC|1),	"PIOCSTATUS"	},
	{ (u_int)(PIOC|2),	"PIOCSTOP"	},
	{ (u_int)(PIOC|3),	"PIOCWSTOP"	},
	{ (u_int)(PIOC|4),	"PIOCRUN"	},
	{ (u_int)(PIOC|5),	"PIOCGTRACE"	},
	{ (u_int)(PIOC|6),	"PIOCSTRACE"	},
	{ (u_int)(PIOC|7),	"PIOCSSIG"	},
	{ (u_int)(PIOC|8),	"PIOCKILL"	},
	{ (u_int)(PIOC|9),	"PIOCUNKILL"	},
	{ (u_int)(PIOC|10),	"PIOCGHOLD"	},
	{ (u_int)(PIOC|11),	"PIOCSHOLD"	},
	{ (u_int)(PIOC|12),	"PIOCMAXSIG"	},
	{ (u_int)(PIOC|13),	"PIOCACTION"	},
	{ (u_int)(PIOC|14),	"PIOCGFAULT"	},
	{ (u_int)(PIOC|15),	"PIOCSFAULT"	},
	{ (u_int)(PIOC|16),	"PIOCCFAULT"	},
	{ (u_int)(PIOC|17),	"PIOCGENTRY"	},
	{ (u_int)(PIOC|18),	"PIOCSENTRY"	},
	{ (u_int)(PIOC|19),	"PIOCGEXIT"	},
	{ (u_int)(PIOC|20),	"PIOCSEXIT"	},
	{ (u_int)(PIOC|21),	"PIOCSFORK"	},
	{ (u_int)(PIOC|22),	"PIOCRFORK"	},
	{ (u_int)(PIOC|23),	"PIOCSRLC"	},
	{ (u_int)(PIOC|24),	"PIOCRRLC"	},
	{ (u_int)(PIOC|25),	"PIOCGREG"	},
	{ (u_int)(PIOC|26),	"PIOCSREG"	},
	{ (u_int)(PIOC|27),	"PIOCGFPREG"	},
	{ (u_int)(PIOC|28),	"PIOCSFPREG"	},
	{ (u_int)(PIOC|29),	"PIOCNICE"	},
	{ (u_int)(PIOC|30),	"PIOCPSINFO"	},
	{ (u_int)(PIOC|31),	"PIOCNMAP"	},
	{ (u_int)(PIOC|32),	"PIOCMAP"	},
	{ (u_int)(PIOC|33),	"PIOCOPENM"	},
	{ (u_int)(PIOC|34),	"PIOCCRED"	},
	{ (u_int)(PIOC|35),	"PIOCGROUPS"	},
	{ (u_int)(PIOC|36),	"PIOCGETPR"	},
	{ (u_int)(PIOC|37),	"PIOCGETU"	},
	{ (u_int)(PIOC|38),	"PIOCSET"	},
	{ (u_int)(PIOC|39),	"PIOCRESET"	},
	{ (u_int)(PIOC|43),	"PIOCUSAGE"	},
	{ (u_int)(PIOC|44),	"PIOCOPENPD"	},
	{ (u_int)(PIOC|45),	"PIOCLWPIDS"	},
	{ (u_int)(PIOC|46),	"PIOCOPENLWP"	},
	{ (u_int)(PIOC|47),	"PIOCLSTATUS"	},
	{ (u_int)(PIOC|48),	"PIOCLUSAGE"	},
	{ (u_int)(PIOC|49),	"PIOCNAUXV"	},
	{ (u_int)(PIOC|50),	"PIOCAUXV"	},
	{ (u_int)(PIOC|51),	"PIOCGXREGSIZE"	},
	{ (u_int)(PIOC|52),	"PIOCGXREG"	},
	{ (u_int)(PIOC|53),	"PIOCSXREG"	},
	{ (u_int)(PIOC|101),	"PIOCGWIN"	},
	{ (u_int)(PIOC|103),	"PIOCNLDT"	},
	{ (u_int)(PIOC|104),	"PIOCLDT"	},
	{ (u_int)0,		 NULL		}
};

const char *
ioctlname(u_int code)
{
	register const struct ioc *ip;
	register const char *str = NULL;
	register int c;

	for (ip = &ioc[0]; ip->name; ip++) {
		if (code == ip->code) {
			str = ip->name;
			break;
		}
	}

	if (str == NULL) {
		c = code >> 8;
		if (isascii(c) && isprint(c))
			(void) sprintf(code_buf, "(('%c'<<8)|%d)",
				c, code & 0xff);
		else
			(void) sprintf(code_buf, "0x%.4X", code);
		str = code_buf;
	}

	return (str);
}

const char *
fcntlname(int code)
{
	register const char *str = NULL;

	if (code >= FCNTLMIN && code <= FCNTLMAX)
		str = FCNTLname[code-FCNTLMIN];
	return (str);
}

const char *
sfsname(int code)
{
	register const char *str = NULL;

	if (code >= SYSFSMIN && code <= SYSFSMAX)
		str = SYSFSname[code-SYSFSMIN];
	return (str);
}

const char *
plockname(int code)
{
	register const char *str = NULL;

	if (code >= PLOCKMIN && code <= PLOCKMAX)
		str = PLOCKname[code-PLOCKMIN];
	return (str);
}

/* ARGSUSED */
const char *
#if defined(i386)
si86name(int code)
#else /* !i386 */
s3bname(int code)
#endif /* !i386 */
{
	register const char *str = NULL;

#if defined(i386)
	switch (code) {
	case SI86SWPI:		str = "SI86SWPI";	break;
	case SI86SYM:		str = "SI86SYM";	break;
	case SI86CONF:		str = "SI86CONF";	break;
	case SI86BOOT:		str = "SI86BOOT";	break;
	case SI86AUTO:		str = "SI86AUTO";	break;
	case SI86EDT:		str = "SI86EDT";	break;
	case SI86SWAP:		str = "SI86SWAP";	break;
	case SI86FPHW:		str = "SI86FPHW";	break;
	case GRNON:		str = "GRNON";		break;
	case GRNFLASH:		str = "GRNFLASH";	break;
	case STIME:		str = "STIME";		break;
	case SETNAME:		str = "SETNAME";	break;
	case RNVR:		str = "RNVR";		break;
	case WNVR:		str = "WNVR";		break;
	case RTODC:		str = "RTODC";		break;
	case CHKSER:		str = "CHKSER";		break;
	case SI86NVPRT:		str = "SI86NVPRT";	break;
	case SANUPD:		str = "SANUPD";		break;
	case SI86KSTR:		str = "SI86KSTR";	break;
	case SI86MEM:		str = "SI86MEM";	break;
	case SI86TODEMON:	str = "SI86TODEMON";	break;
	case SI86CCDEMON:	str = "SI86CCDEMON";	break;
	case SI86CACHE:		str = "SI86CACHE";	break;
	case SI86DELMEM:	str = "SI86DELMEM";	break;
	case SI86ADDMEM:	str = "SI86ADDMEM";	break;
/* 71 through 74 reserved for VPIX */
	case SI86V86: 		str = "SI86V86";	break;
	case SI86SLTIME:	str = "SI86SLTIME";	break;
	case SI86DSCR:		str = "SI86DSCR";	break;
	case RDUBLK:		str = "RDUBLK";		break;
/* NFA entry point */
	case SI86NFA:		str = "SI86NFA";	break;
	case SI86VM86:		str = "SI86VM86";	break;
	case SI86VMENABLE:	str = "SI86VMENABLE";	break;
	case SI86LIMUSER:	str = "SI86LIMUSER";	break;
	case SI86RDID: 		str = "SI86RDID";	break;
	case SI86RDBOOT:	str = "SI86RDBOOT";	break;
/* Merged Product defines */
	case SI86SHFIL:		str = "SI86SHFIL";	break;
	case SI86PCHRGN:	str = "SI86PCHRGN";	break;
	case SI86BADVISE:	str = "SI86BADVISE";	break;
	case SI86SHRGN:		str = "SI86SHRGN";	break;
	case SI86CHIDT:		str = "SI86CHIDT";	break;
	case SI86EMULRDA: 	str = "SI86EMULRDA";	break;
	}
#endif /* i386 */

	return (str);
}

const char *
utscode(int code)
{
	register const char *str = NULL;

	switch (code) {
	case UTS_UNAME:		str = "UNAME";	break;
	case UTS_USTAT:		str = "USTAT";	break;
	case UTS_FUSERS:	str = "FUSERS";	break;
	}

	return (str);
}

const char *
sconfname(int code)
{
	register const char *str = NULL;

	if (code >= SCONFMIN && code <= SCONFMAX)
		str = SCONFname[code-SCONFMIN];
	return (str);
}

const char *
pathconfname(int code)
{
	register const char *str = NULL;

	if (code >= PATHCONFMIN && code <= PATHCONFMAX)
		str = PATHCONFname[code-PATHCONFMIN];
	return (str);
}

const char *
sigarg(int arg)
{
	register char *str = NULL;
	register int sig = (arg & SIGNO_MASK);

	str = code_buf;
	arg &= ~SIGNO_MASK;
	if (arg & ~(SIGDEFER|SIGHOLD|SIGRELSE|SIGIGNORE|SIGPAUSE))
		(void) sprintf(str, "%s|0x%X", signame(sig), arg);
	else {
		(void) strcpy(str, signame(sig));
		if (arg & SIGDEFER)
			(void) strcat(str, "|SIGDEFER");
		if (arg & SIGHOLD)
			(void) strcat(str, "|SIGHOLD");
		if (arg & SIGRELSE)
			(void) strcat(str, "|SIGRELSE");
		if (arg & SIGIGNORE)
			(void) strcat(str, "|SIGIGNORE");
		if (arg & SIGPAUSE)
			(void) strcat(str, "|SIGPAUSE");
	}

	return ((const char *)str);
}

#define	ALL_O_FLAGS \
	(O_NDELAY|O_APPEND|O_SYNC|O_DSYNC|O_NONBLOCK|O_CREAT|O_TRUNC|O_EXCL)

const char *
openarg(int arg)
{
	register char *str = code_buf;

	switch (arg & ~ALL_O_FLAGS) {
	default:
		return ((char *)NULL);
	case O_RDONLY:
		(void) strcpy(str, "O_RDONLY");
		break;
	case O_WRONLY:
		(void) strcpy(str, "O_WRONLY");
		break;
	case O_RDWR:
		(void) strcpy(str, "O_RDWR");
		break;
	}

	if (arg & O_NDELAY)
		(void) strcat(str, "|O_NDELAY");
	if (arg & O_APPEND)
		(void) strcat(str, "|O_APPEND");
	if (arg & O_SYNC)
		(void) strcat(str, "|O_SYNC");
	if (arg & O_DSYNC)
		(void) strcat(str, "|O_DSYNC");
	if (arg & O_NONBLOCK)
		(void) strcat(str, "|O_NONBLOCK");
	if (arg & O_CREAT)
		(void) strcat(str, "|O_CREAT");
	if (arg & O_TRUNC)
		(void) strcat(str, "|O_TRUNC");
	if (arg & O_EXCL)
		(void) strcat(str, "|O_EXCL");

	return ((const char *)str);
}

const char *
whencearg(int arg)
{
	register const char *str = NULL;

	switch (arg) {
	case SEEK_SET:	str = "SEEK_SET";	break;
	case SEEK_CUR:	str = "SEEK_CUR";	break;
	case SEEK_END:	str = "SEEK_END";	break;
	}

	return (str);
}

#define	IPC_FLAGS	(IPC_ALLOC|IPC_CREAT|IPC_EXCL|IPC_NOWAIT)

static char *
ipcflags(int arg)
{
	register char *str = code_buf;

	if (arg&0777)
		(void) sprintf(str, "0%.3o", arg&0777);
	else
		*str = '\0';

	if (arg & IPC_ALLOC)
		(void) strcat(str, "|IPC_ALLOC");
	if (arg & IPC_CREAT)
		(void) strcat(str, "|IPC_CREAT");
	if (arg & IPC_EXCL)
		(void) strcat(str, "|IPC_EXCL");
	if (arg & IPC_NOWAIT)
		(void) strcat(str, "|IPC_NOWAIT");

	return (str);
}

const char *
msgflags(int arg)
{
	register char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|MSG_NOERROR|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & MSG_NOERROR)
		(void) strcat(str, "|MSG_NOERROR");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

const char *
semflags(int arg)
{
	register char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|SEM_UNDO|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & SEM_UNDO)
		(void) strcat(str, "|SEM_UNDO");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

const char *
shmflags(int arg)
{
	register char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|SHM_RDONLY|SHM_RND|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & SHM_RDONLY)
		(void) strcat(str, "|SHM_RDONLY");
	if (arg & SHM_RND)
		(void) strcat(str, "|SHM_RND");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

#define	MSGCMDMIN	IPC_RMID
#define	MSGCMDMAX	IPC_STAT
static const char *const MSGCMDname[MSGCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
};

#define	SEMCMDMIN	IPC_RMID
#define	SEMCMDMAX	SETALL
static const char *const SEMCMDname[SEMCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
	"GETNCNT",
	"GETPID",
	"GETVAL",
	"GETALL",
	"GETZCNT",
	"SETVAL",
	"SETALL",
};

#define	SHMCMDMIN	IPC_RMID
#ifdef	SHM_UNLOCK
#define	SHMCMDMAX	SHM_UNLOCK
#else
#define	SHMCMDMAX	IPC_STAT
#endif
static const char *const SHMCMDname[SHMCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
#ifdef	SHM_UNLOCK
	"SHM_LOCK",
	"SHM_UNLOCK",
#endif
};

const char *
msgcmd(int arg)
{
	register const char *str = NULL;

	if (arg >= MSGCMDMIN && arg <= MSGCMDMAX)
		str = MSGCMDname[arg-MSGCMDMIN];
	return (str);
}

const char *
semcmd(int arg)
{
	register const char *str = NULL;

	if (arg >= SEMCMDMIN && arg <= SEMCMDMAX)
		str = SEMCMDname[arg-SEMCMDMIN];
	return (str);
}

const char *
shmcmd(int arg)
{
	register const char *str = NULL;

	if (arg >= SHMCMDMIN && arg <= SHMCMDMAX)
		str = SHMCMDname[arg-SHMCMDMIN];
	return (str);
}

const char *
strrdopt(int arg)	/* streams read option (I_SRDOPT I_GRDOPT) */
{
	register const char *str = NULL;

	switch (arg) {
	case RNORM:	str = "RNORM";		break;
	case RMSGD:	str = "RMSGD";		break;
	case RMSGN:	str = "RMSGN";		break;
	}

	return (str);
}

const char *
strevents(int arg)	/* bit map of streams events (I_SETSIG & I_GETSIG) */
{
	register char *str = code_buf;

	if (arg & ~(S_INPUT|S_HIPRI|S_OUTPUT|S_MSG|S_ERROR|S_HANGUP))
		return ((char *)NULL);

	*str = '\0';
	if (arg & S_INPUT)
		(void) strcat(str, "|S_INPUT");
	if (arg & S_HIPRI)
		(void) strcat(str, "|S_HIPRI");
	if (arg & S_OUTPUT)
		(void) strcat(str, "|S_OUTPUT");
	if (arg & S_MSG)
		(void) strcat(str, "|S_MSG");
	if (arg & S_ERROR)
		(void) strcat(str, "|S_ERROR");
	if (arg & S_HANGUP)
		(void) strcat(str, "|S_HANGUP");

	return ((const char *)(str+1));
}

const char *
tiocflush(int arg)	/* bit map passsed by TIOCFLUSH */
{
	register char *str = code_buf;

	if (arg & ~(FREAD|FWRITE))
		return ((char *)NULL);

	*str = '\0';
	if (arg & FREAD)
		(void) strcat(str, "|FREAD");
	if (arg & FWRITE)
		(void) strcat(str, "|FWRITE");

	return ((const char *)(str+1));
}

const char *
strflush(int arg)	/* streams flush option (I_FLUSH) */
{
	register const char *str = NULL;

	switch (arg) {
	case FLUSHR:	str = "FLUSHR";		break;
	case FLUSHW:	str = "FLUSHW";		break;
	case FLUSHRW:	str = "FLUSHRW";	break;
	}

	return (str);
}

#define	ALL_MOUNT_FLAGS	\
	(MS_RDONLY|MS_FSS|MS_DATA|MS_NOSUID|MS_REMOUNT|MS_NOTRUNC|MS_OVERLAY)

const char *
mountflags(int arg)	/* bit map of mount syscall flags */
{
	register char *str = code_buf;

	if (arg & ~ALL_MOUNT_FLAGS)
		return ((char *)NULL);

	*str = '\0';
	if (arg & MS_RDONLY)
		(void) strcat(str, "|MS_RDONLY");
	if (arg & MS_FSS)
		(void) strcat(str, "|MS_FSS");
	if (arg & MS_DATA)
		(void) strcat(str, "|MS_DATA");
	if (arg & MS_NOSUID)
		(void) strcat(str, "|MS_NOSUID");
	if (arg & MS_REMOUNT)
		(void) strcat(str, "|MS_REMOUNT");
	if (arg & MS_NOTRUNC)
		(void) strcat(str, "|MS_NOTRUNC");
	if (arg & MS_OVERLAY)
		(void) strcat(str, "|MS_OVERLAY");
	return (*str? (const char *)(str+1) : "0");
}

const char *
svfsflags(int arg)	/* bit map of statvfs syscall flags */
{
	register char *str = code_buf;

	if (arg & ~(ST_RDONLY|ST_NOSUID|ST_NOTRUNC)) {
		(void) sprintf(str, "0x%x", arg);
		return (str);
	}
	*str = '\0';
	if (arg & ST_RDONLY)
		(void) strcat(str, "|ST_RDONLY");
	if (arg & ST_NOSUID)
		(void) strcat(str, "|ST_NOSUID");
	if (arg & ST_NOTRUNC)
		(void) strcat(str, "|ST_NOTRUNC");
	return (*str? (const char *)(str+1) : "0");
}

const char *
fuiname(int arg)	/* fusers() input argument */
{
	register const char *str = NULL;

	switch (arg) {
	case F_FILE_ONLY:	str = "F_FILE_ONLY";		break;
	case F_CONTAINED:	str = "F_CONTAINED";		break;
	}

	return (str);
}

const char *
fuflags(int arg)	/* fusers() output flags */
{
	register char *str = code_buf;

	if (arg & ~(F_CDIR|F_RDIR|F_TEXT|F_MAP|F_OPEN|F_TRACE|F_TTY)) {
		(void) sprintf(str, "0x%x", arg);
		return (str);
	}
	*str = '\0';
	if (arg & F_CDIR)
		(void) strcat(str, "|F_CDIR");
	if (arg & F_RDIR)
		(void) strcat(str, "|F_RDIR");
	if (arg & F_TEXT)
		(void) strcat(str, "|F_TEXT");
	if (arg & F_MAP)
		(void) strcat(str, "|F_MAP");
	if (arg & F_OPEN)
		(void) strcat(str, "|F_OPEN");
	if (arg & F_TRACE)
		(void) strcat(str, "|F_TRACE");
	if (arg & F_TTY)
		(void) strcat(str, "|F_TTY");
	return (*str? (const char *)(str+1) : "0");
}

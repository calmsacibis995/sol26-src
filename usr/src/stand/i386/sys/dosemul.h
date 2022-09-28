/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_I386_SYS_DOSEMUL_H
#define	_I386_SYS_DOSEMUL_H

#pragma ident	"@(#)dosemul.h	1.17	96/05/15 SMI"

#include <sys/types.h>
#include <sys/bootlink.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  Definitions related to file handling for real-mode modules.
 */
#define	DOSfile_ERROR	-1
#define	DOSfile_OK	0

#define	DOSfile_MINFD	5	/* 0-4 reserve for DOS_STDxxx */
#define	DOSfile_MAXFDS	20
#define	DOSfile_LASTFD	(DOSfile_MAXFDS - 1)

/*
 * Structure and defines for tracking chunks of memory
 * requested of "DOS".
 */
typedef struct dml {
	struct dml *next;
	int	seg;
	caddr_t addr;
	ulong	size;
} dmcl_t;

/*
 * Structure and defines for tracking fd's accessed via "DOS".
 */
#define	DOSACCESS_RDONLY	0
#define	DOSACCESS_WRONLY	1
#define	DOSACCESS_RDWR		2

#define	DOSSEEK_TOABS	0
#define	DOSSEEK_FROMFP	1
#define	DOSSEEK_FROMEOF	2

#define	DOSBOOTOPC_FN	"bootops"
#define	DOSBOOTOPR_FN	"bootops.res"

#define	DOSFD_INUSE	0x01
#define	DOSFD_RAMFILE	0x02
#define	DOSFD_BOOTOPC	0x04
#define	DOSFD_BOOTOPR	0x08
#define	DOSFD_STDDEV	0x10
#define	DOSFD_DSKWRT	0x20	/* ... file system writes okay */

typedef struct dffd {
	ulong	flags;
	rffd_t	*rfp;		/* if given file is actually RAM file */
	int	actualfd;	/* if given file is from a file system */
} dffd_t;

/*
 * Find-File Info structure.  Needed to keep state between
 * findfile requests by user programs.
 */
typedef struct dffi {
	struct dffi *next;
	struct dos_fninfo *cookie;
	ushort curmatchattr;
	char *curmatchpath;
	char *curmatchfile;
	char *dentbuf;
	int dirfd;
	int requestreset;
	int curdoserror;
	int nextdentidx;
	int maxdent;
	int curdent;
} ffinfo;

/*
 * Signature that marks valid dos_fninfo structures
 */
#define	FFSIG	0x484D4954

/*
 * This is the header format for a loadable DOS driver.
 */
#pragma pack(1)
struct dos_drvhdr {
	ushort	next_driver_offset;
	ushort	next_driver_segment;
	ushort	attrib_word;
	ushort	strat_offset;
	ushort	intr_offset;
	union {
		char name[8];
		struct dos_blkdrvrinfo {
			char	numunits;
			char	reserved[7];
		} blkinfo;
	} infosection;
};

/*
 * This is the minimal request structure sent to loadable DOS driver.
 */
struct dos_drvreq {
	unchar	reqlen;
	unchar	unit;
	unchar	command;
	ushort	status;
	unchar	reserved[8];
	unchar	media;
	ulong	address;
	ushort	count;
	ushort	sector;
};

/*
 * exe header
 */
struct dos_exehdr {
	ushort	sig;		/* EXE program signature */
	ushort	nbytes;		/* number of bytes in last page */
	ushort	npages;		/* number of 512-byte pages */
	ushort	nreloc;		/* number of relocation table entries */
	ushort	header_mem;	/* header size in paragraphs */
	ushort	require_mem;	/* required memory size in paragraphs */
	ushort	desire_mem;	/* desired memory size in paragraphs */
	ushort	init_ss;	/* in relative paragraphs */
	ushort	init_sp;
	ushort	checksum;
	ushort	init_ip;	/* at entry */
	ushort	init_cs;	/* in paragraphs */
	ushort	reloc_off;	/* offset of first reloc entry */
	ushort	ovly_num;	/* overlay number */
	ushort	reserved[16];
	ulong	newexe;		/* offset to additional header for new exe's */
};

struct dos_psp {
	ushort	sig;
	ushort	nxtgraf;
	unchar	skip1;
	unchar	cpmcall[5];
	ulong	isv22;
	ulong	isv23;
	ulong	isv24;
	ushort	parent_id;
	unchar	htable[20];
	ushort	envseg;
	ulong	savstk;
	ushort	nhdls;
	ulong	htblptr;
	ulong	sharechn;
	unchar	skip2;
	unchar	trunamflg;
	unchar	skip3a[2];
	ushort	version;
	unchar	skip3b[6];
	unchar	woldapp;
	unchar	skip4[7];
	unchar	disp[3];
	unchar	skip5[2];
	unchar	extfcb[7];
	unchar	fcb1[16];
	unchar	fcb2[20];
	unchar	tailc;
	unchar	tail[127];
};

struct dos_fcb {
	unchar	drive;
	char	name[8];
	char	ext[3];
	ushort	curblock;
	ushort	recsize;
	ulong	filesize;
	ushort	cdate;
	ushort	ctime;
	unchar	reserved[8];
	unchar	currec;
	ulong	relrec;
};

struct dos_efcb {
	unchar	sig;
	unchar	reserved1[5];
	unchar	attrib;
	unchar	drive;
	char	name[8];
	char	ext[3];
	ushort	curblock;
	ushort	recsize;
	ulong	filesize;
	ushort	cdate;
	ushort	ctime;
	unchar	reserved2[8];
	unchar	currec;
	ulong	relrec;
};

struct dos_fninfo {
	ulong	sig;
	ffinfo	*statep;
	unchar  rsrvd[13];
	unchar  attr;
	unchar  time[2];
	unchar  date[2];
	ulong   size;
	char    name[13];
};
#pragma pack()

#define	EXE_SIG		0x5a4d		/* sig value in exehdr */
#define	EXE_HDR_SIZE	512		/* "standard" exe header size */
#define	COM_MEM_SIZE	(64*1024)	/* memory block required to run .com */
#define	EFCB_SIG	0xff

#define	RUN_OK		0	/* BSH Run command fully succeeded */
#define	RUN_FAIL	-1	/* BSH Run failed to execute */
#define	RUN_NOTFOUND	-2	/* BSH Run executable not found */

/*
 * Real mode memory access is limited to 20 bits of address space.
 * TOP_RMMEM marks upper bounds of where second level boot should
 * ever try to access from real-mode.
 */
#define	TOP_RMMEM	0xA0000 /* Highest code or data address */

/*
 * Int 21 (DOS soft interrupt) related defines
 */
#define	DOS_STDIN	0
#define	DOS_STDOUT	1
#define	DOS_STDERR	2
#define	DOS_STDAUX	3
#define	DOS_STDPRN	4

#define	DOS_ADRIVE	0
#define	DOS_BDRIVE	1
#define	DOS_CDRIVE	2
#define	DOS_DDRIVE	3

#define	DOS_CMDLINESIZE 256

#define	DOSVECTLEN	4	/* Real-mode vector table entry length */

#define	DOSDENTBUFSIZ	0x1000

/*
 * Restrictions on DOS file names
 */
#define	DOSMAX_NMPART	8
#define	DOSMAX_EXTPART	3
#define	DOSMAX_FILNAM	12

/*
 * DOS file attributes
 */
#define	DOSATTR_RDONLY		0x1
#define	DOSATTR_HIDDEN		0x2
#define	DOSATTR_SYSTEM		0x4
#define	DOSATTR_VOLLBL		0x8
#define	DOSATTR_DIR		0x10
#define	DOSATTR_ARCHIVE		0x20

#define	DOS_EOFCHAR		0x1A

#define	DS_DX(rp)	((char *)((rp)->ds*0x10+rp->edx.word.dx))
#define	DS_SI(rp)	((char *)((rp)->ds*0x10+rp->esi.word.si))

#define	ES_BX(rp)	((char *)((rp)->es*0x10+rp->ebx.word.bx))
#define	ES_DX(rp)	((char *)((rp)->es*0x10+rp->edx.word.dx))
#define	ES_SI(rp)	((char *)((rp)->es*0x10+rp->esi.word.si))
#define	ES_DI(rp)	((char *)((rp)->es*0x10+rp->edi.word.di))

#define	AX(rp)		((rp)->eax.word.ax)
#define	AH(rp)		((rp)->eax.byte.ah)
#define	AL(rp)		((rp)->eax.byte.al)
#define	BX(rp)		((rp)->ebx.word.bx)
#define	BH(rp)		((rp)->ebx.byte.bh)
#define	BL(rp)		((rp)->ebx.byte.bl)
#define	CX(rp)		((rp)->ecx.word.cx)
#define	CH(rp)		((rp)->ecx.byte.ch)
#define	CL(rp)		((rp)->ecx.byte.cl)
#define	DX(rp)		((rp)->edx.word.dx)
#define	DH(rp)		((rp)->edx.byte.dh)
#define	DL(rp)		((rp)->edx.byte.dl)

#define	BP(rp)		((rp)->ebp.word.bp)
#define	SI(rp)		((rp)->esi.word.si)
#define	DI(rp)		((rp)->edi.word.di)

/*
 * Function prototypes
 */
extern	int	newdoint(int, struct real_regs *);
extern	int	olddoint(void);
extern	ushort	peeks(ushort *);
extern	dmcl_t	*findmemreq(int, dmcl_t **);
extern	char	*dosfn_to_unixfn(char *);
extern	void	addmemreq(dmcl_t *);
extern	void	dosemul_init(void);
extern	void	dosbootop(dffd_t *, char *, int);
extern	void	dosopenfile(struct real_regs *);
extern	void	doswritefile(struct real_regs *);
extern	void	dosallocpars(struct real_regs *);
extern	void	dosfreepars(struct real_regs *);
extern	void	dosreallocpars(struct real_regs *);
extern	void	dosfindfirst(struct real_regs *);
extern	void	dosfindnext(struct real_regs *);
extern	void	dosgetcwd(struct real_regs *);
extern	void	pokes(ushort *, ushort);
extern	void	hook21(void);
extern	void	get_dosivec(int, ushort *, ushort *);
extern	void	set_dosivec(int, ushort, ushort);

/*
 * Dirent fixup routines
 */
extern	void	RAMfile_patch_dirents(ffinfo *ffip);

#define	iswhitespace(c) \
	((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t'))

#define	CLEAR_CARRY(rp) \
	((rp)->eflags &= (ulong)~CARRY_FLAG)

#define	SET_CARRY(rp) \
	((rp)->eflags |= (ulong)CARRY_FLAG)

#ifdef	__cplusplus
}
#endif

#endif	/* _I386_SYS_DOSEMUL_H */

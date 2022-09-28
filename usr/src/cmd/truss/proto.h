/*
 * Copyright (c) 1992,1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)proto.h	1.14	96/06/18 SMI"	/* SVr4.0 1.6	*/

#include <sys/procset.h>

/*
 * Function prototypes for most external functions.
 */

extern	int	requested(process_t *, int);
extern	int	jobcontrol(process_t *);
extern	int	signalled(process_t *);
extern	void	faulted(process_t *);
extern	int	sysentry(process_t *);
extern	void	sysexit(process_t *);
extern	void	showbuffer(process_t *, long, int);
extern	void	showbytes(const char *, int, char *);
extern	void	accumulate(timestruc_t *, timestruc_t *, timestruc_t *);
extern	void	msleep(int);

extern	const char *ioctlname(u_int);
extern	const char *fcntlname(int);
extern	const char *sfsname(int);
extern	const char *plockname(int);

#if defined(i386)
extern	const char *si86name(int);
#else /* ! i386 */
extern	const char *s3bname(int);
#endif /* ! i386 */

extern	const char *utscode(int);
extern	const char *sigarg(int);
extern	const char *openarg(int);
extern	const char *whencearg(int);
extern	const char *msgflags(int);
extern	const char *semflags(int);
extern	const char *shmflags(int);
extern	const char *msgcmd(int);
extern	const char *semcmd(int);
extern	const char *shmcmd(int);
extern	const char *strrdopt(int);
extern	const char *strevents(int);
extern	const char *tiocflush(int);
extern	const char *strflush(int);
extern	const char *mountflags(int);
extern	const char *svfsflags(int);
extern	const char *sconfname(int);
extern	const char *pathconfname(int);
extern	const char *fuiname(int);
extern	const char *fuflags(int);

extern	void	expound(process_t *, int, int);
extern	void	prtime(const char *, time_t);
extern	void	prtimestruc(const char *, timestruc_t);
extern	void	print_siginfo(siginfo_t *);

extern	void	Flush(void);
extern	void	Eserialize(void);
extern	void	Xserialize(void);
extern	void	procadd(pid_t);
extern	void	procdel(void);
extern	int	checkproc(process_t *, int, char *, int);

extern	int	syslist(char *, sysset_t *, int *);
extern	int	siglist(char *, sigset_t *, int *);
extern	int	fltlist(char *, fltset_t *, int *);
extern	int	fdlist(char *, fileset_t *);

extern	char 	*fetchstring(long, int);
extern	void	show_cred(process_t *, int);
extern	void	errmsg(const char *, const char *);
extern	void	abend(const char *, const char *);
extern	int	isprocdir(process_t *, const char *);

extern	void	outstring(const char *);

extern	void	show_procset(process_t *, long);
extern	const char *idtype_enum(idtype_t);
extern	const char *woptions(int);

extern	const char *errname(int);
extern	const char *sysname(int, int);
extern	const char *rawsigname(int);
extern	const char *signame(int);
extern	const char *rawfltname(int);
extern	const char *fltname(int);

extern	void	show_xstat(process_t *, long);
extern	void	show_stat(process_t *, long);
extern	void	show_stat64(process_t *, long);

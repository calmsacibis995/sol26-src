/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getpass.c	1.13	96/04/27 SMI"	/* SVr4.0 1.20	*/

/*	3.0 SID #	1.4	*/
/*LINTLIBRARY*/
#ifdef __STDC__
#pragma weak getpass = _getpass
	#pragma weak getpassphrase = _getpassphrase
#endif
#include "synonyms.h"
#include <stdio.h>
#include <signal.h>
#include <termio.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "stdiom.h"

extern int findiop();
extern int kill(), ioctl(), getpid();
static void catch();
static int intrupt;
static char *__getpass();

#define	MAXPASSWD	256	/* max significant characters in password */
#define	SMLPASSWD	8	/* unix standard  characters in password */


char *
getpass(prompt)
const char	*prompt;
{
	return (__getpass(prompt, SMLPASSWD));
}

char *
getpassphrase(prompt)
const char	*prompt;
{
	return (__getpass(prompt, MAXPASSWD));
}

static
char *
__getpass(prompt, size)
const char	*prompt;
int		 size;
{
	struct termio ttyb;
	unsigned short flags;
	register char *p;
	register int c;
	FILE	*fi;
	static char pbuf_st[MAXPASSWD + 1];
	static thread_key_t key = 0;
	extern char *_tsdalloc();
	char *pbuf = (_thr_main() ? pbuf_st : _tsdalloc(&key, MAXPASSWD + 1));
	void	(*sig)();
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	if ((fi = fopen("/dev/tty", "r")) == NULL)
		return ((char *)NULL);
	setbuf(fi, (char *)NULL);
	sig = signal(SIGINT, catch);
	intrupt = 0;
	(void) ioctl(FILENO(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(FILENO(fi), TCSETAF, &ttyb);
	FLOCKFILE(lk, stderr);
	(void) fputs(prompt, stderr);
	p = pbuf;
	while (!intrupt &&
		(c = GETC(fi)) != '\n' && c != '\r' && c != EOF) {
		if(p < &pbuf[ size ])
			*p++ = (char)c;
	}
	*p = '\0';
	ttyb.c_lflag = flags;
	(void) ioctl(FILENO(fi), TCSETAW, &ttyb);
	(void) PUTC('\n', stderr);
	FUNLOCKFILE(lk);
	(void) signal(SIGINT, sig);
#if 0
	/* fi would never be == to stdin ! */
	if (fi != stdin)
#endif
	(void) fclose(fi);
	if (intrupt)
		(void) kill(getpid(), SIGINT);
	return (pbuf);
}

static void
catch()
{
	++intrupt;
}

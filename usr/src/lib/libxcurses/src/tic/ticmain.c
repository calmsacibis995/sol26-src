/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ticmain.c 1.1	96/01/17 SMI"

/*
 *	ticmain.c		
 *
 *	Terminal Information Compiler
 *
 *	Copyright 1990, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 *
 *	This Software is unpublished, valuable, confidential property of
 *	Mortice Kern Systems Inc.  Use is authorized only in accordance
 *	with the terms and conditions of the source licence agreement
 *	protecting this Software.  Any unauthorized use or disclosure of
 *	this Software is strictly prohibited and will result in the
 *	termination of the licence agreement.
 *
 *	If you have any questions, please consult your supervisor.
 *
 *	Portions of this code Copyright 1982 by Pavel Curtis.
 *
 */

#ifdef M_RCSID
#ifndef lint
static char const rcsID[] = "$Header: /rd/src/tic/rcs/ticmain.c 1.11 1995/06/22 18:40:30 ross Exp $";
#endif
#endif

#include "tic.h"
#include <ctype.h>
#include <sys/stat.h>

int curr_line;
int check_only = 0;
int debug_level = 0;
time_t start_time;
char *source_file = M_TERMINFO_DIR;
char *destination = M_TERMINFO_DIR;

char boolean[BOOLCOUNT];
short number[NUMCOUNT];
short string[STRCOUNT];

static char const usage[] = m_textstr(3103, "usage:  %s [-v[n]] [-c] <file>\n", "U _");
static char const src_err[] = m_textstr(3104, "terminfo definition file \"%s\" ", "E filename");
static char const dst_err[] = m_textstr(3105, "terminfo database \"%s\" ", "E filename");
static char const dstdir_err[] = m_textstr(3106, "terminfo database directory \"%s/%s\" ", "E pathname");

static void init(void);

int
main(int argc, char **argv)
{
	char *ap;
	setlocale(LC_ALL, "");
	_cmdname = m_cmdname(*argv);
	for (--argc, ++argv; 0 < argc && **argv == '-'; --argc, ++argv) {
		ap = &argv[0][1];
		if (*ap == '-' && ap[1] == '\0') {
			--argc;
			++argv;
			break;
		}
		while (*ap != '\0') {
			switch (*ap++) {
			case 'c':
				check_only = 1;
				continue;
			case 'v':
				debug_level = 1;
				if (isdigit(*ap)) 
					debug_level = (int) strtol(ap, &ap, 0);
				break;
			default:
				(void) fprintf(stderr, m_strmsg(usage), _cmdname);
				return (USAGE);
			}
			break;
		}
	}
	/* There must be only one source file. */
	if (argc != 1) {
		(void) fprintf(stderr, m_strmsg(usage), _cmdname);
		return (USAGE);
	}
	source_file = *argv;
	init();
	compile();
	if (warnings > 0) {
		return 1;
	}
	return (0);
}

/*f
 *	Miscellaneous initializations
 *
 *	Open source file as standard input
 *	Check for access rights to destination directories
 *	Create any directories which don't exist.
 */
static void
init(void)
{
	char *s;
	char const *p;
	char dir[2];
	struct stat statbuf;
	static char const dirnames[] = "abcdefghijklmnopqrstuvwxyz0123456789";

	curr_line = 0;
	start_time = time(NULL);
	if (freopen(source_file, "r", stdin) == NULL) {
		(void) eprintf(m_strmsg(src_err), source_file);
		exit(ERROR);
	}
	if ((s = getenv("TERMINFO")) != NULL)
		destination = s; 
	if (access(destination, 7) < 0) {
		(void) eprintf(m_strmsg(dst_err), destination);
		exit(ERROR);
	}
	if (chdir(destination) < 0) {
		(void) eprintf(m_strmsg(dst_err), destination);
		exit(ERROR);
	}
	dir[1] = '\0';
	for (p = dirnames; *p != '\0'; ++p) {
		*dir = *p;
		if (stat(dir, &statbuf) < 0) {
			(void) mkdir(dir, M_DIRMODE);
		} else if (access(dir, 7) < 0) {
			(void) eprintf(m_strmsg(dstdir_err), destination, dir);
			exit(1);
		} else if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
			(void) eprintf(m_strmsg(dstdir_err), destination, dir);
			exit(1);
		}
	}
}

/*f
 *	Reset boolean[], number[], and string[].
 */
void
reset(void)
{
	int i;
	for (i = 0; i < BOOLCOUNT; ++i)
		boolean[i] = 0;
	for (i = 0; i < NUMCOUNT; ++i)
		number[i] = -1;
	for (i = 0; i < STRCOUNT; ++i)
		string[i] = -1;
}

/*f
 *	Return a linear index value.
 *
 *	Search in the following order boolnames[], numnames[], and strnames[]
 *	for the matching capability name.  Then map the array and index into
 *	a linear index.  Return -1 if capname is not valid.
 *
 *	While this linear approach is slow, TIC is seldom used once the
 *	database is created, therefore we don't bother spending extra
 *	effort to speed it up.
 */
int
find(char const *capname, void **arrayp, int *indexp)
{
	char **p;
	for (p = boolnames; *p != NULL; ++p)
		if (strcmp(*p, capname) == 0) {
			*arrayp = (void*) boolean;
			*indexp = (int)(p - boolnames);
			return (0);
		}
	for (p = numnames; *p != NULL; ++p)
		if (strcmp(*p, capname) == 0) {
			*arrayp = (void*) number;
			*indexp = (int)(p - numnames);
			return (0);
		}
	for (p = strnames; *p != NULL; ++p)
		if (strcmp(*p, capname) == 0) {
			*arrayp = (void*) string;
			*indexp = (int)(p - strnames);
			return (0);
		}
	return (-1);
}

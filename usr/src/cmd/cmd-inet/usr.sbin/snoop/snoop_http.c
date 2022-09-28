/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 */

#ident	"@(#)snoop_http.c	1.1	96/01/30 SMI"	/* SunOS	*/

#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include "snoop.h"

#define	MIN(a, b) (((a) < (b)) ? (a) : (b))
extern char *dlc_header;
static int printable();

interpret_http(flags, line, fraglen)
	int flags;
	char *line;
	int fraglen;
{
	char *p, *q;
	int c;

	if (flags & F_SUM) {
		c = printable(line);
		if (c < 10) {
			(void) sprintf(get_sum_line(),
				"HTTP (body)");
		} else {
			line[MIN(c, 80)+1] = '\0';
			(void) sprintf(get_sum_line(),
				"HTTP %s", line);
		}
	}

	if (flags & F_DTAIL) {

	show_header("HTTP: ", "HyperText Transfer Protocol", fraglen);
	show_space();
	p = line;
	for (;;) {
		c = printable(p);
		if (c == 0)
			break;

		q = strchr(p, 0x0d);
		if (q == NULL)
			break;

		if (q - p > c)	/* check for non-printable stuff */
			break;

		*(p + c) = '\0';
		(void) sprintf(get_line((char *)line - dlc_header, 1),
		"%s", p);

		p = q + 2; /* skip the CRLF */
	}
	show_space();

	}

	return (fraglen);
}

static int
printable(line)
	char *line;
{
	char *p = line;

	while (isprint(*p))
		p++;

	return (p - line);
}

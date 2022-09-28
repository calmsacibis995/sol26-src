/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_rw_file.c 1.12	96/10/01 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include <print/list.h>
#include <print/ns.h>

#include "ns_rw_file.h"


/*
 * FUNCTION:
 *	static char *_file_getline(FILE *fp)
 * INPUT:
 *	FILE *fp - file pointer to read from
 * OUTPUT:
 *	char *(return) - an entry from the stream
 * DESCRIPTION:
 *	This routine will read ina line at a time.  If the line end in  a
 *	newline, it returns.  If the line ends in a backslash newline, it
 *	continues reading more.  It will ignore lines that start in # or
 *	blank lines.
 */
static char *
_file_getline(FILE *fp)
{
	char entry[BUFSIZ], *tmp;
	int size;

	size = sizeof (entry);
	tmp  = entry;

	/* find an entry */
	while (fgets(tmp, size, fp)) {
		if ((tmp == entry) && ((*tmp == '#') || (*tmp == '\n'))) {
			continue;
		} else {
			if ((*tmp == '#') || (*tmp == '\n')) {
				*tmp = NULL;
				break;
			}

			size -= strlen(tmp);
			tmp += strlen(tmp);

			if (*(tmp-2) != '\\')
				break;

			size -= 2;
			tmp -= 2;
		}
	}

	if (tmp == entry)
		return (NULL);
	else
		return (strdup(entry));
}


/*
 * FUNCTION:
 *	static ns_printer_t *_file_next_entry(FILE *fp,
 *			      ns_printer_t *(*conv)(char *, char *), char *svc)
 * INPUT:
 *	FILE *fp - the file pointer to get the entry from
 *	ns_printer_t *(*conv)(char *, char *) - function to convert entry
 *	char *svc - source of data
 * OUTPUT:
 *	static ns_printer_t *(return) - the next printer object in the file.
 * DESCRIPTION:
 *
 */
static ns_printer_t *
_file_next_entry(FILE *fp, ns_printer_t *(*conv)(char *, char *), char *svc)

{
	char *entry;

	if ((entry = _file_getline(fp)) != NULL)
		return ((conv)(entry, svc));
	return (NULL);
}


ns_printer_t *
_file_get_name(const char *file, const char *name,
		ns_printer_t *(*conv)(char *, char *), char *svc)
{
	ns_printer_t *printer = NULL;
	FILE *fp;

	if ((fp = fopen(file, "r")) != NULL) {
		while ((printer = _file_next_entry(fp, conv, svc)) != NULL) {
			if (ns_printer_match_name(printer, name) == 0)
				break;
			ns_printer_destroy(printer);
		}
		fclose(fp);
	}

	return (printer);
}


ns_printer_t **
_file_get_list(const char *file, ns_printer_t *(*conv)(char *, char *),
		char *svc)
{
	ns_printer_t	**list = NULL,
			*printer;
	FILE *fp;

	if ((fp = fopen(file, "r")) != NULL) {
		while ((printer = _file_next_entry(fp, conv, svc)) != NULL) {
			if (list_locate((void **)list,
					(int (*)(void *, void *))
					ns_printer_match_name,
					printer->name) != NULL) {
				ns_printer_destroy(printer);
				continue;
			}

			list = (ns_printer_t **)list_append((void **)list,
							    (void *)printer);
		}
		fclose(fp);
	}

	/* Should qsort list here (maybe soon) */

	return (list);
}


int
_file_put_printer(const char *file, const ns_printer_t *printer,
		ns_printer_t *(*iconv)(char *, char *), char *svc,
		char *(*oconv)(ns_printer_t *))
{
	ns_printer_t *tmp;
	FILE	*ifp,
		*ofp;
	char *tmpfile;
	int fd;
	int exit_status = 0;

	tmpfile = malloc(strlen(file) + 1 + 20);
	(void) sprintf(tmpfile, "%s.%ld", file, getpid());

	while (1) {	/* syncronize writes */
		fd = open(file, O_RDWR|O_CREAT);
		if (fd < 0) {
			if (errno == EAGAIN)
				continue;
			free(tmpfile);
			return (-1);
		}
		if (lockf(fd, F_TLOCK, 0) == 0)
			break;
		close(fd);
	}

	if ((ifp = fdopen(fd, "r")) == NULL) {
		close(fd);
		free(tmpfile);
		return (-1);
	}

	if ((ofp = fopen(tmpfile, "wb+")) != NULL) {
		char *pentry;

		fprintf(ofp,
	"#\n#\tIf you hand edit this file, comments and structure may change.\n"
	"#\tThe preferred method of modifying this file is through the use of\n"
	"#\tlpset(1M) or fncreate_printer(1M)\n#\n");

	/*
	 * Handle the special case of lpset -x all
	 * This deletes all entries in the file
	 * In this case, just don't write any entries to the tmpfile
	 */

		if (!((strcmp(printer->name, "all") == 0) &&
				(printer->attributes == NULL))) {

			pentry = (oconv)((ns_printer_t *)printer);

			while ((tmp = _file_next_entry(ifp, iconv, svc))
								!= NULL) {
				char *entry;
				if (ns_printer_match_name(tmp, printer->name)
								== 0) {
					entry = pentry;
					pentry = NULL;
				} else
					entry = (oconv)(tmp);

				if (entry != NULL) {
					fprintf(ofp, "%s\n", entry);
					free(entry);
				}
			}

			if (pentry != NULL) {
				fprintf(ofp, "%s\n", pentry);
				free(pentry);
			}
		}

		fclose(ofp);
		rename(tmpfile, file);
	}
	else
		exit_status = -1;

	fclose(ifp);	/* releases the lock, after rename on purpose */
	chmod(file, 0644);
	free(tmpfile);
	return (exit_status);
}

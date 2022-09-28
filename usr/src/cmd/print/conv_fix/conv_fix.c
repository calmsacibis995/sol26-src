/*
 * Copyright (c) 1994 by SunSoft, Inc.
 */

#pragma ident	"@(#)conv_fix.c	1.4	96/04/21 SunSoft"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>

extern char *optarg;

/*
 * FUNCTION:
 *	static char *_file_getline(FILE *fp)
 * INPUT:
 *	FILE *fp - file pointer to read from
 * OUTPUT:
 *	char *(return) - an entry from the stream
 * DESCRIPTION:
 *	This routine will read in a line at a time.  If the line ends in a
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

main(int ac, char *av[])
{
  int   c;
  char  file[80], ofile[80];
  char *cp;
  FILE *fp, *fp2;

  while ((c = getopt(ac, av, "f:o:")) != EOF)
    switch (c) {
    case 'f':
      strcpy( file, optarg );
      break;
    case 'o':
      strcpy( ofile, optarg );
      break;
    default:
      fprintf( stderr, "Usage: %s [-f file] [-o output file]\n", av[0] );
      exit(1);
    }

  if ( (fp = fopen( file, "r" )) != NULL ) {
    if ( (fp2 = fopen( ofile, "a" )) != NULL ) {
      while ( (cp = _file_getline( fp )) != NULL ) {
	fprintf( fp2, "%s", cp );
      }
      exit(0);
    }
    else {
      fprintf( stderr, "fp2 fopen failed.\n" );
      exit(1);
    }
  }
  else {
    fprintf( stderr, "fp fopen failed.\n" );
    exit(1);
  }
}

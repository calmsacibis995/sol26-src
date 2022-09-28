/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getprimary.c	1.3	92/07/14 SMI"       /* SVr4.0 1.5 */

#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <userdefs.h>

struct group *getgrnam();
struct passwd *getpwent();

extern void exit();

main(argc, argv)
int argc;
char *argv[];
{
	struct group *gstruct;		/* group structure */
	struct passwd *pstruct;		/* passwd structure */
	register nprinted = 0;
	
	if(argc != 2) {
		(void) fprintf( stdout, "usage: getprimary group\n" );
		exit( EX_SYNTAX );
	}

	/* validate group name */
	if((gstruct = getgrnam( argv[1] )) == NULL) {
		(void) fprintf( stdout, "invalid group: %s\n",  argv[1] );
		exit( EX_BADARG );
	}

	/* search passwd file looking for matches on group */
	if( (pstruct = getpwent()) != NULL ) {

		if(pstruct->pw_gid == gstruct->gr_gid) {
			nprinted++;
			(void) fprintf( stdout, "%s", pstruct->pw_name );
		}

		while( (pstruct = getpwent()) != NULL )
			if(pstruct->pw_gid == gstruct->gr_gid) {
				(void) fprintf( stdout, "%s%s", (nprinted++? ",": ""),
					pstruct->pw_name );
			}

		(void) fprintf( stdout, "\n" );

	}
		
	exit( EX_SUCCESS );
	/*NOTREACHED*/
}
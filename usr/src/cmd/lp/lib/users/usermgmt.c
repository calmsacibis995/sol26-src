/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)usermgmt.c	1.4	93/11/18 SMI"	/* SVr4.0 1.11	*/
/* LINTLIBRARY */

# include	<stdio.h>

# include	"lp.h"
# include	"users.h"

static loaded = 0;
static struct user_priority *ppri_tbl;
struct user_priority *ld_priority_file();
static USER usr;

#if	defined(__STDC__)
int putuser ( char * user, USER * pri_s )
#else
int putuser (user, pri_s)
    char	*user;
    USER	*pri_s;
#endif
{
    FILE *f;

    if (!loaded)
    {
	if (!(ppri_tbl = ld_priority_file(Lp_Users)))
	    return(-1);
	loaded = 1;
    }

    if (!add_user(ppri_tbl, user, pri_s->priority_limit))
    {
	return(-1);
    }

    if (!(f = open_lpfile(Lp_Users, "w", LPU_MODE)))
	return(-1);
    output_tbl(f, ppri_tbl);
    close_lpfile(f);
    return(0);
}

#if	defined(__STDC__)
USER * getuser ( char * user )
#else
USER * getuser (user)
    char	*user;
#endif
{
    int limit;

    /* root and lp do not get a limit */
    if (STREQU(user, "root") || STREQU(user, LPUSER))
    {
	usr.priority_limit = 0;
	return(&usr);
    }

    if (!loaded)
    {
	if (!(ppri_tbl = ld_priority_file(Lp_Users)))
	    return((USER *)0);

	loaded = 1;
    }

    for (limit = PRI_MIN; limit <= PRI_MAX; limit++)
	if (bang_searchlist(user, ppri_tbl->users[limit - PRI_MIN]))
	{
	    usr.priority_limit = limit;
	    return(&usr);
	}

    usr.priority_limit = ppri_tbl->deflt_limit;
    return(&usr);
}

#if	defined(__STDC__)
int deluser ( char * user )
#else
int deluser (user)
    char	*user;
#endif
{
    FILE *f;

    if (!loaded)
    {
	if (!(ppri_tbl = ld_priority_file(Lp_Users)))
	    return(-1);

	loaded = 1;
    }

    del_user(ppri_tbl, user);

    if (!(f = open_lpfile(Lp_Users, "w", LPU_MODE)))
	return(-1);

    output_tbl(f, ppri_tbl);
    close_lpfile(f);
    return(0);
}

#if	defined(__STDC__)
int getdfltpri ( void )
#else
int getdfltpri ()
#endif
{
    if (!loaded)
    {
	if (!(ppri_tbl = ld_priority_file(Lp_Users)))
	    return(-1);

	loaded = 1;
    }

    return (ppri_tbl->deflt);
}

void
trashusers(void)
{
    int limit;

    if (loaded)
    {
	if (ppri_tbl)
	{
	    for (limit = PRI_MIN; limit <= PRI_MAX; limit++)
		freelist (ppri_tbl->users[limit - PRI_MIN]);
	    ppri_tbl = 0;
	}
	loaded = 0;
    }
}


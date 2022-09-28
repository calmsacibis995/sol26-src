/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __RMM_INT_H
#define	__RMM_INT_H

#pragma ident	"@(#)rmm_int.h	1.13	96/01/10 SMI"

#include <rpc/types.h>
#include <regex.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct ident_list {
	char	*i_type;		/* name of file system */
	char	*i_dsoname;		/* name of the shared object */
	char	**i_media;		/* list of appropriate media */
	bool_t	(*i_ident)(int, char *, int *, int);
};


#define A_PREMOUNT	0x01	/* execute action before mounting */

struct action_list {
	int		a_flag;		/* behavior flag */
	char		*a_dsoname;	/* name of the shared object */
	char		*a_media;	/* appropriate media */
	int		a_argc;		/* argc of action arg list */
	char		**a_argv;	/* argv of action arg list */
	bool_t		(*a_action)(struct action_arg **, int, char **);
};


/* "mount" fs types (go in ma_flags) */
#define	MA_UFS		0x1000
#define	MA_HSFS		0x2000
#define	MA_PCFS		0x4000

#define	MA_FS_ANY	(MA_UFS|MA_HSFS|MA_PCFS) /* any/all FS(s) specified */


/* "mount" options (go in ma_flags) */
#define	MA_SUID		0x002	/* mount without nosuid flag */
#define	MA_SHARE	0x004	/* export */
#define	MA_READONLY	0x020	/* mount it readonly */
#define	MA_NODOT	0x100	/* mount it (hsfs) "notraildot" */
#define	MA_NOMAPLCASE	0x200	/* mount it (hsfs) "nomaplcase" */
#define	MA_NOINTR	0x200	/* mount it (hsfs) "nointr" */
#define	MA_NORR		0x200	/* mount it (hsfs) "norr" */


struct mount_args {
	char	*ma_namere;	/* regular expression */
	regex_t	ma_re;		/* compiled regular expression */
	u_int	ma_flags;	/*  various mount options */
	char	*ma_shareflags;	/* flags to share */
};

extern char	*rmm_dsodir;	/* directory for DSO */
extern char	*rmm_config;	/* config file path */
extern int	rmm_debug;	/* debug flag */

extern char	*prog_name;	/* name of the program */
extern pid_t	prog_pid;	/* pid of the program */

extern struct ident_list 	**ident_list;
extern struct action_list 	**action_list;
extern struct mount_args	**mount_args;

void			*dso_load(char *, char *, int);
void			dprintf(const char *fmt, ...);
char			*rawpath(char *);
void			config_read(void);
char			*sh_to_regex(char *);
char			*getmntpoint(char *);
int			makepath(char *, mode_t);
bool_t			fs_supported(char *, struct mount_args *);

#define	MAX_ARGC	300
#define	MAX_IDENTS	100
#define	MAX_ACTIONS	500
#define MAX_MOUNTS	100

#define	NULLC		'\0'

#ifdef	__cplusplus
}
#endif

#endif /* __RMM_INT_H */

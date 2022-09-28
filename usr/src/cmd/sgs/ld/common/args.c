/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)args.c	1.52	96/09/30 SMI"

/*
 * Processes the command line flags.  The options recognized by ld are the
 * following:
 *
 *	OPTION			MEANING
 *
 *	-a { -dn only } make the output file executable
 *
 *	-b { -dy only }	turn off special handling for PIC/non-PIC relocations
 *
 *	-c path		specify a cache directory
 *
 *	-dy		dynamic mode: build a dynamically linked executable or
 *			a shared object - build a dynamic structure in the
 *			output file and make that file's symbols available for
 *			run-time linking
 *
 *	-dn		static mode: build a statically linked executable or a
 *			relocatable object file
 *
 *	-e name		make name the new entry point (e_entry)
 *
 *	-F name		specify that this files symbol table is acting as a
 *			filter on the symbol table of the shared object name
 *
 *	-f name		specify that this files symbol table is acting as a
 *			auxiliary filter on the symbol table of the shared
 *			object name
 *
 *	-h name { -dy -G only }
 *			make name the output filename in the dynamic structure
 *
 *	-i		ignore any LD_LIBRARY_PATH setting.
 *
 *	-I name		make name the interpreter pathname written into the
 *			program execution header of the output file
 *
 *	-lx		search for libx.[so|a] using search directories
 *
 *	-m		generate a memory map and show multiply defined symbols
 *
 *	-o name		use name as the output filename
 *
 *	-r { -dn only }	retain relocation in the output file (ie. produce a
 *			relocatable object file)
 *
 *	-R path		specify a library search path for use at run time.
 *
 *	-s		strip the debugging sections and their associated
 *			relocations from the output file
 *
 *	-t		turnoff warnings about multiply-defined symbols that
 *			are not the same size
 *
 *	-u name		make name an undefined entry in the ld symbol table
 *
 *	-z now		mark object as requiring non-lazy relocation
 *
 *	-z defs | nodefs
 *			issue a fatal error | don't, if undefined symbols remain
 *
 *	-z loadfltr	force filter to load filtees immediately at runtime
 *
 *	-z nodelete	mark object as non-deletable
 *
 *	-z muldefs 	multiply defined symbols are allowable
 *
 *	-z noversion	don't record versioning sections
 *
 *	-z text { -dy only }
 *			issue a fatal error if any text relocations remain
 *
 *	-z weakextract	allow extraction of archive members to resolve weak
 *			references
 *
 *	-B reduce	reduce symbols if possible
 *
 *	-B static	in searching for libx, choose libx.a
 *
 *	-B dynamic { -dy only }
 *			in searching for libx, choose libx.so
 *
 *	-B symbolic { -dy -G }
 *			shared object symbol resolution flag ...
 *
 *	-B group	RTLD_GROUP runtime symbol lookup symantics required
 *
 *	#ifdef	DEBUG
 *	-D option1,option2,...
 *			turn on debugging for each indicated option
 *	#endif
 *
 *	-G { -dy }	produce a shared object
 *
 *	-L path		prepend path to library search path
 *
 *	-M name		read a mapfile (if name is a dir, read all files in dir)
 *
 *	-N needed_str	create a dynamic dependency (DT_NEEDED) on needed_str
 *
 *	-Q[y|n]		add|do not add ld version to comment section of output
 *			file
 *
 *	-S name		dlopen(name) a support library
 *
 * 	-V		print ld version to stderr
 *
 *	-YP path	change LIBPATH to path
 *
 *	-YL path	change YLDIR (1st) part of LIBPATH to path
 *			(undocumented)
 *
 *	-YU path	change YUDIR (2nd) part of LIBPATH to path
 *			(undocumented)
 */
#include	<sys/link.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	"debug.h"
#include	"msg.h"
#include	"_ld.h"

/*
 * Define a set of local argument flags, the settings of these will be
 * verified in check_flags() and lead to the appropriate output file flags
 * being initialized.
 */
typedef	enum {
	SET_UNKNOWN = -1,
	SET_FALSE = 0,
	SET_TRUE = 1
} Setstate;

static Setstate	dflag	= SET_UNKNOWN;
static Setstate	zdflag	= SET_UNKNOWN;
static Setstate	Qflag	= SET_UNKNOWN;

static Boolean	aflag	= FALSE;
static Boolean	bflag	= FALSE;
static Boolean	rflag	= FALSE;
static Boolean	sflag	= FALSE;
static Boolean	zmflag	= FALSE;
static Boolean	zlflag	= FALSE;
static Boolean	zndflag	= FALSE;
static Boolean	znoflag	= FALSE;
static Boolean	znvflag	= FALSE;
static Boolean	ztflag	= FALSE;
static Boolean	zwflag	= FALSE;
static Boolean	Bgflag	= FALSE;
static Boolean	Blflag	= FALSE;
static Boolean	Brflag	= FALSE;
static Boolean	Bsflag	= FALSE;
static Boolean	Gflag	= FALSE;
static Boolean	Vflag	= FALSE;

/*
 * Prepend environment string as a series of options to the argv array.
 */
int
prepend_argv(char * ld_options, int * argcp, char *** argvp)
{
	int	nargc;			/* New argc */
	char **	nargv;			/* New argv */
	char *	arg, * string;
	int	count;

	/*
	 * Get rid of leading white space, and make sure the string has size.
	 */
	while (isspace(*ld_options))
		ld_options++;
	if (*ld_options == '\0')
		return (1);

	nargc = 0;
	arg = string = ld_options;
	/*
	 * Walk the environment string counting any arguments that are
	 * separated by white space.
	 */
	while (*string != '\0') {
		if (isspace(*string)) {
			nargc++;
			while (isspace(*string))
				string++;
			arg = string;
		} else
			string++;
	}
	if (arg != string)
		nargc++;

	/*
	 * Allocate a new argv array big enough to hold the new options
	 * from the environment string and the old argv options.
	 */
	if ((nargv = (char **)calloc(nargc + *argcp, sizeof (char *))) == 0)
		return (S_ERROR);

	/*
	 * Initialize first element of new argv array to be the first
	 * element of the old argv array (ie. calling programs name).
	 * Then add the new args obtained from the environment.
	 */
	nargv[0] = (*argvp)[0];
	nargc = 0;
	arg = string = ld_options;
	while (*string != '\0') {
		if (isspace(*string)) {
			nargc++;
			*string++ = '\0';
			nargv[nargc] = arg;
			while (isspace(*string))
				string++;
			arg = string;
		} else
			string++;
	}
	if (arg != string) {
		nargc++;
		nargv[nargc] = arg;
	}

	/*
	 * Now add the original argv array (skipping argv[0]) to the end of
	 * the new argv array, and overwrite the old argc and argv.
	 */
	for (count = 1; count < *argcp; count++) {
		nargc++;
		nargv[nargc] = (*argvp)[count];
	}
	*argcp = ++nargc;
	*argvp = nargv;

	return (1);
}

/*
 * print usage message to stderr - 2 modes, summary message only,
 * and full usage message
 */
static void
usage_mesg(Boolean detail)
{
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_USAGE),
	    MSG_ORIG(MSG_STR_OPTIONS));

	if (detail == FALSE)
		return;

	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_A));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_B));
#ifdef	ENABLE_CACHE
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_C));
#endif
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_D));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_E));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_F));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_H));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_I));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_L));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_M));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_O));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_R));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_S));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_T));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_U));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZB));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZD));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZL));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZM));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZND));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZNV));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZT));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_ZW));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CBD));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CBG));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CBL));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CBR));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CBS));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CD));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CF));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CG));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CI));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CL));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CM));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CN));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CQ));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CR));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CS));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CV));
	(void) fprintf(stderr, MSG_INTL(MSG_ARG_DETAIL_CY));
}

/*
 * Checks the command line option flags for consistency.
 */
static int
check_flags(Ofl_desc * ofl, int argc)
{
	u_longlong_t	offlags = 0;
	Word		dtflags = 0;

	if (Plibpath && (Llibdir || Ulibdir)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_YP),
		    Llibdir ? 'L' : 'U');
		offlags |= FLG_OF_FATAL;
	}

	if (rflag) {
		if (dflag == SET_UNKNOWN)
			dflag = SET_FALSE;
		offlags |= FLG_OF_RELOBJ;
	}

	if (zdflag == SET_TRUE)
		offlags |= FLG_OF_NOUNDEF;

	if (zmflag)
		offlags |= FLG_OF_MULDEFS;

	if (znvflag)
		offlags |= FLG_OF_NOVERSEC;

	if (zwflag)
		offlags |= FLG_OF_WEAKEXT;

	if (sflag)
		offlags |= FLG_OF_STRIP;

	if (Qflag == SET_TRUE)
		offlags |= FLG_OF_ADDVERS;

	if (Blflag)
		offlags |= FLG_OF_AUTOLCL;

	if (Brflag)
		offlags |= FLG_OF_PROCRED;

	if (dflag != SET_FALSE) {
		/*
		 * -Bdynamic on by default, setting is rechecked as input
		 * files are processed.
		 */
		offlags |= (FLG_OF_DYNAMIC | FLG_OF_DYNLIBS | FLG_OF_PROCRED);

		if (aflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DY), MSG_ORIG(MSG_ARG_A));
			offlags |= FLG_OF_FATAL;
		}

		if (bflag)
			offlags |= FLG_OF_BFLAG;

		if (Bgflag == TRUE) {
			if (zdflag == SET_FALSE) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_BGROUP),
				    MSG_ORIG(MSG_ARG_ZNODEF));
				offlags |= FLG_OF_FATAL;
			}
			dtflags |= DF_1_GROUP;
			offlags |= FLG_OF_NOUNDEF;
		}

		if (znoflag)
			dtflags |= DF_1_NOW;

		if (zlflag) {
			if (!(ofl->ofl_filtees)) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_NOFLTR),
				    MSG_ORIG(MSG_ARG_ZLOADFLTR));
				offlags |= FLG_OF_FATAL;
			}
			dtflags |= DF_1_LOADFLTR;
		}
		if (zndflag)
			dtflags |= DF_1_NODELETE;

		if (ztflag)
			offlags |= FLG_OF_PURETXT;

		if (!Gflag && !rflag) {
			/*
			 * Dynamically linked executable.
			 */
			offlags |= FLG_OF_EXEC;

			if (zdflag != SET_FALSE)
				offlags |= FLG_OF_NOUNDEF;

			if (Bsflag) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_BSYMBOLIC),
				    MSG_INTL(MSG_ARG_DYNEXEC));
				offlags |= FLG_OF_FATAL;
			}
			if (ofl->ofl_soname) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_H),
				    MSG_INTL(MSG_ARG_DYNEXEC));
				offlags |= FLG_OF_FATAL;
			}
			if (ofl->ofl_filtees) {
				if (ofl->ofl_flags & FLG_OF_AUX) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_ARG_INCOMP),
					    MSG_ORIG(MSG_ARG_F),
					    MSG_INTL(MSG_ARG_DYNEXEC));
				} else {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_ARG_INCOMP),
					    MSG_ORIG(MSG_ARG_CF),
					    MSG_INTL(MSG_ARG_DYNEXEC));
				}
				offlags |= FLG_OF_FATAL;
			}
		} else if (!rflag) {
			/*
			 * Shared library.
			 */
			offlags |= FLG_OF_SHAROBJ;

			if (Bsflag)
				offlags |= FLG_OF_SYMBOLIC;
		}
	} else {
		offlags |= FLG_OF_STATIC;

		if (bflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_B));
			offlags |= FLG_OF_FATAL;
		}
		if (ofl->ofl_soname) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_H));
			offlags |= FLG_OF_FATAL;
		}
		if (ofl->ofl_cache) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_C));
			offlags |= FLG_OF_FATAL;
		}
		if (ofl->ofl_filtees) {
			if (ofl->ofl_flags & FLG_OF_AUX) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_F));
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_CF));
			}
			offlags |= FLG_OF_FATAL;
		}
		if (ztflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_ZTEXT));
			offlags |= FLG_OF_FATAL;
		}
		if (Gflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_DN), MSG_ORIG(MSG_ARG_CG));
			offlags |= FLG_OF_FATAL;
		}
		if (aflag && rflag) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
			    MSG_ORIG(MSG_ARG_A), MSG_ORIG(MSG_ARG_R));
			offlags |= FLG_OF_FATAL;
		}

		if (rflag) {
			/*
			 * we can only strip the symbol table and string table
			 * if no output relocations will refer to them
			 */
			if (sflag) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_STRIP));
			}
			if (ofl->ofl_interp) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_R), MSG_ORIG(MSG_ARG_CI));
				offlags |= FLG_OF_FATAL;
			}
		} else {
			/*
			 * Static executable.
			 */
			offlags |= FLG_OF_EXEC | FLG_OF_PROCRED;

			if (zdflag != SET_FALSE)
				offlags |= FLG_OF_NOUNDEF;

		}
	}
	ofl->ofl_flags |= offlags;
	ofl->ofl_dtflags |= dtflags;

	/*
	 * If the user didn't supply an output file name supply a default.
	 */
	if (ofl->ofl_name == NULL)
		ofl->ofl_name = MSG_ORIG(MSG_STR_AOUT);

	/*
	 * We set the entrance criteria after all input argument processing as
	 * it is only at this point we're sure what the output image will be
	 * (static or dynamic).
	 */
	if (ent_setup(ofl) == S_ERROR)
		return (S_ERROR);

	/*
	 * Process any mapfiles after establishing the entrance criteria as
	 * the user may be redefining or adding sections/segments.
	 */
	if (ofl->ofl_maps.head) {
		Listnode *	lnp;
		const char *	name;

		for (LIST_TRAVERSE(&ofl->ofl_maps, lnp, name))
			if (map_parse(name, ofl) == S_ERROR)
				return (S_ERROR);

		if (ofl->ofl_flags & FLG_OF_SEGSORT)
			return (sort_seg_list(ofl));
	}

	/*
	 * Check that we have something to work with.  This check is carried out
	 * after mapfile processing as its possible a mapfile is being used to
	 * define symbols, in which case it would be sufficient to build the
	 * output file purely from the mapfile.
	 */
	if (!files) {
		if (Vflag && (argc == 2))
			exit(EXIT_SUCCESS);
		else {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_NOFILES));
			exit(EXIT_FAILURE);
		}
	}
	return (1);
}

/*
 *
 * Pass 1 -- process_flags: collects all options and sets flags
 */
int
process_flags(Ofl_desc * ofl, int argc, char ** argv)
{
	int	error = 0;	/* Collect all argument errors before exit */
	int	c;		/* character returned by getopt */

	if (argc < 2) {
		usage_mesg(FALSE);
		return (S_ERROR);
	}

getmore:
	while ((c = getopt(argc, argv, MSG_ORIG(MSG_STR_OPTIONS))) != -1) {
		DBG_CALL(Dbg_args_flags((optind - 1), c));

		switch (c) {

		case 'a':
			aflag = TRUE;
			break;

		case 'b':
			bflag = TRUE;
			break;

#ifdef	ENABLE_CACHE
		case 'c':
			if (ofl->ofl_cache)
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_MTONCE),
				    MSG_ORIG(MSG_ARG_C));
			else
				ofl->ofl_cache = (const char *)optarg;
			break;
#endif

		case 'd':
			if ((optarg[0] == 'n') && (optarg[1] == '\0')) {
				if (dflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_D));
				else
					dflag = SET_FALSE;
			} else if ((optarg[0] == 'y') && (optarg[1] == '\0')) {
				if (dflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_D));
				else
					dflag = SET_TRUE;
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
				    MSG_ORIG(MSG_ARG_D), optarg);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
			break;

		case 'e':
			if (ofl->ofl_entry)
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_MTONCE),
				    MSG_ORIG(MSG_ARG_E));
			else
				ofl->ofl_entry = (void *)optarg;
			break;

		case 'f':
			if (ofl->ofl_filtees &&
			    (!(ofl->ofl_flags & FLG_OF_AUX))) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_F), MSG_ORIG(MSG_ARG_CF));
				ofl->ofl_flags |= FLG_OF_FATAL;
			} else {
				if ((ofl->ofl_filtees =
				    add_string(ofl->ofl_filtees, optarg)) ==
				    (const char *)S_ERROR)
					return (S_ERROR);
				ofl->ofl_flags |= FLG_OF_AUX;
			}
			break;

		case 'F':
			if (ofl->ofl_filtees &&
			    (ofl->ofl_flags & FLG_OF_AUX)) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_INCOMP),
				    MSG_ORIG(MSG_ARG_CF), MSG_ORIG(MSG_ARG_F));
				ofl->ofl_flags |= FLG_OF_FATAL;
			} else {
				if ((ofl->ofl_filtees =
				    add_string(ofl->ofl_filtees, optarg)) ==
				    (const char *)S_ERROR)
					return (S_ERROR);
			}
			break;

		case 'h':
			if (ofl->ofl_soname)
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_MTONCE),
				    MSG_ORIG(MSG_ARG_H));
			else
				ofl->ofl_soname = (const char *)optarg;
			break;

		case 'i':
			ofl->ofl_flags |= FLG_OF_IGNENV;
			break;

		case 'I':
			if (ofl->ofl_interp)
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_MTONCE),
				    MSG_ORIG(MSG_ARG_CI));
			else
				ofl->ofl_interp = (const char *)optarg;
			break;

		case 'l':
			files = TRUE;
			break;

		case 'm':
			ofl->ofl_flags |= FLG_OF_GENMAP;
			break;

		case 'o':
			if (ofl->ofl_name)
				eprintf(ERR_WARNING, MSG_INTL(MSG_ARG_MTONCE),
				    MSG_ORIG(MSG_ARG_O));
			else
				ofl->ofl_name = (const char *)optarg;
			break;
		case 'r':
			rflag = TRUE;
			break;

		case 'R':
			/*
			 * Multiple instances of this option may occur.  Each
			 * additional instance is effectively concatenated to
			 * the previous separated by a colon.
			 */
			if ((ofl->ofl_rpath = add_string(ofl->ofl_rpath,
			    optarg)) == (const char *)S_ERROR)
				return (S_ERROR);
			break;

		case 's':
			sflag = TRUE;
			break;

		case 't':
			ofl->ofl_flags |= FLG_OF_NOWARN;
			break;

		case 'u':
			break;

		case 'z':
			if (strcmp(optarg, MSG_ORIG(MSG_ARG_DEFS)) == 0) {
				if (zdflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_ZDEFNODEF));
				else
					zdflag = SET_TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_NODEFS)) == 0) {
				if (zdflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_ZDEFNODEF));
				else
					zdflag = SET_FALSE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_NOVERSION)) == 0) {
				znvflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_TEXT)) == 0) {
				ztflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_MULDEFS)) == 0) {
				zmflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_NODELETE)) == 0) {
				zndflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_NOW)) == 0) {
				znoflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_WEAKEXT)) == 0) {
				zwflag = TRUE;
			} else if (strcmp(optarg,
			    MSG_ORIG(MSG_ARG_LOADFLTR)) == 0) {
				zlflag = TRUE;
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
				    MSG_ORIG(MSG_ARG_Z), optarg);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
			break;

		case 'D':
			/*
			 * If we have not yet read any input files go ahead
			 * and process any debugging options (this allows any
			 * argument processing, entrance criteria and library
			 * initialization to be displayed).  Otherwise, if an
			 * input file has been seen, skip interpretation until
			 * process_files (this allows debugging to be turned
			 * on and off around individual groups of files).
			 */
			if (!files)
				if ((dbg_mask = dbg_setup(optarg)) == S_ERROR)
					exit(0);
			break;

		case 'B':
			if (strcmp(optarg, MSG_ORIG(MSG_STR_SYMBOLIC)) == 0)
				Bsflag = TRUE;
			else if (strcmp(optarg, MSG_ORIG(MSG_ARG_REDUCE)) == 0)
				Brflag = TRUE;
			else if (strcmp(optarg, MSG_ORIG(MSG_ARG_LOCAL)) == 0)
				Blflag = TRUE;
			else if (strcmp(optarg, MSG_ORIG(MSG_ARG_GROUP)) == 0)
				Bgflag = TRUE;
			else if (strcmp(optarg, MSG_ORIG(MSG_STR_DYNAMIC)) &&
			    strcmp(optarg, MSG_ORIG(MSG_ARG_STATIC))) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
				    MSG_ORIG(MSG_ARG_CB), optarg);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
			break;

		case 'G':
			Gflag = TRUE;
			break;

		case 'L':
			break;

		case 'M':
			if (list_appendc(&(ofl->ofl_maps), optarg) == 0)
				return (S_ERROR);
			break;

		case 'N':
			break;

		case 'Q':
			if ((optarg[0] == 'n') && (optarg[1] == '\0')) {
				if (Qflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_CQ));
				else
					Qflag = SET_FALSE;
			} else if ((optarg[0] == 'y') && (optarg[1] == '\0')) {
				if (Qflag != SET_UNKNOWN)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_CQ));
				else
					Qflag = SET_TRUE;
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
				    MSG_ORIG(MSG_ARG_CQ), optarg);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
			break;

		case 'S':
			if (list_appendc(&lib_support, optarg) == 0)
				return (S_ERROR);
			break;

		case 'V':
			if (!Vflag)
				(void) fprintf(stderr, ofl->ofl_sgsid);
			Vflag = TRUE;
			break;

		case 'Y':
			if (strncmp(optarg, MSG_ORIG(MSG_ARG_LCOM), 2) == 0) {
				if (Llibdir)
				    eprintf(ERR_WARNING,
					MSG_INTL(MSG_ARG_MTONCE),
					MSG_ORIG(MSG_ARG_CYL));
				else
					Llibdir = optarg + 2;
			} else if (strncmp(optarg,
			    MSG_ORIG(MSG_ARG_UCOM), 2) == 0) {
				if (Ulibdir)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_CYU));
				else
					Ulibdir = optarg + 2;
			} else if (strncmp(optarg,
			    MSG_ORIG(MSG_ARG_PCOM), 2) == 0) {
				if (Plibpath)
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_ARG_MTONCE),
					    MSG_ORIG(MSG_ARG_CYP));
				else
					Plibpath = optarg + 2;
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
				    MSG_ORIG(MSG_ARG_CY), optarg);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
			break;

		case '?':
			error++;
			break;

		default:
			break;
		}
	}
	for (; optind < argc; optind++) {

		/*
		 * If we detect some more options return to getopt().
		 * Checking argv[optind][1] against null prevents a forever
		 * loop if an unadorned `-' argument is passed to us.
		 */
		if (argv[optind][0] == '-')
			if (argv[optind][1] == '\0')
				continue;
			else
				goto getmore;
		files = TRUE;
	}

	/*
	 * Having parsed everything, did we have any errors.
	 */
	if (error) {
		usage_mesg(TRUE);
		return (S_ERROR);
	}

	return (check_flags(ofl, argc));
}

/*
 * Pass 2 -- process_files: skips the flags collected in pass 1 and processes
 * files.
 */
int
process_files(Ofl_desc * ofl, int argc, char ** argv)
{
	int	c;

	optind = 1;		/* reinitialize optind */
getmore:
	while ((c = getopt(argc, argv, MSG_ORIG(MSG_STR_OPTIONS))) != -1) {
		Ifl_desc *	ifl;

		DBG_CALL(Dbg_args_flags((optind - 1), c));
		switch (c) {
			case 'l':
				if (find_library(optarg, ofl) == S_ERROR)
					return (S_ERROR);
				break;
			case 'B':
				if (strcmp(optarg,
				    MSG_ORIG(MSG_STR_DYNAMIC)) == 0) {
					if (ofl->ofl_flags & FLG_OF_DYNAMIC)
						ofl->ofl_flags |=
							FLG_OF_DYNLIBS;
					else {
						eprintf(ERR_FATAL,
						    MSG_INTL(MSG_ARG_INCOMP),
						    MSG_ORIG(MSG_ARG_DN),
						    MSG_ORIG(MSG_ARG_BDYNAMIC));
						ofl->ofl_flags |= FLG_OF_FATAL;
					}
				} else if (strcmp(optarg,
				    MSG_ORIG(MSG_ARG_STATIC)) == 0)
					ofl->ofl_flags &= ~FLG_OF_DYNLIBS;
				break;
			case 'L':
				if (add_libdir(ofl, optarg) == S_ERROR)
					return (S_ERROR);
				break;
			case 'N':
				/*
				 * Record DT_NEEDED string
				 */
				if (!(ofl->ofl_flags & FLG_OF_DYNAMIC)) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_ARG_INCOMP),
					    MSG_ORIG(MSG_ARG_DN),
					    MSG_ORIG(MSG_ARG_N));
					ofl->ofl_flags |= FLG_OF_FATAL;
				}
				if ((ifl = (Ifl_desc *)libld_calloc(1,
				    sizeof (Ifl_desc))) == (Ifl_desc *)S_ERROR)
					return (S_ERROR);
				ifl->ifl_name = MSG_ORIG(MSG_STR_COMMAND);
				ifl->ifl_soname = optarg;
				ifl->ifl_flags = FLG_IF_NEEDSTR;
				if (list_appendc(&ofl->ofl_sos, ifl) == 0)
					return (S_ERROR);
				break;
			case 'D':
				dbg_mask = dbg_setup(optarg);
				break;
			case 'u':
				if (sym_add_u(optarg, ofl) ==
				    (Sym_desc *)S_ERROR)
					return (S_ERROR);
				break;
			default:
				break;
			}
		}
	for (; optind < argc; optind++) {
		int	fd;

		/*
		 * If we detect some more options return to getopt().
		 * Checking argv[optind][1] against null prevents a forever
		 * loop if an unadorned `-' argument is passed to us.
		 */
		if (argv[optind][0] == '-') {
			if (argv[optind][1] == '\0')
				continue;
			else
				goto getmore;
		}
		if ((fd = open(argv[optind], O_RDONLY)) == -1) {
			int err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), argv[optind],
			    strerror(err));
			ofl->ofl_flags |= FLG_OF_FATAL;
			continue;
		}

		DBG_CALL(Dbg_args_files(optind, argv[optind]));

		if (process_open(argv[optind], 0, fd, ofl,
		    (FLG_IF_CMDLINE | FLG_IF_NEEDED)) == (Ifl_desc *)S_ERROR)
			return (S_ERROR);
	}

	/*
	 * If we've had some form of fatal error while processing the command
	 * line files we might as well return now.
	 */
	if (ofl->ofl_flags & FLG_OF_FATAL)
		return (1);

	/*
	 * If any version definitions have been established, either via input
	 * from a mapfile or from the input relocatable objects, make sure any
	 * version dependencies are satisfied, and version symbols created.
	 */
	if (ofl->ofl_verdesc.head)
		if (vers_check_defs(ofl) == S_ERROR)
			return (S_ERROR);

	/*
	 * Now that all command line files have been processed see if there are
	 * any additional `needed' shared object dependencies.
	 */
	if (ofl->ofl_soneed.head)
		if (finish_libs(ofl) == S_ERROR)
			return (S_ERROR);

	/*
	 * If segment ordering was specified (using mapfile) verify things
	 * are ok.
	 */
	if (ofl->ofl_flags & FLG_OF_SEGORDER)
		ent_check(ofl);

	return (1);
}

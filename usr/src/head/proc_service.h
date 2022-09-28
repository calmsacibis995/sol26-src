/*
 *      Copyright (c) 1995, by Sun Microsytems, Inc.
 *	All rights reserved.
 */

#ifndef _PROC_SERVICE_H
#define	_PROC_SERVICE_H

#pragma ident	"@(#)proc_service.h	1.10	96/09/17 SMI"

/*
 *
 *  Description:
 *	Types, global variables, and function definitions for provider
 * of import functions for user of libthread_db.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/lwp.h>
#include <sys/auxv.h>
#include <elf.h>

typedef unsigned long	psaddr_t;

typedef enum {
	PS_OK,		/* generic "call succeeded" */
	PS_ERR,		/* generic. */
	PS_BADPID,	/* bad process handle */
	PS_BADLID,	/* bad lwp identifier */
	PS_BADADDR,	/* bad address */
	PS_NOSYM,	/* p_lookup() could not find given symbol */
	PS_NOFREGS
			/*
			 * FPU register set not available for given
			 * lwp
			 */
}	ps_err_e;

struct ps_prochandle;

#define	PS_OBJ_EXEC	((const char *)0x0)
#define	PS_OBJ_LDSO	((const char *)0x1)


extern ps_err_e	ps_pget_ehdr(const struct ps_prochandle *,
			const char *, Elf32_Ehdr *);

extern ps_err_e	ps_pauxv(const struct ps_prochandle *, auxv_t **);

extern ps_err_e	ps_pstop(const struct ps_prochandle *);
extern ps_err_e	ps_pcontinue(const struct ps_prochandle *);
extern ps_err_e	ps_lstop(const struct ps_prochandle *, lwpid_t);
extern ps_err_e ps_lcontinue(const struct ps_prochandle *, lwpid_t);
extern ps_err_e ps_pglobal_lookup(const struct ps_prochandle *,
			const char *, const char *, psaddr_t *);
extern ps_err_e	ps_pglobal_sym(const struct ps_prochandle *,
			const char *, const char *, Elf32_Sym *);
extern ps_err_e ps_pdread(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_pdwrite(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_ptread(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_ptwrite(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_pread(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_pwrite(const struct ps_prochandle *,
			psaddr_t, char *, int);
extern ps_err_e ps_lgetregs(const struct ps_prochandle *,
			lwpid_t, prgregset_t);
/*
 * These two need to be weak for compatibility with older
 * libthread_db clients.
 */
#pragma weak ps_lrolltoaddr
#pragma weak ps_kill
extern ps_err_e	ps_lrolltoaddr(const struct ps_prochandle *,
			lwpid_t, psaddr_t, psaddr_t);
extern ps_err_e ps_kill(const struct ps_prochandle *, const int);
extern ps_err_e ps_lsetregs(const struct ps_prochandle *,
		lwpid_t, const prgregset_t);
extern void	ps_plog(const char *fmt, ...);

#if	defined(sparc) || defined(__sparc)
extern ps_err_e ps_lgetxregsize(const struct ps_prochandle *,
		lwpid_t, int *);
extern ps_err_e ps_lgetxregs(const struct ps_prochandle *,
		lwpid_t, caddr_t);
extern ps_err_e ps_lsetxregs(const struct ps_prochandle *,
		lwpid_t, caddr_t);
#endif

extern ps_err_e ps_lgetfpregs(const struct ps_prochandle *,
			lwpid_t, prfpregset_t *);
extern ps_err_e ps_lsetfpregs(const struct ps_prochandle *,
			lwpid_t, const prfpregset_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _PROC_SERVICE_H */

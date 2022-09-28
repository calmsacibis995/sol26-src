/*
 * Copyright (c) 1991, Sun Microsystems, Inc.
 */

#ifndef	_SYS_PROMIF_H
#define	_SYS_PROMIF_H

#pragma ident	"@(#)promif.h	1.14	96/10/15 SMI"

#include <sys/types.h>
#include <sys/obpdefs.h>
#include <sys/archsystm.h>
#include <sys/varargs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * these defines are added to get some nfs_inet files compiled.
 * 1275promif library ignores them for prom_read/prom_write calls.
 */
#define	BLOCK	0
#define	NETWORK	1
#define	BYTE	2

/*
 * resource allocation group: OBP and IEEE 1275-1994.
 */
extern	caddr_t		prom_alloc(caddr_t virthint, u_int size, u_int align);
extern	void		prom_free(caddr_t virt, u_int size);
extern	caddr_t		prom_map(caddr_t virthint, u_int space, u_int phys,
			    u_int size);

extern	caddr_t		prom_malloc(caddr_t virt, u_int size, u_int align);
extern	caddr_t		prom_allocate_virt(u_int align, u_int size);
extern	void		prom_unmap_virt(u_int size, caddr_t virt);

/*
 * Device tree and property group: OBP and IEEE 1275-1994.
 */
extern	dnode_t		prom_childnode(dnode_t nodeid);
extern	dnode_t		prom_nextnode(dnode_t nodeid);
extern	dnode_t		prom_parentnode(dnode_t nodeid);
extern	dnode_t		prom_rootnode(void);
extern	dnode_t		prom_chosennode(void);
extern	dnode_t		prom_alias_node(void);

extern	int		prom_getproplen(dnode_t nodeid, caddr_t name);
extern	int		prom_getprop(dnode_t nodeid, caddr_t name,
			    caddr_t value);
extern	int		prom_getintprop(dnode_t nodeid, caddr_t name,
			    int *value);
extern	caddr_t		prom_nextprop(dnode_t nodeid, caddr_t previous,
			    caddr_t next);
extern	int		prom_setprop(dnode_t nodeid, caddr_t name,
			    caddr_t value, int len);

extern	int		prom_getnode_byname(dnode_t id, char *name);
extern	int		prom_devicetype(dnode_t id, char *type);

/*
 * Device tree and property group: IEEE 1275-1994 Only.
 */
extern	dnode_t		prom_finddevice(char *path);

extern	int		prom_bounded_getprop(dnode_t nodeid,
			    caddr_t name, caddr_t buffer, int buflen);

extern	char		*prom_decode_composite_string(void *buf,
			    size_t buflen, char *prev);

extern	phandle_t	prom_getphandle(ihandle_t i);

/*
 * Device pathnames and pathname conversion: OBP and IEEE 1275-1994.
 */
extern	int		prom_devname_from_pathname(char *path, char *buffer);
extern	char		*prom_path_options(char *pathname);
extern	char		*prom_path_gettoken(char *from, char *to);
extern	void		prom_pathname(char *pathname);
extern	void		prom_strip_options(char *from, char *to);

/*
 * Device pathnames and pathname conversion: IEEE 1275-1994 only.
 */
extern	int		prom_ihandle_to_path(ihandle_t, char *buf,
			    u_int buflen);
extern	int		prom_phandle_to_path(phandle_t, char *buf,
			    u_int buflen);

/*
 * Special device nodes: OBP and IEEE 1275-1994.
 */
extern	ihandle_t	prom_stdin_ihandle(void);
extern	ihandle_t	prom_stdout_ihandle(void);
extern	dnode_t		prom_stdin_node(void);
extern	dnode_t		prom_stdout_node(void);
extern	char		*prom_stdinpath(void);
extern	char		*prom_stdoutpath(void);
extern	int		prom_stdin_devname(char *buffer);
extern	int		prom_stdout_devname(char *buffer);
extern	int		prom_stdin_is_keyboard(void);
extern	int		prom_stdout_is_framebuffer(void);
extern	int		prom_stdin_stdout_equivalence(void);

/*
 * Special device nodes: IEEE 1275-1994 only.
 */
extern	ihandle_t	prom_memory_ihandle(void);
extern	ihandle_t	prom_mmu_ihandle(void);

/*
 * Administrative group: OBP and IEEE 1275-1994.
 */
extern	void		prom_enter_mon(void);
extern	void		prom_exit_to_mon(void);
extern	void		prom_reboot(char *bootstr);

extern	void		prom_panic(char *string);

extern	int		prom_getversion(void);
extern	int		prom_is_openprom(void);
extern	int		prom_is_p1275(void);
extern	int		prom_version_name(char *buf, int buflen);

extern	u_int		prom_gettime(void);

extern	char		*prom_bootpath(void);
extern	char		*prom_bootargs(void);

extern	void		prom_interpret(char *str, int arg1,
			    int arg2, int arg3, int arg4, int arg5);

/*
 * Administrative group: OBP only.
 */
#if	0
extern	int		prom_sethandler(void (*v0_func)(), void (*v2_func)());

extern	struct bootparam *prom_bootparam(void);
#endif

/*
 * Administrative group: IEEE 1275-1994 only.
 */
extern void		*prom_set_callback(void *handler);

extern	int		prom_test(char *service);

/*
 * Promif support group: Generic.
 */
extern	void		prom_init(char *progname, void *prom_cookie);

extern	void		(*prom_set_preprom(void (*)(void)))(void);
extern	void		(*prom_set_postprom(void (*)(void)))(void);

extern	void		prom_montrap(void (*funcptr)());

#ifndef	lint
extern	void		prom_fatal_error(char *errmsg); /* internal promif */
#endif	lint

/*
 * I/O Group: OBP and IEEE 1275.
 */
extern	u_char		prom_getchar(void);
extern	void		prom_putchar(char c);
extern	int		prom_mayget(void);
extern	int		prom_mayput(char c);

extern  int		prom_open(char *name);
extern  int		prom_close(int fd);
extern  int		prom_read(ihandle_t fd, caddr_t buf, u_int len,
			    u_int startblk, char type);
extern  int		prom_write(ihandle_t fd, caddr_t buf, u_int len,
			    u_int startblk, char type);
extern	int		prom_seek(int fd, unsigned long long offset);

extern	void		prom_writestr(char *buf, u_int bufsize);
extern	void		prom_dnode_to_pathname(dnode_t, char *);

extern	void		prom_printf(char *fmt, ...);
extern	void		prom_vprintf(char *fmt, va_list adx);
extern	char		*prom_sprintf(char *s, char *fmt, ...);
extern	char		*prom_vsprintf(char *s, char *fmt, va_list adx);

/*
 * promif tree searching routines ... OBP and IEEE 1275-1994.
 */
typedef struct prom_stack {
		dnode_t *sp;
		dnode_t *minstack;
		dnode_t *maxstack;
} pstack_t;

extern	pstack_t	*prom_stack_init(dnode_t *buf, size_t max);
extern	void		prom_stack_fini(pstack_t *ps);

extern	dnode_t		prom_findnode_byname(dnode_t id, char *name,
			    pstack_t *ps);
extern	dnode_t		prom_findnode_bydevtype(dnode_t id, char *devtype,
			    pstack_t *ps);

#define	PROM_STOP	{	\
	prom_printf("File %s line %d\n", __FILE__, __LINE__); \
	prom_enter_mon();	\
}

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROMIF_H */

/*
 * Copyright (c) 1991-1994, Sun Microsystems, Inc.
 */

#ifndef	_SYS_PROMIF_H
#define	_SYS_PROMIF_H

#pragma ident	"@(#)promif.h	1.12	96/10/15 SMI"

#include <sys/types.h>
#include <sys/cpu.h>
#ifdef	_KERNEL
#include <sys/obpdefs.h>
#endif	/* _KERNEL */
#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/archsystm.h>
#include <sys/varargs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  These are for V0 ops only.  We sometimes have to specify
 *  in the promlib which type of operation we need to perform
 *  and since we can't such a property from a V0 prom, we
 *  sometimes just assume it.  V2 and later proms do the right thing.
 */
#define	BLOCK	0
#define	NETWORK	1
#define	BYTE	2

extern	caddr_t		prom_alloc(caddr_t virthint, u_int size, int align);
extern	char		*prom_bootargs(void);
extern	char		*prom_bootpath(void);
extern	void		prom_enter_mon(void);
extern	void		prom_exit_to_mon(void);
extern	void		prom_free(caddr_t virt, u_int size);
extern	u_char		prom_getchar(void);
extern	u_int		prom_gettime(void);
extern	void		prom_init(char *progname, void *cookie);
extern	int		prom_is_openprom(void);
extern	int		prom_is_p1275(void);
extern	int		prom_version_name(char *buf, int buflen);

extern	int		prom_devname_from_pathname(char *path, char *buffer);

extern	caddr_t		prom_map(caddr_t virthint, u_int space,
			    u_int phys, u_int size);
extern  int		prom_close(int fd);
extern  int		prom_open(char *name);
extern  int		prom_read(int fd, caddr_t buf, u_int len,
			    u_int startblk, char type);
extern	int		prom_write(int fd, caddr_t buf, u_int len,
			    u_int startblk, char devtype);
extern	int		prom_seek(int fd, unsigned long long offset);
extern	void		prom_panic(char *string);
extern	void		prom_printf(char *fmt, ...);
extern	void		prom_vprintf(char *fmt, va_list adx);
extern	char		*prom_sprintf(char *s, char *fmt, ...);
extern	char		*prom_vsprintf(char *s, char *fmt, va_list adx);
extern	void		prom_putchar(char c);
extern	void		prom_reboot(char *bootstr);
extern	dnode_t		prom_nextnode(dnode_t nodeid);
extern	dnode_t		prom_childnode(dnode_t nodeid);
extern	dnode_t		prom_optionsnode(void);
extern	int		prom_getproplen(dnode_t nodeid, caddr_t name);
extern	int		prom_getprop(dnode_t nodeid, caddr_t name,
				caddr_t value);
extern	caddr_t		prom_nextprop(dnode_t nodeid, caddr_t previous,
				caddr_t next);

extern	char		*prom_decode_composite_string(void *buf,
				size_t buflen, char *prev);

extern	int		prom_stdin_is_keyboard(void);
extern	int		prom_stdout_is_framebuffer(void);

#define	PROM_STOP	{	\
	prom_printf("File %s line %d\n", __FILE__, __LINE__); \
	prom_enter_mon();	\
}

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROMIF_H */

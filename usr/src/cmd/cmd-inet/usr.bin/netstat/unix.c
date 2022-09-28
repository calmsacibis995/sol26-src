/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)unix.c	1.9	96/05/30 SMI"	/* SVr4.0 1.2 */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 		All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stream.h>
#include <sys/tiuser.h>
#include <sys/socketvar.h>
#include <sys/stropts.h>
#include <sys/un.h>

#include <sys/kmem.h>
#include <sys/kmem_impl.h>

extern void	readmem(long, int, int, char *, unsigned, char *);
static char *	typetoname();

static void	kmsocket(void *kaddr, void *buf);
kmem_cache_t *	kmem_cache_find(char *name);
int		kmem_cache_apply(kmem_cache_t *kcp,
				void (*func)(void *kaddr, void *buf));

static int first;

/*
 * Print a summary of connections related to a unix protocol.
 */
void
unixpr()
{
	first = 1;
	kmem_cache_apply(kmem_cache_find("sock_cache"), kmsocket);

}

/*
 * Print one socket structure at address kaddr.
 */
/* ARGSUSED1 */
static void
kmsocket(void *kaddr, void *buf)
{
	struct sonode		sos;
	struct sonode		*so;
	struct sockaddr_un	*sa;
	long conn_vp, local_vp;

	readmem((unsigned)kaddr, 1, 0, (char *)&sos, sizeof (sos),
		"socket table slot");
	so = &sos;

	if (so->so_family != AF_UNIX)
		return;

	if (first) {
		(void) printf("Active UNIX domain sockets\n");
		(void) printf("%-8.8s %-10.10s %8.8s %8.8s  "
				"Local Addr      Remote Addr\n",
				"Address", "Type", "Vnode", "Conn");
		first = 0;
	}
	(void) printf("%8lx ", (long)kaddr);
	(void) printf("%-10.10s ", typetoname(so->so_serv_type));
	/* XXX get inode number? Extract vp instead */
	if ((so->so_state & SS_ISBOUND) &&
	    (so->so_ux_laddr.sou_magic == SOU_MAGIC_EXPLICIT)) {
		local_vp = (long)so->so_ux_laddr.sou_vp;
	} else {
		local_vp = 0;
	}
	if ((so->so_state & SS_ISCONNECTED) &&
	    (so->so_ux_faddr.sou_magic == SOU_MAGIC_EXPLICIT)) {
		conn_vp = (long)so->so_ux_faddr.sou_vp;
	} else {
		conn_vp = 0;
	}

	(void) printf("%8lx %8lx", local_vp, conn_vp);
	if ((so->so_state & SS_ISBOUND) &&
	    so->so_laddr_sa != NULL && so->so_laddr_len != 0) {
		if (so->so_state & SS_FADDR_NOXLATE) {
			(void) printf(" (socketpair)  ");
		} else {
			/*
			 * Read in the local address
			 */
			sa = (struct sockaddr_un *)calloc(1, so->so_laddr_len);
			if (sa == NULL) {
				(void) printf("\n");
				return;
			}
			readmem((unsigned)so->so_laddr_sa, 1, -1, (char *)sa,
				so->so_laddr_len, "socket laddr");
			if (so->so_laddr_len > sizeof (sa->sun_family))
				(void) printf("%*.*s ",
				    so->so_laddr_len - sizeof (sa->sun_family),
				    so->so_laddr_len - sizeof (sa->sun_family),
				    sa->sun_path);
			else
				(void) printf("               ");
			free(sa);
			sa = NULL;
		}
	} else
		(void) printf("               ");

	if ((so->so_state & SS_ISCONNECTED) &&
	    so->so_faddr_sa != NULL && so->so_faddr_len != 0) {
		if (so->so_state & SS_FADDR_NOXLATE) {
			(void) printf(" (socketpair)  ");
		} else {
			/*
			 * Read in the remote address
			 */
			sa = (struct sockaddr_un *)calloc(1, so->so_faddr_len);
			if (sa == NULL) {
				(void) printf("\n");
				return;
			}
			readmem((unsigned)so->so_faddr_sa, 1, -1, (char *)sa,
				so->so_faddr_len, "socket faddr");
			if (so->so_faddr_len > sizeof (sa->sun_family))
				(void) printf("%*.*s",
				    so->so_faddr_len - sizeof (sa->sun_family),
				    so->so_faddr_len - sizeof (sa->sun_family),
				    sa->sun_path);
			else
				(void) printf("               ");

			free(sa);
			sa = NULL;
		}
	} else
		(void) printf("               ");

	(void) printf("\n");
}

static char *
typetoname(type)
{
	switch (type) {
	case T_CLTS:
		return ("dgram");

	case T_COTS:
		return ("stream");

	case T_COTS_ORD:
		return ("stream-ord");

	default:
		return ("");
	}
}


/*
 * Find the kernel address of the cache with the specified name
 */
kmem_cache_t *
kmem_cache_find(char *name)
{
	extern u_int kmem_null_cache_addr;
	kmem_cache_t c, *cp;

	if (kmem_null_cache_addr == 0)
		fail(0, "kmem_null_cache not in symbol table\n");
	readmem(kmem_null_cache_addr, 0, 0, (char *)&c, sizeof (c),
		"kmem null cache");
	for (cp = c.cache_next; cp != (kmem_cache_t *)kmem_null_cache_addr;
	    cp = c.cache_next) {
		readmem((u_int)cp, 0, 0, (char *)&c, sizeof (c), "cache");
		if (strcmp(c.cache_name, name) == 0)
			return (cp);
	}
	return (NULL);
}

/*
 * Apply func to each allocated object in the specified kmem cache
 */
int
kmem_cache_apply(kmem_cache_t *kcp, void (*func)(void *kaddr, void *buf))
{
	kmem_cache_t *cp;
	kmem_magazine_t *kmp, *mp;
	kmem_slab_t s, *sp;
	kmem_bufctl_t bc, *bcp;
	void *buf, *ubase, *kbase, **maglist;
	int magsize, magbsize, magcnt, magmax;
	int chunks, refcnt, flags, bufsize, chunksize, i, cpu_seqid;
	char *valid;
	int errcode = -1;
	extern u_int ncpus_addr;
	int ncpus, csize;

	if (ncpus_addr == 0)
		fail(0, "ncpus not in symbol table\n");
	readmem(ncpus_addr, 0, 0, (char *)&ncpus, sizeof (int), "ncpus");

	csize = KMEM_CACHE_SIZE(ncpus);
	cp = malloc(csize);

	readmem((u_long)kcp, 0, 0, (void *)cp, csize, "cache");

	magsize = cp->cache_magazine_size;
	magbsize = sizeof (kmem_magazine_t) + (magsize - 1) * sizeof (void *);
	mp = malloc(magbsize);

	magmax = (cp->cache_fmag_total + 2 * ncpus + 100) * magsize;
	maglist = malloc(magmax * sizeof (void *));
	magcnt = 0;

	kmp = cp->cache_fmag_list;
	while (kmp != NULL) {
		readmem((u_long)kmp, 0, 0, (void *)mp, magbsize,
			"kmem magazine");
		for (i = 0; i < magsize; i++) {
			maglist[magcnt] = mp->mag_round[i];
			if (++magcnt > magmax)
				goto out2;
		}
		kmp = mp->mag_next;
	}

	for (cpu_seqid = 0; cpu_seqid < ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		if (ccp->cc_rounds <= 0)
			break;
		if ((kmp = ccp->cc_loaded_mag) == NULL)
			break;
		readmem((u_long)kmp, 0, 0, (void *)mp, magbsize, "magazine");
		for (i = 0; i < ccp->cc_rounds; i++) {
			maglist[magcnt] = mp->mag_round[i];
			if (++magcnt > magmax)
				goto out2;
		}
		if ((kmp = ccp->cc_full_mag) == NULL)
			break;
		readmem((u_long)kmp, 0, 0, (void *)mp, magbsize, "magazine");
		for (i = 0; i < magsize; i++) {
			maglist[magcnt] = mp->mag_round[i];
			if (++magcnt > magmax)
				goto out2;
		}
	}

	flags = cp->cache_flags;
	bufsize = cp->cache_bufsize;
	chunksize = cp->cache_chunksize;
	valid = malloc(cp->cache_slabsize / bufsize);
	ubase = malloc(cp->cache_slabsize + sizeof (kmem_bufctl_t));

	sp = cp->cache_nullslab.slab_next;
	while (sp != &kcp->cache_nullslab) {
		readmem((u_long)sp, 0, 0, (void *)&s, sizeof (s), "slab");
		if (s.slab_cache != kcp)
			goto out3;
		chunks = s.slab_chunks;
		refcnt = s.slab_refcnt;
		kbase = s.slab_base;
		readmem((u_long)kbase, 0, 0, (void *)ubase,
			chunks * chunksize, "slab base");
		memset(valid, 1, chunks);
		bcp = s.slab_head;
		for (i = refcnt; i < chunks; i++) {
			if (flags & KMF_HASH) {
				readmem((u_long)bcp, 0, 0, (void *)&bc,
					sizeof (bc), "kmem hash");
				buf = bc.bc_addr;
			} else {
				bc = *((kmem_bufctl_t *)
					((int)bcp - (int)kbase + (int)ubase));
				buf = (void *)((int)bcp - cp->cache_offset);
			}
			valid[((int)buf - (int)kbase) / chunksize] = 0;
			bcp = bc.bc_next;
		}
		for (i = 0; i < chunks; i++) {
			void *kbuf = (char *)kbase + i * chunksize;
			void *ubuf = (char *)ubase + i * chunksize;
			int m;
			if (!valid[i])
				continue;
			for (m = 0; m < magcnt; m++)
				if (kbuf == maglist[m])
					break;
			if (m == magcnt)
				(*func)(kbuf, ubuf);
		}
		sp = s.slab_next;
	}
	errcode = 0;
out3:
	free(valid);
	free(ubase);
out2:
	free(mp);
	free(maglist);
out1:
	free(cp);
	return (errcode);
}

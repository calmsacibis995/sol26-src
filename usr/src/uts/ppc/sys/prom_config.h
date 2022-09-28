/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_CONFIG_H
#define	_SYS_PROM_CONFIG_H

#pragma ident	"@(#)prom_config.h	1.3	96/07/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following structure describes a node in the in-kernel copy
 * of the PROM device tree.
 *
 * The pn_lock/pn_cv field protect the pn_owner field, which is used
 * to lock the node while a thread is accessing it's configuration
 * information. This is primarily to prevent a thread from examining
 * the 'options' node while another thread is changing properties on
 * it.
 */

struct prom_node {
	dnode_t		pn_nodeid;
	ddi_prop_t 	*pn_propp;
	kmutex_t	pn_lock;
	kcondvar_t	pn_cv;
	kthread_id_t	pn_owner;
	struct prom_node *pn_child;
	struct prom_node *pn_sibling;
	struct prom_node *pn_hashlink;
	caddr_t pn_addr;
};

typedef struct prom_node prom_node_t;


/*
 * An opaque handle returned by prom_config_begin.
 */
typedef void *prom_config_handle_t;

/*
 * Interfaces for extracting PROM configuration information after
 * the kernel has copied the PROM tree.
 */
int prom_config_begin(prom_config_handle_t *handlep, dnode_t nodeid);
void prom_config_end(prom_config_handle_t handle);
int prom_config_getproplen(prom_config_handle_t handle, char *name);
int prom_config_getprop(prom_config_handle_t handle, char *name, char *buffer);
int prom_config_setprop(prom_config_handle_t handle, char *propname,
    char *propval, int proplen);
char *prom_config_nextprop(prom_config_handle_t handle, char *previous,
    char *next);
dnode_t prom_config_childnode(prom_config_handle_t handle);
dnode_t prom_config_nextnode(prom_config_handle_t handle);
dnode_t prom_config_finddevice(char *name);
dnode_t prom_config_topnode(prom_config_handle_t handle);
int prom_config_ihandle_to_path(ihandle_t ihandle, char *buf, u_int len);
int prom_config_stdin_is_keyboard(void);
int prom_config_stdout_is_framebuffer(void);
char *prom_config_stdinpath(void);
char *prom_config_stdoutpath(void);
int prom_config_version_name(char *buf, int len);
void prom_config_setup(void);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_CONFIG_H */

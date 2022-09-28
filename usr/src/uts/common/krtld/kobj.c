/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma	ident	"@(#)kobj.c	1.82	96/09/17 SMI"

/*
 * Kernel's linker/loader
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/map.h>
#include <sys/reboot.h>
#include <sys/bootconf.h>
#include <sys/debug.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <vm/as.h>
#include <vm/seg_kp.h>
#include <sys/elf.h>
#include <sys/elf_notes.h>
#include <sys/link.h>
#include <sys/kobj.h>
#include <sys/disp.h>
#include <sys/modctl.h>
#include <sys/varargs.h>
#include <sys/kstat.h>
#include <sys/kobj_impl.h>

static struct module *load_exec(val_t *);
static struct module *load_linker(val_t *);
static struct modctl *add_primary(char *filename);
static int bind_primary(val_t *);
static int load_primary(struct module *);
static int get_progbits_size(struct module *, struct proginfo *,
	struct proginfo *);
static int get_progbits(struct module *, struct _buf *, struct modctl *);
static int get_syms(struct module *, struct _buf *);
static int do_common(struct module *);
static int do_dependents(struct modctl *);
static int do_symbols(struct module *, u_int);
static void module_assign(struct modctl *, struct module *);
static void free_module_data(struct module *);
static int kobj_packing_module(char *);
static int map_setup(void);
static char *depends_on(struct module *);
static char *getmodpath(void);
static char *basename(char *);
static void attr_val(val_t *);
static caddr_t kobj_mod_alloc(size_t);
static char *expand_cpu(char *);

static Elf32_Sym *lookup_one(struct module *, char *);
static Elf32_Sym *lookup_kernel(char *);
static void sym_insert(struct module *, char *, symid_t);
static u_int hash_name(char *);
static u_int gethashsize(u_int);

static void kprintf(void *, char *, ...);
static caddr_t segbrk(caddr_t *, u_int, int, caddr_t);

static struct kobjopen_tctl *kobjopen_alloc(char *filename);
static void kobjopen_free(struct kobjopen_tctl *ltp);
static void kobjopen_thread(struct kobjopen_tctl *ltp);

extern int splimp();
extern int splx();
extern int kcopy();
extern int elf_mach_ok(Elf32_Ehdr *);

void kobj_sync_instruction_memory(caddr_t, int);

extern int modrootloaded;
extern int swaploaded;
extern int moddebug;
extern struct bootops *bootops;
extern int kobj_map_space_len;
extern kmutex_t kobj_lock;
extern struct modctl modules;
extern int mod_mix_changed;
extern int last_module_id;

extern caddr_t s_text;	/* start of kernel text segment */
extern caddr_t s_data;	/* start of kernel data segment */
extern caddr_t e_text;	/* end of kernel text segment */
extern caddr_t e_data;	/* end of kernel data segment */

#ifdef KOBJ_DEBUG
/*
 * Values that can be or'd in to kobj_debug and their effects:
 *
 *	D_DEBUG		- misc. debugging information.
 *	D_SYMBOLS	- list symbols and their values as they are entered
 *			  into the hash table
 *	D_RELOCATIONS	- display relocation processing information
 *	D_LOADING	- display information about each module as it
 *			  is loaded.
 */
int kobj_debug = 	0;
#endif

#ifndef MOD_MAX_MODULES
#define	MOD_MAX_MODULES		100
#endif

#define	KOBJ_MAP_SEGMENTS	(MOD_MAX_MODULES)
#define	MODPATH_PROPNAME	"module-path"

/*
 * Flags for memory allocation.
 */
#define	KM_WAIT			0x0		/* wait for it */
#define	KM_NOWAIT		0x1		/* return immediately */
#define	KM_PACK			0x2		/* special allocator */
#define	KM_TEMP			0x1000	/* use boot memory in standalone mode */

#define	ALIGN(x, a)	((a) == 0 ? (int)(x) : \
			(((int)(x) + (a) - 1) & ~((a) - 1)))

#define	_moddebug	GET(moddebug)
#define	_modrootloaded	GET(modrootloaded)
#define	_swaploaded	GET(swaploaded)
#define	_bootops	GET(bootops)

#define	GET(x)		(&x == NULL ? 0 : x)

#define	mod(X)		(struct module *)((X)->modl_modp->mod_mp)

void	*romp;		/* rom vector (opaque to us) */
struct bootops *ops;	/* bootops vector */
void *dbvec;		/* debug vector */

#if defined(i386) || defined(__ppc)
void *bopp;	/* XXX i386 kadb support */
#endif

/*
 * kobjopen thread control structure
 */
struct kobjopen_tctl {
	ksema_t		sema;
	char		*name;		/* name of file */
	struct vnode	*vp;		/* vnode return from vn_open() */
	int		errno;		/* error return from vnopen    */
};

#ifdef	MPSAS
void	sas_prisyms(struct modctl_list *);
void	sas_syms(struct module *);
#endif

static struct modctl *kobj_modules = NULL;	/* modules loaded */
static struct modctl_list *primaries = NULL;	/* primary kernel module list */
static struct map *kobj_map;			/* symbol space resource map */
static char *kobj_map_space;			/* segkp space for symbols */
static char *module_path;			/* module search path */
static caddr_t kobj_packables;			/* list of packable modules */
static int kobj_packsize;			/* size of packable modules */
static int mmu_pagesize;			/* system pagesize */
static int lg_pagesize;				/* "large" pagesize */
static char *cpulist;				/* cpu name list from boot */
static int kobj_last_module_id = 0;		/* id assignment */
static int kobj_locked = 0;			/* kobj symbol lock count */
static kobj_notify_list * notify_load = 0;	/* kobj module load */
						/*	notifcation list */
static kobj_notify_list * notify_unload = 0;	/* kobj module unload */
						/*	notifcation list */
/*
 * Beginning and end of the kernel's
 * dynamic text/data segments.
 */
static caddr_t _text;
static caddr_t _etext;
static caddr_t _data;
static caddr_t _edata;

static Elf32_Addr dynseg = 0;	/* load address of "dynamic" segment */

int _mod_pool_pages = 0;
int standalone = 1;			/* an unwholey kernel? */
int use_iflush;				/* iflush after relocations */

#ifdef	KOBJ_DEBUG
/*
 * Diagnostic/Debugging messages
 */
static const char *
	kmsg_fld =	"krtld: file=%s\nkrtld:\ttext:0x%8x size: 0x%8x "
			"data:0x%8x dsize: 0x%8x\n";
#endif

void (*_printf)();			/* printf routine */

static kobj_stat_t kobj_stat;

#define	MINALIGN	8	/* at least a double-word */

#if defined(__ppc)
/*
 *	PowerPC has 32Mb range for directly called functions.
 *	See PSARC PowerPC Kernel case for more discussion of this.
 */
static void *
kobj_text_zalloc(size_t size, int flag)
{
	extern void *lokmem_zalloc();

	int kmem_flag = 0;

	/*
	 * If we are running standalone in the
	 * linker, we ask boot for memory.
	 * Either it's temporary memory that we lose
	 * once boot is mapped out or we allocate it
	 * permanently using the dynamic data segment.
	 */
	if (standalone) {
#ifdef BOOTSCRATCH
		if (flag & KM_TEMP)
			return (BOP_ALLOC(ops, (caddr_t)0,
			    roundup(size, mmu_pagesize), BO_NO_ALIGN));
		else
#endif
			return (segbrk(&_edata, size, MINALIGN, 0));
	}
	if (flag & KM_WAIT)
		kmem_flag |= KM_SLEEP;
	if (flag & KM_NOWAIT)
		kmem_flag |= KM_NOSLEEP;

	kobj_stat.nalloc_calls++;
	kobj_stat.nalloc += size;
	return (lokmem_zalloc(size, kmem_flag));
}

static void
kobj_text_free(void *address, size_t size)
{
	extern void lokmem_free();

	if (standalone)
		return;

	lokmem_free(address, size);
	kobj_stat.nfree_calls++;
	kobj_stat.nfree += size;
}
#endif

/*
 * XXX fix dependencies on "kernel"; this should work
 * for other standalone binaries as well.
 *
 * XXX Fix hashing code to use one pointer to
 * hash entries.
 *	|----------|
 *	| nbuckets |
 *	|----------|
 *	| nchains  |
 *	|----------|
 *	| bucket[] |
 *	|----------|
 *	| chain[]  |
 *	|----------|
 */

/*
 * Note: support has been added to kadb to help debug the linker
 * itself prior to the handoff to unix.  Just boot with kadb -k
 * and the linker's symbols will be available from the first kadb
 * prompt. Caution: do not set breakpoints on instructions that
 * haven't been relocated yet.  kadb will replace the instruction
 * with a software trap instruction and the linker will scrog this
 * when it tries to relocate the original instruction; you'll get
 * an 'unimplemented instruction' trap.
 */

/*
 * Load, bind and relocate all modules that
 * form the primary kernel. At this point, our
 * externals have not been relocated.
 */
void
kobj_init(
	void *romvec,
	void *dvec,
	struct bootops *bootvec,
	val_t *bootaux)
{
	struct module *mp;
	Elf32_Addr entry;

	/*
	 * Save these to pass on to
	 * the booted standalone.
	 */
	romp = romvec;
	dbvec = dvec;

#if defined(i386)
	/*
	 * XXX This wierdness is needed because the 386 stuff passes
	 * (struct bootops **) rather than (struct bootops *) when
	 * the debugger is loaded.  Note: it also does the same
	 * with romp, but we simply pass that on without using it.
	 */
	bopp = (void *)bootvec;
	ops = (dvec) ? *(struct bootops **)bootvec : bootvec;
#elif defined(__ppc)
	bopp = (void *)bootvec;
	ops = *(struct bootops **)bootvec;
#else
	ops = bootvec;
#endif
	_printf = (void (*)())ops->bsys_printf;

	/*
	 * Save the interesting attribute-values
	 * (scanned by kobj_boot).
	 */
	attr_val(bootaux);

	/*
	 * Check bootops version.
	 */
	if (BOP_GETVERSION(ops) != BO_VERSION)
		_printf(ops, "Warning: Using boot version %d, "
		    "expected %d\n", BOP_GETVERSION(ops), BO_VERSION);

	/*
	 * Set the module search path.
	 */
	module_path = getmodpath();

	/*
	 * These two modules have actually been
	 * loaded by boot, but we finish the job
	 * by introducing them into the world of
	 * loadable modules.
	 */
	if ((mp = load_exec(bootaux)) == (struct module *)NULL ||
	    load_linker(bootaux) == (struct module *)NULL)
		goto fail;

	/*
	 * Load all the primary dependent modules.
	 */
	if (load_primary(mp) == -1)
		goto fail;

	/*
	 * Glue it together.
	 */
	if (bind_primary(bootaux) == -1)
		goto fail;

	entry = bootaux[BA_ENTRY].ba_val;

#ifdef KOBJ_DEBUG
	/*
	 * Okay, we'll shut up now.
	 */
	if (kobj_debug & D_PRIMARY)
		kobj_debug = 0;
#endif
	/*
	 * Post setup.
	 */
	standalone = 0;
#ifdef	MPSAS
	sas_prisyms(primaries);
#endif
	s_text = _text;
	e_text = _etext;

	/* sync_instruction_memory */
	kobj_sync_instruction_memory(s_text, e_text - s_text);

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_DEBUG)
		_printf(ops, "krtld: transfering control to: 0x%x\n", entry);
#endif
	s_data = _data;
	e_data = _edata;
	_printf = (void (*)())kprintf;

	/*
	 * Make sure the mod system (and kadb) knows about
	 * the modules already loaded.
	 */
	last_module_id = kobj_last_module_id;
	bcopy(kobj_modules, &modules, sizeof (modules));
	modules.mod_next->mod_prev = &modules;
	modules.mod_prev->mod_next = &modules;

	exitto((caddr_t)entry);
fail:

	_printf(ops, "krtld: error during initial load/link phase\n");
}

/*
 * Set up any global information derived
 * from attribute/values in the boot or
 * aux vector.
 */
static void
attr_val(val_t *bootaux)
{
	Elf32_Phdr *phdr;
	int phnum, phsize;
	int i;

	mmu_pagesize = bootaux[BA_PAGESZ].ba_val;
	lg_pagesize = bootaux[BA_LPAGESZ].ba_val;
	use_iflush = bootaux[BA_IFLUSH].ba_val;
	cpulist = bootaux[BA_CPU].ba_ptr;

	phdr = (Elf32_Phdr *)bootaux[BA_PHDR].ba_ptr;
	phnum = bootaux[BA_PHNUM].ba_val;
	phsize = bootaux[BA_PHENT].ba_val;
	for (i = 0; i < phnum; i++) {
		phdr = (Elf32_Phdr *)(bootaux[BA_PHDR].ba_val + i * phsize);

		if (phdr->p_type != PT_LOAD)
			continue;
		/*
		 * Bounds of the various segments.
		 */
		if (!(phdr->p_flags & PF_X)) {
			dynseg = phdr->p_vaddr;
		} else {
			if (phdr->p_flags & PF_W) {
				_data = (caddr_t)phdr->p_vaddr;
				_edata = _data + phdr->p_memsz;
			} else {
				_text = (caddr_t)phdr->p_vaddr;
				_etext = _text + phdr->p_memsz;
			}
		}
	}
}

/*
 * Set up the booted executable.
 */
static struct module *
load_exec(val_t *bootaux)
{
	char filename[MAXPATHLEN];
	struct modctl *cp;
	struct module *mp;
	Elf32_Dyn *dyn;
	Elf32_Sym *sp;
	int nsize = 0;
	int i;
	char *libname, *cpulib, *cpudt = NULL;

	BOP_GETPROP(ops, "whoami", filename);

	if ((cp = add_primary(filename)) == NULL)
		return ((struct module *)NULL);

	mp = (struct module *)kobj_zalloc(sizeof (struct module), KM_NOWAIT);
	if (mp == (struct module *)NULL)
		return ((struct module *)NULL);
	cp->mod_mp = mp;

	/*
	 * We don't have the following information
	 * since this module is an executable and not
	 * a relocatable .o.
	 */
	mp->symtbl_section = NULL;
	mp->shdrs = NULL;
	mp->strhdr = NULL;

	/*
	 * Since this module is the only exception,
	 * we cons up some section headers.
	 */
	mp->symhdr = (Elf32_Shdr *)
	    kobj_zalloc(sizeof (Elf32_Shdr), KM_NOWAIT);
	mp->strhdr = (Elf32_Shdr *)
	    kobj_zalloc(sizeof (Elf32_Shdr), KM_NOWAIT);

	if (mp->symhdr == (Elf32_Shdr *)NULL ||
	    mp->strhdr == (Elf32_Shdr *)NULL)
		return ((struct module *)NULL);

	mp->symhdr->sh_type = SHT_SYMTAB;
	mp->strhdr->sh_type = SHT_STRTAB;
	/*
	 * Scan the dynamic structure.
	 */
	for (dyn = (Elf32_Dyn *) bootaux[BA_DYNAMIC].ba_ptr;
	    dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_SYMTAB:
			dyn->d_un.d_ptr += dynseg;
			mp->symspace = mp->symtbl = (char *)dyn->d_un.d_ptr;
			mp->symhdr->sh_addr = dyn->d_un.d_ptr;
			break;
		case DT_HASH:
			dyn->d_un.d_ptr += dynseg;
			mp->nsyms = *((u_int *)dyn->d_un.d_ptr + 1);
			mp->hashsize = *(u_int *)dyn->d_un.d_ptr;
			break;
		case DT_STRTAB:
			dyn->d_un.d_ptr += dynseg;
			mp->strings = (char *)dyn->d_un.d_ptr;
			mp->strhdr->sh_addr = dyn->d_un.d_ptr;
			break;
		case DT_STRSZ:
			mp->strhdr->sh_size = dyn->d_un.d_val;
			break;
		case DT_SYMENT:
			mp->symhdr->sh_entsize = dyn->d_un.d_val;
			break;
		}
	}
	/*
	 * Count up the size of the strings for all
	 * entries including NULL +  (n - 1 spaces).
	 */
	cpulib = NULL;
	for (dyn = (Elf32_Dyn *) bootaux[BA_DYNAMIC].ba_ptr;
	    dyn->d_tag != DT_NULL; dyn++)
		if (dyn->d_tag == DT_NEEDED) {
			char *_cpulib;

			libname = mp->strings + dyn->d_un.d_val;
			if (cpulist != NULL &&
			    (_cpulib = expand_cpu(libname)) != NULL) {
				/*
				 * Currently the DT_NEEDED lists are limited
				 * to one $CPU expansion per object.
				 */
				if (cpulib == NULL) {
					cpudt = libname;
					libname = cpulib = _cpulib;
				} else
					_printf(ops, "krtld: multiple $CPU "
					    "macros encountered in %s, only "
					    "the first will be used.\n",
					    cp->mod_filename);
			}
			nsize += strlen(libname) + 1;
		}
	/*
	 * Collapse any DT_NEEDED entries into one string.
	 */
	if (nsize) {
		if ((mp->depends_on = kobj_zalloc(nsize, KM_NOWAIT)) == NULL)
			return ((struct module *)NULL);

		for (dyn = (Elf32_Dyn *)bootaux[BA_DYNAMIC].ba_ptr;
		    dyn->d_tag != DT_NULL; dyn++) {
			if (dyn->d_tag == DT_NEEDED) {
				libname = mp->strings + dyn->d_un.d_val;
				if (*mp->depends_on)
					(void) strcat(mp->depends_on, " ");
				if (libname == cpudt)
					libname = cpulib;
				(void) strcat(mp->depends_on, libname);
			}
		}
	}
	mp->flags = KOBJ_EXEC|KOBJ_PRIM;	/* NOT a relocatable .o */
	mp->symhdr->sh_size = mp->nsyms * mp->symhdr->sh_entsize;
	/*
	 * We allocate our own table since we don't
	 * hash undefined references.
	 */
	mp->chains = kobj_zalloc(mp->nsyms * sizeof (symid_t), KM_NOWAIT);
	mp->buckets = kobj_zalloc(mp->hashsize * sizeof (symid_t), KM_NOWAIT);

	if (mp->chains == NULL || mp->buckets == NULL)
		return (NULL);

	mp->text = _text;
	mp->data = _data;
	mp->filename = cp->mod_filename;

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_printf(ops, kmsg_fld, mp->filename, mp->text, mp->text_size,
			mp->data, mp->data_size);
	}
#endif
	/*
	 * Insert symbols into the hash table.
	 */
	for (i = 0; i < mp->nsyms; i++) {
		sp = (Elf32_Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;

		sym_insert(mp, mp->strings + sp->st_name, i);
	}

	return (mp);
}

/*
 * Set up the linker module.
 */
static struct module *
load_linker(val_t *bootaux)
{
	struct module *kmp = (struct module *)kobj_modules->mod_mp;
	struct module *mp;
	struct modctl *cp;
	register int i;
	Elf32_Shdr *shp;
	Elf32_Sym *sp;
	int shsize;
	char *dlname = (char *)bootaux[BA_LDNAME].ba_ptr;

	if ((cp = add_primary(dlname)) == NULL)
		return ((struct module *)NULL);

	mp = (struct module *)kobj_zalloc(sizeof (struct module), KM_NOWAIT);
	if (mp == (struct module *)NULL)
		return ((struct module *)NULL);

	cp->mod_mp = mp;
	mp->hdr = *(Elf32_Ehdr *)bootaux[BA_LDELF].ba_ptr;
	shsize = mp->hdr.e_shentsize * mp->hdr.e_shnum;
	if ((mp->shdrs = (char *)kobj_zalloc(shsize, KM_NOWAIT)) == NULL)
		return ((struct module *)NULL);
	bcopy(bootaux[BA_LDSHDR].ba_ptr, mp->shdrs, shsize);

	for (i = 1; i < (int)mp->hdr.e_shnum; i++) {
		shp = (Elf32_Shdr *)(mp->shdrs + (i * mp->hdr.e_shentsize));

		if (shp->sh_flags & SHF_ALLOC) {
			if (shp->sh_flags & SHF_WRITE) {
				if (mp->data == NULL)
					mp->data = (char *)shp->sh_addr;
			} else if (mp->text == NULL) {
				mp->text = (char *)shp->sh_addr;
			}
		}
		if (shp->sh_type == SHT_SYMTAB) {
			mp->symtbl_section = i;
			mp->symhdr = shp;
			mp->symspace = mp->symtbl = (char *)shp->sh_addr;
		}
	}
	mp->nsyms = mp->symhdr->sh_size / mp->symhdr->sh_entsize;
	mp->flags = KOBJ_INTERP|KOBJ_PRIM;
	mp->strhdr = (Elf32_Shdr *)
		(mp->shdrs + mp->symhdr->sh_link * mp->hdr.e_shentsize);
	mp->strings = (char *)mp->strhdr->sh_addr;
	mp->hashsize = gethashsize(mp->nsyms);

	mp->chains = kobj_zalloc(mp->nsyms * sizeof (symid_t), KM_NOWAIT);
	mp->buckets = kobj_zalloc(mp->hashsize * sizeof (symid_t), KM_NOWAIT);

	if (mp->chains == NULL || mp->buckets == NULL)
		return (NULL);

	mp->bss = bootaux[BA_BSS].ba_val;
	mp->bss_align = 0;	/* pre-aligned during allocation */
	mp->bss_size = (u_int)_edata - mp->bss;
	mp->text_size = _etext - mp->text;
	mp->data_size = _edata - mp->data;
	mp->filename = cp->mod_filename;

	/*
	 * Now that we've figured out where the linker is,
	 * set the limits for the booted object.
	 */
	kmp->text_size = (u_int)(mp->text - kmp->text);
	kmp->data_size = (u_int)(mp->data - kmp->data);

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_printf(ops, kmsg_fld, mp->filename, mp->text, mp->text_size,
			mp->data, mp->data_size);
	}
#endif

	/*
	 * Insert the symbols into the hash table.
	 */
	for (i = 0; i < mp->nsyms; i++) {
		sp = (Elf32_Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;
		if (sp->st_name >= mp->strhdr->sh_size)
			return ((struct module *)NULL);
		if (ELF32_ST_BIND(sp->st_info) == STB_GLOBAL) {
			if (sp->st_shndx == SHN_COMMON)
				sp->st_shndx = SHN_ABS;
		}
		sym_insert(mp, mp->strings + sp->st_name, i);
	}
	return (mp);
}

/*
 * kobj_notify_add()
 */
int
kobj_notify_add(kobj_notify_list * knp)
{
	kobj_notify_list **	knl;

	if (knp->kn_version != KOBJ_NVERSION_CURRENT)
		return (-1);

	switch (knp->kn_type) {
	case KOBJ_NOTIFY_MODLOAD:
		knl = &notify_load;
		break;
	case KOBJ_NOTIFY_MODUNLOAD:
		knl = &notify_unload;
		break;
	default:
		return (-1);
	}
	knp->kn_next = 0;
	knp->kn_prev = 0;

	mutex_enter(&kobj_lock);

	if (*knl == 0) {
		/*
		 * first item on list
		 */
		(*knl) = knp;
	} else {
		/*
		 * Insert at head of list.
		 */
		(*knl)->kn_prev = knp;
		knp->kn_next = *knl;
		(*knl) = knp;
	}

	mutex_exit(&kobj_lock);
	return (0);
}


/*
 * kobj_notify_remove()
 */
int
kobj_notify_remove(kobj_notify_list * knp)
{
	kobj_notify_list **	knl;
	kobj_notify_list *	tknp;

	switch (knp->kn_type) {
	case KOBJ_NOTIFY_MODLOAD:
		knl = &notify_load;
		break;
	case KOBJ_NOTIFY_MODUNLOAD:
		knl = &notify_unload;
		break;
	default:
		return (-1);
	}

	mutex_enter(&kobj_lock);

	/* LINTED */
	if (tknp = knp->kn_next)
		tknp->kn_prev = knp->kn_prev;

	/* LINTED */
	if (tknp = knp->kn_prev)
		tknp->kn_next = knp->kn_next;
	else
		*knl = knp->kn_next;

	mutex_exit(&kobj_lock);

	return (0);
}

/*
 * kobj_notify_load()
 *
 * Common routine which can be used for both loading & unloading
 * notifcation.
 */
static void
kobj_notify_load(kobj_notify_list *knp, struct modctl *modp)
{
	while (knp) {
		void (*	fptr)(unsigned int, struct modctl *);
		fptr = (void(*)(unsigned int, struct modctl *))knp->kn_func;
		fptr(knp->kn_type, modp);
		knp = knp->kn_next;
	}
}


/*
 * Ask boot for the module path.
 */
static char *
getmodpath(void)
{
	register char *path;
	int len;

	if ((len = BOP_GETPROPLEN(ops, MODPATH_PROPNAME)) == -1)
		return (MOD_DEFPATH);

	path = (char *)kobj_zalloc(len, KM_NOWAIT);

	(void) BOP_GETPROP(ops, MODPATH_PROPNAME, path);

	return (*path ? path : MOD_DEFPATH);
}

static struct modctl *
add_primary(char *filename)
{
	struct modctl *cp;
	struct modctl_list *lp;

	cp = (struct modctl *)
	    kobj_zalloc(sizeof (struct modctl), KM_NOWAIT);

	if (cp == (struct modctl *)NULL)
		return ((struct modctl *)NULL);

	cp->mod_filename = (char *)
	    kobj_zalloc(strlen(filename)+1, KM_NOWAIT);
	if (cp->mod_filename == NULL) {
		kobj_free(cp, sizeof (struct modctl));
		return ((struct modctl *)NULL);
	}

	/*
	 * For symbol lookup, we assemble our own
	 * modctl list of the primary modules.
	 */
	if ((lp =
	    kobj_zalloc(sizeof (struct modctl_list), KM_NOWAIT)) == NULL) {
		kobj_free(cp->mod_filename, strlen(filename) + 1);
		kobj_free(cp, sizeof (struct modctl));
		return ((struct modctl *)NULL);
	}

	(void) strcpy(cp->mod_filename, filename);
	cp->mod_modname = basename(cp->mod_filename);
	cp->mod_id = kobj_last_module_id++;
	/*
	 * Link the module in. We'll pass this info on
	 * to the mod squad later.
	 */
	if (kobj_modules == (struct modctl *)NULL) {
		kobj_modules = cp;
		cp->mod_prev = cp->mod_next = cp;
	} else {
		cp->mod_prev = kobj_modules->mod_prev;
		cp->mod_next = kobj_modules;
		kobj_modules->mod_prev->mod_next = cp;
		kobj_modules->mod_prev = cp;
	}

	lp->modl_modp = cp;
	if (primaries == (struct modctl_list *)NULL) {
		primaries = lp;
	} else {
		struct modctl_list *last;

		for (last = primaries; last->modl_next;
		    last = last->modl_next)
			;
		last->modl_next = lp;
	}
	return (cp);
}

static int
bind_primary(val_t *bootaux)
{
	struct modctl_list *lp;
	struct modctl *cp;
	struct module *mp;
	Elf32_Dyn *dyn;
	Elf32_Word relasz;
	Elf32_Word relaent;
	char *rela;

	/*
	 * Do common symbols.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);
		/*
		 * These modules should have their
		 * common already taken care of.
		 */
		if (mp->flags & (KOBJ_EXEC|KOBJ_INTERP))
			continue;

		if (do_common(mp) < 0)
			return (-1);
	}
	/*
	 * Resolve symbols.
	 */
	for (lp = primaries; lp; lp = lp->modl_next)
		if (do_symbols(mod(lp), 0) < 0)
			return (-1);
	/*
	 * Do relocations.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if (mp->flags & KOBJ_EXEC) {
			relasz = 0;
			relaent = 0;
			rela = (char *)NULL;

			for (dyn = (Elf32_Dyn *)bootaux[BA_DYNAMIC].ba_ptr;
			    dyn->d_tag != DT_NULL; dyn++) {
				switch (dyn->d_tag) {
				case DT_RELASZ:
				case DT_RELSZ:
					relasz = dyn->d_un.d_val;
					break;
				case DT_RELAENT:
				case DT_RELENT:
					relaent = dyn->d_un.d_val;
					break;
				case DT_RELA:
				case DT_REL:
					rela = (char *)(dyn->d_un.d_ptr +
						dynseg);
					break;
				}
			}
			if (relasz == 0 ||
			    relaent == 0 || rela == (char *)NULL)
				return (-1);

#ifdef	KOBJ_DEBUG
			if (kobj_debug & D_RELOCATIONS)
				_printf(ops, "krtld: relocating: file=%s "
					"KOBJ_EXEC\n", mp->filename);
#endif
			if (do_relocate(mp, rela, relasz/relaent,
			    relaent, (Elf32_Addr)mp->text) < 0)
				return (-1);
		} else {
			if (do_relocations(mp) < 0)
				return (-1);
		}

		/* sync_instruction_memory */
		kobj_sync_instruction_memory(mp->text, mp->text_size);
	}

	for (lp = primaries; lp; lp = lp->modl_next) {
		cp = lp->modl_modp;
		mp = (struct module *)cp->mod_mp;

		/*
		 * XXX This is a crock.  Since we can't
		 * force ld to use the full symbol table,
		 * we reload the complete symbol/string
		 * tables (for debugging) once the relocations
		 * have been performed.
		 */
#ifdef BOOTSCRATCH
		if (mp->flags & KOBJ_EXEC) {
#else
		if ((mp->flags & KOBJ_EXEC) && dbvec != NULL) {
#endif
			struct _buf *file;
			int n;

			file = kobj_open_file(mp->filename);
			if (file == (struct _buf *)-1)
				return (-1);
			if (kobj_read_file(file, (char *)&mp->hdr,
			    sizeof (mp->hdr), 0) < 0)
				return (-1);
			n = mp->hdr.e_shentsize * mp->hdr.e_shnum;
			mp->shdrs = kobj_zalloc(n, KM_NOWAIT);
			if (mp->shdrs == NULL)
				return (-1);
			if (kobj_read_file(file, mp->shdrs, n,
			    mp->hdr.e_shoff) < 0)
				return (-1);
			if (get_syms(mp, file) < 0)
				return (-1);
			kobj_close_file(file);
		}
	}

	return (0);
}

/*
 * Load all the primary dependent modules.
 */
static int
load_primary(struct module *mp)
{
	struct modctl *cp;
	struct module *dmp;
	register char *p, *q;
	char modname[MODMAXNAMELEN];

	/*
	 * If this is the booted executable and a
	 * dependency was specified by boot.
	 */
	if (!(mp->flags & KOBJ_EXEC))
		mp->depends_on = depends_on(mp);

	if ((p = mp->depends_on) == NULL)
		return (0);


	/* CONSTANTCONDITION */
	while (1) {
nextdep:
		/*
		 * Skip space.
		 */
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		/*
		 * Get module name.
		 */
		q = modname;
		while (*p && *p != ' ' && *p != '\t')
			*q++ = *p++;

		if (q == modname)
			break;

		*q = '\0';
		/*
		 * Check for dup dependencies.
		 */
		cp = kobj_modules;
		do {
			/*
			 * Already loaded.
			 */
			if (strcmp(modname, cp->mod_filename) == 0)
				goto nextdep;

			cp = cp->mod_next;
		} while (cp != kobj_modules);

		if ((cp = add_primary(modname)) == NULL)
			return (-1);
		cp->mod_busy = 1;
		/*
		 * Load it.
		 */
		kobj_load_module(cp, 1);
		cp->mod_busy = 0;

		if ((dmp = cp->mod_mp) == NULL)
			return (-1);

		dmp->flags |= KOBJ_PRIM;
		/*
		 * Recurse.
		 */
		if (load_primary(dmp) == -1)
			return (-1);
	}
	return (0);
}


/*
 * Return a string listing module dependencies.
 */
static char *
depends_on(struct module *mp)
{
	Elf32_Dyn *dyn = NULL;
	Elf32_Shdr *shp;
	Elf32_Sym *sp;
	int nsize = 0;
	int shn;
	char *p = NULL;
	char *libname, *cpulib, *cpudt = NULL;

	/*
	 * Find dynamic section.
	 */
	for (shn = 1; shn < (int)mp->hdr.e_shnum; shn++) {
		shp = (Elf32_Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (shp->sh_type == SHT_DYNAMIC) {
			dyn = (Elf32_Dyn *)shp->sh_addr;
			break;
		}
	}
	/*
	 * Count up string size for combined DT_NEEDED entries.
	 * (adding n-1 space-separators + NULL)
	 */
	cpulib = NULL;
	for (; dyn && dyn->d_tag != DT_NULL; dyn++)
		if (dyn->d_tag == DT_NEEDED) {
			char *_cpulib;
			libname = (char *)dyn->d_un.d_ptr;
			if (cpulist != NULL &&
			    (_cpulib = expand_cpu(libname)) != NULL) {
				/*
				 * Currently the DT_NEEDED lists are limited
				 * to one $CPU expansion per object.
				 */
				if (cpulib == NULL) {
					cpudt = libname;
					libname = cpulib = _cpulib;
				} else
					_printf(ops, "krtld: multiple $CPU "
					    "macros encountered in %s, only "
					    "the first will be used.\n",
					    mp->filename);
			}
			nsize += strlen(libname) + 1;
		}
	/*
	 * Collapse into one string.
	 */
	if (nsize) {
		p = kobj_zalloc(nsize, KM_WAIT);

		for (; dyn->d_tag != DT_NULL; dyn++) {
			if (dyn->d_tag == DT_NEEDED) {
				if (*p)
					(void) strcat(p, " ");
				libname = (char *)dyn->d_un.d_ptr;
				if (libname == cpudt)
					libname = cpulib;
				(void) strcat(p, libname);
			}
		}
	}
	/*
	 * Didn't find a DT_NEEDED entry,
	 * try the old "_depends_on" mechanism
	 */
	if (p == NULL && (sp = lookup_one(mp, "_depends_on"))) {
		register char *q = (char *)sp->st_value;

		/*
		 * Idiot checks. Make sure it's
		 * in-bounds and NULL terminated.
		 */
		if (kobj_addrcheck(mp, q) || q[sp->st_size - 1] != '\0') {
			_printf(ops, "Error processing dependency for %s\n",
			    mp->filename);
			return (NULL);
		}
		p = (char *)kobj_zalloc(strlen(q) + 1, KM_WAIT);
		(void) strcpy(p, q);
	}
	return (p);
}

void
kobj_get_packing_info(char *unixfile)
{
	struct _buf *file;
	Elf32_Ehdr ehdr;
	Elf32_Phdr *phdr, *notephdr;
	int phdrs;
	int gpcnt; 	/* general purpose counter */
	int delta;
	caddr_t notep;
	Elf32_Nhdr *notehdr;

	if (unixfile == NULL)
		return;

	if ((file = kobj_open_file(unixfile)) == (struct _buf *)-1)
		return;

	if ((kobj_read_file(file, (caddr_t)&ehdr, sizeof (ehdr), 0)) < 0)
		goto badread;

#ifndef BOOTSCRATCH
	/*
	 * XXX - the ELF header for the booted executable (unixfile)
	 * may not have been initialized in bind_primary() because of
	 * the workaround for 1181021 (x86 boot scratch memory is not
	 * sufficient).  Since we have to read in the ELF header here
	 * anyway, we take this opportunity to initialize module 0.
	 */
	((struct module *)modules.mod_mp)->hdr = ehdr;
#endif

	phdrs = sizeof (Elf32_Phdr) * ehdr.e_phnum;

	phdr = kmem_alloc(phdrs, KM_SLEEP);

	if ((kobj_read_file(file, (caddr_t)phdr, phdrs, ehdr.e_phoff)) < 0)
		goto bad2;

	gpcnt = 0;
	notephdr = phdr;

	while (gpcnt < (int)ehdr.e_phnum) {
		if (notephdr->p_type == PT_NOTE)
			break;
		notephdr++;
		gpcnt++;
	}

	if (gpcnt == ehdr.e_phnum)
		goto bad2;

	notep = kmem_alloc(notephdr->p_filesz, KM_SLEEP);

	if ((kobj_read_file(file, notep, notephdr->p_filesz,
		notephdr->p_offset)) < 0)
			goto bad;

	notehdr = (Elf32_Nhdr *)notep;
	gpcnt = 0;

	while (gpcnt < notephdr->p_filesz) {
		switch (notehdr->n_type) {
			case ELF_NOTE_MODULE_PACKING: {
				caddr_t src;

				src = (caddr_t)notehdr + sizeof (Elf32_Nhdr) +
					notehdr->n_namesz;
				src = (caddr_t)(ALIGN(src, 4));

				kobj_packables = kmem_alloc(notehdr->n_descsz,
				    KM_SLEEP);

				kobj_packsize = notehdr->n_descsz;
				bcopy(src, kobj_packables, notehdr->n_descsz);
				break;
			}
			case ELF_NOTE_MODULE_SIZE: {
				int *src;

				if (_mod_pool_pages != 0)
					break;

				src = (int *)((caddr_t)notehdr +
					sizeof (Elf32_Nhdr) +
					notehdr->n_namesz);
				src = (int *)(ALIGN(src, 4));
				_mod_pool_pages = *src;
				break;
			}
		}
		delta = sizeof (Elf32_Nhdr) + ALIGN(notehdr->n_namesz, 4) +
			ALIGN(notehdr->n_descsz, 4);
		gpcnt += delta;
		notehdr = (Elf32_Nhdr *)((caddr_t)notehdr + delta);
	}
bad:
	kmem_free(notep, notephdr->p_filesz);
bad2:
	kmem_free(phdr, phdrs);
badread:
	kobj_close_file(file);
}

void
kobj_getmodinfo(void *xmp, struct modinfo *modinfo)
{
	struct module *mp;
	mp = (struct module *)xmp;

	modinfo->mi_base = mp->text;
	modinfo->mi_size = mp->text_size + mp->data_size;
}

/* return non-zero for a bad address */
int
kobj_addrcheck(void *xmp, caddr_t adr)
{
	struct module *mp;

	mp = (struct module *)xmp;

	if ((adr >= mp->text && adr < mp->text + mp->text_size) ||
	    (adr >= mp->data && adr < mp->data + mp->data_size))
		return (0); /* ok */
	if (mp->bss && adr >= (caddr_t)mp->bss &&
	    adr < (caddr_t)mp->bss + mp->bss_size)
		return (0);
	return (1);
}

void
kobj_load_module(struct modctl *modp, int use_path)
{
	char *filename = modp->mod_filename;
	char *modname = modp->mod_modname;
	int i;
	int n;
	struct _buf *file;
	struct module *mp = NULL;
	static int map_error;

	if (_swaploaded && kobj_map_space == NULL && map_error == 0) {
		if (map_setup() < 0)
			map_error = 1;
	}
	mp = kobj_zalloc(sizeof (*mp), KM_WAIT);

	if ((file = kobj_open_path(filename, use_path)) == (struct _buf *)-1)
		goto bad;

	mp->filename = kobj_zalloc(strlen(file->_name) + 1, KM_WAIT);
	(void) strcpy(mp->filename, file->_name);

	if (kobj_read_file(file, (char *)&mp->hdr, sizeof (mp->hdr), 0) < 0) {
		_printf(ops, "kobj_load_module: %s read header failed\n",
		    modname);
		goto bad;
	}
	for (i = 0; i < SELFMAG; i++) {
		if (mp->hdr.e_ident[i] != ELFMAG[i]) {
			if (_moddebug & MODDEBUG_ERRMSG)
				_printf(ops, "%s not an elf module\n", modname);
			goto bad;
		}
	}

	/*
	 * It's ELF, but is it our ISA?  Interpreting the header
	 * from a file for a byte-swapped ISA could cause a huge
	 * and unsatisfiable value to be passed to kobj_zalloc below
	 * and therefore hang booting.
	 */
	if (!elf_mach_ok(&mp->hdr)) {
		if (_moddebug & MODDEBUG_ERRMSG)
			_printf(ops, "%s not an elf module for this ISA\n",
			    modname);
		goto bad;
	}

	n = mp->hdr.e_shentsize * mp->hdr.e_shnum;
	mp->shdrs = kobj_zalloc(n, KM_WAIT);

	if (kobj_read_file(file, mp->shdrs, n, mp->hdr.e_shoff) < 0) {
		_printf(ops, "kobj_load_module: %s error reading "
		    "section headers\n", modname);
		goto bad;
	}

	/* read in sections */
	if (get_progbits(mp, file, modp) < 0) {
		_printf(ops, "%s error reading sections\n", modname);
		goto bad;
	}

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_printf(ops, kmsg_fld, mp->filename, mp->text, mp->text_size,
			mp->data, mp->data_size);
	}
#endif

	/* read in symbols; adjust values for each section's real address */
	if (get_syms(mp, file) < 0) {
		_printf(ops, "%s error reading symbols\n", modname);
		goto bad;
	}
	module_assign(modp, mp);

	/*
	 * For primary kernel modules, we defer
	 * symbol resolution and relocation until
	 * all primary objects have been loaded.
	 */
	if (!standalone) {
		/* load all dependents */
		if (do_dependents(modp) < 0) {
			_printf(ops, "%s error doing dependents\n", modname);
			goto bad;
		}
		/*
		 * resolve undefined and common symbols,
		 * also allocates common space
		 */
		if (do_common(mp) < 0) {
			_printf(ops, "%s error doing common\n", modname);
			goto bad;
		}
		/* process relocation tables */
		if (do_relocations(mp) < 0) {
			_printf(ops, "%s error doing relocations\n", modname);
			goto bad;
		}

		/* sync_instruction_memory */
		kobj_sync_instruction_memory(mp->text, mp->text_size);
#ifdef	MPSAS
		sas_syms(mp);
#endif
		kobj_notify_load(notify_load, modp);
	}
	kobj_close_file(file);
	return;
bad:
	if (file != (struct _buf *)-1)
		kobj_close_file(file);
	free_module_data(mp);

	module_assign(modp, NULL);
}

static void
module_assign(struct modctl *cp, struct module *mp)
{
	if (standalone) {
		cp->mod_mp = mp;
		return;
	}
	/*
	 * Get the mutex to make life easy in the ksyms driver.
	 */
	mutex_enter(&mod_lock);
	cp->mod_mp = mp;
	mod_mix_changed++;
	mutex_exit(&mod_lock);
}

void
kobj_unload_module(struct modctl *modp)
{
	mutex_enter(&mod_lock);
	kobj_notify_load(notify_unload, modp);
	free_module_data((struct module *)modp->mod_mp);
	modp->mod_mp = NULL;
	mod_mix_changed++;
	mutex_exit(&mod_lock);
}

static void
free_module_data(struct module *mp)
{
	struct module_list *lp, *tmp;

	if (mp == NULL)
		return;

	lp = mp->head;
	while (lp) {
		tmp = lp;
		lp = lp->next;
		kobj_free((char *)tmp, sizeof (*tmp));
	}
	if (mp->bss)
		kobj_free((void *)mp->bss, mp->bss_size);
	if (mp->symspace)
		if (mp->flags & KOBJ_SYMSWAP)
			rmfree(kobj_map, mp->symsize, (u_long)mp->symspace);
		else
			kobj_free(mp->symspace, mp->symsize);
	if (mp->text)
#if defined(__ppc)
		kobj_text_free(mp->text, mp->text_size + mp->data_size);
#else
		kobj_free(mp->text, mp->text_size + mp->data_size);
#endif
	if (mp->symhdr)
		kobj_free(mp->symhdr, mp->hdr.e_shentsize);
	if (mp->shdrs)
		kobj_free(mp->shdrs,
		    mp->hdr.e_shentsize * mp->hdr.e_shnum);
	if (mp->depends_on)
		kobj_free(mp->depends_on, strlen(mp->depends_on)+1);

	if (mp->filename)
		kobj_free(mp->filename, strlen(mp->filename)+1);

	kobj_free((char *)mp, sizeof (*mp));
}

static int
get_progbits_size(struct module *mp, struct proginfo *tp, struct proginfo *dp)
{
	struct proginfo *pp;
	u_int shn;
	Elf32_Shdr *shp;

	/*
	 * loop through sections to find out how much space we need
	 * for text, data, (also bss that is already assigned)
	 */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Elf32_Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
		if (shp->sh_addr != 0) {
			_printf(ops, "%s non-zero sect addr in input file\n",
			    mp->filename);
			return (-1);
		}
		pp = (shp->sh_flags & SHF_WRITE)? dp : tp;

		if (shp->sh_addralign > pp->align)
			pp->align = shp->sh_addralign;
		pp->size = ALIGN(pp->size, shp->sh_addralign);
		pp->size += shp->sh_size;
	}
	return (0);
}

static int
get_progbits(struct module *mp, struct _buf *file, struct modctl *modp)
{
	struct proginfo *tp, *dp;
	u_int bits_ptr;
	u_int text, data;
	u_int shn;
	Elf32_Shdr *shp;
	int err = 0;

	tp = (struct proginfo *)kobj_zalloc(sizeof (struct proginfo), KM_WAIT);
	dp = (struct proginfo *)kobj_zalloc(sizeof (struct proginfo), KM_WAIT);
	/*
	 * loop through sections to find out how much space we need
	 * for text, data, (also bss that is already assigned)
	 */
	if ((err = get_progbits_size(mp, tp, dp)) == -1)
		goto done;

	mp->text_size = tp->size;
	mp->data_size = dp->size;

	if (standalone) {
		mp->text = segbrk(&_etext, mp->text_size, tp->align, _data);
		/*
		 * If we can't grow the text segment, try the
		 * data segment before failing.
		 */
		if (mp->text == NULL)
			mp->text = segbrk(&_edata, mp->text_size,
					tp->align, 0);
		mp->data = segbrk(&_edata, mp->data_size, dp->align, 0);

		if (mp->text == NULL || mp->data == NULL) {
			err = -1;
			goto done;
		}
	} else {
		int flags = KM_WAIT;

		mp->text_size += tp->align;
		mp->data_size += dp->align;
		if (!(_moddebug & MODDEBUG_NOPACK))
			if (kobj_packing_module(modp->mod_modname)) {
				flags |= KM_PACK;
				modp->mod_loadflags |= MOD_PACKED;
			}
#if defined(__ppc)
		mp->text = kobj_text_zalloc(mp->text_size + mp->data_size,
		    flags);
#else
		mp->text = kobj_zalloc(mp->text_size + mp->data_size, flags);
#endif
		mp->data = mp->text + mp->text_size;
	}
	text = ALIGN((u_int)mp->text, tp->align);
	data = ALIGN((u_int)mp->data, dp->align);

	/* now loop though sections assigning addresses and loading the data */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Elf32_Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;

		bits_ptr = (shp->sh_flags & SHF_WRITE)? data:  text;
		bits_ptr = ALIGN(bits_ptr, shp->sh_addralign);
		if (shp->sh_type == SHT_PROGBITS) {

			if ((err = kobj_read_file(file, (char *)bits_ptr,
			    shp->sh_size, shp->sh_offset)) < 0)
				goto done;
		} else {
			/*
			 * Zero bss.
			 */
			bzero((caddr_t)bits_ptr, shp->sh_size);
		}
		shp->sh_type = SHT_PROGBITS;
		shp->sh_addr = bits_ptr;
		bits_ptr += shp->sh_size;

		if (shp->sh_flags & SHF_WRITE)
			data = bits_ptr;
		else
			text = bits_ptr;
	}

done:
	(void) kobj_free(tp, sizeof (struct proginfo));
	(void) kobj_free(dp, sizeof (struct proginfo));

	return (err);
}

static int
get_syms(struct module *mp, struct _buf *file)
{
	u_int shn;
	Elf32_Shdr *shp;
	u_int i;
	Elf32_Sym *sp, *ksp;
	char *symname;
	extern char stubs_base[], stubs_end[];

	/*
	 * Find the interesting sections.
	 */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Elf32_Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		switch (shp->sh_type) {
		case SHT_SYMTAB:
			mp->symtbl_section = shn;
			mp->symhdr = kobj_zalloc(mp->hdr.e_shentsize, KM_WAIT);
			/*
			 * XXX We keep the section headers,
			 * so why do we need another copy?
			 */
			bcopy((char *)shp, (char *)mp->symhdr,
				mp->hdr.e_shentsize);
			break;

		case SHT_RELA:
		case SHT_REL:
			/*
			 * Already loaded.
			 */
			if (shp->sh_addr)
				continue;
			shp->sh_addr = (Elf32_Addr)
			    kobj_alloc(shp->sh_size, KM_WAIT|KM_TEMP);

			if (kobj_read_file(file, (char *)shp->sh_addr,
			    shp->sh_size, shp->sh_offset) < 0) {
				_printf(ops, "get_syms: %s, error reading "
				    "section %d\n", mp->filename, shn);
				return (-1);
			}
			break;
		}
	}

	/*
	 * get the associated string table header
	 */
	if ((mp->symhdr == 0) || (mp->symhdr->sh_link >= mp->hdr.e_shnum))
		return (-1);
	mp->strhdr = (Elf32_Shdr *)
		(mp->shdrs + mp->symhdr->sh_link * mp->hdr.e_shentsize);

	mp->nsyms = mp->symhdr->sh_size / mp->symhdr->sh_entsize;
	mp->hashsize = gethashsize(mp->nsyms);
	/*
	 * allocate enough space in one chunk for the symbol table, string
	 * table, hash table buckets, and hash table chains
	 */
	mp->symsize = mp->symhdr->sh_size + mp->strhdr->sh_size + sizeof (int) +
		(mp->hashsize + mp->nsyms) * sizeof (symid_t);

	/* make it a multiple of 4, also ensure non-zero */
	mp->symsize = (mp->symsize + 4) & ~3;

	/*
	 * Normally if KADB is present we lock down all symbol tables
	 * and if KADB is not present we unlock all symbol tables.
	 *
	 * There are reasons why users may want to override the default.
	 * One reason is that if the system is not boot with KADB and
	 * keeps crashing for some reason, symbols may not be in
	 * the crash dump unless the symbols are locked down.
	 * Another reason is that locking down symbols may make the
	 * system behave differently than if they are not locked down.
	 * So you may want to boot with KADB but not lock down symbols.
	 *
	 * There are two flags in moddebug to control the locking of symbols.
	 * MODDEBUG_LOCKSYMBOLS forces symbols to be locked down regardless
	 * of the presence of KADB.  If this flag is not set, then
	 * MODDEBUG_UNLOCKSYMBOLS can be set to unlock symbols even when
	 * KADB is present.
	 */
	if (kobj_map_space == NULL) {
		mp->symspace = kobj_zalloc(mp->symsize, KM_WAIT|KM_TEMP);
		if (!standalone)
			mp->flags |= KOBJ_SYMKMEM;
	} else {
		mp->symspace = (char *)rmalloc_wait(kobj_map, mp->symsize);
		bzero(mp->symspace, mp->symsize);
		mp->flags |= KOBJ_SYMSWAP;
	}

	mp->symtbl = mp->symspace;
	mp->strings = mp->symspace + mp->symhdr->sh_size;
	mp->buckets = (symid_t *)((((int)mp->strings + mp->strhdr->sh_size) |
		(sizeof (symid_t) - 1)) + 1);
	mp->chains = mp->buckets + mp->hashsize;

	if (kobj_read_file(file, mp->symtbl,
	    mp->symhdr->sh_size, mp->symhdr->sh_offset) < 0 ||
	    kobj_read_file(file, mp->strings,
	    mp->strhdr->sh_size, mp->strhdr->sh_offset) < 0)
		return (-1);

	/*
	 * loop through the symbol table adjusting values to account
	 * for where each section got loaded into memory.  Also
	 * fill in the hash table.
	 */
	for (i = 1; i < mp->nsyms; i++) {
		sp = (Elf32_Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);
		if (sp->st_shndx < SHN_LORESERVE) {
			if (sp->st_shndx >= mp->hdr.e_shnum) {
				_printf(ops, "%s bad shndx in symbol %d\n",
				    file->_name, i);
				return (-1);
			}
			shp = (Elf32_Shdr *)
			    (mp->shdrs +
			    sp->st_shndx * mp->hdr.e_shentsize);
			if (!(mp->flags & KOBJ_EXEC))
				sp->st_value += shp->sh_addr;
		}

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;
		if (sp->st_name >= mp->strhdr->sh_size)
			return (-1);

		symname = mp->strings + sp->st_name;

		if (!(mp->flags & KOBJ_EXEC) &&
		    ELF32_ST_BIND(sp->st_info) == STB_GLOBAL) {
			ksp = kobj_lookup_all(mp, symname, 0);

			if (ksp && ELF32_ST_BIND(ksp->st_info) == STB_GLOBAL &&
			    sp->st_shndx != SHN_UNDEF &&
			    sp->st_shndx != SHN_COMMON &&
			    ksp->st_shndx != SHN_UNDEF &&
			    ksp->st_shndx != SHN_COMMON) {
				/*
				 * Unless this symbol is a stub,
				 * it's multiply defined.
				 */
				if (standalone ||
				    ksp->st_value < (u_int)stubs_base ||
				    ksp->st_value >= (u_int)stubs_end)
					_printf(ops, "%s symbol %s multiply "
						"defined\n", file->_name,
						symname);
			}
		}
		sym_insert(mp, symname, i);
	}
	return (0);
}

static int
do_dependents(struct modctl *modp)
{
	register struct module *mp;
	struct modctl *req;
	struct module_list *lp;
	char modname[MODMAXNAMELEN];
	char *p, *q;
	int retval = 0;

	mp = modp->mod_mp;

	if ((mp->depends_on = depends_on(mp)) == NULL)
		return (0);

	p = mp->depends_on;
	for (;;) {
		retval = 0;
		/*
		 * Skip space.
		 */
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		/*
		 * Get module name.
		 */
		q = modname;
		while (*p && *p != ' ' && *p != '\t')
			*q++ = *p++;

		if (q == modname)
			break;

		*q = '\0';
		if ((req = mod_load_requisite(modp, modname)) == NULL)
			break;

		for (lp = mp->head; lp; lp = lp->next) {
			if (lp->mp == req->mod_mp)
				break;	/* already on the list */
		}

		if (lp == NULL) {
			lp = (struct module_list *)
			    kobj_zalloc(sizeof (*lp), KM_WAIT);

			lp->mp = req->mod_mp;
			lp->next = NULL;
			if (mp->tail)
				mp->tail->next = lp;
			else
				mp->head = lp;
			mp->tail = lp;
		}
		mod_release_mod(req);

	}
	return (retval);
}

static int
do_common(struct module *mp)
{
	/*
	 * first time through, assign all symbols defined in other
	 * modules, and count up how much common space will be needed
	 * (bss_size and bss_align)
	 */
	if (do_symbols(mp, 0) < 0)
		return (-1);
	/*
	 * increase bss_size by the maximum delta that could be
	 * computed by the ALIGN below
	 */
	mp->bss_size += mp->bss_align;
	if (mp->bss_size) {
		mp->bss = (u_int)kobj_zalloc(mp->bss_size, KM_WAIT);
		/* now assign addresses to all common symbols */
		if (do_symbols(mp, ALIGN(mp->bss, mp->bss_align)) < 0)
			return (-1);
	}
	return (0);
}

static int
do_symbols(struct module *mp, u_int bss_base)
{
	int bss_align;
	u_int bss_ptr;
	int err;
	int i;
	Elf32_Sym *sp, *sp1;
	char *name;
	int assign;
	int resolved = 1;

	/*
	 * Nothing left to do (optimization).
	 */
	if (mp->flags & KOBJ_RESOLVED)
		return (0);

	assign = (bss_base) ? 1 : 0;
	bss_ptr = bss_base;
	bss_align = 0;
	err = 0;

	for (i = 1; i < mp->nsyms; i++) {
		sp = (Elf32_Sym *)(mp->symtbl+mp->symhdr->sh_entsize*i);
		/*
		 * we know that st_name is in bounds, since get_sections
		 * has already checked all of the symbols
		 */
		name = mp->strings + sp->st_name;
		if (sp->st_shndx != SHN_UNDEF && sp->st_shndx != SHN_COMMON)
			continue;

		if (ELF32_ST_BIND(sp->st_info) != STB_LOCAL) {
			if ((sp1 = kobj_lookup_all(mp, name, 0)) != NULL) {
				sp->st_shndx = SHN_ABS;
				sp->st_value = sp1->st_value;
				continue;
			}
		}
		if (sp->st_shndx == SHN_UNDEF) {
			resolved = 0;
			/*
			 * If it's not a weak reference and it's
			 * not a primary object, it's an error.
			 * (Primary objects may take more than
			 * one pass to resolve)
			 */
			if (!(mp->flags & KOBJ_PRIM) &&
			    ELF32_ST_BIND(sp->st_info) != STB_WEAK) {
				_printf(ops, "%s: undefined symbol %s\n",
				    mp->filename, name);
				err = -1;
			}
			continue;
		}
		/*
		 * It's a common symbol - st_value is the
		 * required alignment.
		 */
		if (sp->st_value > bss_align)
			bss_align = sp->st_value;
		bss_ptr = ALIGN(bss_ptr, sp->st_value);
		if (assign) {
			sp->st_shndx = SHN_ABS;
			sp->st_value = bss_ptr;
		}
		bss_ptr += sp->st_size;
	}
	if (err)
		return (err);
	if (assign == 0 && mp->bss == NULL) {
		mp->bss_align = bss_align;
		mp->bss_size = bss_ptr;
	} else if (resolved) {
		mp->flags |= KOBJ_RESOLVED;
	}

	return (0);
}

static u_int
hash_name(char *p)
{
	register unsigned long g;
	u_int hval;

	hval = 0;
	while (*p) {
		hval = (hval << 4) + *p++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;
		hval &= ~g;
	}
	return (hval);
}

/* look for name in all modules */
u_int
kobj_getsymvalue(char *name, int kernelonly)
{
	Elf32_Sym *sp;
	struct modctl *modp;
	struct module *mp;

	if ((sp = lookup_kernel(name)) != NULL)
		return (sp->st_value);

	if (kernelonly)
		return (0);	/* didn't find it in the kernel so give up */

	for (modp = modules.mod_next; modp != &modules; modp = modp->mod_next) {
		mp = (struct module *)modp->mod_mp;

		if (mp == NULL || (mp->flags & KOBJ_PRIM))
			continue;

		if ((sp = lookup_one(mp, name)) != NULL)
			return (sp->st_value);
	}
	return (0);
}

/* look for a symbol near value. */
char *
kobj_getsymname(u_int value, u_int *offset)
{
	register char *name;
	struct modctl *modp;

	struct modctl_list *lp;
	struct module *mp;

	/*
	 * Loop through the primary kernel modules.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if ((name = kobj_searchsym(mp, value, offset)) != NULL)
			return (name);
	}
	for (modp = modules.mod_next; modp != &modules; modp = modp->mod_next) {
		mp = (struct module *)modp->mod_mp;

		if (mp == NULL || (mp->flags & KOBJ_PRIM))
			continue;

		if ((name = kobj_searchsym(modp->mod_mp, value,
		    offset)) != NULL)
			return (name);
	}
	return (NULL);
}

/* return address of symbol and size */

u_int
kobj_getelfsym(char *name, void *mp, int *size)
{
	Elf32_Sym *sp;

	if (mp == NULL)
		sp = lookup_kernel(name);
	else
		sp = lookup_one(mp, name);

	if (sp == NULL)
		return (0);

	*size = (int)sp->st_size;
	return (sp->st_value);
}

u_int
kobj_lookup(void *mod, char *name)
{
	Elf32_Sym *sp;

	sp = lookup_one(mod, name);

	if (sp == NULL)
		return (0);

	return (sp->st_value);
}

char *
kobj_searchsym(struct module *mp, u_int value, u_int *offset)
{
	Elf32_Sym *symtabptr;
	char *strtabptr;
	int symnum;
	Elf32_Sym *sym;
	Elf32_Sym *cursym;
	u_int curval;

	*offset = (u_int)-1;		/* assume not found */
	cursym  = NULL;

	if (kobj_addrcheck(mp, (caddr_t)value) != 0)
		return (NULL);		/* not in this module */

	strtabptr  = mp->strings;
	symtabptr  = (Elf32_Sym *)mp->symtbl;

	/*
	 * Scan the module's symbol table for a symbol <= value
	 */
	for (symnum = 0, sym = symtabptr;
		symnum < mp->nsyms;
		symnum++, sym = (Elf32_Sym *)
		    ((char *)sym+mp->symhdr->sh_entsize)) {
		if (ELF32_ST_BIND(sym->st_info) != STB_GLOBAL) {
			if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL)
				continue;

			if (ELF32_ST_TYPE(sym->st_info) != STT_OBJECT &&
			    ELF32_ST_TYPE(sym->st_info) != STT_FUNC)
				continue;
		}

		curval = sym->st_value;

		if (curval > value)
			continue;

		if (value - curval < *offset) {
			*offset = value - curval;
			cursym = sym;
		}
	}
	if (cursym == NULL)
		return (NULL);

	return (strtabptr + cursym->st_name);
}

Elf32_Sym *
kobj_lookup_all(struct module *mp, char *name, int include_self)
{
	Elf32_Sym *sp;
	struct module_list *mlp;
	struct modctl_list *clp;
	struct module *mmp;

	if (include_self && (sp = lookup_one(mp, name)) != NULL)
		return (sp);

	for (mlp = mp->head; mlp; mlp = mlp->next) {
		if ((sp = lookup_one(mlp->mp, name)) != NULL &&
		    ELF32_ST_BIND(sp->st_info) != STB_LOCAL)
			return (sp);
	}
	/*
	 * Loop through the primary kernel modules.
	 */
	for (clp = primaries; clp; clp = clp->modl_next) {
		mmp = mod(clp);

		if (mmp == NULL || mp == mmp)
			continue;

		if ((sp = lookup_one(mmp, name)) != NULL &&
		    ELF32_ST_BIND(sp->st_info) != STB_LOCAL)
			return (sp);
	}
	return (NULL);
}

static Elf32_Sym *
lookup_kernel(char *name)
{
	struct modctl_list *lp;
	struct module *mp;
	Elf32_Sym *sp;

	/*
	 * Loop through the primary kernel modules.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if (mp == NULL)
			continue;

		if ((sp = lookup_one(mp, name)) != NULL)
			return (sp);
	}
	return (NULL);
}

static Elf32_Sym *
lookup_one(struct module *mp, char *name)
{
	u_int hval;
	symid_t *ip;
	char *name1;
	Elf32_Sym *sp;

	hval = hash_name(name);

	for (ip = &mp->buckets[hval % mp->hashsize]; *ip;
	    ip = &mp->chains[*ip]) {
		sp = (Elf32_Sym *)(mp->symtbl +
		    mp->symhdr->sh_entsize * *ip);
		name1 = mp->strings + sp->st_name;
		if (strcmp(name, name1) == 0 &&
		    ELF32_ST_TYPE(sp->st_info) != STT_FILE &&
		    sp->st_shndx != SHN_UNDEF &&
		    sp->st_shndx != SHN_COMMON)
			return (sp);

	}
	return (NULL);
}

static void
sym_insert(struct module *mp, char *name, symid_t index)
{
	symid_t *ip;

#ifdef KOBJ_DEBUG
		if (kobj_debug & D_SYMBOLS) {
			static struct module *lastmp = NULL;
			Elf32_Sym *sp;
			if (lastmp != mp) {
				_printf(ops, "krtld: symbol entry: file=%s\n",
					mp->filename);
				_printf(ops, "krtld:\tsymndx\tvalue\t\t"
					"symbol name\n");
				lastmp = mp;
			}
			sp = (Elf32_Sym *)(mp->symtbl +
				index * mp->symhdr->sh_entsize);
			_printf(ops, "krtld:\t[%3d]\t0x%8x\t%s\n",
			    index, sp->st_value, name);
		}

#endif
	for (ip = &mp->buckets[hash_name(name) % mp->hashsize]; *ip;
	    ip = &mp->chains[*ip]) {
		;
	}
	*ip = index;
}

/*
 * fullname is dynamically allocated to be able to hold the
 * maximum size string that can be constructed from name.
 * path is exactly like the shell PATH variable.
 */
struct _buf *
kobj_open_path(char *name, int use_path)
{
	char *p, *q;
	char *pathp;
	char *pathpsave;
	char *fullname;
	int maxpathlen;
	register struct _buf *file;

	if (!use_path)
		pathp = "";		/* use name as specified */
	else
		pathp = module_path;	/* use configured default path */

	pathpsave = pathp;		/* keep this for error reporting */

	/*
	 * Allocate enough space for the largest possible fullname.
	 * since path is of the form <directory> : <directory> : ...
	 * we're potentially allocating a little more than we need to
	 * but we'll allocate the exact amount when we find the right directory.
	 * (The + 3 below is one for NULL terminator and one for the '/'
	 * we might have to add at the beginning of path and one for
	 * the '/' between path and name.)
	 */
	maxpathlen = strlen(pathp) + strlen(name) + 3;
	fullname = kobj_zalloc(maxpathlen, KM_WAIT);

	for (;;) {
		p = fullname;
		if (*pathp != '\0' && *pathp != '/')
			*p++ = '/';	/* path must start with '/' */
		while (*pathp && *pathp != ':' && *pathp != ' ')
			*p++ = *pathp++;
		if (p != fullname && p[-1] != '/')
			*p++ = '/';
		q = name;
		while (*q)
			*p++ = *q++;
		*p = 0;
		if ((file = kobj_open_file(fullname)) != (struct _buf *)-1) {
			kobj_free(fullname, maxpathlen);
			return (file);
		}
		if (*pathp == 0)
			break;
		pathp++;
	}
	kobj_free(fullname, maxpathlen);
	if (_moddebug & MODDEBUG_ERRMSG)
		_printf(ops, "can't open %s, path is %s\n", name, pathpsave);
	return ((struct _buf *)-1);
}

int
kobj_open(char *filename)
{
	struct vnode *vp;
	register int fd, s;

	if (_modrootloaded) {
		struct kobjopen_tctl *ltp = kobjopen_alloc(filename);
		cred_t *saved_cred;
		int errno;

		/*
		 * Hand off the open to a thread who has a
		 * stack size capable handling the request.
		 */
		if (curthread != &t0 && thread_create(NULL, DEFAULTSTKSZ * 2,
		    kobjopen_thread, (caddr_t)ltp, 0, &p0, TS_RUN,
		    MAXCLSYSPRI) != NULL) {
			sema_p(&ltp->sema);
			errno = ltp->errno;
			vp = ltp->vp;
		} else {
			/*
			 * 1098067: module creds should not be those of the
			 * caller
			 */
			saved_cred = curthread->t_cred;
			curthread->t_cred = kcred;
			errno = vn_open(filename, UIO_SYSSPACE, FREAD, 0, &vp,
			    0);
			curthread->t_cred = saved_cred;
		}
		kobjopen_free(ltp);

		if (errno) {
			if (_moddebug & MODDEBUG_ERRMSG)
				_printf(ops, "kobj_open: vn_open of %s fails, "
				    "errno = %d\n", filename, errno);
			return (-1);
		} else {
			if (_moddebug & MODDEBUG_ERRMSG)
				_printf(ops, "kobj_open: '%s' vp = 0x%x\n",
				    filename, vp);
			return ((int)vp);
		}
	} else {
		/*
		 * If the bootops are nil, it means boot is no longer
		 * available to us. So we make it look as if we can't
		 * open the named file - which is reasonably accurate.
		 */
		fd = -1;

		if (standalone) {
			fd = BOP_OPEN(ops, filename, 0);
		} else if (_bootops) {
			s = splimp();
			fd = BOP_OPEN(ops, filename, 0);
			splx(s);
		}
		if (_moddebug & MODDEBUG_ERRMSG) {
			if (fd < 0)
				_printf(ops, "kobj_open: can't open %s\n",
				    filename);
			else
				_printf(ops, "kobj_open: '%s' descr = 0x%x\n",
				    filename, fd);
		}
		return (fd);
	}
}

/*
 * Calls to kobj_open() are handled off to this routine as a separate thread.
 */
static void
kobjopen_thread(struct kobjopen_tctl *ltp)
{
	ltp->errno = vn_open(ltp->name, UIO_SYSSPACE, FREAD, 0, &(ltp->vp), 0);
	sema_v(&ltp->sema);
	thread_exit();
}

/*
 * allocate and initialize a kobjopen thread structure
 */
static struct kobjopen_tctl *
kobjopen_alloc(char *filename)
{
	struct kobjopen_tctl *ltp = kmem_zalloc(sizeof (*ltp), KM_SLEEP);

	ASSERT(filename != NULL);

	ltp->name = kmem_alloc(strlen(filename) + 1, KM_SLEEP);
	bcopy(filename, ltp->name, strlen(filename) + 1);
	sema_init(&ltp->sema, 0, "kobjopen", SEMA_DEFAULT, NULL);
	return (ltp);
}

/*
 * free a kobjopen thread control structure
 */
static void
kobjopen_free(struct kobjopen_tctl *ltp)
{
	sema_destroy(&ltp->sema);
	kmem_free(ltp->name, strlen(ltp->name) + 1);
	kmem_free(ltp, sizeof (*ltp));
}

int
kobj_read(int descr, char *buf, unsigned size, unsigned offset)
{
	int resid, stat;

	if (_modrootloaded) {
		if ((stat = vn_rdwr(UIO_READ, (struct vnode *)descr, buf, size,
		    (offset_t)offset, UIO_SYSSPACE, 0, (rlim64_t)0, CRED(),
		    &resid)) != 0) {
			_printf(ops, "vn_rdwr failed with error 0x%x\n", stat);
			return (-1);
		}
		return (size - resid);
	} else {
		register int s;
		register int count;

		if (!standalone)
			s = splimp();
		count = 0;

		if (BOP_SEEK(ops, descr, (off_t)0, offset) != 0) {
			_printf(ops, "kobj_read: seek 0x%x failed\n", offset);
			count = -1;
			goto out;
		}

		count = BOP_READ(ops, descr, buf, size);
		if (count < size) {
			if (_moddebug & MODDEBUG_ERRMSG)
				_printf(ops, "kobj_read: req %d bytes, "
				    "got %d\n", size, count);
		}
out:
		if (!standalone)
			splx(s);
		return (count);
	}
}

void
kobj_close(int descr)
{
	if (_moddebug & MODDEBUG_ERRMSG)
		_printf(ops, "kobj_close: 0x%x\n", descr);

	if (_modrootloaded) {
		register struct vnode *vp = (struct vnode *)descr;
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
	} else if (standalone || _bootops)
		(void) BOP_CLOSE(ops, descr);
}

struct _buf *
kobj_open_file(char *name)
{
	register struct _buf *file;

	file = kobj_zalloc(sizeof (struct _buf), KM_WAIT);
	file->_name = kobj_zalloc(strlen(name)+1, KM_WAIT);
	file->_base = kobj_zalloc(MAXBSIZE, KM_WAIT|KM_TEMP);

	if ((file->_fd = kobj_open(name)) == -1) {
		kobj_free(file->_base, MAXBSIZE);
		kobj_free(file->_name, strlen(name)+1);
		kobj_free(file, sizeof (struct _buf));
		return ((struct _buf *)-1);
	}
	file->_cnt = file->_size = file->_off = 0;
	file->_ln = 1;
	file->_ptr = file->_base;
	(void) strcpy(file->_name, name);
	return (file);
}

void
kobj_close_file(struct _buf *file)
{
	kobj_close(file->_fd);
	kobj_free(file->_base, MAXBSIZE);
	kobj_free(file->_name, strlen(file->_name)+1);
	kobj_free(file, sizeof (struct _buf));
}

int
kobj_read_file(struct _buf *file, char *buf, unsigned size, unsigned off)
{
	register int b_size, c_size;
	register int b_off;	/* Offset into buffer for start of bcopy */
	register int count = 0;
	register int page_addr;

	if (_moddebug & MODDEBUG_ERRMSG)
		_printf(ops, "kobj_read_file: size=%x, offset=%x\n", size, off);

	while (size) {
		page_addr = F_PAGE(off);
		b_size = file->_size;
		/*
		 * If we have the filesystem page the caller's refering to
		 * and we have something in the buffer,
		 * satisfy as much of the request from the buffer as we can.
		 */
		if (page_addr == file->_off && b_size > 0) {
			b_off = B_OFFSET(off);
			c_size = b_size - b_off;
			/*
			 * If there's nothing to copy, we're at EOF.
			 */
			if (c_size <= 0)
				break;
			if (c_size > size)
				c_size = size;
			if (buf) {
				if (_moddebug & MODDEBUG_ERRMSG)
					_printf(ops, "copying %x bytes\n",
					    c_size);
				bcopy(file->_base+b_off, buf, c_size);
				size -= c_size;
				off += c_size;
				buf += c_size;
				count += c_size;
			} else {
				_printf(ops, "kobj_read: system error");
				count = -1;
				break;
			}
		} else {
			/*
			 * If the caller's offset is page aligned and
			 * the caller want's at least a filesystem page and
			 * the caller provided a buffer,
			 * read directly into the caller's buffer.
			 */
			if (page_addr == off &&
			    (c_size = F_PAGE(size)) && buf) {
				c_size = kobj_read(file->_fd, buf, c_size,
					page_addr);
				if (c_size < 0) {
					count = -1;
					break;
				}
				count += c_size;
				if (c_size != F_PAGE(size))
					break;
				size -= c_size;
				off += c_size;
				buf += c_size;
			/*
			 * Otherwise, read into our buffer and copy next time
			 * around the loop.
			 */
			} else {
				file->_off = page_addr;
				c_size = kobj_read(file->_fd, file->_base,
						MAXBSIZE, page_addr);
				file->_ptr = file->_base;
				file->_cnt = c_size;
				file->_size = c_size;
				/*
				 * If a _filbuf call or nothing read, break.
				 */
				if (buf == NULL || c_size <= 0) {
					count = c_size;
					break;
				}
			}
			if (_moddebug & MODDEBUG_ERRMSG)
				_printf(ops, "read %x bytes\n", c_size);
		}
	}
	if (_moddebug & MODDEBUG_ERRMSG)
		_printf(ops, "count = %x\n", count);

	return (count);
}

int
kobj_filbuf(struct _buf *f)
{
	if (kobj_read_file(f, (char *)NULL, MAXBSIZE, f->_off+f->_size) > 0) {
		return (kobj_getc(f));
	}
	return (-1);
}

int
map_setup(void)
{
	caddr_t base;
	struct module *mp;
	char *old;
	struct modctl *modp;
	int flags = 0;

	if (kobj_map_space)
		return (0);

	if ((_moddebug & MODDEBUG_LOCKSYMBOLS) ||
	    ((boothowto & RB_DEBUG) &&
	    (_moddebug & MODDEBUG_UNLOCKSYMBOLS) == 0)) {
		return (-1);
	}

	/*
	 * Arrange to lock down the symbol tables if requested.
	 */
	if (kobj_locked)
		flags = KPD_LOCKED;

	if ((base =
	    (caddr_t)segkp_get(segkp, kobj_map_space_len, flags)) == NULL) {
		_printf(ops, "can't allocate address space for kobj symbols\n");
		return (-1);
	}

	kobj_map_space = (char *)base;
	kobj_map = kobj_zalloc(sizeof (struct map) * KOBJ_MAP_SEGMENTS,
	    KM_WAIT);
	mapinit(kobj_map, kobj_map_space_len, (u_long)kobj_map_space,
	    "kobj_map", KOBJ_MAP_SEGMENTS);

	/*
	 * Normally if KADB is present we lock down all symbol tables
	 * and if KADB is not present we unlock all symbol tables.
	 *
	 * There are reasons why users may want to override the default.
	 * One reason is that if the system is not boot with KADB and
	 * keeps crashing for some reason, symbols may not be in
	 * the crash dump unless the symbols are locked down.
	 * Another reason is that locking down symbols may make the
	 * system behave differently than if they are not locked down.
	 * So you may want to boot with KADB but not lock down symbols.
	 *
	 * There are two flags in moddebug to control the locking of symbols.
	 * MODDEBUG_LOCKSYMBOLS forces symbols to be locked down regardless
	 * of the presence of KADB.  If this flag is not set, then
	 * MODDEBUG_UNLOCKSYMBOLS can be set to unlock symbols even when
	 * KADB is present.
	 */


	/*
	 * copy initial symbol tables from kmem_zalloc memory to
	 * kobj_map.
	 */
	modp = &modules;
	do {
		if ((mp = modp->mod_mp) != NULL && mp->symspace != 0 &&
		    (mp->flags & KOBJ_SYMKMEM)) {
			old = mp->symspace;
			mp->symspace = (char *)
			    rmalloc_wait(kobj_map, mp->symsize);
			kcopy(old, mp->symspace, mp->symsize);
			mp->flags |= KOBJ_SYMSWAP;
			mp->flags &= ~KOBJ_SYMKMEM;

			mp->symtbl = mp->symspace;
			mp->strings = mp->symspace + mp->symhdr->sh_size;

			mp->buckets = (symid_t *)
				((((int)mp->strings + mp->strhdr->sh_size) |
				(sizeof (symid_t) - 1)) + 1);
			mp->chains = mp->buckets + mp->hashsize;
			kobj_free(old, mp->symsize);
		}
		modp = modp->mod_next;
	} while (modp != &modules);
	return (0);
}

void
kobj_free(void *address, size_t size)
{
	if (standalone)
		return;

	kmem_free(address, size);
	kobj_stat.nfree_calls++;
	kobj_stat.nfree += size;
}

void *
kobj_zalloc(size_t size, int flag)
{
	caddr_t v;

	if ((v = (caddr_t)kobj_alloc(size, flag)) != (caddr_t)NULL)
		bzero(v, size);

	return ((void *)v);
}


void *
kobj_alloc(size_t size, int flag)
{
	int kmem_flag = 0;
	void * mod_mem = NULL;

	/*
	 * If we are running standalone in the
	 * linker, we ask boot for memory.
	 * Either it's temporary memory that we lose
	 * once boot is mapped out or we allocate it
	 * permanently using the dynamic data segment.
	 */
	if (standalone) {
#ifdef BOOTSCRATCH
		if (flag & KM_TEMP)
			return (BOP_ALLOC(ops, (caddr_t)0,
			    roundup(size, mmu_pagesize), BO_NO_ALIGN));
		else
#endif
			return (segbrk(&_edata, size, MINALIGN, 0));
	}
	if (flag & KM_NOWAIT)
		kmem_flag |= KM_NOSLEEP;
	if (flag & KM_PACK) /* call packing alloc */
		mod_mem = (void *)kobj_mod_alloc(size);

	kobj_stat.nalloc_calls++;
	kobj_stat.nalloc += size;

	if (!mod_mem)
		mod_mem = kmem_alloc(size, kmem_flag);

	return (mod_mem);
}

/*
 * Allow the "mod" system to sync up with the work
 * already done by kobj during the initial loading
 * of the kernel.  This also gives us a chance
 * to reallocate memory that belongs to boot.
 */
void
kobj_sync(void)
{
	struct modctl_list *lp;
	struct module *mp;
	extern char *default_path;

	/*
	 * module_path can be set in /etc/system
	 */
	if (default_path != NULL)
		module_path = default_path;
	else
		default_path = module_path;

	/*
	 * Shuffle symbol tables from boot memory
	 * to kernel memory.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		caddr_t old;

		mp = mod(lp);
		if (mp->symspace < (char *)KERNELBASE) {
			/*
			 * Copy the pieces individually since
			 * they were set up by ld rather than us.
			 */
			if (mp->flags & KOBJ_EXEC) {
				caddr_t p;

				mp->symsize = mp->symhdr->sh_size +
					mp->strhdr->sh_size + sizeof (int) +
					mp->hashsize * sizeof (symid_t) +
					mp->nsyms * sizeof (symid_t);
				mp->symsize = (mp->symsize + 4) & ~3;
				p = kobj_zalloc(mp->symsize, KM_WAIT);
				mp->symspace = p;

				bcopy(mp->symtbl, p, mp->symhdr->sh_size);
				p += mp->symhdr->sh_size;

				bcopy(mp->strings, p, mp->strhdr->sh_size);
				p = (caddr_t)
					((((u_int)p + mp->strhdr->sh_size) |
					(sizeof (symid_t) - 1)) + 1);

				bcopy((caddr_t)mp->buckets, p,
					mp->hashsize * sizeof (symid_t));
				p += mp->hashsize * sizeof (symid_t);

				bcopy((caddr_t)mp->chains, p,
					mp->nsyms * sizeof (symid_t));
			} else {
				old = mp->symspace;
				mp->symspace = (char *)
					kobj_zalloc(mp->symsize, KM_WAIT);
				bcopy(old, mp->symspace, mp->symsize);
			}
			mp->symtbl = mp->symspace;
			mp->strings = mp->symspace + mp->symhdr->sh_size;
			mp->buckets = (symid_t *)
				((((int)mp->strings + mp->strhdr->sh_size) |
				(sizeof (symid_t) - 1)) + 1);
			mp->chains = mp->buckets + mp->hashsize;
			mp->flags |= KOBJ_SYMKMEM;
		}
	}
}

static caddr_t
segbrk(caddr_t *spp, u_int size, int align, caddr_t limit)
{
	u_int va, pva;
	u_int alloc_pgsz = mmu_pagesize;
	u_int alloc_align = BO_NO_ALIGN;
	u_int alloc_size;

	/*
	 * If we are using "large" mappings for the kernel,
	 * request aligned memory from boot using the
	 * "large" pagesize.
	 */
	if (lg_pagesize) {
		alloc_align = lg_pagesize;
		alloc_pgsz = lg_pagesize;
	}
	va = ALIGN((u_int)*spp, align);
	pva = roundup((u_int)*spp, alloc_pgsz);
	/*
	 * Need more pages?
	 */
	if (va + size > pva) {
		alloc_size = roundup(size - (pva - va), alloc_pgsz);
		/*
		 * Check for overlapping segments.
		 */
		if (limit && limit <= *spp + alloc_size)
			return ((caddr_t)0);

		pva = (u_int) BOP_ALLOC(ops, (caddr_t)pva,
					alloc_size, alloc_align);
		if (pva == NULL)
			_printf(ops, "BOP_ALLOC refused, 0x%x bytes at 0x%x\n",
				alloc_size, pva);
	}
	*spp = (caddr_t)(va + size);

	return ((caddr_t)va);
}

/*
 * Calculate the number of output hash buckets.
 * We use the next prime larger than n / 4,
 * so the average hash chain is about 4 entries.
 * More buckets would just be a waste of memory.
 */
static u_int
gethashsize(u_int n)
{
	int f;
	int hsize = MAX(n / 4, 2);

	for (f = 2; f * f <= hsize; f++)
		if (hsize % f == 0)
			hsize += f = 1;

	return (hsize);
}

static char *
basename(char *s)
{
	register char *p, *q;

	q = NULL;
	p = s;
	do {
		if (*p == '/')
			q = p;
	} while (*p++);
	return (q? q+1: s);
}

/* ARGSUSED */
static void
kprintf(void *op, char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	modvprintf(fmt, adx);
	va_end(adx);
}

int
kobj_lock_syms(void)
{
	/*
	 * If kobj_map_space in segkp is not yet allocated, just increment
	 * the counter, so it will be locked when allocated (by map_setup).
	 */
	if (kobj_map_space == NULL) {
		++kobj_locked;
		return (0);
	}

	/*
	 * XXX: If more callers are added, you may need a mutex here.
	 */
	if (kobj_locked++ == 0) {
		return ((int)as_fault(kas.a_hat, &kas, kobj_map_space,
		    kobj_map_space_len, F_SOFTLOCK, S_OTHER));
	}
	return (0);
}

int
kobj_unlock_syms(void)
{
	if (kobj_map_space == NULL) {
		--kobj_locked;
		return (0);
	}

	/*
	 * XXX: If more callers are added, you may need a mutex here.
	 */
	if (--kobj_locked == 0) {
		return ((int)as_fault(kas.a_hat, &kas, kobj_map_space,
		    kobj_map_space_len, F_SOFTUNLOCK, S_OTHER));
	}
	return (0);
}

void
kobj_stat_get(kobj_stat_t *kp)
{
	*kp = kobj_stat;
}


static caddr_t
kobj_mod_alloc(size_t size)
{
	caddr_t modptr = NULL;
	static int mod_pool_size;
	static size_t pool_offset;
	static caddr_t module_pool;

	mutex_enter(&kobj_lock);

	/*
	 *  private memory pool for modules that are not unloaded
	 * _mod_pool_pages is the # of pages to get. This is a
	 * definable global.
	 */

	if (module_pool == NULL) {
		mod_pool_size = _mod_pool_pages * PAGESIZE;

		if (mod_pool_size == 0)
			return (NULL);
		module_pool =
			(caddr_t)kmem_alloc(mod_pool_size, KM_SLEEP);
	}

	if ((pool_offset + size + 4) < mod_pool_size) {
		modptr = (caddr_t)((u_int)module_pool + (u_int)pool_offset);
		pool_offset += size;
		pool_offset = ALIGN(pool_offset, 8);
	}

	mutex_exit(&kobj_lock);
	return (modptr);
}

/*
 * Test to see if a filename is defined in the packing note section
 * of unix.
 *
 * XXX: Linear search. Maybe this could be optimized at
 * some date.
 *
 * Return 0 if not
 * Return 1 if yes
 *
 */
static int
kobj_packing_module(char *module_name)
{
	char *walker = kobj_packables;
	int size = 0;

	if (kobj_packables == NULL || module_name == NULL)
		return (0);

	while (size < kobj_packsize) {
		if ((strcmp(module_name, walker)) == 0)
			return (1);

		size += strlen(walker) + 1; /* +1 for the NULL */
		walker += strlen(walker) + 1;
	}

	return (0);
}

#define	NCPUNAMES	4

/*
 * Check for $CPU in the library path.
 * If it's there, expand path with cpuname.
 */
char *
expand_cpu(char *libname)
{
	char c, *p, *newlib, *endp;
	int diff, i;
	struct _buf *file;
	static char *cpunames[NCPUNAMES];
	static char path[MAXPATHLEN];

	/*
	 * check for $CPU between nulls or slashes
	 */
	p = strchr(libname, '$');
	if (p == NULL || strncmp(p + 1, "CPU", 3) != 0)
		return (NULL);
	c = *(p - 1);
	if (c != '/' && c != '\0')
		return (NULL);
	c = *(p + 4);
	if (c != '/' && c != '\0')
		return (NULL);
	endp = c != '\0' ? p + 4 : NULL;

	/*
	 * copy cpulist and split it into componets.
	 * then put the part of libname before $CPU into path
	 */
	if (cpunames[0] == NULL) {
		char *p1;
		p1 = kobj_zalloc(strlen(cpulist) + 1, KM_NOWAIT);
		if (p1 == NULL) {
			_printf(ops, "out of memory copying cpulist");
			return (NULL);
		}
		strcpy(p1, cpulist);
		for (i = 0; i < NCPUNAMES - 1; i++) {
			cpunames[i] = p1;
			p1 = strchr(p1, ':');
			if (p1 == NULL)
				break;
			*p1++ = '\0';
		}
	}
	diff = p - libname;
	bcopy(libname, path, diff);

	/*
	 * try componets
	 * if we find one, save and return it.
	 */
	for (i = 0; i < NCPUNAMES; i++) {
		if (cpunames[i] == NULL)
			return (NULL);
		strcpy(path + diff, cpunames[i]);
		if (endp != NULL)
			strcat(path, endp);
		if ((file = kobj_open_path(path, 1)) != (struct _buf *)-1) {
			kobj_close_file(file);
			break;
		}
	}
	newlib = kobj_zalloc(strlen(path) + 1, KM_NOWAIT);
	if (newlib == NULL) {
		_printf(ops, "out of memory expanding $CPU");
		return (NULL);
	}
	strcpy(newlib, path);
	return (newlib);
}

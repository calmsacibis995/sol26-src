/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_misc.c	1.76	96/09/26 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/errno.h>
#include <sys/cpuvar.h>
#include <sys/processor.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/callb.h>
#include <sys/fs/ufs_inode.h>
#include <vm/anon.h>
#include <sys/fs/swapnode.h>	/* for swapfs_minfree */
#include <sys/kmem.h>
#include <sys/vmmac.h>		/* for btokmx */
#include <sys/map.h>		/* for kernelmap */
#include <vm/seg_kmem.h>	/* for kvseg */
#include <sys/cpr_impl.h>
#include <sys/cpr.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/obpdefs.h>

/*
 * CPR miscellaneous support routines
 */
#define	cpr_open(path, vpp)	(vn_open(path, UIO_SYSSPACE, FCREAT|FWRITE, \
		0600, vpp, CRCREAT))
#define	cpr_rdwr(rw, vp, basep, cnt)	(vn_rdwr(rw, vp,  (caddr_t)(basep), \
		cnt, 0LL, UIO_SYSSPACE, 0, (rlim64_t)MAXOFF_T, CRED(), \
		(int *)NULL))

extern void clkset(time_t);
extern int cpr_count_kpages(int);
extern i_cpr_find_bootcpu();
extern caddr_t i_cpr_map_setup(void);
extern void i_cpr_map_destroy(void);

extern kmutex_t cpr_slock;

static struct cprconfig cprconfig;
static int cpr_statefile_ok(vnode_t *);
static int growth_ratio(void);
static int get_init_kratio(int npages);
static int cpr_p_online(cpu_t *, int);
static void cpr_save_mp_state(void);
static char *cpr_build_statefile_path(struct cprconfig *);
static int cpr_verify_statefile_path(struct cprconfig *);

#define	INTEGRAL	100	/* to get 1% precision */
#define	EXTRA_RATE	2	/* add EXTRA_RATE% extra space */

int
cpr_init(int fcn)
{
	/*
	 * Allow only one suspend/resume process.
	 */
	if (mutex_tryenter(&cpr_slock) == 0)
		return (EBUSY);

	CPR->c_flags = 0;
	CPR->c_substate = 0;
	CPR->c_cprboot_magic = 0;
	CPR->c_alloc_cnt = 0;

	CPR->c_fcn = fcn;
	CPR->c_flags |= C_SUSPENDING;
	if (fcn == AD_CPR_COMPRESS || fcn == AD_CPR_TESTZ ||
		CPR->c_fcn == AD_CPR_FORCE)
		CPR->c_flags |= C_COMPRESSING;
	/*
	 * reserve CPR_MAXCONTIG virtual pages for cpr_dump()
	 * XXX this implied argument is a bug, it should be passed
	 * XXX explicitly or should be an output arg from i_cpr_map_setup
	 * XXX (in this case we reserve 256Kb, but the code only uses 64Kb).
	 */
	CPR->c_mapping_area = i_cpr_map_setup();
	if (CPR->c_mapping_area == 0) {		/* no space in kernelmap */
		cmn_err(CE_CONT, "cpr_init unable to alloc from kernelmap\n");
		mutex_exit(&cpr_slock);
		return (EAGAIN);
	}
	DEBUG3(errp(" Reserved virtual range from %x for writing kas\n",
		CPR->c_mapping_area));

	return (0);
}

/*
 * This routine releases any resources used during the checkpoint.
 */
void
cpr_done()
{
	cbd_t *dp, *ndp;

	cpr_stat_cleanup();

	/*
	 * release memory used for bitmap
	 */
	ndp = CPR->c_bitmaps_chain;
	while ((dp = ndp) != NULL) {
		ndp = dp->cbd_next;
		kmem_free(dp->cbd_bitmap, dp->cbd_size);
		kmem_free(dp, sizeof (cbd_t));
	}
	CPR->c_bitmaps_chain = NULL;

	i_cpr_map_destroy();
	mutex_exit(&cpr_slock);
}

int
cpr_alloc_statefile()
{
	register int rc = 0;

	/*
	 * Statefile size validation. If checkpoint the first time, disk blocks
	 * allocation will be done; otherwise, just do file size check.
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY)
		(void) VOP_DUMPCTL(C_VP, 1);
	/*
	 * Open an exiting file for writing, the state file needs to be
	 * pre-allocated since we can't and don't want to do allocation
	 * during checkpoint (too much of the OS is disabled).
	 *    - do a preliminary size checking here, if it is too small,
	 *	allocate more space internally and retry.
	 *    - check the vp to make sure it's the right type.
	 */
	else {
		char *path = cpr_build_statefile_path(&cprconfig);

		if (path == NULL ||
		    cpr_verify_statefile_path(&cprconfig) != 0)
			return (ENXIO);

		if (rc = vn_open(path, UIO_SYSSPACE,
		    FCREAT|FWRITE, 0600, &C_VP, CRCREAT)) {
			errp("cpr_alloc_statefile: "
			    "Can't open statefile %s\n", path);

			return (rc);
		}
	}

	/*
	 * Not allow writing to device files for now.
	 */
	if (C_VP->v_type != VREG)
		return (EACCES);

	if (rc = cpr_statefile_ok(C_VP)) {
		(void) VOP_CLOSE(C_VP, FWRITE, 1, (offset_t)0, CRED());
		(void) vn_remove(CPR_STATE_FILE, UIO_SYSSPACE, RMFILE);
		sync();
		return (rc);
	}

	/*
	 * sync out the fs change due to the statefile reservation.
	 */
	(void) VFS_SYNC(C_VP->v_vfsp, 0, CRED());

	/*
	 * Validate disk blocks allocation for the state file.
	 * Ask the file system prepare itself for the dump operation.
	 */
	if (rc = VOP_DUMPCTL(C_VP, 0))
		return (rc);
	return (0);
}

/*
 * Do a simple minded estimate of the space needed to hold the
 * state file, take compression into account (about 3:1) but
 * be fairly conservative here so we have a better chance of
 * succesful completion, because once we stop the cpr process
 * the cost to get back to userland is fairly high.
 *
 * Do disk blocks allocation for the state file if no space has
 * been allocated yet. Since the state file will not be removed,
 * allocation should only be done once.
 */
static int
cpr_statefile_ok(vnode_t *vp)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs;
	char buf[1] = "1";
	const int HEADPAGES = 10; /* assume HEADPAGES are used for headers */
	const int UCOMP_RATE = 20; /* comp. ration*10 for user pages */
	int size, offset, bsize, error, resid;

	fs = ip->i_fs;

	/*
	 * Estimate space needed for the state file.
	 *
	 * State file size in bytes:
	 * 	kernel size + non-cache pte seg + Elf header size +
	 *	+ bitmap size + cpr state file headers size
	 * (round up to fs->fs_bsize)
	 */

	bsize = fs->fs_bsize;

	if (CPR->c_alloc_cnt++ > C_MAX_ALLOC_RETRY) {
		cpr_set_substate(C_ST_STATEF_ALLOC);
		cmn_err(CE_NOTE, "Statefile allocation retry failed.\n");
		return (ENOMEM);
	}

	/*
	 * number of pages short for swapping.
	 */

	STAT->cs_nosw_pages = k_anoninfo.ani_max + k_anoninfo.ani_mem_resv
				- MAX(availrmem - swapfs_minfree, 0);

	DEBUG9(printf("anoninfo:max=%d resv=%d free=%d swspace_left=%d\n",
		k_anoninfo.ani_max, k_anoninfo.ani_phys_resv
		+ k_anoninfo.ani_mem_resv,
		k_anoninfo.ani_free, (k_anoninfo.ani_max
		- k_anoninfo.ani_phys_resv+ k_anoninfo.ani_free)));

	if (STAT->cs_nosw_pages < 0)
		STAT->cs_nosw_pages = 0;

	/*
	 * Try different compression ratio to increase filesize.
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY) {
		if ((CPR->c_flags & C_COMPRESSING) &&
		    CPR->c_alloc_cnt != C_MAX_ALLOC_RETRY)
			size = (int)((longlong_t)((longlong_t)
				STAT->cs_est_statefsz*growth_ratio())/INTEGRAL);
		else
			size = STAT->cs_grs_statefsz;

		DEBUG1(errp("Retry statefile size = %d\n", size));
		DEBUG9(printf("Retry statefile size = %d\n", size));

	} else {
		int npages, ndvram = 0;
		longlong_t ksize = 0;

		/*
		 * The class CB_CL_CPR_RES1 is temporary until we defines a
		 * right class in the callb.h on 495 release.
		 */
		callb_execute_class(CB_CL_CPR_RES1, (int)&ndvram);
		DEBUG1(errp("ndvram size = %d\n", ndvram));
		DEBUG9(printf("ndvram size = %d\n", ndvram));

		npages = cpr_count_kpages(CPR_NOTAG);
		ksize = ndvram + (npages + HEADPAGES) * PAGESIZE +
			sizeof (cpd_t) * npages/CPR_MAXCONTIG;

		DEBUG1(errp("cpr_statefile_ok: ksize %d\n", (long)ksize));
		DEBUG9(printf("cpr_statefile_ok: ksize %ld\n", (long)ksize));

		if (CPR->c_flags & C_COMPRESSING)
			size = (ksize*100/get_init_kratio(npages)) +
				STAT->cs_nosw_pages*PAGESIZE*10/UCOMP_RATE;
		else
			size = ksize + STAT->cs_nosw_pages*PAGESIZE;
	}

	DEBUG9(printf("cpr_statefile_ok: before blkroundup size %d\n", size));

	size = blkroundup(fs, size);

	/*
	 * Export the estimated filesize info, this value will be compared
	 * before dumping out the statefile in the case of no compression.
	 */
	STAT->cs_est_statefsz = size;

	DEBUG1(errp(
		"cpr_statefile_ok: Estimated statefile size %d i_size=%lld\n",
		size, ip->i_size));
	DEBUG9(printf("cpr_statefile_ok: Estimated statefsize=%d i_size=%lld\n",
		size, ip->i_size));

	/*
	 * Check file size, if 0, allocate disk blocks for it;
	 * otherwise, just do validation.
	 */
	rw_enter(&ip->i_contents, RW_READER);
	if (ip->i_size == 0 || ip->i_size < size) {
		rw_exit(&ip->i_contents);

		/*
		 * Write 1 byte to each logincal block to reserve
		 * disk blocks space.
		 */
		for (offset = ip->i_size + (bsize - 1); offset <= size;
			offset += bsize) {
			static char *emsg_format =
				"Need %d more bytes disk space for "
				"statefile.\nCurrent statefile is %s%s.  "
				"See power.conf(4) for instructions "
				"on relocating it to a different disk "
				"partition.\n";

			if (error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&buf,
				sizeof (buf), (offset_t)offset, UIO_SYSSPACE, 0,
				(rlim64_t)MAXOFF_T, CRED(), &resid)) {
				if (error == ENOSPC)
					cmn_err(CE_WARN, emsg_format,
					size - offset + bsize - 1,
					cprconfig.cf_fs, cprconfig.cf_path);
				return (error);
			}
		}
		return (0);
	}
	rw_exit(&ip->i_contents);
	return (0);
}

void
cpr_statef_close()
{
	if (C_VP) {
		(void) VOP_DUMPCTL(C_VP, 1);
		(void) VOP_CLOSE(C_VP, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(C_VP);
		C_VP = 0;
	}
}

/*
 * Write the cprinfo structure to disk.  This contains the original
 * values of any prom properties that we are going to modify.  We fill
 * in the magic number of the file here as a signal to the booter code
 * that the state file is valid.  Be sure the file gets synced, since
 * we are going to be shutting down the OS.
 */
int
cpr_validate_cprinfo(struct cprinfo *ci)
{
	struct vnode *vp;
	int rc;

	if (strlen(CPR_STATE_FILE) >= MAXPATHLEN) {
		cmn_err(CE_NOTE, "CPR path len %d over limits\n",
			strlen(CPR_STATE_FILE));
		return (-1);
	}

	ci->ci_magic = CPR->c_cprboot_magic = CPR_DEFAULT_MAGIC;

	if ((rc = cpr_open(CPR_DEFAULT, &vp))) {
		cmn_err(CE_NOTE, "Failed to open cprinfo file, rc = %d\n", rc);
		return (rc);
	}
	if ((rc = cpr_rdwr(UIO_WRITE,
	    vp, ci, sizeof (struct cprinfo))) != 0) {
		cmn_err(CE_NOTE, "Failed writing cprinfo file, rc = %d\n", rc);
		(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (rc);
	}
	if ((rc = VOP_FSYNC(vp, FSYNC, CRED())) != 0)
		cmn_err(CE_NOTE, "Cannot fsync generic, rc = %d\n", rc);

	(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
	VN_RELE(vp);

	DEBUG2(errp("cpr_validate_cprinfo: magic=0x%x boot-file=%s "
	    "boot-device=%s auto-boot?=%s diag-switch?=%s\n",
	    ci->ci_magic, ci->ci_bootfile,
	    ci->ci_bootdevice, ci->ci_autoboot, ci->ci_diagsw));

	return (rc);
}

/*
 * Clear the magic number in the defaults file.  This tells the booter
 * program that the state file is not current and thus prevents
 * any attempt to restore from an obsolete state file.
 */
void
cpr_void_cprinfo(struct cprinfo *ci)
{
	struct vnode *vp;

	if (CPR->c_cprboot_magic == CPR_DEFAULT_MAGIC) {
		ci->ci_magic = 0;
		if (cpr_open(CPR_DEFAULT, &vp)) {
			cmn_err(CE_NOTE, "cpr_void_cprinfo: "
			    "Failed to open %s\n", CPR_DEFAULT);

			return;
		}

		(void) cpr_rdwr(UIO_WRITE, vp, ci, sizeof (struct cprinfo));
		(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(vp);
	}
}

/*
 * Open the file whose pathname is provided by caller, read the
 * cprinfo structure it contains, and verify the magic number.
 */
int
cpr_cprinfo_is_valid(char *file, int magic, struct cprinfo *cip)
{
	struct vnode *vp;
	int rc = 0;

	if (rc = cpr_open(file, &vp)) {
		cmn_err(CE_NOTE, "Failed to open %s\n", file);
		return (0);
	}

	rc = cpr_rdwr(UIO_READ, vp, cip, sizeof (struct cprinfo));
	(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
	VN_RELE(vp);
	if (!rc && cip->ci_magic == magic)
		return (1);

	return (0);
}

/*
 * clock/time related routines
 */
static time_t   cpr_time_stamp;

void
cpr_save_time()
{
	cpr_time_stamp = hrestime.tv_sec;
}

/*
 * correct time based on saved time stamp or hardware clock
 */
void
cpr_restore_time()
{
	clkset(cpr_time_stamp);
}

/*
 * need to grow statefile by the following times.
 */
static int
growth_ratio()
{
	return (((longlong_t)STAT->cs_grs_statefsz * INTEGRAL)/
		STAT->cs_dumped_statefsz + EXTRA_RATE);
}

struct comp_ratio {
	int spage;	/* low mem page # */
	int epage;	/* high mem page boundry */
	int ratio;	/* rate * 100 */
};

/*
 * tunable compression ratios for kernel pages
 * The # of kernel pages and their comp ratios fairly closely follow
 * the following 2nd derivative curve:
 *
 *	ratio = 2.1 + sqrt((npages - 2320)/3160)
 *
 * The table below is generated from the above formular which is modified
 * to produce conservative numbers.
 */
static struct comp_ratio init_kratio[] = {
	0,	2300,	210,		/* kernel pages for 16 MB */
	2300,	2500,	220,		/* kernel pages for 16 MB */
	2500,	2700,	230,		/* kernel pages for 16 MB */
	2700,	2900,	242,		/* kernel pages for 16/32 MB */
	2900,	3100,	248,		/* kernel pages for 16/32 MB */
	3100,	3300,	252,		/* kernel pages for 32 MB */
	3300,	3500,	258,		/* kernel pages for 32 MB */
	3500,	3700,	263,		/* kernel pages for 32/48 MB */
	3700,	3900,	268,		/* kernel pages for 32/48 MB */
	3900,	4100,	273,		/* kernel pages for 32/48/64 MB */
	4100,	4300,	280,		/* kernel pages for 32/48/64 MB */
	4300,	4600,	285,		/* kernel pages for 64 MB */
	4600,	15000,	290 		/* kernel pages for 64 MB */
};

static int
get_init_kratio(int npages)
{
	int i;

	for (i = 0; i < sizeof (init_kratio)/sizeof (struct comp_ratio); i++) {
		if (init_kratio[i].spage < npages &&
		    init_kratio[i].epage >= npages)
			return (init_kratio[i].ratio);
	}
	/*
	 * otherwise return no compression
	 */
	return (100);
}

/*
 * CPU ONLINE/OFFLINE CODE
 */
int
cpr_mp_offline()
{
	cpu_t *cp, *bootcpu;
	int rc = 0;

	/*
	 * Do nothing for UP.
	 */
	if (ncpus == 1)
		return (0);

	DEBUG1(errp("on CPU %d (%x)\n", CPU->cpu_id, CPU));
	DEBUG9(printf("on CPU %d (%x)\n", CPU->cpu_id, (int)CPU));
	cpr_save_mp_state();

	bootcpu = cpu[i_cpr_find_bootcpu()];
	if (!CPU_ACTIVE(bootcpu))
		if ((rc = cpr_p_online(bootcpu, P_ONLINE)))
			return (rc);

	cp = cpu_list;
	do {
		if (cp == bootcpu)
			continue;

		if ((rc = cpr_p_online(cp, P_OFFLINE)))
			return (rc);
	} while ((cp = cp->cpu_next) != cpu_list);


	return (rc);
}

int
cpr_mp_online()
{
	cpu_t *cp, *bootcpu = CPU;
	int rc = 0;

	/*
	 * Do nothing for UP.
	 */
	if (ncpus == 1)
		return (0);

	DEBUG1(errp("on CPU %d (%x)\n", CPU->cpu_id, CPU));
	DEBUG9(printf("on CPU %d (%x)\n", CPU->cpu_id, (int)CPU));

	/*
	 * restart all online cpus
	 */
	for (cp = bootcpu->cpu_next; cp != bootcpu; cp = cp->cpu_next) {
		if (CPU_CPR_IS_OFFLINE(cp))
			continue;

		if ((rc = cpr_p_online(cp, P_ONLINE)))
			return (rc);
	}

	/*
	 * turn off the boot cpu if it was offlined
	 */
	if (CPU_CPR_IS_OFFLINE(bootcpu)) {
		if ((rc = cpr_p_online(bootcpu, P_OFFLINE)))
			return (rc);
	}
	return (0);
}

static void
cpr_save_mp_state()
{
	cpu_t *cp;

	cp = cpu_list;
	do {
		cp->cpu_cpr_flags &= ~CPU_CPR_ONLINE;
		if (CPU_ACTIVE(cp))
			CPU_SET_CPR_FLAGS(cp, CPU_CPR_ONLINE);
	} while ((cp = cp->cpu_next) != cpu_list);
}

/*
 * The followings are CPR MP related routines.
 *
 */

static int
cpr_p_online(cpu_t *cp, int state)
{
	int rc;

	DEBUG1(errp("changing cpu %d to state %d\n", cp->cpu_id, state));
	DEBUG9(printf("changing cpu %d to state %d\n", cp->cpu_id, state));

	mutex_enter(&cpu_lock);
	switch (state) {
	case P_ONLINE:
		rc = cpu_online(cp);
		break;
	case P_OFFLINE:
		rc = cpu_offline(cp);
		break;
	}
	mutex_exit(&cpu_lock);
	if (rc) {
		cmn_err(CE_WARN,
			"failed to change processor %d to state %d (rc %d)\n",
				cp->cpu_id, state, rc);
	}
	return (rc);
}

/*
 * Construct the pathname of the state file and return a pointer to
 * caller.  Read the config file to get the mount point of the
 * filesystem and the pathname within fs.
 */
static char *
cpr_build_statefile_path(struct cprconfig *cf)
{
	static char full_path[MAXNAMELEN];
	struct vnode *vp;
	int err;

	if ((err = vn_open(CPR_CONFIG, UIO_SYSSPACE, FREAD, 0, &vp, 0)) != 0) {
		cmn_err(CE_NOTE, "cpr: Unable to open "
		    "configuration file %s.  Errno = %d.", CPR_CONFIG, err);

		return (NULL);
	}

	if (cpr_rdwr(UIO_READ, vp, cf, sizeof (*cf)) != 0 ||
	    cf->cf_magic != CPR_CONFIG_MAGIC) {
		cmn_err(CE_NOTE, "cpr: Unable to "
		    "read config file %s\n", CPR_CONFIG);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);

		return (NULL);
	}

	(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
	VN_RELE(vp);

	if (strlen(cf->cf_path) + strlen(cf->cf_fs) >= MAXNAMELEN - 1) {
		cmn_err(CE_NOTE, "cpr: statefile path too long.\n");

		return (NULL);
	}

	(void) strcpy(full_path, cf->cf_fs);
	(void) strcat(full_path, "/");
	(void) strcat(full_path, cf->cf_path + (*cf->cf_path == '/' ? 1 : 0));

	return (full_path);
}

/*
 * Verify that the information in the configuration file regarding the
 * location for the statefile is still valid.  The path given there
 * must be a directory which is the mount point of a ufs filesystem, and
 * the full device path for that fs must be equal to the one given
 * in the configuration file.
 */
static int
cpr_verify_statefile_path(struct cprconfig *cf)
{
	extern struct vfs *rootvfs;
	extern struct vfssw vfssw[];
	extern int lookupname(char *,
	    enum uio_seg, enum symfollow, vnode_t **, vnode_t **);
	struct vfs *vfsp;
	int error;
	struct vnode *vp;
	char devpath[OBP_MAXPATHLEN];

	/*
	 * We need not worry about locking or the timing of releasing
	 * the vnode, since we are single-threaded now.
	 */

	if ((error = lookupname(cf->cf_fs,
	    UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) != 0) {
		errp("cpr: Mount point %s for statefile fs not found\n",
		    cf->cf_fs);

		return (error);
	}

	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		struct ufsvfs *ufsvfsp;

		if (strcmp(vfssw[vfsp->vfs_fstype].vsw_name, "ufs"))
			continue;

		ufsvfsp = (struct ufsvfs *) vfsp->vfs_data;
		if (ufsvfsp != NULL && ufsvfsp->vfs_root == vp)
			break;
	}
	VN_RELE(vp);

	if (vfsp == NULL) {
		errp("cpr: %s is not the mount point of a ufs filesystem.\n",
		    cf->cf_fs);

		return (ENODEV);
	}

	if (ddi_dev_pathname(vfsp->vfs_dev, devpath)
	    != 0 || strcmp(devpath, cf->cf_fs_dev)) {
		errp("cpr: device path for statefile fs has changed\n ");
		return (ENXIO);
	}

	return (0);
}

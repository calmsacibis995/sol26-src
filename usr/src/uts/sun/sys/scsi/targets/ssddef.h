/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Copyright (c) 1995 by Cray Research, Inc.
 */

#ifndef	_SYS_SCSI_TARGETS_SSDDEF_H
#define	_SYS_SCSI_TARGETS_SSDDEF_H

#pragma ident	"@(#)ssddef.h	1.21	96/09/14 SMI"

#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for SCSI direct access devices
 */

#if	defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Manifest defines
 */

#define	USCSI_DEFAULT_MAXPHYS	0x80000

/*
 * Local definitions, for clarity of code
 */
#define	SSD_SCSI_DEVP	(un->un_sd)
#define	SSD_DEVINFO	(SSD_SCSI_DEVP->sd_dev)
#define	SSD_INQUIRY	(SSD_SCSI_DEVP->sd_inq)
#define	SSD_RQSENSE	(SSD_SCSI_DEVP->sd_sense)
#define	SSD_MUTEX	(&SSD_SCSI_DEVP->sd_mutex)
#define	ROUTE		(&SSD_SCSI_DEVP->sd_address)
#define	SCBP(pkt)	((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)	((*(pkt)->pkt_scbp) & STATUS_MASK)
#define	CDBP(pkt)	((union scsi_cdb *)(pkt)->pkt_cdbp)
#define	NO_PKT_ALLOCATED ((struct buf *)0)
#define	ALLOCATING_PKT	((struct buf *)-1)
#define	BP_PKT(bp)	((struct scsi_pkt *)bp->av_back)
#define	BP_HAS_NO_PKT(bp) (bp->av_back == NO_PKT_ALLOCATED)

#define	STATUS_SCBP_C(statusp)	(*(u_char *)(statusp) & STATUS_MASK)

#define	Tgt(devp)	(devp->sd_address.a_target)
#define	Lun(devp)	(devp->sd_address.a_lun)

#define	New_state(un, s)	\
	(un)->un_last_state = (un)->un_state,  (un)->un_state = (s)
#define	Restore_state(un)	\
	{ u_char tmp = (un)->un_last_state; New_state((un), tmp); }



/*
 * Structure for recording whether a device is fully open or closed.
 * Assumptions:
 *
 *	+ There are only 8 partitions possible.
 *	+ BLK, MNT, CHR, SWP don't change in some future release!
 *
 */

#define	SSDUNIT_SHIFT	3
#define	SSDPART_MASK	7
#define	SSDUNIT(dev)	(getminor((dev))>>SSDUNIT_SHIFT)
#define	SSDPART(dev)	(getminor((dev))&SSDPART_MASK)

struct ocinfo {
	/*
	* Types BLK, MNT, CHR, SWP,
	* assumed to be types 0-3.
	*/
	u_long  lyr_open[NDKMAP];
	u_char  reg_open[OTYPCNT - 1];
};
#define	OCSIZE  sizeof (struct ocinfo)
union ocmap {
	u_char chkd[OCSIZE];
	struct ocinfo rinfo;
};
#define	lyropen rinfo.lyr_open
#define	regopen rinfo.reg_open

/*
 * Private info for scsi disks.
 *
 * Pointed to by the un_private pointer
 * of one of the SCSI_DEVICE structures.
 */

struct ssa_disk {
	struct scsi_device *un_sd;	/* back pointer to SCSI_DEVICE */
	struct scsi_pkt *un_rqs;	/* ptr to request sense command pkt */
	struct buf *un_rqs_bp;		/* ptr to request sense bp */
	ksema_t	un_rqs_sema;		/* sema to protect req sense pkt */
	struct	buf *un_sbufp;		/* for use in special io */
	char		*un_srqbufp;	/* sense buffer for special io */
	kcondvar_t	un_sbuf_cv;	/* Conditional Variable on sbufp */
	union	ocmap un_ocmap;		/* open partition map, block && char */
	struct	dk_map un_map[NDKMAP];	/* logical partitions */
	u_long	un_offset[NDKMAP];	/* starting block for partitions */
	struct	dk_geom un_g;		/* disk geometry */
	u_char	un_last_pkt_reason;	/* used for suppressing multiple msgs */
	struct	dk_vtoc un_vtoc;	/* disk Vtoc */
	struct	diskhd un_utab;		/* for queuing */
	struct	kstat *un_stats;	/* for statistics */
	struct	kstat *un_pstats[NDKMAP];	/* for partition statistics */
	ksema_t	un_semoclose;		/* lock for serializing opens/closes */
	u_int	un_err_blkno;		/* disk block where error occurred */
	long	un_capacity;		/* capacity of drive */
	u_char	un_exclopen;		/* exclusive open bits */
	u_char	un_gvalid;		/* geometry is valid */
	u_char	un_state;		/* current state */
	u_char	un_last_state;		/* last state */
	u_char	un_format_in_progress;	/* disk is formatting currently */
	u_char	un_start_stop_issued;	/* START_STOP cmd issued to disk */
	u_char	un_asciilabel[LEN_DKL_ASCII];	/* Copy of asciilabel */
	short	un_throttle;		/* max outstanding cmds */
	short	un_save_throttle;	/* max outstanding cmds saved */
	short	un_ncmds;		/* number of cmds in transport */
	long	un_tagflags;		/* Pkt Flags for Tagged Queueing  */
	short	un_sbuf_busy;		/* Busy wait flag for the sbuf */
	short	un_resvd_status;	/* Reservation Status */
	opaque_t	un_mhd_token;	/* scsi watch request */
	int	un_resvd_timeid;	/* timeout id for resvd recover */
	int	un_reset_throttle_timeid; /* timeout id to reset throttle */
	int	un_restart_timeid;	/* timeout id for restart unit */
	int	un_reissued_timeid;	/* timeout id for sdrestarts */
	u_char	un_ssa_fast_writes;	/* SSA(Pluto) supports fast writes */
	ddi_devid_t	un_devid;	/* device id */
	u_int	un_max_xfer_size;	/* max transfer size */
};

#define	SSD_MAX_XFER_SIZE	(1024 * 1024)

_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex, ssa_disk))
_NOTE(READ_ONLY_DATA(ssa_disk::un_sd))
_NOTE(SCHEME_PROTECTS_DATA("save sharing",
	ssa_disk::un_mhd_token
	ssa_disk::un_state
	ssa_disk::un_tagflags
	ssa_disk::un_format_in_progress
	ssa_disk::un_gvalid
	ssa_disk::un_resvd_timeid))

_NOTE(SCHEME_PROTECTS_DATA("stable data",
	ssa_disk::un_max_xfer_size
	ssa_disk::un_offset))

_NOTE(SCHEME_PROTECTS_DATA("semaphore",
	ssa_disk::un_rqs
	ssa_disk::un_rqs_bp))

_NOTE(SCHEME_PROTECTS_DATA("cv",
	ssa_disk::un_sbufp
	ssa_disk::un_srqbufp
	ssa_disk::un_sbuf_busy))

_NOTE(SCHEME_PROTECTS_DATA("Unshared data",
	uio
	buf
	scsi_pkt
	uscsi_cmd
	scsi_capacity
	scsi_cdb scsi_arq_status
	dk_label
	dk_map))

#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#define	MAX_THROTTLE	256

/*
 * Disk driver states
 */

#define	SSD_STATE_NORMAL	0
#define	SSD_STATE_OFFLINE	1
#define	SSD_STATE_RWAIT		2
#define	SSD_STATE_DUMPING	3
#define	SSD_STATE_EJECTED	4
#define	SSD_STATE_SUSPENDED	5

/*
 * The table is to be interpreted as follows: The rows lists all the states
 * and each column is a state that a state in each row *can* reach. The entries
 * in the table list the event that cause that transition to take place.
 * For e.g.: To go from state RWAIT to SUSPENDED, event (d)-- which is the
 * invocation of DDI_SUSPEND-- has to take place. Note the same event could
 * cause the transition from one state to two different states. e.g., from
 * state SUSPENDED, when we get a DDI_RESUME, we just go back to the *last
 * state* whatever that might be. (NORMAL or OFFLINE).
 *
 *
 * State Transition Table:
 *
 *		      NORMAL  OFFLINE  RWAIT  DUMPING  SUSPENDED
 *
 *   NORMAL		 -	(a)	 (b)     (c)	   (d)
 *
 *   OFFLINE		(e)	 -       (e)     (c)       (d)
 *
 *   RWAIT		(f)	NP        -      (c)       (d)
 *
 *   DUMPING		NP      NP        NP      -         NP
 *
 *   SUSPENDED          (g)     (g)       (b)     NP*       -
 *
 *
 *   NP :	Not Possible.
 *   (a): 	Disk does not respond.
 *   (b): 	Packet Allocation Fails
 *   (c):	Panic - Crash dump
 *   (d):	DDI_SUSPEND is called.
 *   (e):	Disk has a successful I/O completed.
 *   (f):	ssdrunout() calls ssdstart() which sets it NORMAL
 *   (g):	DDI_RESUME is called.
 *    * :	When suspended, we dont change state during panic dump
 */

/*
 * Error levels
 */

#define	SSDERR_ALL		0
#define	SSDERR_UNKNOWN		1
#define	SSDERR_INFORMATIONAL	2
#define	SSDERR_RECOVERED	3
#define	SSDERR_RETRYABLE	4
#define	SSDERR_FATAL		5

/*
 * Parameters
 */

/*
 * 60 seconds is a *very* reasonable amount of time for most slow CD
 * operations.
 */

#define	SSD_IO_TIME	60

/*
 * 2 hours is an excessively reasonable amount of time for format operations.
 */

#define	SSD_FMT_TIME	120*60

/*
 * 5 seconds is what we'll wait if we get a Busy Status back
 */

#define	SSD_BSY_TIMEOUT		(drv_usectohz(5 * 1000000))

/*
 * 60 seconds is what we will wait for to reset the
 * throttle back to it MAX_THROTTLE.
 */
#define	SSD_RESET_THROTTLE_TIMEOUT	60

/*
 * Number of times we'll retry a normal operation.
 *
 * This includes retries due to transport failure
 * (need to distinguish between Target and Transport failure)
 */

#define	SSD_RETRY_COUNT		3


/*
 * Maximum number of units we can support
 * (controlled by room in minor device byte)
 * XXX: this is out of date!
 */
#define	SSD_MAXUNIT		32

/*
 * Reservation Status's
 */
#define	SSD_RELEASE			0x0000
#define	SSD_RESERVE			0x0001
#define	SSD_TKOWN			0x0002
#define	SSD_LOST_RESERVE		0x0004
#define	SSD_FAILFAST			0x0080
#define	SSD_WANT_RESERVE		0x0100
#define	SSD_RESERVATION_CONFLICT	0x0200
#define	SSD_PRIORITY_RESERVE		0x0400

/*
 * delay before reclaiming reservation is 6 seconds, in units of micro seconds
 */
#define	SSD_REINSTATE_RESV_DELAY	6000000

/*
 * sdintr action codes
 */

#define	COMMAND_DONE		0
#define	COMMAND_DONE_ERROR	1
#define	QUE_COMMAND		2
#define	QUE_SENSE		3
#define	JUST_RETURN		4

/*
 * Commands for sd_start_stop
 */
#define	SSD_STOP	((caddr_t)0)
#define	SSD_START	((caddr_t)1)

#define	VIDMAX 8
#define	PIDMAX 16

/*
 * Options
 */
#define	SSD_NODISC	0x0001	/* has problem w/ disconnect-reconnect */
#define	SSD_NOPARITY	0x0002	/* target does not generate parity */
#define	SSD_MULTICMD	0x0004	/* target supports SCSI-2 multiple commands */
#define	SSD_EIOENABLE	0x0008	/* Enable retruning EIO on media change	*/
#define	SSD_QUEUEING	0x0010	/* Enable Command Queuing to Host Adapter */

/*
 * Some internal error codes for driver functions.
 */
#define	SSD_EACCES	1

/*
 * this should be moved to common/sys/scsi/generic/commands.h (Group 1 cmd)
 */
#define	SCMD_SYNCHRONIZE_CACHE	0x35
#define	SSA_PRIORITY_RESERVE	0x80

/*
 * Error returns for ssd_validate_geometry()
 */

#define	SSD_BAD_LABEL		-1
#define	SSD_NO_MEM_FOR_LABEL	-2

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_TARGETS_SSDDEF_H */

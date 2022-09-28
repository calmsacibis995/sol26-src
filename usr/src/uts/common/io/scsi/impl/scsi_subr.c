/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_subr.c	1.62	96/09/25 SMI"

#include <sys/scsi/scsi.h>

/*
 * Utility SCSI routines
 */

/*
 * Polling support routines
 */

extern uintptr_t scsi_callback_id;

static int scsi_poll_busycnt = SCSI_POLL_TIMEOUT;

/*
 * Common buffer for scsi_log
 */

extern kmutex_t scsi_log_mutex;
static char scsi_log_buffer[256];


#define	A_TO_TRAN(ap)	(ap->a_hba_tran)
#define	P_TO_TRAN(pkt)	((pkt)->pkt_address.a_hba_tran)
#define	P_TO_ADDR(pkt)	(&((pkt)->pkt_address))


int
scsi_poll(struct scsi_pkt *pkt)
{
	register busy_count, rval = -1, savef;
	long savet;
	void (*savec)();

	/*
	 * save old flags..
	 */
	savef = pkt->pkt_flags;
	savec = pkt->pkt_comp;
	savet = pkt->pkt_time;

	pkt->pkt_flags |= FLAG_NOINTR;

	/*
	 * XXX there is nothing in the SCSA spec that states that we should not
	 * do a callback for polled cmds; however, removing this will break sd
	 * and probably other target drivers
	 */
	pkt->pkt_comp = 0;

	/*
	 * we don't like a polled command without timeout.
	 * 60 seconds seems long enough.
	 */
	if (pkt->pkt_time == 0) {
		pkt->pkt_time = SCSI_POLL_TIMEOUT;
	}

	for (busy_count = 0; busy_count < scsi_poll_busycnt; busy_count++) {

		if (scsi_transport(pkt) != TRAN_ACCEPT) {
			break;
		}
		if (pkt->pkt_reason == CMD_INCOMPLETE && pkt->pkt_state == 0) {
			drv_usecwait(1000000);

		} else if (pkt->pkt_reason != CMD_CMPLT) {
			break;

		} else if (((*pkt->pkt_scbp) & STATUS_MASK) == STATUS_BUSY) {
			drv_usecwait(1000000);

		} else {
			rval = 0;
			break;
		}
	}

	pkt->pkt_flags = savef;
	pkt->pkt_comp = savec;
	pkt->pkt_time = savet;
	return (rval);
}

/*
 * Command packaging routines.
 *
 * makecom_g*() are original routines and scsi_setup_cdb()
 * is the new and preferred routine.
 */

/*
 * These routines put LUN information in CDB byte 1 bits 7-5.
 * This was required in SCSI-1. SCSI-2 allowed it but it preferred
 * sending LUN information as part of IDENTIFY message.
 * This is not allowed in SCSI-3.
 */

void
makecom_g0(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G0(pkt, devp, flag, cmd, addr, (u_char) cnt);
}

void
makecom_g0_s(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int cnt, int fixbit)
{
	MAKECOM_G0_S(pkt, devp, flag, cmd, cnt, (u_char) fixbit);
}

void
makecom_g1(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G1(pkt, devp, flag, cmd, addr, cnt);
}

void
makecom_g5(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G5(pkt, devp, flag, cmd, addr, cnt);
}

/*
 * Following routine does not put LUN information in CDB.
 * This interface must be used for SCSI-2 targets having
 * more than 8 LUNs or a SCSI-3 target.
 */
int
scsi_setup_cdb(union scsi_cdb *cdbp, u_char cmd, u_int addr, u_int cnt,
    u_int addtl_cdb_data)
{
	u_int	addr_cnt;

	cdbp->scc_cmd = cmd;

	switch (CDB_GROUPID(cmd)) {
		case CDB_GROUPID_0:
			/*
			 * The following calculation is to take care of
			 * the fact that format of some 6 bytes tape
			 * command is different (compare 6 bytes disk and
			 * tape read commands).
			 */
			addr_cnt = (addr << 8) + cnt;
			addr = (addr_cnt & 0x1fffff00) >> 8;
			cnt = addr_cnt & 0xff;
			FORMG0ADDR(cdbp, addr);
			FORMG0COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_1:
		case CDB_GROUPID_2:
			FORMG1ADDR(cdbp, addr);
			FORMG1COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_4:
			FORMG4ADDR(cdbp, addr);
			FORMG4COUNT(cdbp, cnt);
			FORMG4ADDTL(cdbp, addtl_cdb_data);
			break;

		case CDB_GROUPID_5:
			FORMG5ADDR(cdbp, addr);
			FORMG5COUNT(cdbp, cnt);
			break;

		default:
			return (0);
	}

	return (1);
}


/*
 * Common iopbmap data area packet allocation routines
 */

struct scsi_pkt *
get_pktiopb(struct scsi_address *ap, caddr_t *datap, int cdblen, int statuslen,
    int datalen, int readflag, int (*func)())
{
	scsi_hba_tran_t	*tran = A_TO_TRAN(ap);
	dev_info_t	*pdip = tran->tran_hba_dip;
	struct scsi_pkt	*pkt = NULL;
	struct buf	local;

	if (!datap)
		return (pkt);
	*datap = (caddr_t)0;
	bzero((caddr_t)&local, sizeof (struct buf));
	if (ddi_iopb_alloc(pdip, (ddi_dma_lim_t *)0,
	    (u_int) datalen, &local.b_un.b_addr)) {
		return (pkt);
	}
	if (readflag)
		local.b_flags = B_READ;
	local.b_bcount = datalen;
	pkt = (*tran->tran_init_pkt) (ap, NULL, &local,
		cdblen, statuslen, 0, PKT_CONSISTENT,
		(func == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC,
		NULL);
	if (!pkt) {
		ddi_iopb_free(local.b_un.b_addr);
		if (func != NULL_FUNC) {
			ddi_set_callback(func, NULL, &scsi_callback_id);
		}
	} else {
		*datap = local.b_un.b_addr;
	}
	return (pkt);
}

/*
 *  Equivalent deallocation wrapper
 */

void
free_pktiopb(struct scsi_pkt *pkt, caddr_t datap, int datalen)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	register scsi_hba_tran_t	*tran = A_TO_TRAN(ap);

	(*tran->tran_destroy_pkt)(ap, pkt);
	if (datap && datalen) {
		ddi_iopb_free(datap);
	}
	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
}

/*
 * Common naming functions
 */

static char scsi_tmpname[64];

char *
scsi_dname(int dtyp)
{
	static char *dnames[] = {
		"Direct Access",
		"Sequential Access",
		"Printer",
		"Processor",
		"Write-Once/Read-Many",
		"Read-Only Direct Access",
		"Scanner",
		"Optical",
		"Changer",
		"Communications",
		"Array Controller"
	};

	if ((dtyp & DTYPE_MASK) <= DTYPE_COMM) {
		return (dnames[dtyp&DTYPE_MASK]);
	} else if (dtyp == DTYPE_NOTPRESENT) {
		return ("Not Present");
	}
	return ("<unknown device type>");

}

char *
scsi_rname(u_char reason)
{
	static char *rnames[] = {
		"cmplt",
		"incomplete",
		"dma_derr",
		"tran_err",
		"reset",
		"aborted",
		"timeout",
		"data_ovr",
		"cmd_ovr",
		"sts_ovr",
		"badmsg",
		"nomsgout",
		"xid_fail",
		"ide_fail",
		"abort_fail",
		"reject_fail",
		"nop_fail",
		"per_fail",
		"bdr_fail",
		"id_fail",
		"unexpected_bus_free",
		"tag reject",
		"terminated"
	};
	if (reason > CMD_TAG_REJECT) {
		return ("<unknown reason>");
	} else {
		return (rnames[reason]);
	}
}

char *
scsi_mname(u_char msg)
{
	static char *imsgs[23] = {
		"COMMAND COMPLETE",
		"EXTENDED",
		"SAVE DATA POINTER",
		"RESTORE POINTERS",
		"DISCONNECT",
		"INITIATOR DETECTED ERROR",
		"ABORT",
		"REJECT",
		"NO-OP",
		"MESSAGE PARITY",
		"LINKED COMMAND COMPLETE",
		"LINKED COMMAND COMPLETE (W/FLAG)",
		"BUS DEVICE RESET",
		"ABORT TAG",
		"CLEAR QUEUE",
		"INITIATE RECOVERY",
		"RELEASE RECOVERY",
		"TERMINATE PROCESS",
		"CONTINUE TASK",
		"TARGET TRANSFER DISABLE",
		"RESERVED (0x14)",
		"RESERVED (0x15)",
		"CLEAR ACA"
	};
	static char *imsgs_2[6] = {
		"SIMPLE QUEUE TAG",
		"HEAD OF QUEUE TAG",
		"ORDERED QUEUE TAG",
		"IGNORE WIDE RESIDUE",
		"ACA",
		"LOGICAL UNIT RESET"
	};

	if (msg < 23) {
		return (imsgs[msg]);
	} else if (IS_IDENTIFY_MSG(msg)) {
		return ("IDENTIFY");
	} else if (IS_2BYTE_MSG(msg) &&
	    (int)((msg) & 0xF) < sizeof (imsgs_2)) {
		return (imsgs_2[msg & 0xF]);
	} else {
		return ("<unknown msg>");
	}

}

char *
scsi_cname(u_char cmd, register char **cmdvec)
{
	while (*cmdvec != (char *)0) {
		if (cmd == **cmdvec) {
			return ((char *)((int)(*cmdvec)+1));
		}
		cmdvec++;
	}
	return (sprintf(scsi_tmpname, "<undecoded cmd 0x%x>", cmd));
}

char *
scsi_cmd_name(u_char cmd, struct scsi_key_strings *cmdlist, char *tmpstr)
{
	int i = 0;

	while (cmdlist[i].key !=  -1) {
		if (cmd == cmdlist[i].key) {
			return ((char *)cmdlist[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<undecoded cmd 0x%x>", cmd));
}

static
struct scsi_asq_key_strings {
	u_short	asc;
	u_short	ascq;
	char 	*message;
	    } extended_sense_list[] = {
		0x00, 0, "no additional sense info",
		0x01, 0, "no index/sector signal",
		0x02, 0, "no seek complete",
		0x03, 0, "peripheral device write fault",
		0x04, 0, "LUN not ready",
		0x05, 0, "LUN does not respond to selection",
		0x06, 0, "reference position found",
		0x07, 0, "multiple peripheral devices selected",
		0x08, 0, "LUN communication failure",
		0x09, 0, "track following error",
		0x0a, 0, "error log overflow",
		0x0c, 0, "write error",
		0x10, 0, "ID CRC or ECC error",
		0x11, 0, "unrecovered read error",
		0x12, 0, "address mark not found for ID field",
		0x13, 0, "address mark not found for data field",
		0x14, 0, "recorded entity not found",
		0x15, 0, "random positioning error",
		0x16, 0, "data sync mark error",
		0x17, 0, "recovered data with no error correction",
		0x18, 0, "recovered data with error correction",
		0x19, 0, "defect list error",
		0x1a, 0, "parameter list length error",
		0x1b, 0, "synchronous data xfer error",
		0x1c, 0, "defect list not found",
		0x1d, 0, "miscompare during verify",
		0x1e, 0, "recovered ID with ECC",
		0x1f, 0, "partial defect list transfer",
		0x20, 0, "invalid command operation code",
		0x21, 0, "logical block address out of range",
		0x22, 0, "illegal function",
		0x24, 0, "invalid field in cdb",
		0x25, 0, "LUN not supported",
		0x26, 0, "invalid field in param list",
		0x27, 0, "write protected",
		0x28, 0, "medium may have changed",
		0x29, 0, "power on, reset, or bus reset occurred",
		0x2a, 0, "parameters changed",
		0x2b, 0, "copy cannot execute since host cannot disconnect",
		0x2c, 0, "command sequence error",
		0x2d, 0, "overwrite error on update in place",
		0x2f, 0, "commands cleared by another initiator",
		0x30, 0, "incompatible medium installed",
		0x31, 0, "medium format corrupted",
		0x32, 0, "no defect spare location available",
		0x33, 0, "tape length error",
		0x36, 0, "ribbon, ink, or toner failure",
		0x37, 0, "rounded parameter",
		0x39, 0, "saving parameters not supported",
		0x3a, 0, "medium not present",
		0x3b, 0, "sequential positioning error",
		0x3d, 0, "invalid bits in indentify message",
		0x3e, 0, "LUN has not self-configured yet",
		0x3f, 0, "target operating conditions have changed",
		0x40, 0, "ram failure",
		0x41, 0, "data path failure",
		0x42, 0, "power-on or self-test failure",
		0x43, 0, "message error",
		0x44, 0, "internal target failure",
		0x45, 0, "select or reselect failure",
		0x46, 0, "unsuccessful soft reset",
		0x47, 0, "scsi parity error",
		0x48, 0, "initiator detected error message received",
		0x49, 0, "invalid message error",
		0x4a, 0, "command phase error",
		0x4b, 0, "data phase error",
		0x4c, 0, "logical unit failed self-configuration",
		0x4d, 0, "tagged overlapped commands (ASCQ = queue tag)",
		0x4e, 0, "overlapped commands attempted",
		0x50, 0, "write append error",
		0x51, 0, "erase failure",
		0x52, 0, "cartridge fault",
		0x53, 0, "media load or eject failed",
		0x54, 0, "scsi to host system interface failure",
		0x55, 0, "system resource failure",
		0x57, 0, "unable to recover TOC",
		0x58, 0, "generation does not exist",
		0x59, 0, "updated block read",
		0x5a, 0, "operator request or state change input",
		0x5b, 0, "log exception",
		0x5c, 0, "RPL status change",
		0x5d, 0, "drive operation marginal, service immediately"
			" (failure prediction threshold exceeded)",
		0x5d, 0xff, "failure prediction threshold exceeded (false)",
		0x5e, 0, "low power condition active",
		0x60, 0, "lamp failure",
		0x61, 0, "video aquisition error",
		0x62, 0, "scan head positioning error",
		0x63, 0, "end of user area encountered on this track",
		0x64, 0, "illegal mode for this track",
		0x65, 0, "voltage fault",
		0x66, 0, "automatic document feeder cover up",
		0x67, 0, "configuration failure",
		0x68, 0, "logical unit not configured",
		0x69, 0, "data loss on logical unit",
		0x6a, 0, "informational, refer to log",
		0x6b, 0, "state change has occured",
		0x6c, 0, "rebuild failure occured",
		0x6d, 0, "recalculate failure occured",
		0x6e, 0, "command to logical unit failed",
		0x70, 0xffff,
			"decompression exception short algorithm id of ASCQ",
		0x71, 0, "decompression exception long algorithm id",

		0xffff, NULL,
};

char *
scsi_esname(u_int key, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if (key == extended_sense_list[i].asc) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", key));
}

char *
scsi_asc_name(u_int asc, u_int ascq, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if ((asc == extended_sense_list[i].asc) &&
		    ((ascq == extended_sense_list[i].ascq) ||
		    (extended_sense_list[i].ascq == 0xffff))) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", asc));
}

char *
scsi_sname(u_char sense_key)
{
	if (sense_key >= (u_char)(NUM_SENSE_KEYS+NUM_IMPL_SENSE_KEYS)) {
		return ("<unknown sense key>");
	} else {
		return (sense_keys[sense_key]);
	}
}



/*
 * Print a piece of inquiry data- cleaned up for non-printable characters.
 */

static void
inq_fill(char *p, int l, char *s)
{
	register unsigned i = 0;
	char c;

	if (!p)
		return;

	while (i++ < l) {
		/* clean string of non-printing chars */
		if ((c = *p++) < ' ' || c >= 0177) {
			c = ' ';
		}
		*s++ = c;
	}
	*s++ = 0;
}

/*
 * The first part/column of the error message will be at least this length.
 * This number has been calculated so that each line fits in 80 chars.
 */
#define	SCSI_ERRMSG_COLUMN_LEN	42

void
scsi_errmsg(struct scsi_device *devp, struct scsi_pkt *pkt, char *label,
    int severity, int blkno, int err_blkno,
    struct scsi_key_strings *cmdlist, struct scsi_extended_sense *sensep)
{
	u_char com;
	char buf[256], tmpbuf[64], pad[SCSI_ERRMSG_COLUMN_LEN];
	dev_info_t *dev = devp->sd_dev;
	static char *error_classes[] = {
		"All", "Unknown", "Informational",
		"Recovered", "Retryable", "Fatal"
	};
	int i, buflen;

	/*
	 * We need to put our space padding code because kernel version
	 * of sprintf(9F) doesn't support %-<number>s type of left alignment.
	 */
	for (i = 0; i < SCSI_ERRMSG_COLUMN_LEN; i++) {
		pad[i] = ' ';
	}

	bzero((caddr_t)buf, 256);
	com = ((union scsi_cdb *)pkt->pkt_cdbp)->scc_cmd;
	(void) sprintf(buf, "Error for Command: %s",
	    scsi_cmd_name(com, cmdlist, tmpbuf));
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[buflen], "%s Error Level: %s",
		    pad, error_classes[severity]);
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
	} else {
		(void) sprintf(&buf[buflen], " Error Level: %s",
		    error_classes[severity]);
	}
	scsi_log(dev, label, CE_WARN, buf);

	if (blkno != -1 || err_blkno != -1 &&
	    ((com & 0xf) == SCMD_READ) || ((com & 0xf) == SCMD_WRITE)) {
		bzero((caddr_t)buf, 256);
		(void) sprintf(buf, "Requested Block: %d", blkno);
		buflen = strlen(buf);
		if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
			(void) sprintf(&buf[buflen], "%s Error Block: %d\n",
			    pad, err_blkno);
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
		} else {
			(void) sprintf(&buf[buflen], " Error Block: %d\n",
			    err_blkno);
		}
		scsi_log(dev, label, CE_CONT, buf);
	}

	bzero((caddr_t)buf, 256);
	strcpy(buf, "Vendor: ");
	inq_fill(devp->sd_inq->inq_vid, 8, &buf[strlen(buf)]);
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[strlen(buf)], "%s Serial Number: ", pad);
	} else {
		(void) sprintf(&buf[strlen(buf)], " Serial Number: ");
	}
	inq_fill(devp->sd_inq->inq_serial, 12, &buf[strlen(buf)]);
	scsi_log(dev, label, CE_CONT, "%s\n", buf);

	if (sensep) {
		bzero((caddr_t)buf, 256);
		(void) sprintf(buf, "Sense Key: %s\n",
		    sense_keys[sensep->es_key]);
		scsi_log(dev, label, CE_CONT, buf);

		bzero((caddr_t)buf, 256);
		(void) sprintf(&buf[strlen(buf)],
		    "ASC: 0x%x (%s), ASCQ: 0x%x, FRU: 0x%x",
		    sensep->es_add_code,
		    scsi_asc_name(sensep->es_add_code,
			sensep->es_qual_code, tmpbuf),
		    sensep->es_qual_code, sensep->es_fru_code);
		scsi_log(dev, label, CE_CONT, "%s\n", buf);
	}
}

/*ARGSUSED*/
void
scsi_log(dev_info_t *dev, char *label, u_int level,
    const char *fmt, ...)
{
	auto char name[256];
	va_list ap;
	int log_only = 0;
	int boot_only = 0;
	int console_only = 0;

	mutex_enter(&scsi_log_mutex);

	if (dev) {
		if (level == CE_PANIC || level == CE_WARN) {
			sprintf(name, "%s (%s%d):\n",
				ddi_pathname(dev, scsi_log_buffer), label,
				ddi_get_instance(dev));
		} else if (level == CE_NOTE ||
		    level >= (u_int) SCSI_DEBUG) {
			sprintf(name,
			    "%s%d:", label, ddi_get_instance(dev));
		} else if (level == CE_CONT) {
			name[0] = '\0';
		}
	} else {
		sprintf(name, "%s:", label);
	}

	va_start(ap, fmt);
	(void) vsprintf(scsi_log_buffer, fmt, ap);
	va_end(ap);

	switch (scsi_log_buffer[0]) {
	case '!':
		log_only = 1;
		break;
	case '?':
		boot_only = 1;
		break;
	case '^':
		console_only = 1;
		break;
	}

	switch (level) {
		case CE_NOTE:
			level = CE_CONT;
			/* FALLTHROUGH */
		case CE_CONT:
		case CE_WARN:
		case CE_PANIC:
			if (boot_only) {
				cmn_err(level, "?%s\t%s", name,
					&scsi_log_buffer[1]);
			} else if (console_only) {
				cmn_err(level, "^%s\t%s", name,
					&scsi_log_buffer[1]);
			} else if (log_only) {
				cmn_err(level, "!%s\t%s", name,
					&scsi_log_buffer[1]);
			} else {
				cmn_err(level, "%s\t%s", name,
					scsi_log_buffer);
			}
			break;
		case (u_int) SCSI_DEBUG:
		default:
			cmn_err(CE_CONT, "^DEBUG: %s\t%s", name,
					scsi_log_buffer);
			break;
	}

	mutex_exit(&scsi_log_mutex);
}

int
scsi_get_device_type_scsi_options(dev_info_t *dip,
    struct scsi_device *devp, int default_scsi_options)
{

	caddr_t config_list	= NULL;
	int options		= default_scsi_options;
	struct scsi_inquiry  *inq = devp->sd_inq;
	caddr_t vidptr, datanameptr;
	int	vidlen, dupletlen;
	int config_list_len, len;

	/*
	 * look up the device-type-scsi-options-list and walk thru
	 * the list
	 * compare the vendor ids of the earlier inquiry command and
	 * with those vids in the list
	 * if there is a match, lookup the scsi-options value
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "device-type-scsi-options-list",
	    (caddr_t)&config_list, &config_list_len) == DDI_PROP_SUCCESS) {

		/*
		 * Compare vids in each duplet - if it matches, get value for
		 * dataname and then lookup scsi_options
		 * dupletlen is calculated later.
		 */
		for (len = config_list_len, vidptr = config_list; len > 0;
		    vidptr += dupletlen, len -= dupletlen) {

			vidlen = strlen(vidptr);
			datanameptr = vidptr + vidlen + 1;

			if ((vidlen != 0) &&
			    bcmp(inq->inq_vid, vidptr, vidlen) == 0) {
				/*
				 * get the data list
				 */
				options = ddi_prop_get_int(DDI_DEV_T_ANY,
				    dip, 0,
				    datanameptr, default_scsi_options);
				break;
			}
			dupletlen = vidlen + strlen(datanameptr) + 2;
		}
	}

	return (options);
}

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_WATCH_H
#define	_SYS_SCSI_SCSI_WATCH_H

#pragma ident	"@(#)scsi_watch.h	1.6	96/06/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct scsi_watch_result {
	struct scsi_status		*statusp;
	struct scsi_extended_sense	*sensep;
	u_char				actual_sense_length;
	struct scsi_pkt			*pkt;
};

/*
 * 120 seconds is a *very* reasonable amount of time for most slow devices
 */
#define	SCSI_WATCH_IO_TIME	120

void scsi_watch_init();
void scsi_watch_fini();

#ifdef	__STDC__

opaque_t scsi_watch_request_submit(struct scsi_device *devp,
    int interval, int sense_length, int (*callback)(), caddr_t cb_arg);
void scsi_watch_request_terminate(opaque_t token);
void scsi_watch_resume(opaque_t token);
void scsi_watch_suspend(opaque_t token);

#else	/* __STDC__ */

opaque_t scsi_watch_request_submit();
void scsi_watch_request_terminate();
void scsi_watch_resume();
void scsi_watch_suspend();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_WATCH_H */

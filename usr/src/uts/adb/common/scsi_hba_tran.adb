#include <sys/scsi/scsi.h>

scsi_hba_tran
./"devinfop"16t"hba_privp"16t"tgt_privp"n{tran_hba_dip,X}{tran_hba_private,X}{tran_tgt_private,X}
+/"scsi_devp"16t"tran_tgt_init"16t"tran_tgt_probe"n{tran_sd,X}{tran_tgt_init,X}{tran_tgt_probe,X}
+/"tran_tgt_free"16t"tran_start"16t"tran_reset"n{tran_tgt_free,X}{tran_start,X}{tran_reset,X}
+/"tran_abort"16t"tran_getcap"16t"tran_setcap"n{tran_abort,X}{tran_getcap,X}{tran_setcap,X}
+/"tran_init_pkt"16t"tran_dest_pkt"16t"tran_dmafree"n{tran_init_pkt,X}{tran_destroy_pkt,X}{tran_dmafree,X}
+/"tran_sync_pkt"16t"tran_rst_notify"16t"tran_get_bus_addr"n{tran_sync_pkt,X}{tran_reset_notify,X}{tran_get_bus_addr,X}
+/"tran_get_name"16t"tran_clear_aca"16t"tran_clear_task_set"n{tran_get_name,X}{tran_clear_aca,X}{tran_clear_task_set,X}
+/"tran_terminate_task"16t"tran_hba_flags"n{tran_terminate_task,X}16t{tran_hba_flags,X}
+/"tran_min_xfer"16t"min burst size"16t"max burst size"n{tran_min_xfer,X}{tran_min_burst_size,V}16t{tran_max_burst_size,V}{END}

#pragma ident  "@(#)isp.adb 1.14     96/10/18 SMI"
#include <sys/types.h>
#include <sys/scsi/scsi.h>
#include <sys/scsi/adapters/ispmail.h>
#include <sys/scsi/adapters/ispvar.h>

isp
./"tran"16t"dev"16t"bus"8t"clock"24t"iblock"n{isp_tran,X}{isp_dip,X}{isp_bus,B}{isp_clock_frequency,X}{isp_iblock,X}
+/"next"16t"major"8t"minor"8t"prod"8t"tgt-opt"8t"options"n{isp_next,X}{isp_major_rev,x}{isp_minor_rev,x}{isp_cust_prod,x}{isp_target_scsi_options_defined,x}{isp_scsi_options,X}
+/"target options"n{isp_target_scsi_options,16X}
+/"tag limit"8t"reset delay"16t"hostid"8t"suspended"n{isp_scsi_tag_age_limit,X}{isp_scsi_reset_delay,X}{isp_initiator_id,B}{isp_suspended,B}
+/"cap"n{isp_cap,16x}
+/"synch"n{isp_synch,16x}
+/"biu reg"16t"mbox reg"16t"sxp reg"n{isp_biu_reg,X}{isp_mbox_reg,X}{isp_sxp_reg,X}
+/"risc reg"16t"reg no"n{isp_risc_reg,X}{isp_reg_number,X}
+/"mbox"n{isp_mbox,52B}
+/"shutdown"8t"prop_upd"8t"cmd area"n{isp_shutdown,B}8t{isp_prop_update,x}8t{isp_cmdarea,X}
+/"dma cookie"n{isp_dmacookie,4X}
+/"dma hndl"16t"dma acc_hndl"16t"req dvma"16t"resp dvma"n{isp_dmahandle,X}{isp_dma_acc_handle,X}{isp_request_dvma,X}{isp_response_dvma,X}
+/"pci acc_hndl"16t"biu acc_hndl"16t"mbox acc_hndl"16t"sxp acc_hndl"n{isp_pci_config_acc_handle,X}{isp_biu_acc_handle,X}{isp_mbox_acc_handle,X}{isp_sxp_acc_handle,X}
+/"risc acc_hndl"16t"que space"n{isp_risc_acc_handle,X}{isp_queue_space,X}
+/"request mutex:"
.$<<mutex{OFFSETOK}
+/"response mutex:"
.$<<mutex{OFFSETOK}
+/n"req in"16t"req out"16t"res in"16t"res out"n{isp_request_in,x}8t{isp_request_out,x}8t{isp_response_in,x}8t{isp_response_out,x}
+/"request ptr"16t"request base"16t"response ptr"16t"response base"n{isp_request_ptr,X}{isp_request_base,X}{isp_response_ptr,X}{isp_response_base,X}
+/"waitq mutex:"
.$<<mutex{OFFSETOK}
+/"waitf"16t"waitb"16t"waitq timeout"n{isp_waitf,X}{isp_waitb,X}{isp_waitq_timeout,X}
+/"burst sz"16t"conf1 fifo"8t"free"8t"alive"n{isp_burst_size,X}{isp_conf1_fifo,x}8t{isp_free_slot,x}{isp_alive,x}
+/"reset list"16t"kmem cache"n{isp_reset_notify_listf,X}{isp_kmem_cache,X}{END}

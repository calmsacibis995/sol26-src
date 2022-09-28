#include <sys/scsi/scsi.h>
#include <sys/scsi/impl/pkt_wrapper.h>

scsi_cmd
#if defined(i386) || defined(__ppc)
.$<<scsi_pkt{OFFSETOK}
+/"flags"16t"hba flags"16t"callbk cmdp"n{cmd_flags,X}{cmd_cflags,X}{cmd_cblinkp,X}
+/"dma handle"16t"dma windowp"16t"dma segp"n{cmd_dmahandle,X}{cmd_dmawin,X}{cmd_dmaseg,X}
+/"cmd privp"16t"cdb len"16t"scb len"n{cmd_private,X}{cmd_cdblen,B}{cmd_scblen,B}
+/"tgt privlen"16t"rqs len"16t"tot xfer"n{cmd_privlen,B}{cmd_rqslen,B}{cmd_totxfer,X}
+/"cmd_pkt_priv"n{cmd_pkt_private,12X}
#endif

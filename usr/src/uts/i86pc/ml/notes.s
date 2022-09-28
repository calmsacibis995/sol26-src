#include <sys/elf_notes.h>

#pragma ident  "@(#)notes.s  1.3     96/05/29 SMI"

#if defined(lint)
#include <sys/types.h>
#else

/
/ Sun defined names for modules to be packed x86 version
/
#define	PACKSPECFS	"specfs\000"
#define PACKTS		"TS\000"
#define PACKTS_DPTBL	"TS_DPTBL\000"
#define PACKKD		"kd\000"
#define PACKASY		"asy\000"
#define PACKRPCMOD	"rpcmod\000"
#define PACKIP		"ip\000"
#define PACKROOTNEX	"rootnex\000"
#define PACKOPTIONS	"options\000"
#define PACKSAD		"sad\000"
#define PACKPSEUDO	"pseudo\000"
#define PACKTCP		"tcp\000"
#define PACKUDP		"udp\000"
#define PACKICMP	"icmp\000"
#define PACKARP		"arp\000"
#define PACKTIMOD	"timod\000"
#define PACKCHANMUX	"chanmux\000"
#define PACKELFEXEC	"elfexec\000"
#define PACKMM		"mm\000"
#define PACKFIFOFS	"fifofs\000"
#define PACKCHAR	"char\000"
#define PACKANSI	"ansi\000"
#define PACKEMAP	"emap\000"
#define PACKLDTERM	"ldterm\000"
#define PACKTTCOMPAT	"ttcompat\000"
#define PACKCN		"cn\000"
#define PACKINTPEXEC	"intpexec\000"
#define PACKSOCKMOD	"sockmod\000"
#define PACKPIPE	"pipe\000"
#define PACKTMPFS	"tmpfs\000"
#define PACKDOORFS	"doorfs\000"
#define PACKLOG		"log\000"
#define PACKKSTAT	"kstat\000"
#define PACKTL		"tl\000"
#define PACKIPC		"ipc\000"
#define PACKTLIMOD	"tlimod\000"
#define PACKAUTOFS	"autofs\000"
#define PACKNAMEFS	"namefs\000"
#define PACKCONNLD	"connld\000"
#define PACKSY		"sy\000"
#define PACKPTM		"ptm\000"
#define PACKPTS		"pts\000"
#define PACKPTEM	"ptem\000"
#define PACKREDIRMOD	"redirmod\000"
#define PACKBUFMOD	"bufmod\000"
#define PACKPCKT	"pckt\000"
#define PACKIA		"IA\000"
#define PACKFDFS	"fdfs\000"

/
/ Define the size for the packing pool
/
	.section        .note
	.align          4
	.4byte           .name1_end - .name1_begin 
	.4byte           .desc1_end - .desc1_begin
	.4byte		ELF_NOTE_MODULE_SIZE
.name1_begin:
	.string         ELF_NOTE_PACKSIZE
.name1_end:
	.align          4
/
/ The size is 161 pages, this is hand calculated.
/
.desc1_begin:
	.4byte		0xa2
.desc1_end:
	.align		4
/
/ Tag a group of modules as necessary for packing
/
	.section        .note
	.align          4
	.4byte          .name2_end - .name2_begin
	.4byte          .desc2_end - .desc2_begin
	.4byte		ELF_NOTE_MODULE_PACKING
.name2_begin:
	.string         ELF_NOTE_PACKHINT
.name2_end:
	.align          4
/
/ These are the modules defined above
/
.desc2_begin:
	.string PACKSPECFS
	.string PACKTS
	.string PACKTS_DPTBL
	.string PACKKD
	.string PACKASY
	.string PACKRPCMOD
	.string PACKIP
	.string PACKROOTNEX
	.string PACKOPTIONS
	.string PACKSAD
	.string PACKPSEUDO
	.string PACKTCP
	.string PACKUDP
	.string PACKICMP
	.string PACKARP
	.string PACKTIMOD
	.string PACKCHANMUX
	.string PACKELFEXEC
	.string PACKMM
	.string PACKFIFOFS
	.string PACKCHAR
	.string PACKANSI
	.string PACKEMAP
	.string PACKLDTERM
	.string PACKTTCOMPAT
	.string PACKCN
	.string PACKINTPEXEC
	.string PACKSOCKMOD
	.string PACKPIPE
	.string PACKTMPFS
	.string PACKDOORFS
	.string PACKLOG
	.string PACKKSTAT
	.string PACKTL
	.string PACKIPC
	.string PACKTLIMOD
	.string PACKAUTOFS
	.string PACKNAMEFS
	.string PACKCONNLD
	.string PACKSY
	.string PACKPTM
	.string PACKPTS
	.string PACKPTEM
	.string PACKREDIRMOD
	.string PACKBUFMOD
	.string PACKPCKT
	.string PACKIA
	.string PACKFDFS	
.desc2_end:
	.align          4
 
#endif


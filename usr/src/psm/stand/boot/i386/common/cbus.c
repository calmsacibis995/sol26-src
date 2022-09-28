/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)cbus.c	1.9	96/05/24 SMI"

/*
 * max number of C-bus II elements & processors.
 * (processors == elements - broadcast element).
 */
#define	MAX_CBUS_ELEMENTS	16

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootdef.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/salib.h>
#include "cbus.h"

#define	PROT_READ	1
#define	PROT_WRITE	2
#define	PROT_RW			(PROT_READ|PROT_WRITE)

#ifndef MIN
#define	MIN(a, b)		(((a) > (b)) ? (b) : (a))
#endif

#define	psm_unmap_phys(x, y)
#define	psm_map_phys(a, l, m)	 (a)

#define	SIXTEEN_MB		(16 * 1024 * 1024)

EXT_CFG_OVERRIDE_T		CbusGlobal;

RRD_CONFIGURATION_T		CbusJumpers;

EXT_ID_INFO_T			CbusExtIDTable[MAX_CBUS_ELEMENTS];
ULONG				CbusValidIDs;

extern void			CbusMemoryFree(ULONG, ULONG);

static ULONG RRDextsignature[] = { (ULONG)0xfeedbeef, 0 };

static ULONG RRDsignature[] = { (ULONG)0xdeadbeef, 0 };

static UCHAR CorollaryOwns[] =
	"Copyright(C) Corollary, Inc. 1991. All Rights Reserved";

void
CbusAddBelow16MB();

void
Cbus1HandleJumpers();

PUCHAR
CbusFindString(
IN PUCHAR	Str,
IN PUCHAR	StartAddr,
IN LONG		Len
);

void
CbusEstablishMaps(
IN PEXT_ID_INFO Table,
IN ULONG Count
);

void
CbusMemToSolaris(
struct bootmem *mrp
);

ULONG
CbusReadExtIDs(
IN PEXT_ID_INFO From,
IN PEXT_ID_INFO To
);

void
CbusMemoryFree(
IN ULONG Address,
IN ULONG Size
);

ULONG
Cbus1987Table(
IN PEXT_ID_INFO Table
)
{
	ULONG			i, start;
	ULONG			IdpCount = 0;
	PEXT_ID_INFO		Idp = Table;

	/*
	 * describe all existing memory under 16Mb.
	 * and give all non-jumpered memory to the table.
	 */
	for (i = 1; i < ATMB; i++) {
		if (CbusJumpers.mem[i] && CbusJumpers.jmp[i] == 0) {
			/* C-bus memory that isn't disabled */
			start = i;
			while (++i < ATMB && CbusJumpers.mem[i])
				if (CbusJumpers.jmp[i])
					break;

			Idp->pm = 0;
			Idp->io_function = IOF_MEMORY;
			Idp->pel_start = MB(start);
			Idp->pel_size = MB(i - start);
			Idp++;
			IdpCount++;
		}
	}

	/*
	 * describe all existing memory over 16Mb
	 */
	for (i = ATMB; i < OLDRRD_MAXMB; i++) {
		if (CbusJumpers.mem[i]) {
			/*
			 * memory exists...
			 */
			Idp->pm = 0;
			Idp->io_function = IOF_MEMORY;

			start = i;
			Idp->pel_start = MB(start);
			for (i++; i < OLDRRD_MAXMB && CbusJumpers.mem[i]; i++)
				;

			Idp->pel_size = MB(i - start);
			Idp++;
			IdpCount++;
		}
	}

	return (IdpCount);
}

/*
 * Routine Description:
 *
 * 	- this routine reads in the global RRD information which pertains
 * 	to the entire system.  if the machine is not really a
 * 	Corollary/Corollary-clone machine, then this routine returns FALSE.
 *
 * 	For robustness, we check for the following before concluding that
 * 	we are indeed a Corollary C-bus I or C-bus II licensee.
 *
 * 		THESE 3 searches are supported by all Corollary
 * 		machines from Cbus1 XM forward (including Cbus2):
 *
 * 	a) Corollary string in the BIOS ROM 64K area		(0x000F0000)
 * 	b) Corollary string in the RRD RAM/ROM 64K area		(0xFFFE0000)
 * 	c) 2 Corollary extended configuration tables
 * 				in the RRD RAM/ROM 64K area	(0xFFFE0000)
 *
 * 		THESE searches are supported by all Corollary
 * 		Cbus1 machines prior to XM:
 *
 * 	b) Corollary string in the RRD RAM/ROM 64K area		(0xFFFE0000)
 * 	c) 2 Corollary extended configuration tables
 * 				in the RRD RAM/ROM 64K area	(0xFFFE0000)
 *
 * 	If any of the above checks fail, it is assumed that this machine
 * 	is a non-Corollary machine.
 *
 * 	If the above checks succeed, then we proceed to fill in various
 * 	memory configuration structures for use by the Solaris boot loader.
 * 	CbusExtraMemory[] will contain all the memory ranges in the machine.
 */

BOOL
CbusInitializePlatform(
struct bootmem *mrp
)
{
	PEXT_CFG_HEADER		p;
	PVOID			s;
	ULONG			OverrideLength = 0, EntryLength;
	PUCHAR			Bios;

	/*
	 * Map in the 64K of BIOS ROM @ 0xF0000 and scan it for our signature...
	 */
	Bios = (PUCHAR)psm_map_phys((PVOID)0xF0000, 0x10000, PROT_RW);

	if (!Bios)
		return (FALSE);

	if (!CbusFindString((PUCHAR)"Corollary", Bios, (LONG)0x10000)) {
		/*
		 * not really a Corollary machine at all
		 */
		psm_unmap_phys((PVOID)Bios, 0x10000);
		return (FALSE);
	}
	psm_unmap_phys(Bios, 0x10000);

	/*
	 * Map in the 64K of RRD @ 0xFFFE0000 and scan it for our signature.
	 */

	Bios = (PUCHAR)psm_map_phys((PVOID)0xFFFE0000, 0x10000, PROT_RW);

	if (!Bios)
		return (FALSE);

	if (!CbusFindString((PUCHAR)"Corollary", Bios, (LONG)0x10000)) {
		/*
		 * not really a Corollary machine at all
		 */
		psm_unmap_phys(Bios, 0x10000);
		return (FALSE);
	}
	if (CbusFindString((PUCHAR)"Some Random String", Bios, (LONG)0x10000)) {
		/*
		 * not really a Corollary machine at all
		 */
		psm_unmap_phys(Bios, 0x10000);
		return (FALSE);
	}

	psm_unmap_phys(Bios, 0x10000);
	/*
	 * Map in the 32K of RRD RAM information @ 0xE0000,
	 */
	Bios = (PUCHAR)psm_map_phys((PVOID)RRD_RAM, 0x8000, PROT_RW);

	if (!Bios)
		return (FALSE);

	if (!CbusFindString(CorollaryOwns, Bios, (LONG)0x8000)) {
		/*
		 * an obsolete Corollary machine (of the assymetric flavor)
		 */
		psm_unmap_phys((PVOID)Bios, 0x8000);
		return (FALSE);
	}

	/*
	 * At this point, we are assured that it is indeed a
	 * Corollary architecture machine.  Search for our
	 * extended configuration tables, note we still search for
	 * the existence of our earliest 'configuration' structure,
	 * ie: the 0xdeadbeef version.  This is not to find out where
	 * the memory is, but to find out which Cbus1 megabytes have been
	 * 'jumpered' so that I/O cards can use the RAM address(es) for
	 * their own dual-ported RAM buffers.  This early configuration
	 * structure will not be present in Cbus2.
	 */

	s = (PVOID)CbusFindString((PUCHAR)RRDsignature, Bios, (LONG)0x8000);

	if (s) {
		bcopy((PVOID)s, (PVOID)&CbusJumpers, JUMPER_SIZE);
	}
#ifdef DEBUG
	else {
	/*
	 * RRD configuration is not expected on Cbus2, but is for Cbus1
	 */
		printf("PSM: No RRD ROM configuration table\n");
	}
#endif

	/*
	 *
	 * Now go for the extended configuration structure which will
	 * tell us about memory, processors and I/O devices.
	 */

	p = (PEXT_CFG_HEADER)CbusFindString((PUCHAR)RRDextsignature,
					Bios, (LONG)0x8000);

	/*
	 * no extended configuration table because no RRDs employed
	 * it before Cbus I XM.  hence, it must be a 64MB(max)
	 * machine.  set up reasonable defaults and proceed.
	 */
	if (!p) {
#ifdef DEBUG
		printf("No extended configuration table\n");
		printf("Defaulting to Corollary 64MB model\n");
#endif

		/*
		 * only turn this on if Solaris can support
		 * discontiguous memory ranges
		 */
		CbusGlobal.useholes = 1;

		CbusGlobal.baseram = MB(64);
		CbusGlobal.memory_ceiling = MB(64);
		goto finishup;
	}

	/*
	 * Read in the 'extended ID information' table which,
	 * among other things, will give us the processor
	 * configuration.
	 *
	 * Multiple structures are strung together with a "checkword",
	 * "length", and "data" structure.  The first null "checkword"
	 * entry marks the end of the extended configuration
	 * structure.
	 *
	 * We are only actively reading two types of structures, and
	 * they MUST be in the following order, although not necessarily
	 * consecutive:
	 *
	 * - ext_id_info
	 *
	 * - ext_cfg_override
	 *
	 * We ignore all other extended configuration entries built
	 * by RRD - they are mainly for early UNIX kernels.
	 */

	do {
		EntryLength = p->ext_cfg_length;
		switch (p->ext_cfg_checkword) {
		case EXT_ID_INFO:
			CbusValidIDs = CbusReadExtIDs((PEXT_ID_INFO)(p+1),
					(PEXT_ID_INFO)CbusExtIDTable);

			break;

		case EXT_CFG_OVERRIDE:
			/*
			 *
			 * We just copy the size of the structures
			 * we know about.  If an rrd tries to pass us
			 * more than we know about, we ignore the
			 * overflow.  Underflow is interpreted as
			 * "this must be a pre-XM machine".
			 */
			if (EntryLength < sizeof (EXT_CFG_OVERRIDE_T))
				break;

			OverrideLength = MIN(sizeof (EXT_CFG_OVERRIDE_T),
								EntryLength);

			bcopy((PVOID)(p + 1), (PVOID)&CbusGlobal,
								OverrideLength);
			break;
		case EXT_CFG_END:
			/*
			 * if no EXT_CFG_OVERRIDE, provide
			 * reasonable defaults and proceed.
			 */

			if (OverrideLength == 0) {
				CbusGlobal.useholes = 1;
				CbusGlobal.memory_ceiling = MB(64);

				printf("No extended configuration table\n");
				printf("Defaulting to 64MB machine behavior\n");
			}

			/*
			 * 3 possible places to get the processor configuration
			 * information from.  this information lets us tell
			 * the user which types of processors are being
			 * disabled.
			 *
			 * the extended_ID_info table is guaranteed to be
			 * the most current.  this is really the table that
			 * ALL board information should be in, but it is only
			 * the case for XM machines and onwards (circa
			 * summer 1992).
			 *
			 * the next most current is the extended
			 * processor_config, found on most 1991 machines.
			 *
			 * lastly, build the oldcputable, which
			 * will support machines from 1987 onward.
			 */

			if (CbusValidIDs == 0) {
				/*
				 * no extended ID table - build a memory table
				 * from the original configuration table.
				 */
				CbusValidIDs = Cbus1987Table(CbusExtIDTable);
			}

			CbusEstablishMaps(CbusExtIDTable, CbusValidIDs);

		finishup:
			CbusAddBelow16MB();
			Cbus1HandleJumpers();

			psm_unmap_phys((PVOID)Bios, 0x8000);
			CbusMemToSolaris(mrp);
				return (TRUE);
		default:
			/*
			 * Skip unused or unrecognized configuration entries
			 */
			break;
		}
		/*
		 * Get past the header, add in the length and then
		 * we're at the next entry.
		 */
		p = (PEXT_CFG_HEADER) ((PUCHAR)(p + 1) + EntryLength);
	/*CONSTCOND*/
	} while (1);
	/*NOTREACHED*/
}

void
CbusAddBelow16MB()
{
	unsigned i, j = 0;

	/*
	 * add the 0->640K memory range first
	 */
	CbusMemoryFree(0, 640 * 1024);

	/*
	 * now add the memory between 1MB and 16MB
	 */
	for (i = 1; i < ATMB; i++) {
		if (CbusJumpers.mem[i]) {
			if (CbusJumpers.jmp[i])
				break;		/* jumper means no mem */
						/* stop at first "no mem" */
			j++;
		} else
			break;
	}

	if (j)
		CbusMemoryFree(MB(1), MB(j));
}

/*

Routine Description:

	"Recover" any low memory which has been repointed at the EISA bus
	via EISA config.  This typically happens when ISA/EISA cards with
	dual-ported memory are in the machine, and memory accesses need
	to be pointed at the card, as opposed to C-bus general purpose memory.
	The Corollary architecture provides a way to still gain use of the
	general purpose memory, as well as be able to use the I/O dual-port
	memory, by double mapping the C-bus memory at a high memory address.

	note that memory accesses in the 640K-1MB hole are by default, pointed
	at the EISA bus, and don't need to be repointed via EISA config.

	note this is where jumper decoding and memory_holes set up
	happens, but the actual memory is not released here.  Actually,
	due to the Solaris memory manager, it is not ever released!

*/

void
Cbus1HandleJumpers()
{
	ULONG				i, j;
	ULONG				DoublyMapped;
	ULONG				Address;
	extern RRD_CONFIGURATION_T	CbusJumpers;

	if (CbusGlobal.useholes == 0)
		return;

	/*
	 *
	 * if the base of RAM is _zero_ (ie: XM or later), then we
	 * will recover the holes up above the "top of RAM", so any
	 * I/O device dual-port RAM at the low address will continue to work.
	 *
	 * if the base of RAM is NON-ZERO, then we are on an older
	 * Corollary system which is not supported by this PSM - the
	 * standard Solaris uniprocessor PSM should be used instead.
	 */

	if (CbusGlobal.baseram != 0)
		return;

	DoublyMapped = CbusGlobal.memory_ceiling;

	/*
	 * reclaim the 640K->1MB hole first
	 */
	CbusMemoryFree(DoublyMapped + 640 * 1024, 384 * 1024);

	/*
	 * see if this memory span has been dipswitch
	 * disabled.  if this memory exists (in C-bus
	 * space and it has been jumpered, add it to
	 * the list so it will be freed later.
	 */
	for (i = 1; i < ATMB; i++) {
		if (CbusJumpers.jmp[i] && CbusJumpers.mem[i]) {
			Address = MB(i) + DoublyMapped;
			j = 1;
			for (i++; i < ATMB && CbusJumpers.jmp[i] &&
					CbusJumpers.mem[i]; i++)
				j++;
				CbusMemoryFree(Address, MB(j));
			}
		}
}

/*

Routine Description:

	Searches a given virtual address for the specified string
	up to the specified length.

Arguments:

	Str - Supplies a pointer to the string

	StartAddr - Supplies a pointer to memory to be searched

	Len - Maximum length for the search

Return Value:

	Pointer to the string if found, 0 otherwise.

*/

PUCHAR
CbusFindString(
IN PUCHAR	Str,
IN PUCHAR	StartAddr,
IN LONG		Len
)
{
	LONG	Index, n;

	for (n = 0; Str[n]; ++n)
		;

	if (--n < 0)
		return (StartAddr);

	for (Len -= n; Len > 0; --Len, ++StartAddr)
		if ((Str[0] == StartAddr[0]) && (Str[n] == StartAddr[n])) {
			for (Index = 1; Index < n; ++Index)
				if (Str[Index] != StartAddr[Index])
					break;
			if (Index >= n)
				return (StartAddr);
		}

	return ((PUCHAR)0);
}

/*

Routine Description:

	Parse the given RRD extended ID configuration table, and construct
	various PSM data structures accordingly.

	note that psm_map_phys() address & length parameters are in bytes.
	Mappings obtained with it at CbusProbe() time are valid to
	use throughout the life of the kernel on any CPU.  Mappings obtained
	with it at other times are not necessarily visible to all CPUs.
	Note that each CPU has his own PDE at all times.

Arguments:

	Table - Supplies a pointer to the RRD extended information table

	Count - Supplies a count of the maximum number of entries.
*/

void
CbusEstablishMaps(
IN PEXT_ID_INFO Table,
IN ULONG Count
)
{
	ULONG			Index;
	PEXT_ID_INFO		Idp = Table;
	ULONG			Address, Size;

	for (Index = 0; Index < Count; Index++, Idp++) {

	/*
	 *  Establish virtual maps for each I/O and/or
	 *  memory board.  Note that I/O devices may or may
	 *  not have an attached processor - a CPU is NOT required!
	 *  memory, on the other hand, may NOT have a processor
	 *  on the same board.
	 */

		if (Idp->pm == 0 && Idp->io_function == IOF_MEMORY) {
		/*
		 * Add this memory card's ranges to our list
		 * of memory present.
		 *
		 * Add this memory range to our list
		 * of additional memory to free later.
		 */

		/*
		 * any memory below 16MB has already been reported by
		 * the BIOS, so trim the request.  this is because
		 * requests can arrive in the flavor of a memory board
		 * with 64MB (or more) on one board!
		 *
		 * note that Cbus1 "hole" memory is always reclaimed
		 * at the doubly-mapped address in high memory, never
		 * in low memory.
		 */

			Address = Idp->pel_start;
			Size = Idp->pel_size;

			if (Address + Size <= SIXTEEN_MB)
				continue;

			if (Address < SIXTEEN_MB) {
				Size -= (SIXTEEN_MB - Address);
				Address = SIXTEEN_MB;
			}

			CbusMemoryFree(Address, Size);
		}
	}
}

/*

Routine Description:

	Read in the C-bus II extended id information table.

Arguments:

	From - Supplies a pointer to the RRD source table

	To - Supplies a pointer to the destination storage for the table

Return Value:

	Number of valid table entries.

*/

ULONG
CbusReadExtIDs(
IN PEXT_ID_INFO From,
IN PEXT_ID_INFO To
)
{
	ULONG Index = 0;
	ULONG ValidEntries = 0;

	for (; Index < MAX_CBUS_ELEMENTS && From->id != LAST_EXT_ID; Index++) {

	/*
	 * we cannot skip blank RRD entries
	 *
	 * if (From->pm == 0 && From->io_function == IOF_INVALID_ENTRY)
	 *	continue;
	 */

		bcopy((PVOID)From, (PVOID)To, sizeof (EXT_ID_INFO_T));
		From++;
		To++;
		ValidEntries++;
	}

	/*
	 *  WARNING: this is not necessarily the number of valid CPUs !!!
	 */
	return (ValidEntries);
}

#define	MAX_MEMORY_RANGES	32

ULONG				CbusExtraMemoryIndex;

typedef struct _extramemory_t {
	ULONG	Status;		/* currently unused */
	ULONG	BaseAddress;
	ULONG	ByteCount;
} EXTRA_MEMORY, *PEXTRA_MEMORY;

EXTRA_MEMORY			CbusExtraMemory [MAX_MEMORY_RANGES];

#define	SIXTEEN_MB		(16 * 1024 * 1024)

/*
 *
 * Routine Description:
 *
 * Add the specified memory range to our list of memory ranges to
 * give to MM later.  Called each time we find a memory card during startup,
 * also called on Cbus1 systems when we add a jumpered range (between 8 and
 * 16MB).  ranges are jumpered via EISA config when the user has added a
 * dual-ported RAM card and wants to configure it into the middle of memory
 * (not including 640K-1MB, which is jumpered for free) somewhere.
 *
 * Arguments:
 *
 *	Address - Supplies a start physical address in bytes of memory to free
 *
 *	Size - Supplies a length in bytes spanned by this range
 *
 */

void
CbusMemoryFree(
IN ULONG Address,
IN ULONG Size
)
{
	PEXTRA_MEMORY Descriptor;

	Descriptor = &CbusExtraMemory[CbusExtraMemoryIndex++];

	/*
	 *
	 * add the card provided we have space above.
	 *
	 */
	if (CbusExtraMemoryIndex > MAX_MEMORY_RANGES)
		return;

	Descriptor->BaseAddress = Address;
	Descriptor->ByteCount = Size;
}

/*
 * parse our memory table and integrate it into Solaris' memory structures
 */
void
CbusMemToSolaris(
struct bootmem *mrp
)
{
	struct bootmem	*origmrp = mrp;
	ULONG		himem_present = 0;
	ULONG		i;
	ULONG		Address, Size;
	PEXTRA_MEMORY	Descriptor;

	/*
	 * Check for memory above 16MB - if there isn't any present in the
	 * machine, then remove the Solaris entry so it won't be scanned.
	 */
	for (i = 0; i < CbusExtraMemoryIndex; i++) {
		Descriptor = &CbusExtraMemory[i];
		Address = Descriptor->BaseAddress;
		Size = Descriptor->ByteCount;
		if (Address + Size > SIXTEEN_MB)
			himem_present = 1;
	}

	if (himem_present == 0)
		for (mrp = origmrp; mrp->extent > 0; mrp++)
			if (mrp->base == SIXTEEN_MB)
				mrp->extent = 0;

	for (i = 0; i < CbusExtraMemoryIndex; i++) {

		Descriptor = &CbusExtraMemory[i];
		Address = Descriptor->BaseAddress;
		Size = Descriptor->ByteCount;

		/*
		 * find the place to insert the memory, this assumes
		 * the list from Solaris is already sorted
		 */
		for (mrp = origmrp; mrp->extent > 0; mrp++)
			if (Address <= mrp->base)
				break;

		/*
		 * now add it to the list for Solaris - if it's already there
		 * then we just skip this entry.  if we can add or subtract
		 * from the current entry, we do that.  if it needs an
		 * entirely new entry, then we'll need to shuffle everything
		 * after it to keep the list sorted.
		 */
		if (mrp->base == Address) {
			if (mrp->extent != Size)
				mrp->extent = Size;
			continue;
		}

		/*
		 * shuffle any needed entries to add ours in a sorted manner.
		 * actually, since Solaris gives us 3 entries (0-640, 1MB-16MB,
		 * and 16MB on up), we'll never need to shuffle - we can just
		 * add to the end.
		 */
		mrp->base = Address;
		mrp->extent = Size;
		mrp->flags = BF_IGNCMOSRAM;
#if 0
		mrp[btep->memrngcnt++]		????
#endif
	}
}

/*
 * Copyright 1994 by Sun Microsystems, Inc.
 */

#ifndef	_P9100REG_H
#define	_P9100REG_H

#pragma ident	"@(#)p9100reg.h	1.8	96/06/05 SMI"

/*
 * The following describes the structure of the  Weitek  Power  9100
 * User Interface Controller.
 *
 * Some of the addresses of the p9100 depend on  the  byte  and  bit
 * ordering  of the host.  The values _LITTLE_ENDIAN and _BIG_ENDIAN
 * from isa_defs.h should set the right  addresses.   The  endianess
 * can  be  forced  by  setting  P9100_LITTLE_ENDIAN to 0 or 1.  The
 * values P9100_ENDIAN* can be set to a constant if they  are  wrong
 * for  your  machine.  If its desired to determine this at runtime,
 * set the #define P9100_ENDIAN* to the runtime variable  containing
 * a  0-7  for  the  value  that  is  in  bits  16-18 of the address
 * (P9100_ADDR_SWAP_*).
 *
 * In the comments below, a hex digit of 'z' indicates that  bit  19
 * is  0,  and  bits  16-18  are  the bit swap, byte swap, half swap
 * values.  Hence, 'z' will generally be '0' and 's'  will  be  '8'.
 */

#include <sys/types.h>
#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef volatile struct p9100_regs  {

	/* System Control Registers */

	u_long volatile : 32;				/* 0x000z0000    */
	u_long volatile p9100_cr_sysconfig;		/* 0x000z0004 rw */
	u_long volatile p9100_cr_interrupt;		/* 0x000z0008 rw */
	u_long volatile p9100_cr_interrupt_en;		/* 0x000z000c rw */
	u_long volatile p9100_cr_alt_write_bank;	/* 0x000z0010 rw */
	u_long volatile p9100_cr_alt_read_bank;		/* 0x000z0014 rw */

	u_long volatile p9100_cr_filler1[0xe8/4];	/* 0x000z0018    */

	/* Video Control Registers */

	u_long volatile p9100_cr_filler2;		/* 0x000z0100 hg */
	u_long volatile p9100_cr_hrzc;			/* 0x000z0104 ro */
	u_long volatile p9100_cr_hrzt;			/* 0x000z0108 rw */
	u_long volatile p9100_cr_hrzsr;			/* 0x000z010c rw */
	u_long volatile p9100_cr_hrzbr;			/* 0x000z0110 rw */
	u_long volatile p9100_cr_hrzbf;			/* 0x000z0114 rw */
	u_long volatile p9100_cr_prehrzc;		/* 0x000z0118 rw */
	u_long volatile p9100_cr_vrtc;			/* 0x000z011c ro */
	u_long volatile p9100_cr_vrtt;			/* 0x000z0120 rw */
	u_long volatile p9100_cr_vrtsr;			/* 0x000z0124 rw */
	u_long volatile p9100_cr_vrtbr;			/* 0x000z0128 rw */
	u_long volatile p9100_cr_vrtbf;			/* 0x000z012c rw */
	u_long volatile p9100_cr_prevrtc;		/* 0x000z0130 rw */
	u_long volatile p9100_cr_sraddr;		/* 0x000z0134 ro */
	u_long volatile p9100_cr_srtctl;		/* 0x000z0138 rw */
	u_long volatile p9100_cr_qsfcounter;		/* 0x000z013c ro */
	u_long volatile p9100_cr_srtctl2;		/* 0x000z0140 rw */
	u_long volatile p9100_cr_filler3[0x3c/4];	/* 0x000z0144 hg */

	/* Vram Control Registers */

	u_long volatile : 32;				/* 0x000z0180    */
	u_long volatile p9100_cr_mem_config;		/* 0x000z0184 rw */
	u_long volatile p9100_cr_rfperiod;		/* 0x000z0188 rw */
	u_long volatile p9100_cr_rfcount;		/* 0x000z018c ro */
	u_long volatile p9100_cr_rlmax;			/* 0x000z0190 rw */
	u_long volatile p9100_cr_rlcur;			/* 0x000z0194 ro */
	u_long volatile p9100_cr_pu_config;		/* 0x000z0198 ro */
	u_long volatile p9100_cr_filler4[0x64/4];	/* 0x000z019c    */

	/* Ramdac Control Registers */

	u_long volatile p9100_cr_ramdac[16];		/* 0x000z0200 rw */
	u_long volatile p9100_cr_filler5[0x1c0/4];	/* 0x000z0240    */

	/* Video Coprocessor Interface Registers */

	u_long volatile p9100_cr_video_coprocessor[256]; /* 0x000z0400 rw */
	u_long volatile p9100_cr_filler6[0x1800/4];	/* 0x000z0800    */

	u_long volatile p9100_dr_status;		/* 0x000z2000 ro */
	u_long volatile p9100_dr_blit;			/* 0x000z2004 ro */
	u_long volatile p9100_dr_quad;			/* 0x000z2008 ro */
	u_long volatile p9100_dr_pixel8;		/* 0x000z200c wo */
	u_long volatile : 32;				/* 0x000z2010    */
	u_long volatile p9100_dr_next_pixels;		/* 0x000z2014 wo */
	u_long volatile p9100_dr_filler6[0x68/4];	/* 0x000z2018    */
	u_long volatile p9100_dr_pixel1[32];		/* 0x000z2080 wo */
	u_long volatile p9100_dr_filler7[0x80/4];	/* 0x000z2100    */

	/* Parameter Engine Control Registers */

	u_long volatile : 32;				/* 0x000z2180    */
	u_long volatile p9100_dr_oor;			/* 0x000z2184 ro */
	u_long volatile : 32;				/* 0x000z2188    */
	u_long volatile p9100_dr_cindex;		/* 0x000z218c rw */
	long   volatile p9100_dr_w_off_xy;		/* 0x000z2190 rw */
	long   volatile p9100_dr_pe_w_min;		/* 0x000z2194 ro */
	long   volatile p9100_dr_pe_w_max;		/* 0x000z2198 ro */
	long   volatile : 32;				/* 0x000z219c    */
	u_long volatile p9100_dr_yclip;			/* 0x000z21a0 ro */
	u_long volatile p9100_dr_xclip;			/* 0x000z21a4 ro */
	u_long volatile p9100_dr_xedge_lt;		/* 0x000z21a8 ro */
	u_long volatile p9100_dr_xedge_gt;		/* 0x000z21ac ro */
	u_long volatile p9100_dr_yedge_lt;		/* 0x000z21b0 ro */
	u_long volatile p9100_dr_yedge_gt;		/* 0x000z21b4 ro */
	u_long volatile p9100_dr_filler8[0x48/4];	/* 0x000z21b8    */

	/* Drawing Engine Pixel Processing Registers */

	u_long volatile p9100_dr_color0;		/* 0x000z2200 rw */
	u_long volatile p9100_dr_color1;		/* 0x000z2204 rw */
	u_long volatile p9100_dr_pmask;			/* 0x000z2208 rw */
	u_long volatile p9100_dr_draw_mode;		/* 0x000z220c rw */
	long   volatile p9100_dr_pat_originx;		/* 0x000z2210 rw */
	long   volatile p9100_dr_pat_originy;		/* 0x000z2214 rw */
	u_long volatile p9100_dr_raster;		/* 0x000z2218 rw */
	u_long volatile p9100_dr_pixel8_reg;		/* 0x000z221c rw */
	long   volatile p9100_dr_w_min;			/* 0x000z2220 wo */
	long   volatile p9100_dr_w_max;			/* 0x000z2224 wo */
	u_long volatile p9100_dr_filler9[0x10/4];	/* 0x000z2228    */
	u_long volatile p9100_dr_color2;		/* 0x000z2238 rw */
	u_long volatile p9100_dr_color3;		/* 0x000z223c rw */
	u_long volatile p9100_dr_filler10[0x40/4];	/* 0x000z2228    */
	u_long volatile p9100_dr_pattern[4];		/* 0x000z2280 rw */
	u_long volatile p9100_dr_software[4];		/* 0x000z2290 rw */
	long   volatile p9100_dr_b_w_min;		/* 0x000z22a0 ro */
	long   volatile p9100_dr_b_w_max;		/* 0x000z22a4 ro */
	u_long volatile p9100_dr_filler11[0xd58/4];	/* 0x000z22a8    */

	/* Parameter Engine Coordinate Registers */

	long   volatile : 32;				/* 0x000z3000    */
	long   volatile : 32;				/* 0x000z3004    */
	long   volatile p9100_dr_x0;			/* 0x000z3008 rw */
	long   volatile : 32;				/* 0x000z300c    */
	long   volatile p9100_dr_y0;			/* 0x000z3010 rw */
	long   volatile : 32;				/* 0x000z3014    */
	long   volatile p9100_dr_xy0;			/* 0x000z3018 rw */
	long   volatile : 32;				/* 0x000z301c    */
	long   volatile : 32;				/* 0x000z3020    */
	long   volatile : 32;				/* 0x000z3024    */
	long   volatile p9100_dr_rel_x0;		/* 0x000z3028 wo */
	long   volatile : 32;				/* 0x000z302c    */
	long   volatile p9100_dr_rel_y0;		/* 0x000z3030 wo */
	long   volatile : 32;				/* 0x000z3034    */
	long   volatile p9100_dr_rel_xy0;		/* 0x000z3038 wo */
	long   volatile : 32;				/* 0x000z303c    */
	long   volatile : 32;				/* 0x000z3040    */
	long   volatile : 32;				/* 0x000z3044    */
	long   volatile p9100_dr_x1;			/* 0x000z3048 rw */
	long   volatile : 32;				/* 0x000z304c    */
	long   volatile p9100_dr_y1;			/* 0x000z3050 rw */
	long   volatile : 32;				/* 0x000z3054    */
	long   volatile p9100_dr_xy1;			/* 0x000z3058 rw */
	long   volatile : 32;				/* 0x000z305c    */
	long   volatile : 32;				/* 0x000z3060    */
	long   volatile : 32;				/* 0x000z3064    */
	long   volatile p9100_dr_rel_x1;		/* 0x000z3068 wo */
	long   volatile : 32;				/* 0x000z306c    */
	long   volatile p9100_dr_rel_y1;		/* 0x000z3070 wo */
	long   volatile : 32;				/* 0x000z3074    */
	long   volatile p9100_dr_rel_xy1;		/* 0x000z3078 wo */
	long   volatile : 32;				/* 0x000z307c    */
	long   volatile : 32;				/* 0x000z3080    */
	long   volatile : 32;				/* 0x000z3084    */
	long   volatile p9100_dr_x2;			/* 0x000z3088 rw */
	long   volatile : 32;				/* 0x000z308c    */
	long   volatile p9100_dr_y2;			/* 0x000z3090 rw */
	long   volatile : 32;				/* 0x000z3094    */
	long   volatile p9100_dr_xy2;			/* 0x000z3098 rw */
	long   volatile : 32;				/* 0x000z309c    */
	long   volatile : 32;				/* 0x000z30a0    */
	long   volatile : 32;				/* 0x000z30a4    */
	long   volatile p9100_dr_rel_x2;		/* 0x000z30a8 wo */
	long   volatile : 32;				/* 0x000z30ac    */
	long   volatile p9100_dr_rel_y2;		/* 0x000z30b0 wo */
	long   volatile : 32;				/* 0x000z30b4    */
	long   volatile p9100_dr_rel_xy2;		/* 0x000z30b8 wo */
	long   volatile : 32;				/* 0x000z30bc    */
	long   volatile : 32;				/* 0x000z30c0    */
	long   volatile : 32;				/* 0x000z30c4    */
	long   volatile p9100_dr_x3;			/* 0x000z30c8 rw */
	long   volatile : 32;				/* 0x000z30cc    */
	long   volatile p9100_dr_y3;			/* 0x000z30d0 rw */
	long   volatile : 32;				/* 0x000z30d4    */
	long   volatile p9100_dr_xy3;			/* 0x000z30d8 rw */
	long   volatile : 32;				/* 0x000z30dc    */
	long   volatile : 32;				/* 0x000z30e0    */
	long   volatile : 32;				/* 0x000z30e4    */
	long   volatile p9100_dr_rel_x3;		/* 0x000z30e8 wo */
	long   volatile : 32;				/* 0x000z30ec    */
	long   volatile p9100_dr_rel_y3;		/* 0x000z30f0 wo */
	long   volatile : 32;				/* 0x000z30f4    */
	long   volatile p9100_dr_rel_xy3;		/* 0x000z30f8 wo */
	long   volatile : 32;				/* 0x000z30fc    */
	long   volatile p9100_dr_filler12[0x100/4];	/* 0x000z3100    */

	/* Meta Coordinate Pseudo Registers */

	long   volatile : 32;				/* 0x000z3200    */
	long   volatile : 32;				/* 0x000z3204    */
	long   volatile p9100_dr_point_x;		/* 0x000z3208 wo */
	long   volatile : 32;				/* 0x000z320c    */
	long   volatile p9100_dr_point_y;		/* 0x000z3210 wo */
	long   volatile : 32;				/* 0x000z3214    */
	long   volatile p9100_dr_point_xy;		/* 0x000z3218 wo */
	long   volatile : 32;				/* 0x000z321c    */
	long   volatile : 32;				/* 0x000z3220    */
	long   volatile : 32;				/* 0x000z3224    */
	long   volatile p9100_dr_point_rel_x;		/* 0x000z3228 wo */
	long   volatile : 32;				/* 0x000z322c    */
	long   volatile p9100_dr_point_rel_y;		/* 0x000z3230 wo */
	long   volatile : 32;				/* 0x000z3234    */
	long   volatile p9100_dr_point_rel_xy;		/* 0x000z3238 wo */
	long   volatile : 32;				/* 0x000z323c    */
	long   volatile : 32;				/* 0x000z3240    */
	long   volatile : 32;				/* 0x000z3244    */
	long   volatile p9100_dr_line_x;		/* 0x000z3248 wo */
	long   volatile : 32;				/* 0x000z324c    */
	long   volatile p9100_dr_line_y;		/* 0x000z3250 wo */
	long   volatile : 32;				/* 0x000z3254    */
	long   volatile p9100_dr_line_xy;		/* 0x000z3258 wo */
	long   volatile : 32;				/* 0x000z325c    */
	long   volatile : 32;				/* 0x000z3260    */
	long   volatile : 32;				/* 0x000z3264    */
	long   volatile p9100_dr_line_rel_x;		/* 0x000z3268 wo */
	long   volatile : 32;				/* 0x000z326c    */
	long   volatile p9100_dr_line_rel_y;		/* 0x000z3270 wo */
	long   volatile : 32;				/* 0x000z3274    */
	long   volatile p9100_dr_line_rel_xy;		/* 0x000z3278 wo */
	long   volatile : 32;				/* 0x000z327c    */
	long   volatile : 32;				/* 0x000z3280    */
	long   volatile : 32;				/* 0x000z3284    */
	long   volatile p9100_dr_tri_x;			/* 0x000z3288 wo */
	long   volatile : 32;				/* 0x000z328c    */
	long   volatile p9100_dr_tri_y;			/* 0x000z3290 wo */
	long   volatile : 32;				/* 0x000z3294    */
	long   volatile p9100_dr_tri_xy;		/* 0x000z3298 wo */
	long   volatile : 32;				/* 0x000z329c    */
	long   volatile : 32;				/* 0x000z32a0    */
	long   volatile : 32;				/* 0x000z32a4    */
	long   volatile p9100_dr_tri_rel_x;		/* 0x000z32a8 wo */
	long   volatile : 32;				/* 0x000z32ac    */
	long   volatile p9100_dr_tri_rel_y;		/* 0x000z32b0 wo */
	long   volatile : 32;				/* 0x000z32b4    */
	long   volatile p9100_dr_tri_rel_xy;		/* 0x000z32b8 wo */
	long   volatile : 32;				/* 0x000z32bc    */
	long   volatile : 32;				/* 0x000z32c0    */
	long   volatile : 32;				/* 0x000z32c4    */
	long   volatile p9100_dr_quad_x;		/* 0x000z32c8 wo */
	long   volatile : 32;				/* 0x000z32cc    */
	long   volatile p9100_dr_quad_y;		/* 0x000z32d0 wo */
	long   volatile : 32;				/* 0x000z32d4    */
	long   volatile p9100_dr_quad_xy;		/* 0x000z32d8 wo */
	long   volatile : 32;				/* 0x000z32dc    */
	long   volatile : 32;				/* 0x000z32e0    */
	long   volatile : 32;				/* 0x000z32e4    */
	long   volatile p9100_dr_quad_rel_x;		/* 0x000z32e8 wo */
	long   volatile : 32;				/* 0x000z32ec    */
	long   volatile p9100_dr_quad_rel_y;		/* 0x000z32f0 wo */
	long   volatile : 32;				/* 0x000z32f4    */
	long   volatile p9100_dr_quad_rel_xy;		/* 0x000z32f8 wo */
	long   volatile : 32;				/* 0x000z32fc    */
	long   volatile : 32;				/* 0x000z3300    */
	long   volatile : 32;				/* 0x000z3304    */
	long   volatile p9100_dr_rect_x;		/* 0x000z3308 wo */
	long   volatile : 32;				/* 0x000z330c    */
	long   volatile p9100_dr_rect_y;		/* 0x000z3310 wo */
	long   volatile : 32;				/* 0x000z3314    */
	long   volatile p9100_dr_rect_xy;		/* 0x000z3318 wo */
	long   volatile : 32;				/* 0x000z331c    */
	long   volatile : 32;				/* 0x000z3320    */
	long   volatile : 32;				/* 0x000z3324    */
	long   volatile p9100_dr_rect_rel_x;		/* 0x000z3328 wo */
	long   volatile : 32;				/* 0x000z332c    */
	long   volatile p9100_dr_rect_rel_y;		/* 0x000z3330 wo */
	long   volatile : 32;				/* 0x000z3334    */
	long   volatile p9100_dr_rect_rel_xy;		/* 0x000z3338 wo */
	long   volatile : 32;				/* 0x000z333c    */
	long   volatile p9100_dr_filler13[0xcc0/4];	/* 0x000z3340    */

	u_long volatile p9100_pa_pixel8_array[0x4000/4]; /* 0x000z4000    */
	u_long volatile p9100_pa_filler1[0x8000/4];	/* 0x000z8000    */

} p9100_regs_t;						/* 0x00010000    */

	/* The Entire P9100 Address Space */

typedef volatile struct p9100   {
	/* 0x000z0000    */
	p9100_regs_t volatile	p9100_regs[8];
	/* 0x00080000    */
	ulong_t volatile	p9100_filler[0x780000/4];
	/* 0x00800000 rw */
	uchar_t volatile	p9100_frame_buffer[0x800000];
} p9100_t;						/* 0x01000000    */

typedef p9100_t volatile *p9100p_t;

	/* Address Endian Bits */

#define	P9100_ADDR_SWAP_HALF_WORDS	4		/* 0x00040000 */
#define	P9100_ADDR_SWAP_BYTES		2		/* 0x00020000 */
#define	P9100_ADDR_SWAP_BITS		1		/* 0x00010000 */

#ifndef P9100_LITTLE_ENDIAN
#if defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#define	P9100_LITTLE_ENDIAN 1
#elif defined(_BIT_ENDIAN) && !defined(_LITTLE_ENDIAN)
#define	P9100_LITTLE_ENDIAN 0
#endif
#endif

#if P9100_LITTLE_ENDIAN

#ifndef P9100_ENDIAN_PIXEL1
#define	P9100_ENDIAN_PIXEL1  \
	(P9100_ADDR_SWAP_HALF_WORDS | \
	P9100_ADDR_SWAP_BYTES | \
	P9100_ADDR_SWAP_BITS)				/* 0x00070000 */
#endif

#ifndef P9100_ENDIAN_PIXEL8
#define	P9100_ENDIAN_PIXEL8  \
	(P9100_ADDR_SWAP_HALF_WORDS | \
	P9100_ADDR_SWAP_BYTES)				/* 0x00060000 */
#endif

#ifndef P9100_ENDIAN_PIXEL8_ARRAY
#define	P9100_ENDIAN_PIXEL8_ARRAY  \
	(P9100_ADDR_SWAP_HALF_WORDS | \
	P9100_ADDR_SWAP_BYTES)				/* 0x00060000 */
#endif

#ifndef P9100_ENDIAN_PATTERN
#define	P9100_ENDIAN_PATTERN \
	(P9100_ADDR_SWAP_HALF_WORDS | \
	P9100_ADDR_SWAP_BITS)				/* 0x00070000 */

#ifndef P9100_ENDIAN_RAMDAC
#define	P9100_ENDIAN_RAMDAC  \
	P9100_ADDR_SWAP_HALF_WORDS			/* 0x00040000 */
#endif  P9100_ENDIAN_RAMDAC

#endif

#else   /* !P9100_LITTLE_ENDIAN */

#ifndef P9100_ENDIAN_PIXEL1
#define	P9100_ENDIAN_PIXEL1		0
#endif

#ifndef P9100_ENDIAN_PIXEL8
#define	P9100_ENDIAN_PIXEL8		0
#endif

#ifndef P9100_ENDIAN_PIXEL8_ARRAY
#define	P9100_ENDIAN_PIXEL8_ARRAY	0
#endif

#ifndef P9100_ENDIAN_PATTERN
#define	P9100_ENDIAN_PATTERN		0
#endif

#ifndef P9100_ENDIAN_RAMDAC
#define	P9100_ENDIAN_RAMDAC		0
#endif  P9100_ENDIAN_RAMDAC

#endif

#ifndef P9100CR
#define	P9100CR p9100_regs[0]
#endif

#ifndef P9100DR
#define	P9100DR p9100_regs[0]
#endif

#ifndef P9100DRP1
#define	P9100DRP1 p9100_regs[P9100_ENDIAN_PIXEL1]
#endif

#ifndef P9100DRP8
#define	P9100DRP8 p9100_regs[P9100_ENDIAN_PIXEL8]
#endif

#ifndef P9100PA
#define	P9100PA   p9100_regs[P9100_ENDIAN_PIXEL8_ARRAY]
#endif

#ifndef P9100DRPT
#define	P9100DRPT p9100_regs[P9100_ENDIAN_PATTERN]
#endif

#ifndef P9100RD
#define	P9100RD p9100_regs[P9100_ENDIAN_RAMDAC]
#endif

#define	p9100_alt_write_bank \
	P9100CR.p9100_cr_alt_write_bank			/* 0x000z0010 rw */
#define	p9100_alt_read_bank \
	P9100CR.p9100_cr_alt_read_bank			/* 0x000z0014 rw */
#define	p9100_b_w_max	P9100DR.p9100_dr_b_w_max	/* 0x000z22a4 rw */
#define	p9100_b_w_min	P9100DR.p9100_dr_b_w_min	/* 0x000z22a0 rw */
#define	p9100_bground	P9100DR.p9100_dr_color1		/* 0x000z2204 rw */
#define	p9100_blit	P9100DR.p9100_dr_blit		/* 0x000z2004 ro */
#define	p9100_cindex	P9100DR.p9100_dr_cindex		/* 0x000z218c rw */
#define	p9100_color0	P9100DR.p9100_dr_color0		/* 0x000z2200 rw */
#define	p9100_color1	P9100DR.p9100_dr_color1		/* 0x000z2204 rw */
#define	p9100_color2	P9100DR.p9100_dr_color2		/* 0x000z2238 rw */
#define	p9100_color3	P9100DR.p9100_dr_color3		/* 0x000z223c rw */
#define	p9100_draw_mode	P9100DR.p9100_dr_draw_mode	/* 0x000z220c rw */
#define	p9100_fground	P9100DR.p9100_dr_color0		/* 0x000z2200 rw */
#define	p9100_hrzbf	P9100CR.p9100_cr_hrzbf		/* 0x000z0114 rw */
#define	p9100_hrzbr	P9100CR.p9100_cr_hrzbr		/* 0x000z0110 rw */
#define	p9100_hrzc	P9100CR.p9100_cr_hrzc		/* 0x000z0104 ro */
#define	p9100_hrzsr	P9100CR.p9100_cr_hrzsr		/* 0x000z010c rw */
#define	p9100_hrzt	P9100CR.p9100_cr_hrzt		/* 0x000z0108 rw */
#define	p9100_interrupt	P9100CR.p9100_cr_interrupt	/* 0x000z0008 rw */
#define	p9100_interrupt_en \
	P9100CR.p9100_cr_interrupt_en			/* 0x000z000c rw */
#define	p9100_line_rel_x \
	P9100DR.p9100_dr_line_rel_x			/* 0x000z3268 wo */
#define	p9100_line_rel_xy \
	P9100DR.p9100_dr_line_rel_xy			/* 0x000z3278 wo */
#define	p9100_line_rel_y \
	P9100DR.p9100_dr_line_rel_y			/* 0x000z3270 wo */
#define	p9100_line_x	P9100DR.p9100_dr_line_x		/* 0x000z3248 wo */
#define	p9100_line_xy	P9100DR.p9100_dr_line_xy	/* 0x000z3258 wo */
#define	p9100_line_y	P9100DR.p9100_dr_line_y		/* 0x000z3250 wo */
#define	p9100_mem_config \
	P9100CR.p9100_cr_mem_config			/* 0x000z0184 rw */
#define	p9100_next_pixels \
	P9100DR.p9100_dr_next_pixels			/* 0x000z2014 wo */
#define	p9100_oor	P9100DR.p9100_dr_oor		/* 0x000z2184 ro */
#define	p9100_pat_originx \
	P9100DR.p9100_dr_pat_originx			/* 0x000z2210 rw */
#define	p9100_pat_originy \
	P9100DR.p9100_dr_pat_originy			/* 0x000z2214 rw */
#define	p9100_pattern	P9100DRPT.p9100_dr_pattern	/* 0x000z2280 rw */
#define	p9100_pe_w_max	P9100DR.p9100_dr_pe_w_max	/* 0x000z2198 ro */
#define	p9100_pe_w_min	P9100DR.p9100_dr_pe_w_min	/* 0x000z2194 ro */
#define	p9100_pixel1	P9100DRP1.p9100_dr_pixel1	/* 0x000z2080 wo */
#define	p9100_pixel8	P9100DRP8.p9100_dr_pixel8	/* 0x000z200c wo */
#define	p9100_pixel8_array \
	P9100PA.p9100_pa_pixel8_array			/* 0x000z4000 wo */
#define	p9100_pixel8_reg \
	P9100DRP8.p9100_dr_pixel8_reg			/* 0x000z221c rw */
#define	p9100_pmask	P9100DR.p9100_dr_pmask		/* 0x000z2208 rw */
#define	p9100_point_rel_x \
	P9100DR.p9100_dr_point_rel_x			/* 0x000z3228 wo */
#define	p9100_point_rel_xy \
	P9100DR.p9100_dr_point_rel_xy			/* 0x000z3238 wo */
#define	p9100_point_rel_y \
	P9100DR.p9100_dr_point_rel_y			/* 0x000z3230 wo */
#define	p9100_point_x	P9100DR.p9100_dr_point_x	/* 0x000z3208 wo */
#define	p9100_point_xy	P9100DR.p9100_dr_point_xy	/* 0x000z3218 wo */
#define	p9100_point_y	P9100DR.p9100_dr_point_y	/* 0x000z3210 wo */
#define	p9100_prehrzc	P9100CR.p9100_cr_prehrzc	/* 0x000z0118 rw */
#define	p9100_prevrtc	P9100CR.p9100_cr_prevrtc	/* 0x000z0130 rw */
#define	p9100_pu_config	P9100CR.p9100_cr_pu_config	/* 0x000z0198 rw */
#define	p9100_qsfcounter \
	P9100CR.p9100_cr_qsfcounter			/* 0x000z013c ro */
#define	p9100_quad	P9100DR.p9100_dr_quad		/* 0x000z2008 ro */
#define	p9100_quad_rel_x \
	P9100DR.p9100_dr_quad_rel_x			/* 0x000z32e8 wo */
#define	p9100_quad_rel_xy \
	P9100DR.p9100_dr_quad_rel_xy			/* 0x000z32f8 wo */
#define	p9100_quad_rel_y \
	P9100DR.p9100_dr_quad_rel_y			/* 0x000z32f0 wo */
#define	p9100_quad_x	P9100DR.p9100_dr_quad_x		/* 0x000z32c8 wo */
#define	p9100_quad_xy	P9100DR.p9100_dr_quad_xy	/* 0x000z32d8 wo */
#define	p9100_quad_y	P9100DR.p9100_dr_quad_y		/* 0x000z32d0 wo */
#define	p9100_ramdac	P9100RD.p9100_cr_ramdac		/* 0x000z0200 rw */
#define	p9100_raster	P9100DR.p9100_dr_raster		/* 0x000z2218 rw */
#define	p9100_rect_rel_x \
	P9100DR.p9100_dr_rect_rel_x			/* 0x000z3328 wo */
#define	p9100_rect_rel_xy \
	P9100DR.p9100_dr_rect_rel_xy			/* 0x000z3338 wo */
#define	p9100_rect_rel_y \
	P9100DR.p9100_dr_rect_rel_y			/* 0x000z3330 wo */
#define	p9100_rect_x	P9100DR.p9100_dr_rect_x		/* 0x000z3308 wo */
#define	p9100_rect_xy	P9100DR.p9100_dr_rect_xy	/* 0x000z3318 wo */
#define	p9100_rect_y	P9100DR.p9100_dr_rect_y		/* 0x000z3310 wo */
#define	p9100_rel_x0	P9100DR.p9100_dr_rel_x0		/* 0x000z3028 wo */
#define	p9100_rel_x1	P9100DR.p9100_dr_rel_x1		/* 0x000z3068 wo */
#define	p9100_rel_x2	P9100DR.p9100_dr_rel_x2		/* 0x000z30a8 wo */
#define	p9100_rel_x3	P9100DR.p9100_dr_rel_x3		/* 0x000z30e8 wo */
#define	p9100_rel_xy0	P9100DR.p9100_dr_rel_xy0	/* 0x000z3038 wo */
#define	p9100_rel_xy1	P9100DR.p9100_dr_rel_xy1	/* 0x000z3078 wo */
#define	p9100_rel_xy2	P9100DR.p9100_dr_rel_xy2	/* 0x000z30b8 wo */
#define	p9100_rel_xy3	P9100DR.p9100_dr_rel_xy3	/* 0x000z30f8 wo */
#define	p9100_rel_y0	P9100DR.p9100_dr_rel_y0		/* 0x000z3030 wo */
#define	p9100_rel_y1	P9100DR.p9100_dr_rel_y1		/* 0x000z3070 wo */
#define	p9100_rel_y2	P9100DR.p9100_dr_rel_y2		/* 0x000z30b0 wo */
#define	p9100_rel_y3	P9100DR.p9100_dr_rel_y3		/* 0x000z30f0 wo */
#define	p9100_rfcount	P9100CR.p9100_cr_rfcount	/* 0x000z018c ro */
#define	p9100_rfperiod	P9100CR.p9100_cr_rfperiod	/* 0x000z0188 rw */
#define	p9100_rlcur	P9100CR.p9100_cr_rlcur		/* 0x000z0194 ro */
#define	p9100_rlmax	P9100CR.p9100_cr_rlmax		/* 0x000z0190 rw */
#define	p9100_software	P9100DR.p9100_dr_software	/* 0x000z2290 rw */
#define	p9100_sraddr	P9100CR.p9100_cr_sraddr		/* 0x000z0134 ro */
#define	p9100_srtctl	P9100CR.p9100_cr_srtctl		/* 0x000z0138 rw */
#define	p9100_srtctl2	P9100CR.p9100_cr_srtctl2	/* 0x000z0140 rw */
#define	p9100_status	P9100DR.p9100_dr_status		/* 0x000z2000 ro */
#define	p9100_sysconfig	P9100CR.p9100_cr_sysconfig	/* 0x000z0004 rw */
#define	p9100_tri_rel_x	P9100DR.p9100_dr_tri_rel_x	/* 0x000z32a8 wo */
#define	p9100_tri_rel_xy \
	P9100DR.p9100_dr_tri_rel_xy			/* 0x000z32b8 wo */
#define	p9100_tri_rel_y	P9100DR.p9100_dr_tri_rel_y	/* 0x000z32b0 wo */
#define	p9100_tri_x	P9100DR.p9100_dr_tri_x		/* 0x000z3288 wo */
#define	p9100_tri_xy	P9100DR.p9100_dr_tri_xy		/* 0x000z3298 wo */
#define	p9100_tri_y	P9100DR.p9100_dr_tri_y		/* 0x000z3290 wo */
#define	p9100_vrtbf	P9100CR.p9100_cr_vrtbf		/* 0x000z012c rw */
#define	p9100_vrtbr	P9100CR.p9100_cr_vrtbr		/* 0x000z0128 rw */
#define	p9100_vrtc	P9100CR.p9100_cr_vrtc		/* 0x000z011c ro */
#define	p9100_vrtsr	P9100CR.p9100_cr_vrtsr		/* 0x000z0124 rw */
#define	p9100_vrtt	P9100CR.p9100_cr_vrtt		/* 0x000z0120 rw */
#define	p9100_w_max	P9100DR.p9100_dr_w_max		/* 0x000z2224 rw */
#define	p9100_w_min	P9100DR.p9100_dr_w_min		/* 0x000z2220 rw */
#define	p9100_w_off_xy	P9100DR.p9100_dr_w_off_xy	/* 0x000z2190 rw */
#define	p9100_x0	P9100DR.p9100_dr_x0		/* 0x000z3008 rw */
#define	p9100_x1	P9100DR.p9100_dr_x1		/* 0x000z3048 rw */
#define	p9100_x2	P9100DR.p9100_dr_x2		/* 0x000z3088 rw */
#define	p9100_x3	P9100DR.p9100_dr_x3		/* 0x000z30c8 rw */
#define	p9100_xclip	P9100DR.p9100_dr_xclip		/* 0x000z21a4 ro */
#define	p9100_xedge_gt	P9100DR.p9100_dr_xedge_gt	/* 0x000z21ac ro */
#define	p9100_xedge_lt	P9100DR.p9100_dr_xedge_lt	/* 0x000z21a8 ro */
#define	p9100_xy0	P9100DR.p9100_dr_xy0		/* 0x000z3018 rw */
#define	p9100_xy1	P9100DR.p9100_dr_xy1		/* 0x000z3058 rw */
#define	p9100_xy2	P9100DR.p9100_dr_xy2		/* 0x000z3098 rw */
#define	p9100_xy3	P9100DR.p9100_dr_xy3		/* 0x000z30d8 rw */
#define	p9100_y0	P9100DR.p9100_dr_y0		/* 0x000z3010 rw */
#define	p9100_y1	P9100DR.p9100_dr_y1		/* 0x000z3050 rw */
#define	p9100_y2	P9100DR.p9100_dr_y2		/* 0x000z3090 rw */
#define	p9100_y3	P9100DR.p9100_dr_y3		/* 0x000z30d0 rw */
#define	p9100_yclip	P9100DR.p9100_dr_yclip		/* 0x000z21a0 ro */
#define	p9100_yedge_gt	P9100DR.p9100_dr_yedge_gt	/* 0x000z21b4 ro */
#define	p9100_yedge_lt	P9100DR.p9100_dr_yedge_lt	/* 0x000z21b0 ro */

#define	p9100_bitflip	P9100DRPT.p9100_cr_ramdac[11]	/* 0x000z022c rw */

	/* Alt Bank Registers 0x000z001? rw */

#define	P9100_ALT_BANK_SHIFT		16
#define	P9100_ALT_BANK_MASK		0x3f0000

	/* Coordinate Registers 0x000z3??? rw */

#define	P9100_COORD_BITS		14
#define	P9100_COORD_SIGN_BIT		(1 << (P9100_COORD_BITS - 1))
#define	P9100_COORD_MIN			(-P9100_COORD_SIGN_BIT)
#define	P9100_COORD_MAX			(P9100_COORD_SIGN_BIT - 1)
#define	P9100_COORD_MASK		((1 << P9100_COORD_BITS) - 1)
#define	P9100_COORD_SIGN_EXTEND(x)	(((x) & P9100_COORD_SIGN_BIT) ? \
	((x) | P9100_COORD_MIN) : ((x) & P9100_COORD_MAX))

	/* Draw Mode Register 0x000z220c rw */

#define	P9100_DRAWMODE_PICK_CTRL	0x00000008
#define	P9100_DRAWMODE_PICK		0x00000004
#define	P9100_DRAWMODE_DEST_BUFFER_CTRL	0x00000002
#define	P9100_DRAWMODE_DEST_BUFFER	0x00000001

	/* Interrupt Register 0x000z0008 rw */

#define	P9100_INT_VBLANKED_CTRL		0x00000020
#define	P9100_INT_VBLANKED		0x00000010
#define	P9100_INT_PICKED_CTRL		0x00000008
#define	P9100_INT_PICKED		0x00000004
#define	P9100_INT_DE_IDLE_CTRL		0x00000002
#define	P9100_INT_DE_IDLE		0x00000001

	/* Interrupt Enable Register 0x000z000c rw */

#define	P9100_INTEN_MEN_CTRL		0x00000080
#define	P9100_INTEN_MEN			0x00000040
#define	P9100_INTEN_VBLANKED_EN_CTRL	0x00000020
#define	P9100_INTEN_VBLANKED_EN		0x00000010
#define	P9100_INTEN_PICKED_EN_CTRL	0x00000008
#define	P9100_INTEN_PICKED_EN		0x00000004
#define	P9100_INTEN_DE_IDLE_EN_CTRL	0x00000002
#define	P9100_INTEN_DE_IDLE_EN		0x00000001

	/* Memory Configuration Register 0x000z0184 rw */

#define	P9100_MEMCFG_VRAM_READ_SAMPLE	0x80000000
#define	P9100_MEMCFG_SLOW_HOST_HIFC	0x40000000
#define	P9100_MEMCFG_MEMORY_CONFIG_MASK	0x20000007
#define	P9100_MEMCFG_MEMORY_CONFIG_1	0x00000001  /* 2*128K Vr, 1*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_2	0x00000002  /* 2*128K Vr, 1*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_3	0x00000003  /* 4*128K Vr, 1*2Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_4	0x00000004  /* 1*256K Vr, 1*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_5	0x00000005  /* 2*256K Vr, 1*2Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_6	0x00000006  /* 2*256K Vr, 1*2Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_7	0x00000007  /* 4*256K Vr, 1*4Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_11	0x20000003  /* 4*128K Vr, 2*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_13	0x20000005  /* 2*256K Vr, 2*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_14	0x20000006  /* 2*256K Vr, 2*1Mb */
#define	P9100_MEMCFG_MEMORY_CONFIG_15	0x20000007  /* 4*256K Vr, 2*2Mb */
#define	P9100_MEMCFG_BLNKDLY_MASK	0x18000000
#define	P9100_MEMCFG_BLNKDLY_SHIFT	27
#define	P9100_MEMCFG_SOE_MODE_MASK	0x03000000
#define	P9100_MEMCFG_SOE_MODE_SHIFT	24
#define	P9100_MEMCFG_SOE_MODE_1_BANK	0x00000000
#define	P9100_MEMCFG_SOE_MODE_2_BANK	0x01000000
#define	P9100_MEMCFG_SOE_MODE_4_BANK	0x02000000
#define	P9100_MEMCFG_SHIFTCLK_MODE_MASK	0x00c00000
#define	P9100_MEMCFG_SHIFTCLK_MODE_SHIFT 22
#define	P9100_MEMCFG_SHIFTCLK_MODE_1_BANK 0x00000000
#define	P9100_MEMCFG_SHIFTCLK_MODE_2_BANK 0x00400000
#define	P9100_MEMCFG_SHIFTCLK_MODE_4_BANK 0x00800000
#define	P9100_MEMCFG_VAD_SHT		0x00200000
#define	P9100_MEMCFG_VIDEO_CLK_SEL	0x00100000
#define	P9100_MEMCFG_BLANK_EDGE		0x00080000
#define	P9100_MEMCFG_CRTC_FREQ_MASK	0x0000e000
#define	P9100_MEMCFG_CRTC_FREQ_SHIFT	13
#define	P9100_MEMCFG_SHIFTCLK_FREQ_MASK	0x00001c00
#define	P9100_MEMCFG_SHIFTCLK_FREQ_SHIFT 10
#define	P9100_MEMCFG_HOLD_RESET		0x00000200
#define	P9100_MEMCFG_DAC_MODE		0x00000100
#define	P9100_MEMCFG_DAC_ACCESS_ADJ	0x00000080
#define	P9100_MEMCFG_PRIORITY_SELECT	0x00000040
#define	P9100_MEMCFG_VRAM_WRITE_ADJ	0x00000020
#define	P9100_MEMCFG_VRAM_READ_ADJ	0x00000010
#define	P9100_MEMCFG_VRAM_MISS_ADJ	0x00000008

	/* Out Of Range Register 0x000z2184 ro */

#define	P9100_OOR_X3_OOR		0x00000080
#define	P9100_OOR_X2_OOR		0x00000040
#define	P9100_OOR_X1_OOR		0x00000020
#define	P9100_OOR_X0_OOR		0x00000010
#define	P9100_OOR_Y3_OOR		0x00000008
#define	P9100_OOR_Y2_OOR		0x00000004
#define	P9100_OOR_Y1_OOR		0x00000002
#define	P9100_OOR_Y0_OOR		0x00000001

	/* Power Up Configuration 0x000z0198 rw */

#define	P9100_PUCONF_BUS_MASK		0xc0000000
#define	P9100_PUCONF_BUS_INTERNAL	0x00000000
#define	P9100_PUCONF_BUS_PCI		0x40000000
#define	P9100_PUCONF_BUS_VLB		0x80000000
#define	P9100_PUCONF_CFGBA_MASK		0x1c000000
#define	P9100_PUCONF_CFGBA_SHIFT	27
#define	P9100_PUCONF_INIT_MODESELECT	0x02000000
#define	P9100_PUCONF_INIT_VGA_PRESENT	0x01000000
#define	P9100_PUCONF_RAMDAC_MASK	0x0000f000
#define	P9100_PUCONF_RAMDAC_BT485	0x00000000
#define	P9100_PUCONF_RAMDAC_IBM525	0x00008000
#define	P9100_PUCONF_REG		0x00000100
#define	P9100_PUCONF_SYNTH_MASK		0x000000e0
#define	P9100_PUCONF_SYNTH_ICD2061A	0x00000000
#define	P9100_PUCONF_SYNTH_IBM525	0x00000020
#define	P9100_PUCONF_MEM_DEPTH		0x00000010
#define	P9100_PUCONF_SAM		0x00000008
#define	P9100_PUCONF_EEPROM_TYPE_MASK	0x00000006
#define	P9100_PUCONF_EEPROM_TYPE_AT24C01 0x00000000
#define	P9100_PUCONF_M_BOARD		0x00000001

	/* Raster Register 0x000z2218 rw */

#define	P9100_RASTER_TRANSPARENT_ENABLE	0x00020000
#define	P9100_RASTER_QUAD_DRAW_MODE	0x00010000
#define	P9100_RASTER_PIXEL1_TRANSPARENT	0x00008000
#define	P9100_RASTER_PATTERN_DEPTH	0x00004000
#define	P9100_RASTER_SOLID_COLOR_DISABLE 0x00002000
#define	P9100_RASTER_ROP_MINTERMS	0x000000ff
#define	P9100_RASTER_ROP_P_MASK		0x000000f0
#define	P9100_RASTER_ROP_S_MASK		0x000000cc
#define	P9100_RASTER_ROP_D_MASK		0x000000aa

	/* Screen Repaint Timing Control 0x000z0138 rw */

#define	P9100_SRTCTL_SRC_INCS_MASK	0x00000600
#define	P9100_SRTCTL_SRC_INCS_SHIFT	9
#define	P9100_SRTCTL_SRC_INCS_0		0x00000000
#define	P9100_SRTCTL_SRC_INCS_256	0x00000200
#define	P9100_SRTCTL_SRC_INCS_512	0x00000400
#define	P9100_SRTCTL_SRC_INCS_1024	0x00000600
#define	P9100_SRTCTL_INTERNAL_VSYNC	0x00000100
#define	P9100_SRTCTL_INTERNAL_HSYNC	0x00000080
#define	P9100_SRTCTL_ENABLE_VIDEO	0x00000020
#define	P9100_SRTCTL_HBLNK_RELOAD	0x00000010
#define	P9100_SRTCTL_DISPLAY_BUFFER	0x00000008
#define	P9100_SRTCTL_QSFSELECT		0x00000007

	/* Screen Repaint Timing Control 2 0x000z0140 rw */

#define	P9100_SRTCTL2_HSYNC_PLT_MASK		0x0000000c
#define	P9100_SRTCTL2_HSYNC_PLT_SHIFT		2
#define	P9100_SRTCTL2_HSYNC_PLT_HIGH_TRUE	0x00000000
#define	P9100_SRTCTL2_HSYNC_PLT_LOW_TRUE	0x00000004
#define	P9100_SRTCTL2_HSYNC_PLT_FORCED_LOW	0x00000008
#define	P9100_SRTCTL2_HSYNC_PLT_FORCED_HIGH	0x0000000c
#define	P9100_SRTCTL2_VSYNC_PLT_MASK		0x00000003
#define	P9100_SRTCTL2_VSYNC_PLT_SHIFT		0
#define	P9100_SRTCTL2_VSYNC_PLT_HIGH_TRUE	0x00000000
#define	P9100_SRTCTL2_VSYNC_PLT_LOW_TRUE	0x00000001
#define	P9100_SRTCTL2_VSYNC_PLT_FORCED_LOW	0x00000002
#define	P9100_SRTCTL2_VSYNC_PLT_FORCED_HIGH	0x00000003

	/* Status Register 0x000z2000 ro */

#define	P9100_STATUS_ISSUE_QB		0x80000000
#define	P9100_STATUS_BUSY		0x40000000
#define	P9100_STATUS_QUAD_OR_BUSY	(P9100_STATUS_ISSUE_QB | \
	P9100_STATUS_BUSY)
#define	P9100_STATUS_PICKED		0x00000080
#define	P9100_STATUS_PIXEL_SOFTWARE	0x00000040
#define	P9100_STATUS_BLIT_SOFTWARE	0x00000020
#define	P9100_STATUS_QUAD_SOFTWARE	0x00000010
#define	P9100_STATUS_QUAD_CONCAVE	0x00000008
#define	P9100_STATUS_QUAD_HIDDEN	0x00000004
#define	P9100_STATUS_QUAD_VISIBLE	0x00000002
#define	P9100_STATUS_QUAD_INTERSECTS	0x00000001

	/* System Configuration Register 0x000z0004 rw */

#define	P9100_SYSCONF_SHIFT3_MASK	0x60000000
#define	P9100_SYSCONF_SHIFT3_SHIFT	29
#define	P9100_SYSCONF_SHIFT3_0		0x00000000
#define	P9100_SYSCONF_SHIFT3_1024	0x20000000
#define	P9100_SYSCONF_SHIFT3_2048	0x40000000
#define	P9100_SYSCONF_SHIFT3_4096	0x60000000
#define	P9100_SYSCONF_SHIFT3_MIN	1024
#define	P9100_SYSCONF_SHIFT3_MAX	4096
#define	P9100_SYSCONF_PIXEL_SIZE_MASK	0x1c000000
#define	P9100_SYSCONF_PIXEL_SIZE_SHIFT	26
#define	P9100_SYSCONF_PIXEL_SIZE_8BPP	0x08000000
#define	P9100_SYSCONF_PIXEL_SIZE_16BPP	0x0c000000
#define	P9100_SYSCONF_PIXEL_SIZE_24BPP	0x1c000000
#define	P9100_SYSCONF_PIXEL_SIZE_32BPP	0x14000000
#define	P9100_SYSCONF_DISABLE_SELFTIME	0x02000000
#define	P9100_SYSCONF_DRIVELOAD2	0x01000000
#define	P9100_SYSCONF_PLLBACKUP		0x00800000
#define	P9100_SYSCONF_SHIFT0_MASK	0x00700000
#define	P9100_SYSCONF_SHIFT0_SHIFT	20
#define	P9100_SYSCONF_SHIFT0_0		0x00000000
#define	P9100_SYSCONF_SHIFT0_128	0x00300000
#define	P9100_SYSCONF_SHIFT0_256	0x00400000
#define	P9100_SYSCONF_SHIFT0_512	0x00500000
#define	P9100_SYSCONF_SHIFT0_1024	0x00600000
#define	P9100_SYSCONF_SHIFT0_2048	0x00700000
#define	P9100_SYSCONF_SHIFT0_MIN	128
#define	P9100_SYSCONF_SHIFT0_MAX	2048
#define	P9100_SYSCONF_SHIFT1_MASK	0x000e0000
#define	P9100_SYSCONF_SHIFT1_SHIFT	17
#define	P9100_SYSCONF_SHIFT1_0		0x00000000
#define	P9100_SYSCONF_SHIFT1_64		0x00040000
#define	P9100_SYSCONF_SHIFT1_128	0x00060000
#define	P9100_SYSCONF_SHIFT1_256	0x00080000
#define	P9100_SYSCONF_SHIFT1_512	0x000a0000
#define	P9100_SYSCONF_SHIFT1_1024	0x000c0000
#define	P9100_SYSCONF_SHIFT1_MIN	64
#define	P9100_SYSCONF_SHIFT1_MAX	1024
#define	P9100_SYSCONF_SHIFT2_MASK	0x0001c000
#define	P9100_SYSCONF_SHIFT2_SHIFT	14
#define	P9100_SYSCONF_SHIFT2_0		0x00000000
#define	P9100_SYSCONF_SHIFT2_32		0x00004000
#define	P9100_SYSCONF_SHIFT2_64		0x00008000
#define	P9100_SYSCONF_SHIFT2_128	0x0000c000
#define	P9100_SYSCONF_SHIFT2_256	0x00010000
#define	P9100_SYSCONF_SHIFT2_512	0x00014000
#define	P9100_SYSCONF_SHIFT2_1024	0x00018000
#define	P9100_SYSCONF_SHIFT2_MIN	32
#define	P9100_SYSCONF_SHIFT2_MAX	512
#define	P9100_SYSCONF_PIXEL_SWAP_HALF	0x00002000
#define	P9100_SYSCONF_PIXEL_SWAP_BYTE	0x00001000
#define	P9100_SYSCONF_PIXEL_SWAP_BITS	0x00000800
#define	P9100_SYSCONF_PIXEL_BUF_READ	0x00000400
#define	P9100_SYSCONF_PIXEL_BUF_WRITE	0x00000200
#define	P9100_SYSCONF_ID_MASK		0x00000007
#define	P9100_SYSCONF_ID_N4C_A2		0x00000002
#define	P9100_SYSCONF_ID_N4E_A4		0x00000004


	/* Video Control Mask */

#define	P9100_VIDCTRL_VALUE_MASK	0x00000fff

	/* Window Minimum and Maximum Mask */

#define	P9100_WINDOW_MASK		0x00001fff
#define	P9100_WINDOW_XY_MASK		0x1fff1fff

	/* Xclip Register 0x000z21a4 ro */

#define	P9100_XCLIP_X3_LT_MIN		0x00000080
#define	P9100_XCLIP_X2_LT_MIN		0x00000040
#define	P9100_XCLIP_X1_LT_MIN		0x00000020
#define	P9100_XCLIP_X0_LT_MIN		0x00000010
#define	P9100_XCLIP_X3_GT_MAX		0x00000008
#define	P9100_XCLIP_X2_GT_MAX		0x00000004
#define	P9100_XCLIP_X1_GT_MAX		0x00000002
#define	P9100_XCLIP_X0_GT_MAX		0x00000001

	/* Xedge_gt Register 0x000z21ac ro */

#define	P9100_XEDGEGT_X0_LT_X2		0x00000020
#define	P9100_XEDGEGT_X1_LT_X3		0x00000010
#define	P9100_XEDGEGT_X3_LT_X0		0x00000008
#define	P9100_XEDGEGT_X2_LT_X3		0x00000004
#define	P9100_XEDGEGT_X1_LT_X2		0x00000002
#define	P9100_XEDGEGT_X0_LT_X1		0x00000001

	/* Xedge_lt Register 0x000z21a8 ro */

#define	P9100_XEDGELT_X0_GT_X2		0x00000020
#define	P9100_XEDGELT_X1_GT_X3		0x00000010
#define	P9100_XEDGELT_X3_GT_X0		0x00000008
#define	P9100_XEDGELT_X2_GT_X3		0x00000004
#define	P9100_XEDGELT_X1_GT_X2		0x00000002
#define	P9100_XEDGELT_X0_GT_X1		0x00000001

	/* Yclip Register 0x000z21a0 ro */

#define	P9100_YCLIP_Y3_LT_MIN		0x00000080
#define	P9100_YCLIP_Y2_LT_MIN		0x00000040
#define	P9100_YCLIP_Y1_LT_MIN		0x00000020
#define	P9100_YCLIP_Y0_LT_MIN		0x00000010
#define	P9100_YCLIP_Y3_GT_MAX		0x00000008
#define	P9100_YCLIP_Y2_GT_MAX		0x00000004
#define	P9100_YCLIP_Y1_GT_MAX		0x00000002
#define	P9100_YCLIP_Y0_GT_MAX		0x00000001

	/* Yedge_gt Register 0x000z21b4 ro */

#define	P9100_YEDGEGT_Y0_LT_Y2		0x00000020
#define	P9100_YEDGEGT_Y1_LT_Y3		0x00000010
#define	P9100_YEDGEGT_Y3_LT_Y0		0x00000008
#define	P9100_YEDGEGT_Y2_LT_Y3		0x00000004
#define	P9100_YEDGEGT_Y1_LT_Y2		0x00000002
#define	P9100_YEDGEGT_Y0_LT_Y1		0x00000001

	/* Yedge_lt Register 0x000z21b0 ro */

#define	P9100_YEDGELT_Y0_GT_Y2		0x00000020
#define	P9100_YEDGELT_Y1_GT_Y3		0x00000010
#define	P9100_YEDGELT_Y3_GT_Y0		0x00000008
#define	P9100_YEDGELT_Y2_GT_Y3		0x00000004
#define	P9100_YEDGELT_Y1_GT_Y2		0x00000002
#define	P9100_YEDGELT_Y0_GT_Y1		0x00000001

#define	P9100_PCI_VENDOR_ID		0x100e
#define	P9100_PCI_DEVICE_ID		0x9100

#define	pcicfghdr_p9100_specific0 \
	pcicfghdr_device_specific[0]  /* 0x40 */
#define	pcicfghdr_p9100_specific1 \
	pcicfghdr_device_specific[1]  /* 0x41 */
#define	pcicfghdr_p9100_specific2 \
	pcicfghdr_device_specific[2]  /* 0x42 */

#define	PCI_9100_SPECIFIC0 \
	PCI_OFFSET(pcicfghdr_p9100_specific0) /* 0x40 */
#define	PCI_9100_SPECIFIC1 \
	PCI_OFFSET(pcicfghdr_p9100_specific1) /* 0x41 */
#define	PCI_9100_SPECIFIC2 \
	PCI_OFFSET(pcicfghdr_p9100_specific2) /* 0x42 */

	/* P9100 Specific Register 0 0x40 */

#define	PCI_9100SPEC0_BUS_MASK			0xc0
#define	PCI_9100SPEC0_BUS_VESA			0x80
#define	PCI_9100SPEC0_BUS_PCI			0x40
#define	PCI_9100SPEC0_CONFIG_BUS_ADDR_MASK	0x38
#define	PCI_9100SPEC0_CONFIG_BUS_ADDR_SHIFT	3
#define	PCI_9100SPEC0_EEDAIN			0x01

	/* P9100 Specific Register 1 0x41 */

#define	PCI_9100SPEC1_ALT_COMMAND_SWAP_HALF	0x80
#define	PCI_9100SPEC1_ALT_COMMAND_SWAP_BYTE	0x40
#define	PCI_9100SPEC1_ALT_COMMAND_SWAP_BITS	0x20
#define	PCI_9100SPEC1_NATIVE_SELECT		0x08
#define	PCI_9100SPEC1_NATIVE_ENABLE		0x04
#define	PCI_9100SPEC1_MODESELECT_VGA		0x02

	/* P9100 Specific Register 2 0x42 */

#define	PCI_9100SPEC2_CLOCK_SELECT_MASK		0x1c
#define	PCI_9100SPEC2_VCEN			0x01

#define	P9100_PTR(p9100)	(P9100_NOOP(), (p9100))

#define	P9100_XY(x, y)	\
	(((long)(x) << 16) | (long)(y) & 0xffff)

#define	P9100_PAT_COPY(d, s) \
	(d)[0] = (s)[0],	\
	(d)[1] = (s)[1],	\
	(d)[2] = (s)[2],	\
	(d)[3] = (s)[3]

#define	P9100_LINEBYTES(sysconfig)  \
	(((sysconfig) & P9100_SYSCONF_SHIFT3_MASK ?         \
	1 << ((((sysconfig) & P9100_SYSCONF_SHIFT3_MASK) >> \
	P9100_SYSCONF_SHIFT3_SHIFT) + 9) : 0) +             \
	((sysconfig) & P9100_SYSCONF_SHIFT0_MASK ?          \
	1 << ((((sysconfig) & P9100_SYSCONF_SHIFT0_MASK) >> \
	P9100_SYSCONF_SHIFT0_SHIFT) + 4) : 0) +             \
	((sysconfig) & P9100_SYSCONF_SHIFT1_MASK ?          \
	1 << ((((sysconfig) & P9100_SYSCONF_SHIFT1_MASK) >> \
	P9100_SYSCONF_SHIFT1_SHIFT) + 4) : 0) +             \
	((sysconfig) & P9100_SYSCONF_SHIFT2_MASK ?          \
	1 << ((((sysconfig) & P9100_SYSCONF_SHIFT2_MASK) >> \
	P9100_SYSCONF_SHIFT2_SHIFT) + 4) : 0))

#define	P9100_GET_ENABLE_VIDEO(p9100)   \
	(((p9100) -> p9100_srtctl & P9100_SRTCTL_ENABLE_VIDEO) != 0)

#define	P9100_SET_VBLANK_INTR(p9100)    \
	(p9100) -> p9100_interrupt_en = P9100_INTEN_MEN_CTRL | \
	P9100_INTEN_MEN | P9100_INTEN_VBLANKED_EN_CTRL | \
	P9100_INTEN_VBLANKED_EN

#define	P9100_STOP_VBLANK_INTR(p9100)   \
	(p9100) -> p9100_interrupt_en = P9100_INTEN_VBLANKED_EN_CTRL

#define	P9100_CLEAR_VBLANK_INTR(p9100)  \
	(p9100) -> p9100_interrupt = P9100_INT_VBLANKED_CTRL

	/* These addresses hang the cpu.  I suspect it's a hardware */
	/* bug of the Weitek p9100. */

#define	P9100_BAD_ADDRESSES(addr) \
	(((addr) & 0xfff8fffc) == \
	(ulong_t)&((p9100p_t)0)->p9100_regs[0].p9100_cr_filler2 || \
	(((addr) & 0xfff8fffc) >= \
	(ulong_t)&((p9100p_t)0)->p9100_regs[0].p9100_cr_filler3 && \
	(((addr) & 0xfff8fffc) & 0xfff8fffc) < \
	(ulong_t)&((p9100p_t)0)->p9100_regs[0].p9100_cr_filler3 + \
	sizeof (((p9100p_t)0)->p9100_regs[0].p9100_cr_filler3)) ||           \
	(((addr) & 0xfff8fffc) >= \
	(ulong_t)&((p9100p_t)0)->p9100_regs[0].p9100_pa_filler1 && \
	((addr) & 0xfff8fffc) < \
	(ulong_t)&((p9100p_t)0)->p9100_regs[0].p9100_pa_filler1 + \
	sizeof (((p9100p_t)0)->p9100_regs[0].p9100_pa_filler1)) || \
	((addr) >= (ulong_t)&((p9100p_t)0)->p9100_filler && \
	(addr) < (ulong_t)&((p9100p_t)0)->p9100_filler + \
	sizeof (((p9100p_t)0)->p9100_filler)))

#define	P9100_REPLICATE_8BIT(x)		((ulong_t)(x) * 0x01010101UL)
#define	P9100_REPLICATE_16BIT(x)	((ulong_t)(x) * 0x00010001UL)


#if __ppc
#define	P9100_NOT_INLINE    1
#endif

#if P9100_NOT_INLINE

#define	P9100_NOOP(p9100)   (p9100)

#define	P9100_WAIT_BUSY(p9100)	\
	while ((p9100) -> p9100_status & P9100_STATUS_BUSY)

#define	P9100_DRAW_QUAD(p9100)	\
	while ((p9100) -> p9100_quad & P9100_STATUS_ISSUE_QB)

#define	P9100_DRAW_BLIT(p9100)	\
	while ((p9100) -> p9100_blit & P9100_STATUS_ISSUE_QB)

#define	P9100_WAIT_VSYNC(p9100) \
	while ((p9100) -> p9100_vrtsr & P9100_VIDCTRL_VALUE_MASK) != \
	(p9100) -> p9100_vrtc & P9100_VIDCTRL_VALUE_MASK))

#define	P9100_SET_ENABLE_VIDEO(p9100) \
	(p9100) -> p9100_srtctl |= P9100_SRTCTL_ENABLE_VIDEO

#define	P9100_CLEAR_ENABLE_VIDEO(p9100) \
	(p9100) -> p9100_srtctl &= ~P9100_SRTCTL_ENABLE_VIDEO

#define	P9100_SET_BUFFER_0(p9100)  \
	(p9100) -> p9100_sysconfig &= ~(P9100_SYSCONF_PIXEL_BUF_READ | \
	P9100_SYSCONF_PIXEL_BUF_WRITE)

#define	P9100_SET_BUFFER_1(p9100)  \
	(p9100) -> p9100_sysconfig |= P9100_SYSCONF_PIXEL_BUF_READ | \
	P9100_SYSCONF_PIXEL_BUF_WRITE

#define	P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100)    			\
	/* Errata # 028 */						\
	(*(volatile ulong *)((ulong)(p9100) + 0x800200))

#define	P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100)    			\
	/* Errata # 028 */						\
	(*(volatile ulong *)((ulong)(p9100) + 0x800000))

#else

extern  void P9100_NOOP(void);
extern  void P9100_WAIT_BUSY(p9100p_t);
extern  void P9100_DRAW_QUAD(p9100p_t);
extern  void P9100_DRAW_BLIT(p9100p_t);
extern  void P9100_WAIT_VSYNC(p9100p_t);
extern  void P9100_SET_ENABLE_VIDEO(p9100p_t);
extern  void P9100_CLEAR_ENABLE_VIDEO(p9100p_t);
extern  void P9100_SET_BUFFER_0(p9100p_t);
extern  void P9100_SET_BUFFER_1(p9100p_t);
extern  long P9100_WAIT_SUBR(p9100p_t);
extern  long P9100_QUAD_SUBR(p9100p_t);
extern  long P9100_BLIT_SUBR(p9100p_t);
extern  ulong P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100p_t); /* Errata # 028 */
extern  ulong P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100p_t); /* Errata # 028 */

#endif

#define	P9100_VBASE		0x00000000
#define	P9100_REGISTER_BASE	0x00000000
#define	P9100_REGISTER_SIZE	0x00100000
#define	P9100_FRAME_BUFFER_BASE	0x00800000
#define	P9100_FRAME_BUFFER_SIZE	0x00800000
#define	P9100_SIZE		sizeof (p9100_t)


#define	P9100_VRT_VADDR	0x10000000
#define	P9100_VRT_SIZE	4096

#ifdef	__cplusplus
}
#endif

#endif	/* !_P9100REG_H */

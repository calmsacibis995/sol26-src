/*
 * Copyright 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VIPERIO_H
#define	_VIPERIO_H

#pragma ident	"@(#)viperio.h	1.4	95/11/10 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct  viper_init_struct   {
	u_long  vi_valid;
	u_long	vi_width;
	u_long	vi_height;
	u_long	vi_depth;
	u_long	vi_sysconfig;
	u_long	vi_hrzt;
	u_long	vi_hrzsr;
	u_long	vi_hrzbr;
	u_long	vi_hrzbf;
	u_long	vi_prehrzc;
	u_long	vi_vrtt;
	u_long	vi_vrtsr;
	u_long	vi_vrtbr;
	u_long	vi_vrtbf;
	u_long  vi_prevrtc;
	u_long  vi_srtctl;
	u_long	vi_vclk;
	u_long	vi_memspeed;
	u_long  vi_memconfig;
} viper_init_t;

#define	VIPER_INIT_VCLK_VSYNC		0x80
#define	VIPER_INIT_VCLK_HSYNC		0x40
#define	VIPER_INIT_VCLK_DOUBLER		0x20
#define	VIPER_INIT_VCLK_DOT_FREQ_SHIFT	8

typedef	struct	viper_port_struct	{
	u_long	vp_bustype;
	u_char *vp_bt485_ram_write;
	u_char *vp_bt485_palet_data;
	u_char *vp_bt485_pixel_mask;
	u_char *vp_bt485_ram_read;
	u_char *vp_bt485_color_write;
	u_char *vp_bt485_color_data;
	u_char *vp_bt485_comreg0;
	u_char *vp_bt485_color_read;
	u_char *vp_bt485_comreg1;
	u_char *vp_bt485_comreg2;
	u_char *vp_bt485_stat_reg;
	u_char *vp_bt485_cursor_data;
	u_char *vp_bt485_cursor_x_low;
	u_char *vp_bt485_cursor_x_high;
	u_char *vp_bt485_cursor_y_low;
	u_char *vp_bt485_cursor_y_high;
	u_char *vp_miscout;
	u_char *vp_miscin;
	u_char *vp_seq_index_port;
	u_char *vp_seq_data_port;
} viper_port_t;

#define	vp_bt485_comreg3	vp_bt485_stat_reg

#define	VIPER_PORT_BUS_VLB	1
#define	VIPER_PORT_BUS_PCI	2
#define	VIPER_DAC_UNKNOWN	-1
#define	VIPER_DAC_BT485		485
#define	VIPER_DAC_RGB525	525

typedef	struct	viper_config_struct	{
	ulong_t vp_config_offset;
	ulong_t vp_config_value;
} viper_config_t;

#define	VIPERIOC		((('V' << 8) | 'I') << 8)
#define	VIPERIO_GET_INIT	(VIPERIOC | 1)
#define	VIPERIO_PUT_INIT	(VIPERIOC | 2)
#define	VIPERIO_GET_PORTS	(VIPERIOC | 3)
#define	VIPERIO_GRAPH_MODE	(VIPERIOC | 4)
#define	VIPERIO_TEXT_MODE	(VIPERIOC | 5)
#define	VIPERIO_SET_ICD_CLOCK	(VIPERIOC | 6)

#define	VIPERIO_MECH_PCI	0
#define	VIPERIO_MECH_VLB	1

#define	VIPERIO_GET_MECH	(VIPERIOC | 7)
#define	VIPERIO_GET_CONFIG_REG	(VIPERIOC | 8)
#define	VIPERIO_PUT_CONFIG_REG	(VIPERIOC | 9)

	/* Ioctl number to set the viper ic clock */

#define	VIPER_SET_IC_CLOCK  250

#ifdef	__cplusplus
}
#endif

#endif	/* !_VIPERIO_H */

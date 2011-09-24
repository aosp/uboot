/*
 * (C) Copyright 2011
 * Google, Inc.
 * (C) Copyright 2010
 * Texas Instruments Incorporated, <www.ti.com>
 *
 * Author :
 *	Mike J Chen <mjchen@google.com>
 *
 *	Balaji Krishnamoorthy	<balajitk@ti.com>
 *	Aneesh V		<aneesh@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _TUNGSTEN_MUX_DATA_H_
#define _TUNGSTEN_MUX_DATA_H_

#include <asm/arch/mux_omap4.h>

static const struct pad_conf_entry core_padconf_array_non_essential[] = {
	/* Needed in both regular and MFG build, to communicate
	 * with the AVR and proximity sensor.  For now, we're
	 * going to let the kernel setup the mux conf for anything
	 * it uses and do the minimum in the bootloader.
	 */
	{I2C2_SCL, (IEN | PTU | M0)},
	{I2C2_SDA, (IEN | PTU | M0)},
	{GPMC_A24, (PTU | M3 | OFF_EN | OFF_PU)},	/* gpio_48, UI_AVR_RST_N */
#ifdef CONFIG_MFG
	/* during the first stage of manufacturing diagnostics, many pins are
	 * configured as GPIOs for basic connectivity testing.  This section
	 * makes sure pins are properly configured as either inputs or outputs
	 * for diagnostics. */
	{ABE_MCBSP2_DR, (M3)},		/* gpio_111 - output, no pullup/down */
	{HDMI_HPD, (IEN | PTD | M3)},	/* gpio_63  - input,  pulldown */
	{HDMI_CEC, (M3)},		/* gpio_64  - output, no pullup/down */
	{GPMC_NBE1, (M3)},		/* gpio_60  - output, no pullup/down */
	{GPMC_A16, (M3)},		/* gpio_40  - output, no pullup/down */
	{GPMC_A18, (M3)},		/* gpio 42  - output, no pullup/down */
	{GPMC_A20, (M3)},		/* gpio_44  - output, no pullup/down */

	/* during mfg diags, all I2C pins should be set up for I2C. No need to
	 * set up I2C1 pins.  It's power on reset state is to be I2C and they
	 * can have no other mode.
	 */
	{I2C3_SCL, (IEN | PTU | M0)},
	{I2C3_SDA, (IEN | PTU | M0)},
	{I2C4_SCL, (IEN | PTU | M0)},
	{I2C4_SDA, (IEN | PTU | M0)},
#endif
};

static const struct pad_conf_entry wkup_padconf_array_non_essential[] = {
};

#endif /* _TUNGSTEN_MUX_DATA_H_ */

/*
 * am3517evm.c - board file for TI's AM3517 family of devices.
 *
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * Based on ti/evm/evm.c
 *
 * Copyright (C) 2010
 * Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mem.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/mach-types.h>
#include <i2c.h>
#include <fastboot.h>
#include "am3517evm.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef	CONFIG_CMD_FASTBOOT
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
/* Initialize the name of fastboot flash name mappings */
fastboot_ptentry ptn[6] = {
	{
		.name   = "xloader",
		.start  = 0x0000000,
		.length = 0x0020000,
		/* Written into the first 4 0x20000 blocks
		   Use HW ECC */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
		          FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC |
			  FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_2 |
			  FASTBOOT_PTENTRY_FLAGS_REPEAT_4,
	},
		{
		.name   = "bootloader",
		.start  = 0x0080000,
		.length = 0x01C0000,
		/* Skip bad blocks on write
		   Use HW ECC */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
		          FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC |
			  FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_2,
	},
	{
		.name   = "environment",
		.start  = SMNAND_ENV_OFFSET,  /* set in config file */
		.length = 0x0040000,
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC |
			  FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_1 |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_ENV,
	},
	{
		.name   = "boot",
		/* Test with start close to bad block
		   The is dependent on the individual board.
		   Change to what is required */
		/* .start  = 0x0a00000, */
			/* The real start */
		.start  = 0x0280000,
		.length = 0x0500000,
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC |
			  FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_1 |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_I,
	},
	{
		.name   = "system",
		.start  = 0x00780000,
		.length = 0x1F880000,
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC |
			  FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_1 |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_I,
	},
};
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */
#endif /* CONFIG_CMD_FASTBOOT */



/*
 * Routine: board_init
 * Description: Early hardware init.
 */
int board_init(void)
{
	gpmc_init(); /* in SRAM or SDRAM, finish GPMC */
	/* board id for Linux */
	gd->bd->bi_arch_number = MACH_TYPE_OMAP3517EVM;
	/* boot param addr */
	gd->bd->bi_boot_params = (OMAP34XX_SDRC_CS0 + 0x100);

#ifdef	CONFIG_CMD_FASTBOOT
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	int i;

	for (i = 0; i < 6; i++)
		fastboot_flash_add_ptn (&ptn[i]);
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */
#endif /* CONFIG_CMD_FASTBOOT */

	return 0;
}

/*
 * Routine: misc_init_r
 * Description: Init i2c, ethernet, etc... (done here so udelay works)
 */
int misc_init_r(void)
{
#ifdef CONFIG_DRIVER_OMAP34XX_I2C
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#endif

	dieid_num_r();

	return 0;
}

/*
 * Routine: set_muxconf_regs
 * Description: Setting up the configuration Mux registers specific to the
 *		hardware. Many pins need to be moved from protect to primary
 *		mode.
 */
void set_muxconf_regs(void)
{
	MUX_AM3517EVM();
}

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
       omap_mmc_init(0);
       return 0;
}
#endif

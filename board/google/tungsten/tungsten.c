/*
 * (C) Copyright 2011
 * Google, Inc.
 * (C) Copyright 2010
 * Texas Instruments Incorporated, <www.ti.com>
 *
 * Author :
 *	Mike J Chen <mjchen@google.com>
 *
 * Derived from Panda Board code by
 *	Steve Sakoman  <steve@sakoman.com>
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
#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mmc_host_def.h>
#include <fastboot.h>
#include <mmc.h>
#include <malloc.h>

#include <asm/arch/mux.h>

#include "tungsten.h"

DECLARE_GLOBAL_DATA_PTR;

const struct omap_sysinfo sysinfo = {
	"Board: OMAP4 Tungsten\n"
};

/**
 * @brief board_init
 *
 * @return 0
 */
int board_init(void)
{
	gd->bd->bi_arch_number = MACH_TYPE_STEELHEAD;
	gd->bd->bi_boot_params = (0x80000000 + 0x100); /* boot param addr */
	return 0;
}

/**
 * @brief board_early_init_f
 *
 * @return 0
 */
int board_early_init_f(void)
{
	gpmc_init();
	return 0;
}

int board_eth_init(bd_t *bis)
{
	return 0;
}

void do_set_mux(u32 base, const struct pad_conf_entry const *array, int size)
{
	struct pad_conf_entry const *pad = array;
	const struct pad_conf_entry const *pad_end = &array[size];

	while (pad < pad_end) {
		writew(pad->val, base + pad->offset);
		pad++;
	}
}

/**
 * @brief set_muxconf_regs Setting up the configuration Mux registers
 * specific to the board.
 */
void set_muxconf_regs(void)
{
	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array,
		   sizeof(core_padconf_array) /
		   sizeof(struct pad_conf_entry));
	do_set_mux(CONTROL_PADCONF_WKUP, wkup_padconf_array,
		   sizeof(wkup_padconf_array) /
		   sizeof(struct pad_conf_entry));
}

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	omap_mmc_init(FASTBOOT_MMC_DEVICE_ID);
	return 0;
}
#endif

int board_fbt_key_pressed(void)
{
	/* On Tungsten: GPIO_121 button pressed causes to enter fastboot */
	if (!(__raw_readl(OMAP44XX_GPIO4_BASE + DATA_IN_OFFSET) & (1<<25))){
		printf("Tungsten: GPIO_121 pressed: entering fastboot....\n");
		return 1;
	}
	return 0;
}

struct fbt_partition {
	const char *name;
	unsigned size_kb;
};

struct fbt_partition fbt_partitions[] = {
	{ "-", 128 },
	{ "xloader", 128 },
	{ "bootloader", 256 },
	{ "-", 512 },
	{ "recovery", 8*1024 },
	{ "boot", 8*1024 },
	{ "system", 512*1024 },
	{ "cache", 512*1024 },
	{ "efs", 512 }, /* TBD: possibly for encryption keys, mac addresses, etc. */
	{ "userdata", 0},
	{ 0, 0 },
};

int board_late_init(void)
{
#if 0
  extern int do_mmcinfo (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
  do_mmcinfo(NULL, 0, 0, NULL);
#endif
	dieid_num_r();

	fbt_preboot();

	return 0;
}

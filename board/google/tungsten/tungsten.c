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
#include <linux/string.h>

#include "tungsten_mux_data.h"

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

/**
 * @brief set_muxconf_regs Setting up the configuration Mux registers
 * specific to the board.
 */
void set_muxconf_regs_non_essential(void)
{
	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array_non_essential,
		   sizeof(core_padconf_array_non_essential) /
		   sizeof(struct pad_conf_entry));
	do_set_mux(CONTROL_PADCONF_WKUP, wkup_padconf_array_non_essential,
		   sizeof(wkup_padconf_array_non_essential) /
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
	return 0;
}

struct fbt_partition {
	const char *name;
	unsigned size_kb;
};

struct fbt_partition fbt_partitions[] = {
	{ "--pad", 128 },
	{ "xloader", 128 },
	{ "bootloader", 256 },
	{ "device_info", 512 }, /* device specific info like MAC addresses.
				 * read-only once it has been written to.
				 * bootloader parses this at boot and sends
				 * the contents to the kernel via cmdline args.
				 */
	{ "recovery", 8*1024 },
	{ "boot", 8*1024 },
	{ "system", 1024*1024 },
	{ "cache", 512*1024 },
	{ "efs", 8*1024 }, /* TBD: possibly for encryption keys,
			    * minimum size for a ext4 fs is about 8MB
			    */
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


struct gpio_name_mapping {
	const char* name;
	int num;
};

int tungsten_name_to_gpio(const char* name) {
	static const struct gpio_name_mapping map[] = {
		{ "spdif",		111 },
		{ "hdmi_hpd",		63 },
		{ "hdmi_cec",		64 },
		{ "hdmi_ct_cp_hpd",	60 },
	};
	int i;
	const char* tmp;

	for (i = 0; i < ARRAY_SIZE(map); ++i) {
		if (!strcmp(name, map[i].name))
			return map[i].num;
	}

	for (tmp = name; *tmp; ++tmp)
		if ((*tmp < '0') || (*tmp >
							'9'))
			return -1;

	return simple_strtoul(name, NULL, 10);
}

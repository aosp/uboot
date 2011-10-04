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

#include "steelhead_avr.h"
#include "steelhead_avr_regs.h"
#include "vcnl4000.h"
#include "tungsten_mux_data.h"
#include "pseudorandom_ids.h"

DECLARE_GLOBAL_DATA_PTR;

const struct omap_sysinfo sysinfo = {
	"Board: OMAP4 Tungsten\n"
};

struct mac_generator {
	const u32 salt;
	const char* name;
};

#define MAKE_SALT(a, b, c, d) (((u32)a << 24) | ((u32)b << 16) | \
			      ((u32)c <<  8) | ((u32)d))
static const struct mac_generator mac_defaults[] = {
	{ MAKE_SALT('W','i','F','i'), "androidboot.wifi_macaddr" },
	{ MAKE_SALT('W','i','r','e'), "smsc95xx.mac_addr" },
	{ MAKE_SALT('B','l','u','T'), "androidboot.bt_addr" },
};
static const u32 serial_no_salt = MAKE_SALT('S','e','r','#');
#undef MAKE_SALT

/**
 * @brief board_init
 *
 * @return 0
 */
int board_init(void)
{
	/* gpmc_init() touches bss, which cannot be used until
	 * after relocation.
	 */
	gpmc_init();
	gd->bd->bi_arch_number = MACH_TYPE_STEELHEAD;
	gd->bd->bi_boot_params = (0x80000000 + 0x100); /* boot param addr */
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
	int err;
	int proximity;

	err = detect_vcnl();
	if (err) {
		printf("Error %d returned from detect_vcnl()\n", err);
		return 0;
	}

	proximity = vcnl_get_proximity();
	if (proximity < 0)
		printf("Error %d returned from vcnl_get_proximity()\n",
		       proximity);
	else
		printf("%s: proximity is %d\n", __func__, proximity);

	/* we don't know the threshold to use yet to for a closed
	   sphere.  for an open one, a base reading is about 2300.
	   when a hand is near, it's about 2500 or higher (higher value
	   is caused by stronger reflection by the closer object).  */
	if (proximity >= 2500) {
		printf("Returning key pressed true\n");
		return 1;
	} else {
		printf("Returning key pressed false\n");
		return 0;
	}
}

void board_fbt_start(void)
{
	/* get avr out of reset animation because it consumes a lot of power
	 * and can overheat the device if we're in fastboot mode because
	 * there is no smartreflex code in the bootloader.
	 */
	struct avr_led_set_all_vals vals = {
		.rgb[0] = 5, .rgb[1] = 5, .rgb[2] = 5
	};
	detect_avr();
	udelay(100);
	avr_led_set_all_vals(&vals);
	avr_led_commit_led_state(AVR_LED_COMMMIT_IMMEDIATELY);
}

void board_fbt_end(void)
{
	/* get avr out of reset animation because it consumes a lot of power
	 * and can overheat the device if we're in fastboot mode because
	 * there is no smartreflex code in the bootloader.
	 */
	struct avr_led_set_all_vals vals = {
		.rgb[0] = 0, .rgb[1] = 0, .rgb[2] = 0
	};
	avr_led_set_all_vals(&vals);
	avr_led_commit_led_state(AVR_LED_COMMMIT_IMMEDIATELY);
}

struct fbt_partition {
	const char *name;
	unsigned size_kb;
};

/* For the 16GB eMMC part used in Tungsten, the erase group size is 512KB.
 * So every partition should be at least 512KB to make it possible to use
 * the mmc erase operation when doing 'fastboot erase'.
 * However, the xloader is an exception because in order for the OMAP4 ROM
 * bootloader to find it, it must be at offset 0KB, 128KB, 256KB, or 384KB.
 * Since the partition table is at 0KB, we choose 128KB.  Special care
 * must be kept to prevent erase the partition table when/if the xloader
 * partition is being erased.
 */
struct fbt_partition fbt_partitions[] = {
	{ "--ptbl+pad", 128 },  /* partition table is sector 0-34,
				 * rest is padding to make the xloader
				 * start at the next sector that the ROM
				 * bootloader will look, which is at
				 * offset 128KB into the eMMC.
				 */
	{ "xloader", 384 },	/* pad out to fill whole erase group */
	{ "bootloader", 512 },  /* u-boot, one erase group in size */
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

void board_fbt_finalize_bootargs(char* args, size_t buf_sz) {
	int used = strlen(args);
	int i;

	for (i = 0; i < ARRAY_SIZE(mac_defaults); ++i) {
		u8 m[6];
		char mac[18];

		if (strstr(args, mac_defaults[i].name))
			continue;

		generate_default_mac_addr(mac_defaults[i].salt, m);
		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
				m[5], m[4], m[3], m[2], m[1], m[0]);
		mac[sizeof(mac) - 1] = 0;
		used += snprintf(args + used,
				buf_sz - used,
				" %s=%s",
				mac_defaults[i].name,
				mac);
	}

	args[buf_sz-1] = 0;
}

#ifdef CONFIG_MFG
static void set_default_mac_env_vars() {
	int i;

	for (i = 0; i < ARRAY_SIZE(mac_defaults); ++i) {
		u8 m[6];
		char tmp_buf[18];
		generate_default_mac_addr(mac_defaults[i].salt, m);
		snprintf(tmp_buf, sizeof(tmp_buf),
			"%02x:%02x:%02x:%02x:%02x:%02x",
			m[5], m[4], m[3], m[2], m[1], m[0]);
		tmp_buf[sizeof(tmp_buf) - 1] = 0;
		setenv(mac_defaults[i].name, tmp_buf);
	}
}
#endif

int board_late_init(void)
{
	char tmp_buf[17];
	u64 id_64;

#if 0
  extern int do_mmcinfo (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
  do_mmcinfo(NULL, 0, 0, NULL);
#endif

	dieid_num_r();

	generate_default_64bit_id(serial_no_salt, &id_64);
	snprintf(tmp_buf, sizeof(tmp_buf), "%016llx", id_64);
	tmp_buf[sizeof(tmp_buf)-1] = 0;
	setenv("fbt_id#", tmp_buf);

#ifdef CONFIG_MFG
	set_default_mac_env_vars();
#endif

	fbt_preboot();

	return 0;
}

struct gpio_name_mapping {
	const char* name;
	int num;
};

int name_to_gpio(const char* name) {
	static const struct gpio_name_mapping map[] = {
		{ "aud_intfc_en",	40 },
		{ "hdmi_ls_oe",		41 },
		{ "aud_rstn",		42 },
		{ "wlan_en",		43 },
		{ "aud_pdn",		44 },
		{ "bt_host_wake_bt",	45 },
		{ "bt_en",		46 },
		{ "bt_wakeup_host",	47 },
		{ "ui_avr_rst_n_a",	48 },
		{ "ui_avr_int_n",	49 },
		{ "bt_rst_n",		52 },
		{ "wlan_irq_n",		53 },
		{ "hdmi_ct_cp_hpd",	60 },
		{ "hdmi_hpd",		63 },
		{ "hdmi_cec",		64 },
		{ "spdif",		111 },
		{ "nfc_dl_mode",	162 },
		{ "nfc_en",		163 },
		{ "nfc_irq",		164 },
	};
	int i;
	const char* tmp;

	for (i = 0; i < ARRAY_SIZE(map); ++i) {
		if (!strcmp(name, map[i].name))
			return map[i].num;
	}

	for (tmp = name; *tmp; ++tmp)
		if ((*tmp < '0') || (*tmp > '9'))
			return -1;

	return simple_strtoul(name, NULL, 10);
}

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
#include <asm/gpio.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mmc_host_def.h>
#include <fastboot.h>
#include <mmc.h>
#include <malloc.h>
#include <linux/string.h>

#include "steelhead_avr.h"
#include "steelhead_avr_regs.h"
#include "tungsten_mux_data.h"
#include "pseudorandom_ids.h"

DECLARE_GLOBAL_DATA_PTR;

#define STEELHEAD_REV_ALPHA   0x0
#define STEELHEAD_REV_EVT     0x1
#define STEELHEAD_REV_EVT2    0x2
#define STEELHEAD_REV_DVT     0x3
#define STEELHEAD_REV_DVT1_5  0x4
#define STEELHEAD_REV_DVT2    0x5

static const char const *steelhead_hw_name[] = {
	[STEELHEAD_REV_ALPHA]  = "Steelhead ALPHA",
	[STEELHEAD_REV_EVT]    = "Steelhead EVT",
	[STEELHEAD_REV_EVT2]   = "Steelhead EVT2",
	[STEELHEAD_REV_DVT]    = "Steelhead DVT",
	[STEELHEAD_REV_DVT1_5] = "Steelhead DVT1.5",
	[STEELHEAD_REV_DVT2]   = "Steelhead DVT2",
};
int hwrev_gpios[] = {
	182, /* board_id_0 */
	101, /* board_id_1 */
	171, /* board_id_2 */
};
int steelhead_hw_rev;
int avr_detected;

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
	{ MAKE_SALT('B','l','u','T'), "board_steelhead_bluetooth.btaddr" },
};
static const u32 serial_no_salt = MAKE_SALT('S','e','r','#');
#undef MAKE_SALT

static const char *steelhead_hw_rev_name(void)
{
	int num = ARRAY_SIZE(steelhead_hw_name);

	if (steelhead_hw_rev >= num ||
	    !steelhead_hw_name[steelhead_hw_rev])
		return "Steelhead unknown version";

	return steelhead_hw_name[steelhead_hw_rev];
}

static void init_hw_rev(void)
{
	int i;

	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array_non_essential,
		   sizeof(core_padconf_array_non_essential) /
		   sizeof(struct pad_conf_entry));

	steelhead_hw_rev = 0;

	for (i = 0; i < ARRAY_SIZE(hwrev_gpios); i++)
		steelhead_hw_rev |= gpio_get_value(hwrev_gpios[i]) << i;

	/* put board_id pins into safe mode to save power */
	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array_disable_board_id,
		   sizeof(core_padconf_array_disable_board_id) /
		   sizeof(struct pad_conf_entry));

	printf("Steelhead HW revision: %02x (%s)\n", steelhead_hw_rev,
		steelhead_hw_rev_name());

}

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
	init_hw_rev();
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
	struct mmc *mmc;
	int err;

	omap_mmc_init(CONFIG_MMC_DEV);

	mmc = find_mmc_device(CONFIG_MMC_DEV);
	if (!mmc) {
		printf("mmc device not found!!\n");
		/* Having mmc_initialize() invoke cpu_mmc_init() won't help. */
		return 0;
	}

	err = mmc_init(mmc);
	if (err)
		printf("mmc init failed: err - %d\n", err);

	return 0;
}
#endif

static const struct avr_led_rgb_vals green = {
	.rgb[0] = 0, .rgb[1] = 50, .rgb[2] = 0
};
static const struct avr_led_rgb_vals black = {
	.rgb[0] = 0, .rgb[1] = 0, .rgb[2] = 0
};

int board_fbt_key_pressed(void)
{
	int is_pressed = 0;
	u8 key_code;

	avr_detected = !detect_avr();
	if (!avr_detected) {
		printf("%s: avr not detected, returning false\n", __func__);
		return is_pressed;
	}

	/* check for the mute key to be pressed as an indicator
	 * to enter fastboot mode in preboot mode
	 */
	while (1) {
		if (avr_get_key(&key_code))
			break;
		if (key_code == AVR_KEY_EVENT_EMPTY)
			break;
		if (key_code == (AVR_KEY_MUTE | AVR_KEY_EVENT_DOWN)) {
			/* got mute key down, wait for release */
			int was_green = 1;

			avr_led_set_mode(AVR_LED_MODE_HOST_AUTO_COMMIT);
			avr_led_set_all(&green);
			avr_led_set_mute(&black);
			is_pressed = 1;

			/* spin until key is released, but flash
			 * something on LEDs so user knows we're alive
			 */
			do {
				if (avr_get_key(&key_code))
					break;
				if (was_green) {
					avr_led_set_all(&black);
					was_green = 0;
				} else {
					avr_led_set_all(&green);
					was_green = 1;
				}
				/* wait 100ms to prevent polling
				   too fast, otherwise we can
				   get NAK from AVR
				*/
				udelay(100000);
			} while (key_code & AVR_KEY_EVENT_DOWN);
			avr_led_set_all(&green);
			avr_led_set_mute(&green);
			break;
		}
	}
	/* On a cold boot, the AVR boots up into a boot animation
	 * state automatically.  However, during a OMAP warm reset, the
	 * AVR isn't notified of the reset so we need to make sure
	 * the AVR is in boot animation state.  Alternatively, we could
	 * toggle the gpio reset pin.
	 */
	if (!is_pressed)
		avr_led_set_mode(AVR_LED_MODE_BOOT_ANIMATION);

	printf("Returning key pressed %s\n", is_pressed ? "true" : "false");
	return is_pressed;
}

static unsigned long key_pressed_start_time;
static unsigned long last_time;
#define KEY_CHECK_POLLING_INTERVAL_MS 100
#define RECOVERY_KEY_HOLD_TIME_SECS 10

/* we only check for a long press of mute as an indicator to
 * go into recovery.  due to i2c errors if we poll too fast,
 * we only poll every 100ms right now.
 */
enum fbt_reboot_type board_fbt_key_command(void)
{
	unsigned long time_elapsed;

	if (!avr_detected)
		return FASTBOOT_REBOOT_NONE;

	time_elapsed = get_timer(last_time);
	if (time_elapsed > KEY_CHECK_POLLING_INTERVAL_MS) {
		u8 key_code;

		last_time = get_timer(0);

		if (avr_get_key(&key_code))
			return FASTBOOT_REBOOT_NONE;

		if (key_code == (AVR_KEY_MUTE | AVR_KEY_EVENT_DOWN)) {
			/* key down start */
			key_pressed_start_time = last_time;
			printf("%s: mute key down starting at time %lu\n",
			       __func__, key_pressed_start_time);
		} else if (key_code == AVR_KEY_MUTE) {
			/* key down end, before hold time satisfied */
			printf("%s: mute key released within %lu ms\n",
			       __func__, last_time - key_pressed_start_time);
			key_pressed_start_time = 0;
			avr_led_set_all(&green);
		} else if (key_pressed_start_time) {
			unsigned long time_down;
			time_down = last_time - key_pressed_start_time;

			if (time_down > (RECOVERY_KEY_HOLD_TIME_SECS * 1000)) {
				printf("%s: mute key down more than %u seconds,"
				       " starting recovery\n",
				       __func__, RECOVERY_KEY_HOLD_TIME_SECS);
				return FASTBOOT_REBOOT_RECOVERY;
			}
			printf("%s: mute key still down after %lu ms\n",
			       __func__, time_down);
			/* toggle led ring green and black while down
			   to give user some feedback */
			if ((time_down / KEY_CHECK_POLLING_INTERVAL_MS) & 1)
				avr_led_set_all(&green);
			else
				avr_led_set_all(&black);
		}
	}
	return FASTBOOT_REBOOT_NONE;
}

void board_fbt_start(void)
{
	/* get avr out of boot animation because it consumes a lot of power
	 * and can overheat the device if we're in fastboot mode because
	 * there is no smartreflex code in the bootloader.
	 */
	if (detect_avr() == 0) {
		avr_led_set_mode(AVR_LED_MODE_HOST_AUTO_COMMIT);
		avr_led_set_mute(&green);
		avr_led_set_all(&green);
	}
}

void board_fbt_end(void)
{
	if (avr_detected) {
		/* to match spec, put avr back into boot animation mode. */
		avr_led_set_mode(AVR_LED_MODE_BOOT_ANIMATION);
	}
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
	{ "--ptable",  17},     /* partition table in first 34 sectors */
	{ "environment", 111 },  /* partition used to u-boot environment,
				  * which is also where we store
				  * oem lock/unlock state.  size
				  * must match CONFIG_ENV_SIZE.
				  */
	{ "xloader", 384 },	/* must start at 128KB offset into eMMC
				 * for ROM bootloader to find it.
				 * pad out to fill whole erase group */
	{ "bootloader", 512 },  /* u-boot, one erase group in size */
	{ "device_info", 512 }, /* device specific info like MAC addresses.
				 * read-only once it has been written to.
				 * bootloader parses this at boot and sends
				 * the contents to the kernel via cmdline args.
				 */
	{ "bootloader2", 512 }, /* u-boot, alternate copy */
	{ "misc", 512 }, 	/* misc partition used by recovery for storing
				 * parameters in the case of a power failure
				 * during recovery operation.
				 */
	{ "recovery", 8*1024 },
	{ "boot", 8*1024 },
	{ "efs", 8*1024 },      /* for factory programmed encryption keys,
				 * minimum size for a ext4 fs is about 8MB
				 */
	{ "system", 1024*1024 },
	{ "cache", 512*1024 },
	{ "userdata", 0},
	{ 0, 0 },
};

void board_fbt_finalize_bootargs(char* args, size_t buf_sz) {
	int used = strlen(args);
	int i;
	int bgap_threshold_t_hot  = 83000; /* 83 deg C */
	int bgap_threshold_t_cold = 76000; /* 76 deg C */

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

	/* Add board_id */
	used += snprintf(args + used,
			 buf_sz - used,
			 " board_steelhead.steelhead_hw_rev=%d",
			 steelhead_hw_rev);

	/* Add bandgap threshold temperature based on board_id.
	 * Units before DVT2 didn't have a thermal rework so
	 * we'll throttle at a lower temperature to try to
	 * prevent damage to the OMAP.
	 */
	if (steelhead_hw_rev < STEELHEAD_REV_DVT2) {
		bgap_threshold_t_hot  = 64000; /* 64 deg C */
		bgap_threshold_t_cold = 61000; /* 61 deg C */
	}
	snprintf(args + used,
		 buf_sz - used,
		 " omap_temp_sensor.bgap_threshold_t_hot=%d"
		 " omap_temp_sensor.bgap_threshold_t_cold=%d",
		 bgap_threshold_t_hot, bgap_threshold_t_cold);

	args[buf_sz-1] = 0;
}

#ifdef CONFIG_MFG
static void set_default_mac_env_vars(void)
{
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

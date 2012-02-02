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

enum steelhead_rev {
	STEELHEAD_REV_ALPHA  = 0x0,
	STEELHEAD_REV_EVT    = 0x1,
	STEELHEAD_REV_EVT2   = 0x2,
	STEELHEAD_REV_DVT    = 0x3,
	STEELHEAD_REV_DVT1_5 = 0x4,
	STEELHEAD_REV_DVT2   = 0x5,
	STEELHEAD_REV_DVT3   = 0x6,
	STEELHEAD_REV_DVT4   = 0x7,
	STEELHEAD_REV_PVT    = 0x8,
	STEELHEAD_REV_PROD   = 0x9,
	STEELHEAD_REV_PROD1  = 0xA,
};

static const char const *steelhead_hw_name[] = {
	[STEELHEAD_REV_ALPHA]  = "Steelhead ALPHA",
	[STEELHEAD_REV_EVT]    = "Steelhead EVT",
	[STEELHEAD_REV_EVT2]   = "Steelhead EVT2",
	[STEELHEAD_REV_DVT]    = "Steelhead DVT",
	[STEELHEAD_REV_DVT1_5] = "Steelhead DVT1.5",
	[STEELHEAD_REV_DVT2]   = "Steelhead DVT2",
	[STEELHEAD_REV_DVT3]   = "Steelhead DVT3",
	[STEELHEAD_REV_DVT4]   = "Steelhead DVT4",
	[STEELHEAD_REV_PVT]    = "Steelhead PVT",
	[STEELHEAD_REV_PROD]   = "Steelhead PROD",
	[STEELHEAD_REV_PROD1]  = "Steelhead PROD1",
};
int hwrev_gpios[] = {
	182, /* board_id_0 */
	101, /* board_id_1 */
	171, /* board_id_2 */
};
/* We have 3 bits of board-id to track revision.  Older
 * revisions started to get deprecated and their board-id
 * values reused, so we use a mapping table to converet
 * the raw board-id values to the enum values.
 */
static const enum steelhead_rev board_id_to_steelhead_rev[8] = {
	STEELHEAD_REV_PVT,    /* board_id: 0x0 */
	STEELHEAD_REV_PROD,   /* board_id: 0x1 */
	STEELHEAD_REV_PROD1,  /* board_id: 0x2 */
	STEELHEAD_REV_DVT,    /* board_id: 0x3 */
	STEELHEAD_REV_DVT1_5, /* board_id: 0x4 */
	STEELHEAD_REV_DVT2,   /* board_id: 0x5 */
	STEELHEAD_REV_DVT3,   /* board_id: 0x6 */
	STEELHEAD_REV_DVT4    /* board_id: 0x7 */
};

enum steelhead_rev steelhead_hw_rev;
int avr_detected;

static unsigned long key_pressed_start_time;
static unsigned long last_time;
#define KEY_CHECK_POLLING_INTERVAL_MS 100
#define RECOVERY_KEY_HOLD_TIME_SECS 10

static int force_fastboot = 0;

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
	int board_id;

	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array_non_essential,
		   sizeof(core_padconf_array_non_essential) /
		   sizeof(struct pad_conf_entry));

	board_id = 0;

	for (i = 0; i < ARRAY_SIZE(hwrev_gpios); i++)
		board_id |= gpio_get_value(hwrev_gpios[i]) << i;

	/* put board_id pins into safe mode to save power */
	do_set_mux(CONTROL_PADCONF_CORE, core_padconf_array_disable_board_id,
		   sizeof(core_padconf_array_disable_board_id) /
		   sizeof(struct pad_conf_entry));

	/* not absolutely necessary but good in case the size of the
	   array ever changes */
	if (board_id < ARRAY_SIZE(board_id_to_steelhead_rev)) {
		steelhead_hw_rev = board_id_to_steelhead_rev[board_id];
		printf("HW revision: 0x%x = \"%s\" (board_id 0x%x)\n",
		       steelhead_hw_rev, steelhead_hw_rev_name(), board_id);
	} else {
		/* default to the highest rev we know of */
		steelhead_hw_rev = STEELHEAD_REV_PROD1;
		printf("board_id 0x%x invalid, setting steelhead_hw_rev to "
		       "0x%x = \"%s\"", board_id,
		       steelhead_hw_rev, steelhead_hw_rev_name());
	}
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

	printf("cid = %x%08x%08x%08x\n", mmc->cid[0], mmc->cid[1],
	       mmc->cid[2], mmc->cid[3]);
	if ((mmc->cid[0] == 0x1501004d) &&
	    (mmc->cid[1] == 0x41473446) &&
	    (mmc->cid[2] == 0x4112eb16) &&
	    (mmc->cid[3] == 0xa1cbae11)) {
		/* Unfortunately, we have units deployed with old
		 * eMMC firmware (the cid we check for is specific
		 * to the known bad eMMC firmware, there may be one
		 * or more good versions).  Units with this eMMC firmware
		 * eventually stop erasing.  Instead of letting users 
		 * keep using them until they fail and then reporting 
		 * an issue, force a stop if we detect this old firmware
		 * and force them to update right away instead of getting
		 * a constant trickle of these failed units coming in one
		 * at a time.
		 */
		printf("\teMMC firmware version is bad, must update\n");
		force_fastboot = 1;
	} else {
		printf("\teMMC firmware version okay\n");
	}

	return 0;
}
#endif

static const struct avr_led_rgb_vals red = {
	.rgb[0] = 128, .rgb[1] = 0, .rgb[2] = 0
};
static const struct avr_led_rgb_vals black = {
	.rgb[0] = 0, .rgb[1] = 0, .rgb[2] = 0
};

int board_fbt_key_pressed(void)
{
	int is_pressed = 0;
	u8 key_code;
	unsigned long start_time = get_timer(0);

#define DETECT_AVR_DELAY_MSEC 2000 /* 2 seconds */

	/* If we power up with USB cable connected, the ROM bootloader
	 * delays the OMAP boot long enough (as it checks for peripheral
	 * boot) that the AVR will be ready at this point for us to
	 * query.  If we power up with no USB cable, we will most likely
	 * be here before the AVR is ready.  If we don't detect
	 * the AVR right away, sleep a few seconds and try again.
	 * We don't just poll until the AVR can respond because even
	 * after we detect the AVR, it might not quite be ready to
	 * do key detection so need to wait a bit more.
	 */
	avr_detected = !detect_avr();
	if (!avr_detected) {
		printf("\tavr not detected\n");
		printf("\tdelaying %d milliseconds until we try again\n",
			DETECT_AVR_DELAY_MSEC);
		while((get_timer(0) - start_time) < DETECT_AVR_DELAY_MSEC)
			; /* spin on purpose */
		avr_detected = !detect_avr();
	}
	if (!avr_detected) {
		/* This might happen if avr_updater got interrupted
		 * while an avr firmware update was in progress.
		 * It's better to allow regular booting instead of
		 * stopping in fastboot mode because the OS might
		 * be able to recovery it by doing the update again.
		 * It's also not good to stop in fastboot because we
		 * can't use the LEDs to indicate to the user we're
		 * in this state.
		 */
		printf("%s: avr not detected, returning false\n", __func__);
		return 0;
	}

	/* If we got here from a warm reset, AVR could be in some
	 * other state than host mode so just make sure it is
	 * in host mode.
	 */
	avr_led_set_mode(AVR_LED_MODE_HOST_AUTO_COMMIT);

	if (force_fastboot) {
		printf("Forcing fastboot\n");
		return 1;
	}

	/* check for the mute key to be pressed as an indicator
	 * to enter fastboot mode in preboot mode.  since the
	 * AVR sends an initial boot indication key, we have to
	 * filter that out first.  we also filter out and volume
	 * up/down keys and don't care if we spin until those
	 * stop (it's almost impossible to make the volume
	 * up/down key events repeat indefinitely since they
	 * involve actually rotating the top of the sphere
	 * without pause).
	 */
	while (1) {
		if (avr_get_key(&key_code))
			break;
		if (key_code == AVR_KEY_EVENT_EMPTY)
			break;
		if (key_code == (AVR_KEY_MUTE | AVR_KEY_EVENT_DOWN)) {
			avr_led_set_all(&red);
			avr_led_set_mute(&red);
			is_pressed = 1;
			key_pressed_start_time = get_timer(0);
			/* don't wait for key release */
			break;
		}
	}

	/* All black to indicate we've made our decision to boot. */
	if (!is_pressed) {
		serial_printf("\tsetting to black\n");
		avr_led_set_all(&black);
		avr_led_set_mute(&black);
	}

	printf("Returning key pressed %s\n", is_pressed ? "true" : "false");
	return is_pressed;
}

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
			avr_led_set_all(&red);
		} else if (key_pressed_start_time) {
			unsigned long time_down;
			time_down = last_time - key_pressed_start_time;

			if (time_down > (RECOVERY_KEY_HOLD_TIME_SECS * 1000)) {
				printf("%s: mute key down more than %u seconds,"
				       " starting recovery\n",
				       __func__, RECOVERY_KEY_HOLD_TIME_SECS);
				return FASTBOOT_REBOOT_RECOVERY_WIPE_DATA;
			}
			printf("%s: mute key still down after %lu ms\n",
			       __func__, time_down);
			/* toggle led ring red and black while down
			   to give user some feedback */
			if ((time_down / KEY_CHECK_POLLING_INTERVAL_MS) & 1)
				avr_led_set_all(&red);
			else
				avr_led_set_all(&black);
		}
	}
	return FASTBOOT_REBOOT_NONE;
}

void board_fbt_start(void)
{
	/* in case we entered fastboot by request from ADB or other
	 * means that we couldn't detect in board_fbt_key_command(),
	 * make sure the LEDs are set to red to indicate fastboot mode
	 */
	if (avr_detected) {
		avr_led_set_mute(&red);
		avr_led_set_all(&red);
	}
}

void board_fbt_end(void)
{
	if (avr_detected) {
		/* to match spec, put avr into boot animation mode. */
		avr_led_set_mode(AVR_LED_MODE_BOOT_ANIMATION);
	}
}

struct fbt_partition {
	const char *name;
	const char *type;
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
	{ "--ptable", NULL,  17},  /* partition table in
					* first 34 sectors */
	{ "environment", "raw", 95 },  /* partition used to u-boot environment,
					* which is also where we store
					* oem lock/unlock state.  size
					* must match CONFIG_ENV_SIZE.
					*/
	{ "crypto", "raw", 16},        /* 16KB partition for crypto keys.
					* used when userdata is encrypted.
					*/
	{ "xloader", "raw", 384 },	/* must start at 128KB offset into eMMC
					 * for ROM bootloader to find it.
					 * pad out to fill whole erase group */
	{ "bootloader", "raw", 512 },  /* u-boot, one erase group in size */
	{ "device_info", "raw", 512 }, /* device specific info like MAC
					* addresses.  read-only once it has
					* been written to.  bootloader parses
					* this at boot and sends the contents
					* to the kernel via cmdline args.
					*/
	{ "bootloader2", "raw", 512 }, /* u-boot, alternate copy */
	{ "misc", "raw", 512 }, 	/* misc partition used by recovery for
					 * storing parameters in the case of a
					 * power failure during recovery
					 * operation.
					 */
	{ "recovery", "boot", 8*1024 },
	{ "boot", "boot", 8*1024 },
	{ "efs", "ext4", 8*1024 },      /* for factory programmed keys,
					 * minimum size for a ext4 fs is
					 * about 8MB
					 */
	{ "system", "ext4", 1024*1024 },
	{ "cache", "ext4", 512*1024 },
	{ "userdata", "ext4", 0},
	{ 0, 0, 0 },
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

	/* this is called just before booting normal image.  we
	 * use opportunity to start boot animation.
	 */
	board_fbt_end();
}

struct TOC_entry {
	uint32_t start_offset;
	uint32_t size;
	uint8_t reserved[8];
	uint32_t load_address;
	char filename[12];
};

#define NUM_MPKH_REGISTERS 8
struct MPKH_info {
	char tag[4];
	uint32_t mpkh[NUM_MPKH_REGISTERS];
};

int board_fbt_handle_flash(disk_partition_t *ptn,
			   struct cmd_fastboot_interface *priv)
{
	struct TOC_entry *toc_p;
	void *end_ptr;
	void *image_ptr;
	struct MPKH_info *mpkh_ptr;
	struct MPKH_info my_mpkh;
	struct ctrl_id *ctrl;
	int i;

	/* We support flashing a MLO "package" that has more than one
	 * MLO image, each signed with a different MPK for a different
	 * HS part.  We check if the MLO image has a MPKH_info structure
	 * appended to it that has the tag, and the 8 MPKH values for
	 * us to compare against our own MPKH register.  If no MPKH_info,
	 * we'll just go ahead and accept it presuming the user knows
	 * what they're doing or it's an old MLO file.  If the MPKH_info
	 * structure exists, we'll compare the MPK info and if no
	 * match, we'll try the next image in the downloaded file if
	 * there is one.
	 */
	if ((get_device_type() != HS_DEVICE) ||
	    (strcmp((char *)ptn->name, "xloader"))) {
		/* not an HS part, or not flashing the xloader, just
		 * go ahead and flash it.
		 */
		return 0;
	}


	/* read the MPKH registers into a local struct for easy comparison
	 * with the MKPH_info we expect to find appended to the end
	 * of the MLO image, if any.
	 */
	ctrl = (struct ctrl_id *)CTRL_BASE;
	my_mpkh.tag[0] = 'M';
	my_mpkh.tag[1] = 'P';
	my_mpkh.tag[2] = 'K';
	my_mpkh.tag[3] = 'H';
	for (i = 0; i < NUM_MPKH_REGISTERS; i++) {
		my_mpkh.mpkh[i] = ((uint32_t*)&ctrl->core_std_fuse_mpk_0)[i];
	}

	end_ptr = priv->transfer_buffer + priv->d_bytes;
	image_ptr = priv->transfer_buffer;

	printf("Verifying xloader image before flashing\n");
	do {
		printf("Checking image at offset 0x%x... ",
		       image_ptr - (void*)priv->transfer_buffer);
		toc_p = (struct TOC_entry *)image_ptr;
		if ((void*)(toc_p + 1) >= end_ptr) {
			printf("Image too small, not flashing\n");
			snprintf(priv->response, sizeof(priv->response),
				 "FAILImage too small\n");
			return -1;
		}
		if (strcmp("MLO", toc_p->filename)) {
			/* downloaded image does not have the MLO as the first
			 * TOC entry like we expect, or is too small,
			 * return error
			 */
			printf("Not an MLO image, not flashing\n");
			snprintf(priv->response, sizeof(priv->response),
				"FAILxloader requires MLO image");
			return -1;
		}
		/* check for special case of a simple MLO image with
		 * no MPKH_info appended to it.
		 */
		if (toc_p->start_offset + toc_p->size == priv->d_bytes) {
			printf("    No MPKH_info in image, "
			       "accepting unconditionally\n");
			return 0;
		}
		mpkh_ptr = (struct MPKH_info *)(image_ptr +
						toc_p->start_offset +
						toc_p->size);
		if ((void*)(mpkh_ptr + 1) <= end_ptr) {
			if (memcmp(&my_mpkh, mpkh_ptr, sizeof(my_mpkh)) == 0) {
				printf("    MPKH match, using this image\n");
				priv->image_start_ptr = image_ptr;
				priv->d_bytes = (toc_p->start_offset +
						 toc_p->size);
				return 0;
			}
			printf("    MPKH mismatch, not using this image\n");
#ifdef DEBUG
			for (i = 0; i < NUM_MPKH_REGISTERS; i++) {
				printf("MPKH[%d]: 0x%08x %s 0x%08x\n",
				       i, my_mpkh.mpkh[i],
				       (my_mpkh.mpkh[i] == mpkh_ptr->mpkh[i]) ?
				       "==" : "!=", mpkh_ptr->mpkh[i]);
			}
#endif
		}
		/* go to the next image, if there is one */
		image_ptr += (toc_p->start_offset + toc_p->size +
			      sizeof(*mpkh_ptr));
	} while (image_ptr < end_ptr);
	snprintf(priv->response, sizeof(priv->response),
		 "FAILMLO verification failed");
	return -1;
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

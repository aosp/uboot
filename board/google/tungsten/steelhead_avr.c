/* board/google/tungsten/steelhead_avr.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This is a driver that communicates with an Atmel AVR ATmega328P
 * subboard in the Android@Home device via gpios and i2c.  This subboard
 * is Arduino-Compatible and the firmware in it is developed using the
 * Arduino SDK.
 *
 * The functionality implemented by the subboard is a set of capacitive touch
 * keys and many leds.  To keep things simple for now, we have just
 * one driver that implements two input_device exposing the keys and
 * a misc_device exposing custom ioctls for controlling the leds.  We don't
 * use the Linux led driver API because we have too many leds and want
 * a more custom API to be more efficient.  Also, the subboard firmware
 * implements some macro led modes (like volume mode) which doesn't make
 * sense in the led API.
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <i2c.h>
#include <malloc.h>
#include <errno.h>
#include <fastboot.h>
#include <linux/ctype.h>

#include "steelhead_avr.h"
#include "steelhead_avr_regs.h"

#define LED_BYTE_SZ 3

#define AVR_I2C_CLIENT_ID (0x20) /* 7 bit i2c id */
#define AVR_I2C_BUS_ID    (0x1)

static struct avr_driver_state {
	int avr_detected;

	/* device info */
	u16 firmware_rev;
	u8  hardware_type;
	u8  hardware_rev;
	u16 led_count;

	/* Current LED state. */
	u8 led_mode;
	u8 mute_threshold;
} state;

/* the AVR can't do a normal i2c_read() with the
 * register address passed in one transfer.
 * it needs a delay between sending the register
 * address and the read transfer.
 */
static int avr_i2c_read(u8 cmd, u16 len, u8 *buf)
{
	int rc = i2c_write(AVR_I2C_CLIENT_ID, cmd, 1, NULL, 0);
	if (rc)
		return rc;

	/* Need to wait a little bit between the write of the register ID
	 * and the read of the actual data.  Failure to do so will not
	 * result in a NAK, only corrupt data.
	 */
	udelay(50);

	rc = i2c_read(AVR_I2C_CLIENT_ID, 0, 0, buf, len);
	return rc;
}

static int avr_get_firmware_rev(void)
{
	u8 buf[2];
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	rc = avr_i2c_read(AVR_FW_VERSION_REG_ADDR, sizeof(buf), buf);

	state.firmware_rev = ((u16)buf[0] << 8) | (u16)buf[1];
	return rc;
}

static int avr_get_hardware_type(void)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	return avr_i2c_read(AVR_HW_TYPE_REG_ADDR, 1, &state.hardware_type);
}

static int avr_get_hardware_rev(void)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	return avr_i2c_read(AVR_HW_REVISION_REG_ADDR, 1, &state.hardware_rev);
}

static int avr_get_led_count(void)
{
	u8 buf[2];
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	rc = avr_i2c_read(AVR_LED_GET_COUNT_ADDR, sizeof(buf), buf);

	state.led_count = ((u16)buf[0] << 8) | (u16)buf[1];
	return rc;
}

int avr_led_set_all(const struct avr_led_rgb_vals *req)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	if (!req)
		return -EFAULT;

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_SET_ALL_REG_ADDR, 1,
		       (uchar *)req->rgb, sizeof(req->rgb));

	return rc;
}

int avr_led_set_mute(const struct avr_led_rgb_vals *req)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	if (!req)
		return -EFAULT;

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_SET_MUTE_ADDR, 1,
		       (uchar *)req->rgb, sizeof(req->rgb));

	return rc;
}

int avr_set_mute_threshold(u8 mute_threshold)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_KEY_MUTE_THRESHOLD_REG_ADDR,
		       1, &mute_threshold, sizeof(mute_threshold));

	return rc;
}

static int avr_get_mute_threshold(void)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	rc = avr_i2c_read(AVR_KEY_MUTE_THRESHOLD_REG_ADDR,
			  1, &state.mute_threshold);

	return rc;
}

int avr_led_set_range(struct avr_led_set_range_vals *req)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	if (!req)
		return -EFAULT;
	printf("Sending i2c set range packet, %d bytes\n", 3 + (req->rgb_triples * 3));
	{
		int i;
		for (i = 0; i < (3 + (req->rgb_triples * 3)); i++) {
			printf("0x%x\n", ((uint8_t*)req)[i]);
		}
	}
	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_SET_RANGE_REG_ADDR, 1,
		       (uint8_t*)req, 3 + (req->rgb_triples * 3));

	return rc;
}

int avr_led_set_mode(u8 mode)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_MODE_REG_ADDR,
		       1, &mode, 1);
	/* If the command failed, then skip the update of our internal
	 * bookkeeping and just get out.
	 */
	if (rc)
		return rc;

	state.led_mode = mode;
	return rc;
}

int avr_led_commit_led_state(u8 val)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	return i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_COMMIT_REG_ADDR,
			 1, &val, 1);
}

static int avr_read_event_fifo(u8 *next_event)
{
	if (!next_event)
		return -EFAULT;

	return avr_i2c_read(AVR_KEY_EVENT_FIFO_REG_ADDR, 1, next_event);
}

int detect_avr(void)
{
	int rc;
	struct avr_led_rgb_vals clear_led_req;

	printf("%s\n", __func__);

	rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		goto error;
	}

	/* Cache the firmware revision (also checks to be sure the AVR is
	 * actually there and talking to us).
	 */
	rc = avr_get_firmware_rev();
	if (rc) {
		printf("Failed to fetch AVR firmware revision (rc = %d)\n", rc);
		goto error;
	}

	/* Cache the hardware type and revision.
	 */
	rc = avr_get_hardware_type();
	if (rc) {
		printf("Failed to fetch AVR hardware type (rc = %d)\n", rc);
		goto error;
	}

	rc = avr_get_hardware_rev();
	if (rc) {
		printf("Failed to fetch AVR hardware revision (rc = %d)\n", rc);
		goto error;
	}

	rc = avr_get_led_count();
	if (rc) {
		printf("Failed to fetch AVR led count (rc = %d)\n", rc);
		goto error;
	}

	/* Set the LED state to all off in order to match the internal state we
	 * just established.
	 */
	clear_led_req.rgb[0] = 0x00;
	clear_led_req.rgb[1] = 0x00;
	clear_led_req.rgb[2] = 0x00;
	rc = avr_led_set_all(&clear_led_req);
	if (rc) {
		printf("Failed to clear LEDs on AVR (rc = %d)\n", rc);
		goto error;
	}

	printf("Steelhead AVR detected and initialized\n");

	state.avr_detected = 1;
	return 0;
 error:
	return rc;
}


static int do_avr_reinit(cmd_tbl_t *cmdtp, int flag,
			 int argc, char * const argv[])
{
	/* force redetect */
	if (detect_avr()) {
		puts("Could not detect avr\n");
		return 1;
	}
	return 0;
}

static int do_avr_commit_state(cmd_tbl_t *cmdtp, int flag,
			 int argc, char * const argv[])
{
	if (avr_led_commit_led_state(1)) {
		printf("Error committing led state\n");
		return 1;
	}
	printf("Succeeded committing led state\n");
	return 0;
}

int avr_get_key(u8 *key_code)
{
	u8 next_event;
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	rc = avr_read_event_fifo(&next_event);
	if (rc) {
		printf("Failed to read event fifo, err = %d\n", rc);
		return 1;
	}
	*key_code = next_event;
	if (next_event != AVR_KEY_EVENT_EMPTY) {
		printf("%s returning 0x%x\n", __func__, next_event);
	}
	return 0;
}

static int do_avr_get_key(cmd_tbl_t *cmdtp, int flag,
			 int argc, char * const argv[])
{
	int saw_key = 0;
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	while (1) {
		u8 next_event;
		rc = avr_get_key(&next_event);
		if (rc)
			return 1;
		if (next_event == AVR_KEY_EVENT_EMPTY) {
			if (saw_key)
				printf("done\n");
			else
				printf("avr returned no key events\n");
			break;
		} else {
			int was_down = next_event & AVR_KEY_EVENT_DOWN;
			u8 key_code = next_event & AVR_KEY_EVENT_CODE_MASK;
			char *key_string[3] = {"UNKNOWN", "VOLUME_UP",
					       "VOLUME_DOWN"};
			saw_key = 1;
			printf("avr returned key raw = %d, '%s' %s\n",
			       next_event,
			       key_code < ARRAY_SIZE(key_string) ?
			       key_string[key_code] : "BAD CODE",
			       was_down ? "down" : "up");
		}
	}
	return 0;
}

static int do_avr_get_info(cmd_tbl_t *cmdtp, int flag,
			   int argc, char * const argv[])
{
	printf("avr info:\n");
	printf("  firmware_rev = %d.%d\n",
	       state.firmware_rev >> 8,
	       state.firmware_rev & 0xFF);
	printf("  hardware_type = %d\n", state.hardware_type);
	printf("  hardware_rev = %d\n", state.hardware_rev);
	printf("  led_mode = %d\n", state.led_mode);
	printf("  led_count = %d\n", state.led_count);
	avr_get_mute_threshold();
	printf("  mute_threshold = %d\n", state.mute_threshold);
	return 0;
}

static int do_avr_set_mode(cmd_tbl_t *cmdtp, int flag,
			   int argc, char * const argv[])
{
	ulong raw;
	const char *mode_names[] = {"BOOT_ANIMATION", "HOST_AUTO_COMMIT", "HOST"};
	if (argc != 2) {
		printf("usage: avr set mode "
		       "[0=boot_animation,1=host_auto_commit,2=host]\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 10);
	if (raw > AVR_LED_MODE_HOST) {
		printf("invalid mode %lu, must be 0 to %d\n",
		       raw, AVR_LED_MODE_HOST);
		return 1;
	}
	if (avr_led_set_mode(raw)) {
		printf("Error setting mode [%lu:%s]\n",
		       raw, mode_names[raw]);
	} else {
		printf("Succeeded setting mode [%lu:%s]\n",
		       raw, mode_names[raw]);
	}
	return 0;
}

static int do_avr_set_all(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	struct avr_led_rgb_vals req;
	ulong raw;
	if (argc != 2) {
		printf("usage: avr set all rgb888_value\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 16);
	if (raw > 0x00ffffff) {
		printf("rgb888_value 0x%lx too large\n", raw);
		return 1;
	}
	req.rgb[0] = (raw >> 16) & 0xff;
	req.rgb[1] = (raw >> 8) & 0xff;
	req.rgb[2] = (raw >> 0) & 0xff;
	if (avr_led_set_all(&req)) {
		printf("Error setting all led values\n");
		return 1;
	}
	printf("Succeeded setting all led values\n");
	return 0;
}

static int do_avr_set_mute(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	struct avr_led_rgb_vals req;
	ulong raw;
	if (argc != 2) {
		printf("usage: avr set mute rgb888_value\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 16);
	if (raw > 0x00ffffff) {
		printf("rgb888_value 0x%lx too large\n", raw);
		return 1;
	}
	req.rgb[0] = (raw >> 16) & 0xff;
	req.rgb[1] = (raw >> 8) & 0xff;
	req.rgb[2] = (raw >> 0) & 0xff;
	if (avr_led_set_mute(&req)) {
		printf("Error setting mute led values\n");
		return 1;
	}
	printf("Succeeded setting mute led values\n");
	return 0;
}

static int do_avr_set_mute_threshold(cmd_tbl_t *cmdtp, int flag,
				     int argc, char * const argv[])
{
	ulong raw;
	if (argc != 2) {
		printf("usage: avr set mute_threshold value\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 10);
	if (raw > 0xff) {
		printf("mute_threshold_value 0x%lx too large\n", raw);
		return 1;
	}
	if (avr_set_mute_threshold((u8)raw)) {
		printf("Error setting mute_threshold\n");
		return 1;
	}
	printf("Succeeded setting mute_threshold\n");
	return 0;
}

/* examples:
  1) to set 4 leds starting at 0 to white, red, green, blue:
    avr set range 0 4 ffffffff000000ff000000ff
  2) to set all first led to green and rest to white:
    avr set range 0 32 00ff00ffffff
*/
static int do_avr_set_range(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	struct avr_led_set_range_vals req;
	ulong start, count, rgb_triples, end;
	ulong value, i;
	char *cp;
	if (argc != 4) {
		printf("usage: avr set range start count rgb888_hex_string\n");
		return 1;
	}
	start = simple_strtoul(argv[1], NULL, 10);
	if (start > state.led_count) {
		printf("start %lu too large, must be between 0 and %u\n",
		       start, state.led_count);
		return 1;
	}
	count = simple_strtoul(argv[2], NULL, 10);
	if (count == 0) {
		printf("0 count invalid\n");
		return 1;
	}
	end = start + count - 1;
	if (end >= state.led_count) {
		printf("count %lu too large, must be between 1 and %lu\n",
		       count, state.led_count - start);
		return 1;
	}
	rgb_triples = strlen(argv[3]);
	/* the length of the string must be a multiple of 6 bytes
	   (two bytes each for red, green, and blue */
	if (rgb_triples % 6) {
		printf("rgb888_hex_string is invalid, must have a multiple of 6 characters,"
		       "two each for red, green, and blue\n");
		return 1;
	}
	rgb_triples /= 6;

	req.start = start;
	req.count = count;
	req.rgb_triples = rgb_triples;
	cp = argv[3];
	for (i = 0; i < rgb_triples; i++) {
		int j;
		for (j = 0; j < 3; j++) {
			value = (isdigit(*cp) ? *cp - '0' : (islower(*cp) ? toupper(*cp) : *cp) - 'A' + 10) * 16;
			cp++;
			value += (isdigit(*cp) ? *cp - '0' : (islower(*cp) ? toupper(*cp) : *cp) - 'A' + 10);
			cp++;

			req.rgb[i][j] = value;
		}
	}
	if (avr_led_set_range(&req)) {
		printf("Error setting range led values\n");
		return 1;
	}
	printf("Succeeded setting range led values\n");
	return 0;
}

static cmd_tbl_t cmd_avr_get_sub[] = {
	U_BOOT_CMD_MKENT(info, 1, 1, do_avr_get_info, "", ""),
	U_BOOT_CMD_MKENT(key, 1, 1, do_avr_get_key, "", ""),
};

static int do_avr_get(cmd_tbl_t *cmdtp, int flag,
			 int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2) {
		printf("Too few arguments\n");
		return cmd_usage(cmdtp);
	}

	/* Strip off leading 'get' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_avr_get_sub[0],
			 ARRAY_SIZE(cmd_avr_get_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

static cmd_tbl_t cmd_avr_set_sub[] = {
	U_BOOT_CMD_MKENT(mode, 2, 1, do_avr_set_mode, "", ""),
	U_BOOT_CMD_MKENT(all, 2, 1, do_avr_set_all, "", ""),
	U_BOOT_CMD_MKENT(mute, 2, 1, do_avr_set_mute, "", ""),
	U_BOOT_CMD_MKENT(mute_threshold, 2, 1, do_avr_set_mute_threshold, "", ""),
	U_BOOT_CMD_MKENT(range, 4, 1, do_avr_set_range, "", ""),
};

static int do_avr_set(cmd_tbl_t *cmdtp, int flag,
			 int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2) {
		printf("Too few arguments\n");
		return cmd_usage(cmdtp);
	}

	/* Strip off leading 'set' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_avr_set_sub[0],
			 ARRAY_SIZE(cmd_avr_set_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

static cmd_tbl_t cmd_avr_sub[] = {
	U_BOOT_CMD_MKENT(reinit, 1, 1, do_avr_reinit, "", ""),
	U_BOOT_CMD_MKENT(commit_state, 1, 1, do_avr_commit_state, "", ""),
	U_BOOT_CMD_MKENT(get, 5, 1, do_avr_get, "", ""),
	U_BOOT_CMD_MKENT(set, 6, 1, do_avr_set, "", ""),
};

#ifdef CONFIG_NEEDS_MANUAL_RELOC
void avr_reloc(void)
{
	fixup_cmdtable(cmd_avr_sub, ARRAY_SIZE(cmd_avr_sub));
	fixup_cmdtable(cmd_avr_get_sub, ARRAY_SIZE(cmd_avr_get_sub));
	fixup_cmdtable(cmd_avr_set_sub, ARRAY_SIZE(cmd_avr_set_sub));
}
#endif

static int do_avr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2) {
		printf("Too few arguments\n");
		return cmd_usage(cmdtp);
	}

	if (!state.avr_detected) {
		if (detect_avr()) {
			puts("Could not detect avr\n");
			return 1;
		}
	}

	/* Strip off leading 'avr' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_avr_sub[0], ARRAY_SIZE(cmd_avr_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

U_BOOT_CMD(
	avr, 6, 1, do_avr,
	"steelhead avr sub system",
	"avr reinit\n"
	"avr commit_state\n"
	"avr get key\n"
	"avr get info\n"
	"avr set mode [0=boot_animation,1=host_auto_commit,2=host]\n"
	"avr set all rgb888_hex\n"
	"avr set mute rgb888_hex\n"
	"avr set mute_threshold value\n"
	"avr set range start count rgb888_hex_string\n");


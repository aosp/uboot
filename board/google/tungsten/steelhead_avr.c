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

#include "steelhead_avr.h"
#include "steelhead_avr_regs.h"

#define LED_BYTE_SZ 3
#define SET_RANGE_OVERHEAD 2

#define AVR_I2C_CLIENT_ID (0x20) /* 7 bit i2c id */
#define AVR_I2C_BUS_ID    (0x1)

struct led_bank_state {
	u8 bank_id;
	u8 led_count;
	const char *name;
	u8 *vals;
};

static struct avr_driver_state {
	int avr_detected;

	/* device info */
	u16 firmware_rev;
	u8  hardware_type;
	u8  hardware_rev;

	/* Current LED state. */
	struct led_bank_state *banks;
	u32 bank_cnt;
	u32 total_led_cnt;
	u8 *bank_scratch;
	u32 bank_scratch_bytes;
	u8 led_mode;
} state;

static const struct led_bank_state sphere_bank_template[] = {
	{	.bank_id = 0,
		.led_count = 8,
		.name = "QuarterArc0",
		.vals = NULL  },
	{	.bank_id = 1,
		.led_count = 8,
		.name = "QuarterArc1",
		.vals = NULL  },
	{	.bank_id = 2,
		.led_count = 8,
		.name = "QuarterArc2",
		.vals = NULL  },
	{	.bank_id = 3,
		.led_count = 8,
		.name = "QuarterArc3",
		.vals = NULL  },
};

static const struct led_bank_state rhombus_bank_template[] = {
	{	.bank_id = 0,
		.led_count = 8,
		.name = "Crack_0",
		.vals = NULL  },
	{	.bank_id = 1,
		.led_count = 8,
		.name = "Crack_1",
		.vals = NULL  },
	{	.bank_id = 2,
		.led_count = 8,
		.name = "Crack_2",
		.vals = NULL  },
	{	.bank_id = 3,
		.led_count = 8,
		.name = "Crack_3",
		.vals = NULL  },
	{	.bank_id = 4,
		.led_count = 8,
		.name = "Crack_4",
		.vals = NULL  },
	{	.bank_id = 5,
		.led_count = 8,
		.name = "Crack_5",
		.vals = NULL  },
	{	.bank_id = 6,
		.led_count = 8,
		.name = "Crack_6",
		.vals = NULL  },
	{	.bank_id = 7,
		.led_count = 8,
		.name = "Crack_7",
		.vals = NULL  },
	{	.bank_id = 8,
		.led_count = 8,
		.name = "Crack_8",
		.vals = NULL  },
	{	.bank_id = 9,
		.led_count = 8,
		.name = "Buttons",
		.vals = NULL  },
};

static void cleanup_driver_state(void)
{
	if (NULL != state.banks) {
		u32 i;
		for (i = 0; i < state.bank_cnt; ++i)
			free(state.banks[i].vals);
		free(state.banks);
		state.banks = NULL;
		state.bank_cnt = 0;
	}

	if (NULL != state.bank_scratch) {
		free(state.bank_scratch);
		state.bank_scratch = NULL;
	}
}

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

	/* Revision will come back little-endian. */
	state.firmware_rev = ((u16)buf[1] << 8) | (u16)buf[0];
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

int avr_led_set_all_vals(struct avr_led_set_all_vals *req)
{
	u32 i, j;
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	if (!req)
		return -EFAULT;

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_SET_ALL_REG_ADDR, 1,
		       req->rgb, sizeof(req->rgb));


	/* If the command failed, then skip the update of our internal
	 * bookkeeping and just get out.
	 */
	if (rc)
		return rc;

	/* Update internal LED state. */
	for (i = 0; i < state.bank_cnt; ++i) {
		u32 cnt  = state.banks[i].led_count * LED_BYTE_SZ;
		u8 *data = state.banks[i].vals;
		for (j = 0; j < cnt; j += LED_BYTE_SZ) {
			data[j + 0] = req->rgb[0];
			data[j + 1] = req->rgb[1];
			data[j + 2] = req->rgb[2];
		}
	}

	return rc;
}

static int avr_led_set_bank_vals(struct avr_led_set_bank_vals *req)
{
	u32 cnt, i;
	u8 *data;
	int rc;

	if (!req)
		return -EFAULT;

	if (req->bank_id >= state.bank_cnt)
		return -EINVAL;

	/* Pack the scratch buffer with the command to transmit
	 * to the AVR.
	 */
	BUG_ON(state.bank_scratch_bytes < 4);
	state.bank_scratch[0] = req->bank_id;
	state.bank_scratch[1] = req->rgb[0];
	state.bank_scratch[2] = req->rgb[1];
	state.bank_scratch[3] = req->rgb[2];

	rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	rc = i2c_write(AVR_I2C_CLIENT_ID, AVR_LED_SET_BANK_REG_ADDR, 1,
		       state.bank_scratch, 4);

	/* If the command failed, then skip the update of our internal
	 * bookkeeping and just get out.
	 */
	if (rc) {
		printf("SET_BANK_REG_ADDR command failed\n");
		return rc;
	}

	cnt  = state.banks[req->bank_id].led_count * LED_BYTE_SZ;
	data = state.banks[req->bank_id].vals;
	for (i = 0; i < cnt; i += LED_BYTE_SZ) {
		data[i + 0] = req->rgb[0];
		data[i + 1] = req->rgb[1];
		data[i + 2] = req->rgb[2];
	}

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

static int avr_led_set_vol_indicator(u8 vol)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}

	return i2c_write(AVR_I2C_CLIENT_ID, AVR_VOLUME_SETTING_REG_ADDR,
			 1, &vol, 1);
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

static int avr_led_set_button_ctrl_reg(u8 val)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	return i2c_write(AVR_I2C_CLIENT_ID, AVR_BUTTON_CONTROL_REG_ADDR,
			 1, &val, 1);
}

static int avr_led_set_int_reg(u8 val)
{
	int rc = i2c_set_bus_num(AVR_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       AVR_I2C_BUS_ID, rc);
		return rc;
	}
	return i2c_write(AVR_I2C_CLIENT_ID, AVR_BUTTON_INT_REG_ADDR,
			 1, &val, 1);
}

static int avr_setup_bank_state(void)
{
	const void *bank_template = NULL;
	u32 bank_template_sz = 0;
	u32 bank_template_cnt = 0;
	u32 i;

	/* Pick a bank topology template base on the hardware revision enum
	 * fetched from the AVR.
	 */
	switch (state.hardware_type) {
	case AVR_HE_TYPE_SPHERE:
		bank_template = sphere_bank_template;
		bank_template_sz = sizeof(sphere_bank_template);
		bank_template_cnt = ARRAY_SIZE(sphere_bank_template);
		break;

	case AVR_HE_TYPE_RHOMBUS:
		bank_template = rhombus_bank_template;
		bank_template_sz = sizeof(rhombus_bank_template);
		bank_template_cnt = ARRAY_SIZE(rhombus_bank_template);
		break;

	default:
		printf("Unrecognized hardware type 0x%02x\n",
				state.hardware_type);
		return -ENODEV;
	}

	/* Allocate the memory for the bank state array and
	 * copy the template.
	 */
	state.banks = calloc(1, bank_template_sz);
	if (NULL == state.banks) {
		printf("Failed to allocate memory for bank state array.\n");
		return -ENOMEM;
	}
	state.bank_cnt = bank_template_cnt;
	memcpy(state.banks, bank_template, bank_template_sz);

	/* Allocate the memory for the individual banks. */
	state.bank_scratch_bytes = 0;
	state.total_led_cnt = 0;
	for (i = 0; i < state.bank_cnt; ++i) {
		u32 bank_mem = (LED_BYTE_SZ * state.banks[i].led_count);

		state.bank_scratch_bytes += bank_mem;
		state.total_led_cnt += state.banks[i].led_count;

		state.banks[i].vals = calloc(1, bank_mem);
		if (NULL == state.banks[i].vals) {
			printf("Failed to allocate %d bytes for led bank #%d\n",
					bank_mem, i);
			goto err_alloc_bank_vals;
		}
	}

	/* Allocate the memory for the scratch buffer used during bank set
	 * operations.  We need enough scratch buffer for the R G and B values
	 * for the all the banks in the system, as well as 2 bytes of overhead
	 * (for packing the set range command, we need a start and count value
	 * each of which is 8 bits)
	 */
	state.bank_scratch_bytes += SET_RANGE_OVERHEAD;
	state.bank_scratch = calloc(1, state.bank_scratch_bytes);
	if (NULL == state.bank_scratch) {
		printf("Failed to allocate %d bytes for bank scratch memory\n",
				state.bank_scratch_bytes);
		goto err_alloc_bank_scratch;
	}

	return 0;

 err_alloc_bank_scratch:
	while (--i)
		free(state.banks[i].vals);
 err_alloc_bank_vals:
	free(state.banks);
	return -ENOMEM;
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
	struct avr_led_set_all_vals clear_led_req;

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

	/* Cache the hardware type and revision, then use it to determine the
	 * bank topology and setup the internal state for the banks.
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

	rc = avr_setup_bank_state();
	if (rc) {
		printf("Failed to setup LED bank state (rc = %d)\n", rc);
		goto error;
	}

	/* Set the LED state to all off in order to match the internal state we
	 * just established.
	 */
	clear_led_req.rgb[0] = 0x00;
	clear_led_req.rgb[1] = 0x00;
	clear_led_req.rgb[2] = 0x00;
	rc = avr_led_set_all_vals(&clear_led_req);
	if (rc) {
		printf("Failed to clear LEDs on AVR (rc = %d)\n", rc);
		goto error;
	}

	rc = avr_led_commit_led_state(AVR_LED_COMMMIT_IMMEDIATELY);
	if (rc) {
		printf("Failed to commit clear of LEDs on AVR (rc = %d)\n", rc);
		goto error;
	}

	rc = avr_led_set_int_reg(AVR_BUTTON_INT_ENABLE | AVR_BUTTON_INT_CLEAR);
	if (rc) {
		printf("Failed to set int control register (rc = %d)\n", rc);
		goto error;
	}

	/* Enable buttons inside of the AVR. */
	rc = avr_led_set_button_ctrl_reg(AVR_BUTTON_CONTROL_ENABLE0);
	if (rc) {
		printf("Failed to set button control register (rc = %d)\n", rc);
		goto error;
	}

	printf("Steelhead AVR detected and initialized\n");

	state.avr_detected = 1;
	return 0;
 error:
	cleanup_driver_state();
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
		rc = avr_read_event_fifo(&next_event);
		if (rc) {
			printf("Failed to read event fifo, err = %d\n", rc);
			return 1;
		}
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
	printf("  led_bank_count = %d\n", state.bank_cnt);
	return 0;
}

static int do_avr_get_bank_desc(cmd_tbl_t *cmdtp, int flag,
				int argc, char * const argv[])
{
	int bank_idx;
	if (argc != 2) {
		printf("usage: avr get bank_desc bank#\n");
		return 1;
	}
	bank_idx = simple_strtoul(argv[1], NULL, 10);
	if (bank_idx >= state.bank_cnt) {
		printf("invalid bank_idx %d, must be 0 to %d\n",
		       bank_idx, state.bank_cnt);
		return 1;
	} else {
		printf("led bank %d has id = %d, name = '%s', led_count = %d\n",
		       bank_idx,
		       state.banks[bank_idx].bank_id,
		       state.banks[bank_idx].name,
		       state.banks[bank_idx].led_count);
	}
	return 0;
}

static int do_avr_get_bank_values(cmd_tbl_t *cmdtp, int flag,
				  int argc, char * const argv[])
{
	int bank_idx;
	int start;
	if (argc != 3) {
		printf("usage: avr get bank_values bank# start\n");
		return 1;
	}
	bank_idx = simple_strtoul(argv[1], NULL, 10);
	start = simple_strtoul(argv[2], NULL, 10);
	if (bank_idx >= state.bank_cnt) {
		printf("invalid bank_idx %d, must be 0 to %d\n",
		       bank_idx, state.bank_cnt - 1);
		return 1;
	} else {
		struct led_bank_state *bank = &(state.banks[bank_idx]);
		if (start >= bank->led_count) {
			printf("invalid start led %d, must be 0 to %d\n",
			       start, bank->led_count - 1);
			return 1;
		} else {
			int i;
			printf("led bank %d values:\n", bank_idx);
			for (i = start; i < bank->led_count; i++) {
				printf(" led[%d] = 0x%x 0x%x 0x%x\n",
				       i, bank->vals[i * LED_BYTE_SZ],
				       bank->vals[(i * LED_BYTE_SZ) + 1],
				       bank->vals[(i * LED_BYTE_SZ) + 2]);
			}
		}
	}
	return 0;
}

static int do_avr_set_mode(cmd_tbl_t *cmdtp, int flag,
			   int argc, char * const argv[])
{
	ulong raw;
	const char *mode_names[] = {"BOOT_ANIMATION", "VOLUME", "HOST"};
	if (argc != 2) {
		printf("usage: avr set mode "
		       "[0=boot_animation,1=volume,2=host]\n");
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

static int do_avr_set_bank_values(cmd_tbl_t *cmdtp, int flag,
				  int argc, char * const argv[])
{
	struct avr_led_set_bank_vals req;
	ulong raw;
	if (argc != 5) {
		printf("usage: avr set bank_values bank# red green blue\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 10);
	if (raw >= state.bank_cnt) {
		printf("invalid bank# %lu, must be 0 to %d\n",
		       raw, state.bank_cnt - 1);
		return 1;
	}
	req.bank_id = raw;
	raw = simple_strtoul(argv[2], NULL, 10);
	if (raw > 255) {
		printf("red value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[0] = raw;
	raw = simple_strtoul(argv[3], NULL, 10);
	if (raw > 255) {
		printf("green value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[1] = raw;
	raw = simple_strtoul(argv[4], NULL, 10);
	if (raw > 255) {
		printf("blue value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[2] = raw;
	if (avr_led_set_bank_vals(&req)) {
		printf("Error setting bank values\n");
		return 1;
	}
	printf("Succeeded setting bank values\n");
	return 0;
}

static int do_avr_set_all(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	struct avr_led_set_all_vals req;
	ulong raw;
	if (argc != 4) {
		printf("usage: avr set all red green blue\n");
		return 1;
	}
	raw = simple_strtoul(argv[1], NULL, 10);
	if (raw > 255) {
		printf("red value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[0] = raw;
	raw = simple_strtoul(argv[2], NULL, 10);
	if (raw > 255) {
		printf("green value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[1] = raw;
	raw = simple_strtoul(argv[3], NULL, 10);
	if (raw > 255) {
		printf("blue value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	req.rgb[2] = raw;
	if (avr_led_set_all_vals(&req)) {
		printf("Error setting all led values\n");
		return 1;
	}
	printf("Succeeded setting all led values\n");
	return 0;
}

static int do_avr_set_volume(cmd_tbl_t *cmdtp, int flag,
			     int argc, char * const argv[])
{
	ulong raw;

	if (argc != 2) {
		printf("usage: avr set volume volume#\n");
		return 1;
	}
	raw = simple_strtoul(argv[3], NULL, 10);
	if (raw > 255) {
		printf("volume value of %lu too large, must be 0-255\n", raw);
		return 1;
	}
	if (avr_led_set_vol_indicator(raw)) {
		printf("Error setting volume %lu\n", raw);
		return 1;
	}
	printf("Succeeded in setting volume %lu\n", raw);
	return 0;
}

static cmd_tbl_t cmd_avr_get_sub[] = {
	U_BOOT_CMD_MKENT(info, 1, 1, do_avr_get_info, "", ""),
	U_BOOT_CMD_MKENT(key, 1, 1, do_avr_get_key, "", ""),
	U_BOOT_CMD_MKENT(bank_desc, 2, 1, do_avr_get_bank_desc, "", ""),
	U_BOOT_CMD_MKENT(bank, 3, 1, do_avr_get_bank_values, "", ""),
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
	U_BOOT_CMD_MKENT(bank, 5, 1, do_avr_set_bank_values, "", ""),
	U_BOOT_CMD_MKENT(all, 4, 1, do_avr_set_all, "", ""),
	U_BOOT_CMD_MKENT(volume, 2, 1, do_avr_set_volume, "", ""),
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
	avr, 7, 1, do_avr,
	"steelhead avr sub system",
	"avr reinit\n"
	"avr commit_state\n"
	"avr get key\n"
	"avr get info\n"
	"avr get bank_desc bank#\n"
	"avr get bank bank# start\n"
	"avr set mode [0=boot_animation,1=volume,2=host]\n"
	"avr set all red green blue\n"
	"avr set bank bank# red green blue\n"
	"avr set volume volume#\n");

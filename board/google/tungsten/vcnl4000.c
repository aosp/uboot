/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
/* #define DEBUG */

#include <common.h>
#include <config.h>
#include <command.h>
#include <i2c.h>
#include <malloc.h>
#include <errno.h>
#include <fastboot.h>

#define ADC_BUFFER_NUM	6

#define VCNL4000_COMMAND_REG_ADDR                         0x80
#define VCNL4000_PRODUCT_ID_REG_ADDR                      0x81
#define VCNL4000_IR_LED_CURRENT_REG_ADDR                  0x83
#define VCNL4000_PROXIMITY_RESULT_HIGH_REG_ADDR           0x87
#define VCNL4000_PROXIMITY_RESULT_LOW_REG_ADDR            0x88
#define VCNL4000_PROXIMITY_MODULATOR_TIMING_ADJ_REG_ADDR  0x8A

/* bits in COMMAND_REG */
#define COMMAND_REG_ALS_DATA_RDY  (1 << 6)
#define COMMAND_REG_PROX_DATA_RDY (1 << 5)
#define COMMAND_REG_ALS_OD        (1 << 4)
#define COMMAND_REG_PROX_OD       (1 << 3)

#define READ_TIMEOUT 100000

/* 129 is value recommended in data sheet */
#define DEFAULT_MODULATOR_TIMING (129)

#define DEFAULT_IR_LED_CURRENT   (20) /* 20 mA */

#define VCNL_I2C_CLIENT_ID       (0x13)
#define VCNL_I2C_BUS_ID          (0x1)

static struct vcnl_driver_state {
	int vcnl_detected;
	u8 product_id;
} state;

static int vcnl_get_product_info(void)
{
	u8 raw;
	int err;

	err = i2c_set_bus_num(VCNL_I2C_BUS_ID);
	if (err) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       VCNL_I2C_BUS_ID, err);
		return err;
	}

	err = i2c_read(VCNL_I2C_CLIENT_ID, VCNL4000_PRODUCT_ID_REG_ADDR,
		       1, &raw, 1);
	if (err) {
		printf("failed to read product_id\n");
		return err;
	}
	state.product_id = raw;

	printf("%s: product_id %d, revision %d\n", __func__,
	       ((state.product_id & 0xf0) >> 4),
	       ((state.product_id) & 0x0f));

	return 0;
}

static int vcnl_set_modulator_timing(u8 modulator_timing)
{
	int err;

	err = i2c_set_bus_num(VCNL_I2C_BUS_ID);
	if (err) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       VCNL_I2C_BUS_ID, err);
		return err;
	}

	err = i2c_write(VCNL_I2C_CLIENT_ID,
			    VCNL4000_PROXIMITY_MODULATOR_TIMING_ADJ_REG_ADDR, 1,
			    &modulator_timing, 1);
	if (err)
		printf("Error writing initial value for timing adj\n");

	return err;
}

static int vcnl_set_ir_led_current(int ir_led_value_mA)
{
	u8 raw;
	int err;

	err = i2c_set_bus_num(VCNL_I2C_BUS_ID);
	if (err) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       VCNL_I2C_BUS_ID, err);
		return err;
	}

	/* setup ir led current level */
	err = i2c_read(VCNL_I2C_CLIENT_ID, VCNL4000_IR_LED_CURRENT_REG_ADDR,
		       1, &raw, 1);
	if (err) {
		printf("Error reading ir led current reg\n");
		return err;
	}
	printf("%s: Initial ir led current value = 0x%x (%d mA)\n",
	       __func__, raw, (int)raw * 10);

	/* value to write is requested mA value divided by 10 */
	raw = ir_led_value_mA / 10;
	printf("%s: setting ir led current value = 0x%x (%d mA)\n",
	       __func__, raw, ir_led_value_mA);
	err = i2c_write(VCNL_I2C_CLIENT_ID, VCNL4000_IR_LED_CURRENT_REG_ADDR,
			1, &raw, 1);
	if (err) {
		printf("Error writing ir_led_current\n");
		return err;
	}
	return 0;
}

static int do_vcnl_set_led_current(cmd_tbl_t *cmdtp, int flag,
				   int argc, char * const argv[])
{
	long raw;

	if (argc != 2) {
		printf("usage: vcnl set led_current led_current_value\n");
		return 1;
	}
	raw = simple_strtol(argv[1], NULL, 10);
	if ((raw < 0) || (raw > 200)) {
		printf("invalid ir_led_current %ld, must be between 0-200\n",
		       raw);
		return 1;
	}
	if (raw % 10) {
		printf("invalid ir_led_current %ld, must be multiple of 10\n",
		       raw);
		return 1;
	}
	if (vcnl_set_ir_led_current(raw)) {
		printf("Error setting ir led current %ld\n", raw);
		return 1;
	}
	printf("Succeeded in setting ir led current %ld\n", raw);
	return 0;
}

int vcnl_get_proximity(void)
{
	u8 reg_value[2];
	u16 proximity;
	int err;
	int read_timeout = READ_TIMEOUT;

	err = i2c_set_bus_num(VCNL_I2C_BUS_ID);
	if (err) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       VCNL_I2C_BUS_ID, err);
		return err;
	}

	/* request a proximity measurement to be taken */
	err = i2c_read(VCNL_I2C_CLIENT_ID, VCNL4000_COMMAND_REG_ADDR,
		       1, reg_value, 1);
	if (err) {
		printf("Failed in i2c_read, error %d\n", err);
		return err;
	}
	reg_value[0] |= COMMAND_REG_PROX_OD;
	err = i2c_write(VCNL_I2C_CLIENT_ID, VCNL4000_COMMAND_REG_ADDR,
			1, reg_value, 1);
	if (err) {
		printf("Failed in i2c_write, error %d\n", err);
		return err;
	}
	reg_value[0] &= ~COMMAND_REG_PROX_DATA_RDY;

	/* from empirical testing, the result is ready very quickly
	 * so just poll for completion.
	 */
	do {
		err = i2c_read(VCNL_I2C_CLIENT_ID, VCNL4000_COMMAND_REG_ADDR,
			       1, reg_value, 1);
		if (err) {
			printf("Error %d reading command register\n", err);
			return err;
		}
	} while (((reg_value[0] & COMMAND_REG_PROX_DATA_RDY) == 0) &&
		 (read_timeout-- > 0));
	if (read_timeout <= 0) {
		printf("%s: timed out reading proximity result\n", __func__);
		return -1;
	}

	/* read proximity registers.  first reg is high 8 bits,
	 * second reg is low 8 bits.  read in one i2c transfer.
	 */
	err = i2c_read(VCNL_I2C_CLIENT_ID,
		       VCNL4000_PROXIMITY_RESULT_HIGH_REG_ADDR, 1,
		       reg_value, sizeof(reg_value));
	if (err) {
		printf("Error reading proximity result registers\n");
		return err;
	}
	proximity = (reg_value[0] << 8) | reg_value[1];
	return proximity;
}

static int do_vcnl_get_proximity(cmd_tbl_t *cmdtp, int flag,
				 int argc, char * const argv[])
{
	int proximity = vcnl_get_proximity();
	if (proximity < 0)
		printf("Error %d getting proximity\n", proximity);
	else
		printf("vcnl proximity: %d\n", proximity);
	return 0;
}

static int do_vcnl_get_info(cmd_tbl_t *cmdtp, int flag,
			    int argc, char * const argv[])
{
	printf("%s: product_id %d, revision %d\n", __func__,
		((state.product_id & 0xf0) >> 4),
		((state.product_id) & 0x0f));
	return 0;
}

int detect_vcnl(void)
{
	int rc;

	printf("%s\n", __func__);

	rc = i2c_set_bus_num(VCNL_I2C_BUS_ID);
	if (rc) {
		printf("Failed in i2c_set_bus_num(%d), error %d\n",
		       1, rc);
		return rc;
	}

	/* Cache the firmware revision (also checks to be sure the VCNL is
	 * actually there and talking to us).
	 */
	rc = vcnl_get_product_info();
	if (rc) {
		printf("Failed to fetch VCNL product_info (rc = %d)\n", rc);
		return rc;
	}

	rc = vcnl_set_modulator_timing(DEFAULT_MODULATOR_TIMING);
	if (rc) {
		printf("Failed to setup modulator timing (rc = %d)\n", rc);
		return rc;
	}

	rc = vcnl_set_ir_led_current(DEFAULT_IR_LED_CURRENT);
	if (rc) {
		printf("Failed to set ir led current (rc = %d)\n", rc);
		return rc;
	}

	state.vcnl_detected = 1;
	return 0;
}

static cmd_tbl_t cmd_vcnl_get_sub[] = {
	U_BOOT_CMD_MKENT(info, 1, 1, do_vcnl_get_info, "", ""),
	U_BOOT_CMD_MKENT(proximity, 1, 1, do_vcnl_get_proximity, "", ""),
};

static int do_vcnl_get(cmd_tbl_t *cmdtp, int flag,
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

	c = find_cmd_tbl(argv[0], &cmd_vcnl_get_sub[0],
			 ARRAY_SIZE(cmd_vcnl_get_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

static cmd_tbl_t cmd_vcnl_set_sub[] = {
	U_BOOT_CMD_MKENT(led_current, 2, 1, do_vcnl_set_led_current, "", ""),
};

static int do_vcnl_set(cmd_tbl_t *cmdtp, int flag,
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

	c = find_cmd_tbl(argv[0], &cmd_vcnl_set_sub[0],
			 ARRAY_SIZE(cmd_vcnl_set_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

static cmd_tbl_t cmd_vcnl_sub[] = {
	U_BOOT_CMD_MKENT(get, 2, 1, do_vcnl_get, "", ""),
	U_BOOT_CMD_MKENT(set, 3, 1, do_vcnl_set, "", ""),
};

#ifdef CONFIG_NEEDS_MANUAL_RELOC
void vcnl_reloc(void)
{
	fixup_cmdtable(cmd_vcnl_sub, ARRAY_SIZE(cmd_vcnl_sub));
	fixup_cmdtable(cmd_vcnl_get_sub, ARRAY_SIZE(cmd_vcnl_get_sub));
	fixup_cmdtable(cmd_vcnl_set_sub, ARRAY_SIZE(cmd_vcnl_set_sub));
}
#endif

static int do_vcnl(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2) {
		printf("Too few arguments\n");
		return cmd_usage(cmdtp);
	}

	if (!state.vcnl_detected) {
		if (detect_vcnl()) {
			puts("Could not detect vcnl\n");
			return 1;
		}
	}

	/* Strip off leading 'vcnl' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_vcnl_sub[0], ARRAY_SIZE(cmd_vcnl_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else {
		printf("unknown command %s\n", argv[2]);
		return cmd_usage(cmdtp);
	}
}

U_BOOT_CMD(
	vcnl, 4, 1, do_vcnl,
	"steelhead vcnl proximity detector sub system",
	"vcnl get info\n"
	"vcnl get proximity\n"
	"vcnl set led_current #\n");






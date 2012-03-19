/*
 * (C) Copyright 2011-2012 Google, Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <command.h>
#include <mmc.h>
#include <malloc.h>

static int oem_cmd(struct mmc *mmc, uint mode)
{
	struct mmc_cmd cmd;
	int ret;

	cmd.cmdidx = 62;		/* Samsung OEM command */
	cmd.cmdarg = 0xEFAC62EC;	/* Magic value */
	cmd.resp_type = MMC_RSP_R1b;
	cmd.flags = 0;
	ret = mmc->send_cmd(mmc, &cmd, NULL);
	if (ret) {
		printf("%s: Error %d sending magic.\n", __func__, ret);
		return 1;
	}
	cmd.cmdarg = mode;
	ret = mmc->send_cmd(mmc, &cmd, NULL);
	if (ret) {
		printf("%s: Error %d sending mode 0x%08X.\n",
						__func__, ret, mode);
		return 1;
	}
	return 0;
}


static int read_smart(struct mmc *mmc, uint32_t *dst)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int ret;

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	data.dest = (char *)dst;
	data.blocks = 1;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	ret = mmc->send_cmd(mmc, &cmd, &data);
	if (ret) {
		printf("%s: Error %d reading.\n", __func__, ret);
		return 1;
	}
	return 0;
}

static int get_smart(struct mmc *mmc, uint32_t *data)
{
	int ret;
	/* Enter Smart Report Mode */
	ret = oem_cmd(mmc, 0x0000CCEE) || read_smart(mmc, data);

	/* Exit Smart Report Mode */
	ret = oem_cmd(mmc, 0x00DECCEE) || ret;

	return ret;
}

static int print_smart(struct mmc *mmc, uint32_t *data)
{
	int i;
	uint32_t val;

	if (get_smart(mmc, data))
		return 1;

	val = le32_to_cpup(data);
	switch (val) {
	case 0xD2D2D2D2:
		printf("        Error Mode: Normal\n");
		break;
	case 0x5C5C5C5C:
		printf("        Error Mode: RuntimeFatalError\n");
		break;
	case 0xE1E1E1E1:
		printf("        Error Mode: MetaBrokenError\n");
		break;
	case 0x37373737:
		printf("The Error Mode is OpenFatalError; no valid data.\n");
		return 1;	/* Fatal */
	default:
		printf("Error Mode: Unknown (0x%08X)\n", val);
		return 1;	/* Fatal */
	}
	printf("  Super Block Size: %d bytes\n", le32_to_cpup(data + 1));
	printf("   Super Page Size: %d bytes\n", le32_to_cpup(data + 2));
	printf("Optimal Write Size: %d bytes\n", le32_to_cpup(data + 3));
	printf("Read Reclaim Count: %d\n", le32_to_cpup(data + 20));
	printf(" Optimal Trim Size: %d\n", le32_to_cpup(data + 21));
	printf("   Number of Banks: %d\n", le32_to_cpup(data + 4));
	printf("========== Bad Blocks ===========\n");
	printf("Bank  Initial  Runtime  Remaining\n");
	for (i = 0; i < 4; i++)
		printf("%4d  %7d  %7d  %9d\n", i,
					le32_to_cpup(data + 5 + i * 3),
					le32_to_cpup(data + 6 + i * 3),
					le32_to_cpup(data + 7 + i * 3));
	printf("========== Erase Count ==========\n");
	printf("          Min      Avg      Max\n");
	printf(" All  %7d  %7d  %7d\n", le32_to_cpup(data + 18),
					le32_to_cpup(data + 19),
					le32_to_cpup(data + 17));
	printf(" SLC  %7d  %7d  %7d\n", le32_to_cpup(data + 31),
					le32_to_cpup(data + 32),
					le32_to_cpup(data + 30));
	printf(" MLC  %7d  %7d  %7d\n", le32_to_cpup(data + 34),
					le32_to_cpup(data + 35),
					le32_to_cpup(data + 33));
	return 0;
}

static int do_samsung_smart(cmd_tbl_t *cmdtp, int flag,
						int argc, char * const argv[])
{
	struct mmc *mmc;
	int i, ret;
	char *ep;
	uint32_t *data = NULL;

	if (argc != 2)
		return cmd_usage(cmdtp);
	i = simple_strtoul(argv[1], &ep, 10);
	if (ep == argv[1] || *ep != '\0') {
		printf("'%s' is not a number\n", argv[1]);
		return cmd_usage(cmdtp);
	}

	mmc = find_mmc_device(i);
	if (!mmc) {
		printf("no mmc device at slot %x\n", i);
		return 1;
	}

	/* Allocate space for the report to be read in to. */
	data = malloc(mmc->read_bl_len);
	if (!data) {
		printf("%s: Error allocating SMART buffer.\n", __func__);
		return 1;
	}

	ret = print_smart(mmc, data);

	free(data);

	return ret != 0;
}
U_BOOT_CMD(samsung_smart, 2,	1, do_samsung_smart,
	"list the Samsung e-MMC Smart Report data",
	""
);

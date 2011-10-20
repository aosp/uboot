/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Derived from cmd_sata.c, which is:
 * Copyright (C) 2000-2005, DENX Software Engineering
 *		Wolfgang Denk <wd@denx.de>
 * Copyright (C) Procsys. All rights reserved.
 *		Mushtaq Khan <mushtaq_k@procsys.com>
 *			<mushtaqk_921@yahoo.co.in>
 * Copyright (C) 2008 Freescale Semiconductor, Inc.
 *		Dave Liu <daveliu@freescale.com>
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
#include <command.h>
#include <part.h>

static char blk_curr_name[80];
static block_dev_desc_t *blk_curr_dev;

static int parse_hex_val(char *arg, ulong *val)
{
	char *ep;
	*val = simple_strtoul(arg, &ep, 16);

	if (ep == arg || *ep != '\0')
		return -1;	/* Fail */
	else
		return 0;	/* Success */
}

int do_blk(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	block_dev_desc_t *dev;
	disk_partition_t ptn;
	ulong addr;
	lbaint_t blk, cnt, blks_done;
	int err, is_erase = 0, is_read = 0, is_write = 0;
	/*
	 * All of our commands are of the form "blk <action>" followed by
	 * parameters.  For convenience, convert argc and argv into
	 * num_parms and parms.  For clarity, if num_parms==1, parms[0] is
	 * the only valid parameter.
	 */
	const char *action = argv[1];
	int num_parm = argc - 2;
	char **parm = (char **)argv + 2;

	if (num_parm < 0)
		return cmd_usage(cmdtp);

	if (!strncmp(action, "dev", 3)) {
		switch (num_parm) {
		case 0:
			if (!blk_curr_dev) {
				puts("No current block device\n");
				return 1;
			}
			break;
		case 1:
			dev = get_dev_by_name(parm[0]);
			if (dev == NULL) {
				puts("Unknown device\n");
				return 1;
			}

			strncpy(blk_curr_name, parm[0],
						sizeof(blk_curr_name) - 1);
			blk_curr_name[sizeof(blk_curr_name) - 1] = '\0';
			blk_curr_dev = dev;
			/* Set the partition type for our new device. */
			init_part(blk_curr_dev);
			break;
		default:
			puts("usage: blk device [dev]\n");
			return 1;
		}

		printf("Block Device %s:\n", blk_curr_name);
		dev_print(blk_curr_dev);
		if (num_parm == 1)
			puts("... is now current device\n");

		return 0;
	}
	if (!strncmp(action, "part", 4)) {
		switch (num_parm) {
		case 0:
			if (!blk_curr_dev) {
				puts("No current block device\n");
				return 1;
			}
			dev = blk_curr_dev;
			break;
		case 1:
			dev = get_dev_by_name(parm[0]);
			if (dev == NULL) {
				puts("Unknown device\n");
				return 1;
			}
			break;
		default:
			puts("usage: blk partition [dev]\n");
			return 1;
		}

		init_part(dev);
		print_part(dev);

		return 0;
	}
	if (!strcmp(action, "erase"))
		is_erase = 1;
	else if (!strcmp(action, "read"))
		is_read = 1;
	else if (!strcmp(action, "write"))
		is_write = 1;
	else
		return cmd_usage(cmdtp);

	/*
	 * At this point, we know we have either returned with a usage error,
	 * returned after handling a "dev" or "part" command, or we have an
	 * "erase", "read" or "write" command.
	 */
	if (!blk_curr_dev) {
		puts("No current block device\n");
		return 1;
	}

	if (is_erase) {
		/* Need blk#/partition and optional count. */
		if (num_parm < 1 || num_parm > 2) {
			puts("usage: blk erase blk# cnt\n");
			puts("   or: blk erase partition [cnt]\n");
			return 1;
		}
	} else {
		/* Need address, blk#/partition and optional count. */
		if (num_parm < 2 || num_parm > 3
					|| parse_hex_val(parm[0], &addr)) {
			printf("usage: blk %s addr blk# cnt\n", action);
			printf("   or: blk %s addr partition [cnt]\n", action);
			return 1;
		}
		/* Advance parm so parm[0] is offset and parm[1] is count. */
		parm++;
		num_parm--;
	}

	if (!parse_hex_val(parm[0], &blk)) {
		/* We successfully parsed the parameter as an integer. */
		if (num_parm != 2) {
			printf("Missing cnt parameter\n");
			return 1;
		}
		if (parse_hex_val(parm[1], &cnt)) {
			printf("Invalid cnt parameter: %s\n", parm[1]);
			return 1;
		}

		printf("Block %s: device %s block # 0x%lX, count 0x%lX ...\n",
					action, blk_curr_name, blk, cnt);
		if (is_erase)
			blks_done = blk_curr_dev->block_erase(blk_curr_dev->dev,
							blk, cnt);
		else if (is_read) {
			blks_done = blk_curr_dev->block_read(blk_curr_dev->dev,
							blk, cnt, (void *)addr);
			/* flush cache after read */
			flush_cache(addr, cnt * blk_curr_dev->blksz);
		} else
			blks_done = blk_curr_dev->block_write(blk_curr_dev->dev,
							blk, cnt, (void *)addr);

		printf("0x%lX blocks %s: %s\n", blks_done, action,
					(blks_done == cnt) ? "OK" : "ERROR");
		return (blks_done == cnt) ? 0 : 1;
	}

	if (get_partition_by_name(blk_curr_dev, parm[0], &ptn)) {
		printf("\"%s\" is neither a partition nor a "
					"block number.\n", parm[0]);
		return 1;
	}
	/* We successfully parsed the parameter as a partition name. */
	if (num_parm > 1) {
		if (parse_hex_val(parm[1], &cnt)) {
			printf("Invalid cnt parameter: %s\n", parm[1]);
			return 1;
		}
	} else
		cnt = ptn.size;

	printf("Block %s: device %s block # 0x%lX, count 0x%lX ...\n",
				action, blk_curr_name, ptn.start, cnt);
	if (is_erase)
		err = partition_erase_blks(blk_curr_dev, &ptn, &cnt);
	else if (is_read) {
		err = partition_read_blks(blk_curr_dev, &ptn,
						&cnt, (void *)addr);
		/* flush cache after read */
		flush_cache(addr, cnt * blk_curr_dev->blksz);
	} else
		err = partition_write_blks(blk_curr_dev, &ptn,
						&cnt, (void *)addr);

	if (err) {
		printf("0x%lX blocks %s: ERROR=%d\n", cnt, action, err);
		return 1;
	}
	printf("0x%lX blocks %s: OK\n", cnt, action);
	return 0;
}

U_BOOT_CMD(
	blk, 5, 0, do_blk,
	"Block Device sub system",
	"device [dev] - show or set current device\n"
	"blk partition [dev] - print partition table\n"
	"blk erase blk# cnt\n"
	"blk erase partition [cnt]\n"
	"blk read addr blk# cnt\n"
	"blk read addr partition [cnt]\n"
	"blk write addr blk# cnt\n"
	"blk write addr partition [cnt]"
);

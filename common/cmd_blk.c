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

int do_blk(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong addr, cnt, n;
	lbaint_t blk;
	int is_read;

	switch (argc) {
	case 2:	/* "blk dev" or "blk part" */
		if (strncmp(argv[1], "dev", 3) == 0) {
			if (!blk_curr_name[0] || !blk_curr_dev) {
				puts("No current block device\n");
				return 1;
			}

			printf("Block Device %s:\n", blk_curr_name);
			dev_print(blk_curr_dev);
			return 0;
		} else if (strncmp(argv[1], "part", 4) == 0) {
			if (!blk_curr_name[0] || !blk_curr_dev) {
				puts("No current block device\n");
				return 1;
			}

			init_part(blk_curr_dev);
			print_part(blk_curr_dev);
			return 0;
		}
		return cmd_usage(cmdtp);
	case 3:	/* e.g. "blk dev nand0" or "blk part nand0" */
		if (strncmp(argv[1], "dev", 3) == 0) {
			block_dev_desc_t *dev = get_dev_by_name(argv[2]);
			if (dev == NULL) {
				puts("Unknown device\n");
				return 1;
			}

			strncpy(blk_curr_name, argv[2],
						sizeof(blk_curr_name) - 1);
			blk_curr_name[sizeof(blk_curr_name) - 1] = '\0';
			blk_curr_dev = dev;

			printf("Block Device %s:\n", blk_curr_name);
			dev_print(blk_curr_dev);
			puts("... is now current device\n");
			return 0;
		} else if (strncmp(argv[1], "part", 4) == 0) {
			block_dev_desc_t *dev = get_dev_by_name(argv[2]);
			if (dev == NULL) {
				puts("Unknown device\n");
				return 1;
			}

			init_part(dev);
			print_part(dev);
			return 0;
		}
		return cmd_usage(cmdtp);
	case 4: /* e.g. "blk erase 0 1" */
		if (strcmp(argv[1], "erase") != 0)
			return cmd_usage(cmdtp);

		if (!blk_curr_name[0] || !blk_curr_dev) {
			puts("No current block device\n");
			return 1;
		}

		blk = simple_strtoul(argv[2], NULL, 16);
		cnt = simple_strtoul(argv[3], NULL, 16);

		printf("Block %s: device %s block # %ld, count %ld ...",
					argv[1], blk_curr_name, blk, cnt);

		n = blk_curr_dev->block_erase(blk_curr_dev->dev, blk, cnt);

		printf("%ld blocks %s: %s\n", n, argv[1],
						(n == cnt) ? "OK" : "ERROR");
		return (n == cnt) ? 0 : 1;
	case 5: /* e.g. "blk read 40200000 0 1" or "blk write 40200000 0 1" */
		if (strcmp(argv[1], "read") == 0)
			is_read = 1;
		else if (strcmp(argv[1], "write") == 0)
			is_read = 0;
		else
			return cmd_usage(cmdtp);

		if (!blk_curr_name[0] || !blk_curr_dev) {
			puts("No current block device\n");
			return 1;
		}

		addr = simple_strtoul(argv[2], NULL, 16);
		blk = simple_strtoul(argv[3], NULL, 16);
		cnt = simple_strtoul(argv[4], NULL, 16);

		printf("Block %s: device %s block # %ld, count %ld ...",
					argv[1], blk_curr_name, blk, cnt);

		if (is_read) {
			n = blk_curr_dev->block_read(blk_curr_dev->dev,
							blk, cnt, (u32 *)addr);

			/* flush cache after read */
			flush_cache(addr, cnt * blk_curr_dev->blksz);
		} else
			n = blk_curr_dev->block_write(blk_curr_dev->dev,
							blk, cnt, (u32 *)addr);

		printf("%ld blocks %s: %s\n", n, argv[1],
						(n == cnt) ? "OK" : "ERROR");
		return (n == cnt) ? 0 : 1;
	default: /* Including the case of not getting any arguments */
		return cmd_usage(cmdtp);
	}
}

U_BOOT_CMD(
	blk, 5, 0, do_blk,
	"Block Device sub system",
	"device [dev] - show or set current device\n"
	"blk part [dev] - print partition table\n"
	"blk erase blk# cnt\n"
	"blk read addr blk# cnt\n"
	"blk write addr blk# cnt"
);

/*
 * (C) Copyright 2005
 * 2N Telekomunikace, a.s. <www.2n.cz>
 * Ladislav Michl <michl@2n.cz>
 *
 * (C) Copyright 2011 Google, Inc.
 *   Added generic block interface via CONFIG_PARTITIONS
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
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

#include <common.h>
#include <nand.h>

#ifndef CONFIG_SYS_NAND_BASE_LIST
#define CONFIG_SYS_NAND_BASE_LIST { CONFIG_SYS_NAND_BASE }
#endif

DECLARE_GLOBAL_DATA_PTR;

int nand_curr_device = -1;
nand_info_t nand_info[CONFIG_SYS_MAX_NAND_DEVICE];

static struct nand_chip nand_chip[CONFIG_SYS_MAX_NAND_DEVICE];
static ulong base_address[CONFIG_SYS_MAX_NAND_DEVICE] = CONFIG_SYS_NAND_BASE_LIST;

static struct block_dev_desc nand_blkdev[CONFIG_SYS_MAX_NAND_DEVICE];

static const char default_nand_name[] = "nand";
static __attribute__((unused)) char dev_name[CONFIG_SYS_MAX_NAND_DEVICE][8];


#ifdef CONFIG_PARTITIONS
static nand_info_t *nand_get_info(int dev)
{
	if (dev < 0 || dev >= CONFIG_SYS_MAX_NAND_DEVICE)
		return NULL;
	if (nand_blkdev[dev].if_type != IF_TYPE_MTD_NAND)
		return NULL;
	return &nand_info[dev];
}

block_dev_desc_t *nand_get_dev(int dev)
{
	if (dev < 0 || dev >= CONFIG_SYS_MAX_NAND_DEVICE)
		return NULL;
	if (nand_blkdev[dev].if_type != IF_TYPE_MTD_NAND)
		return NULL;
	return &nand_blkdev[dev];
}

static ulong nand_berase(int dev_num, lbaint_t start, lbaint_t blkcnt)
{
	if (blkcnt == 0)
		return 0;

	nand_info_t *nand = nand_get_info(dev_num);
	if (!nand)
		return 0;

	struct mtd_info *mtd = (struct mtd_info *)nand;
	uint32_t blk_size = mtd->writesize;

	nand_erase_options_t opts = {
	    .length = blkcnt * (loff_t)blk_size,
	    .offset = start  * (loff_t)blk_size,
	    .quiet = 1,
	    .jffs2 = 0,
	    .scrub = 0
	};

	/*
	 * If the jffs2 flag needs to be set, it could be done here (perhaps
	 * via a command or environment variable).
	 */

	/* Note nand_erase_opts will skip over and not fail on bad blocks. */
	return nand_erase_opts(nand, &opts) ? 0 : blkcnt;
}

static ulong nand_bwrite(int dev_num, lbaint_t start, lbaint_t blkcnt,
								const void *src)
{
	if (blkcnt == 0)
		return 0;

	nand_info_t *nand = nand_get_info(dev_num);
	if (!nand)
		return 0;

	struct mtd_info *mtd = (struct mtd_info *)nand;
	uint32_t blk_size = mtd->writesize;
	loff_t offset = (loff_t)blk_size * start;
	size_t length = (size_t)blk_size * blkcnt;
	int flags = 0;

	/*
	 * If the flags needs to be set (e.g. to include WITH_YAFFS_OOB), it
	 * could be done here (perhaps via a command or environment variable).
	 */

	if (nand_write_skip_bad(nand, offset, &length, (u_char *)src, flags))
		return 0;

	return length / blk_size;
}

static ulong nand_bread(int dev_num, lbaint_t start, lbaint_t blkcnt,
								void *dst)
{
	if (blkcnt == 0)
		return 0;

	nand_info_t *nand = nand_get_info(dev_num);
	if (!nand)
		return 0;

	struct mtd_info *mtd = (struct mtd_info *)nand;
	uint32_t blk_size = mtd->writesize;
	loff_t offset = (loff_t)blk_size * start;
	size_t length = (size_t)blk_size * blkcnt;

	if (nand_read_skip_bad(nand, offset, &length, dst))
		return 0;

	return length / blk_size;
}
#endif

static void nand_init_chip(struct mtd_info *mtd, struct nand_chip *nand,
				struct block_dev_desc *blkdev, ulong base_addr)
{
	int maxchips = CONFIG_SYS_NAND_MAX_CHIPS;
	static int __attribute__((unused)) i = 0;

	if (maxchips < 1)
		maxchips = 1;
	mtd->priv = nand;
	blkdev->priv = nand;	/* Not currently used. */

	nand->IO_ADDR_R = nand->IO_ADDR_W = (void  __iomem *)base_addr;
	if (board_nand_init(nand) == 0) {
		if (nand_scan(mtd, maxchips) == 0) {
			if (!mtd->name)
				mtd->name = (char *)default_nand_name;
#ifdef CONFIG_NEEDS_MANUAL_RELOC
			else
				mtd->name += gd->reloc_off;
#endif

#ifdef CONFIG_MTD_DEVICE
			/*
			 * Add MTD device so that we can reference it later
			 * via the mtdcore infrastructure (e.g. ubi).
			 */
			sprintf(dev_name[i], "nand%d", i);
			mtd->name = dev_name[i++];
			add_mtd_device(mtd);
#endif
#ifdef CONFIG_PARTITIONS
			blkdev->if_type = IF_TYPE_MTD_NAND;
			blkdev->dev = blkdev - nand_blkdev; /* array index */
			blkdev->type = DEV_TYPE_HARDDISK;
			blkdev->lba = mtd->size / mtd->writesize;
			blkdev->blksz = mtd->writesize;
			blkdev->block_read = nand_bread;
			blkdev->block_write = nand_bwrite;
			blkdev->block_erase = nand_berase;
			/*
			 * At this point, we don't have environment variables
			 * available.  The MTD's partition definitions are
			 * stored in the environment.  That means that we can
			 * not do an init_part() yet.
			 */
			blkdev->part_type = PART_TYPE_UNKNOWN;
#endif
		} else
			mtd->name = NULL;
	} else {
		mtd->name = NULL;
		mtd->size = 0;
	}

}

void nand_init(void)
{
	int i;
	unsigned int size = 0;
	for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++) {
		nand_init_chip(&nand_info[i], &nand_chip[i], &nand_blkdev[i],
							base_address[i]);
		size += nand_info[i].size / 1024;
		if (nand_curr_device == -1)
			nand_curr_device = i;
	}
	printf("%u MiB\n", size / 1024);

#ifdef CONFIG_SYS_NAND_SELECT_DEVICE
	/*
	 * Select the chip in the board/cpu specific driver
	 */
	board_nand_select_device(nand_info[nand_curr_device].priv, nand_curr_device);
#endif
}

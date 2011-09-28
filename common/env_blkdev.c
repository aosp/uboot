/*
 * (C) Copyright 2011 Google, Inc.
 * (C) Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * Derived from env_mmc.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define DEBUG */

#include <common.h>

#include <environment.h>
#include <search.h>
#include <errno.h>

/* references to names in env_common.c */
extern uchar default_environment[];

char *env_name_spec = CONFIG_SYS_ENV_BLKDEV;

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_MIN_PARTITION_NUM
#define CONFIG_MIN_PARTITION_NUM 0
#endif
#ifndef CONFIG_MAX_PARTITION_NUM
#define CONFIG_MAX_PARTITION_NUM 10
#endif
#ifndef CONFIG_ENV_BLK_PARTITION
#define CONFIG_ENV_BLK_PARTITION "environment"
#endif

/* local functions */
static int block_get_env_ptn(block_dev_desc_t *dev,
				ulong *start, ulong *size, ulong *blksz)
{
	disk_partition_t info;
	int i;

	/*
	 * I thought about stashing the partition info for the case of
	 * being called multiple times.  I decided against that.  I
	 * didn't want to add another level of caching in case the
	 * user changes the partition table.
	 */
	init_part(dev);
	if (dev->part_type == PART_TYPE_UNKNOWN) {
		/* Could not determine the type of partition table. */
		return -1;
	}
	for (i = CONFIG_MIN_PARTITION_NUM; i <= CONFIG_MAX_PARTITION_NUM; i++) {
		if (get_partition_info(dev, i, &info))
			continue;
		if (strcmp((char *)info.name, CONFIG_ENV_BLK_PARTITION)) {
			/* Wrong name, on to next. */
			continue;
		}
		*start = info.start;
		*size = info.size;
		*blksz = info.blksz;
		return 0;
	}
	/* Couldn't find the environment partition. */
	return -1;
}

/* environment access functions */
uchar env_get_char_spec(int index)
{
	return *((uchar *)(gd->env_addr + index));
}

int env_init(void)
{
	/* use default */
	gd->env_addr = (ulong)&default_environment[0];
	gd->env_valid = 1;

	return 0;
}

#ifdef CONFIG_CMD_SAVEENV
int saveenv(void)
{
	/*
	 * Don't ask me why U-Boot block device functions take
	 * lbaint_t args, but return an unsigned long, or why U-Boot
	 * disk partitions are defined in terms of ulongs.
	 */
	block_dev_desc_t *dev;
	ulong start, size, blksz;
	env_t	env_new;
	char	*res;
	lbaint_t blocks_to_write;
	unsigned long blocks_written;

	dev = get_dev_by_name(CONFIG_SYS_ENV_BLKDEV);
	if (!dev) {
		error("Could not find environment device %s\n",
							CONFIG_SYS_ENV_BLKDEV);
		return 1;
	}

	if (block_get_env_ptn(dev, &start, &size, &blksz)) {
		error("Could not find environment partition\n");
		return 1;
	}

	res = (char *)&env_new.data;
	if (CONFIG_ENV_SIZE > (typeof(CONFIG_ENV_SIZE))size * blksz) {
		error("environment partition needs to be at least %u bytes.\n",
							CONFIG_ENV_SIZE);
		return 1;
	}
	if (hexport_r(&env_htab, '\0', &res, ENV_SIZE) < 0) {
		error("Cannot export environment: errno = %d\n", errno);
		return 1;
	}
	env_new.crc = crc32(0, env_new.data, ENV_SIZE);
	printf("Writing to environment partition on %s... ",
							CONFIG_SYS_ENV_BLKDEV);
	blocks_to_write = ALIGN(CONFIG_ENV_SIZE, blksz) / blksz;
	blocks_written = dev->block_write(dev->dev, start, blocks_to_write,
							  (u_char *)&env_new);
	if (blocks_written != blocks_to_write) {
		puts("failed\n");
		return 1;
	}

	puts("done\n");
	return 0;
}
#endif /* CONFIG_CMD_SAVEENV */

void env_relocate_spec(void)
{
#if !defined(ENV_IS_EMBEDDED)
	/*
	 * Some block devices (e.g. MTD) use environment variables for
	 * configuration but we are trying to create the environment right
	 * now.  To get out of this Catch 22, we start with the default
	 * environment (which can be set at compile time).
	 */
	set_default_env("Initializing environment with defaults\n");

#if !defined(CONFIG_ENV_NO_LOAD)
	block_dev_desc_t *dev;
	ulong start, size, blksz;
	lbaint_t blocks_to_read;
	unsigned long blocks_read;
	char buf[CONFIG_ENV_SIZE];

	dev = get_dev_by_name(CONFIG_SYS_ENV_BLKDEV);
	if (!dev) {
		error("Could not find environment device %s\n",
							CONFIG_SYS_ENV_BLKDEV);
		return;
	}

	if (block_get_env_ptn(dev, &start, &size, &blksz)) {
		error("Could not find environment partition\n");
		return;
	}

	blocks_to_read = ALIGN(ARRAY_SIZE(buf), blksz) / blksz;
	blocks_read = dev->block_read(dev->dev, start, blocks_to_read,
							  (u_char *)buf);
	if (blocks_read != blocks_to_read) {
		error("Could not read environment\n");
		return;
	}

	env_import(buf, 1);
	puts("Imported environment from " CONFIG_SYS_ENV_BLKDEV "\n");
#endif /* !CONFIG_ENV_NO_LOAD */
#endif /* !CONFIG_ENV_IS_EMBEDDED */
}

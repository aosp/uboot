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
#include <malloc.h>

/* references to names in env_common.c */
extern uchar default_environment[];

char *env_name_spec = CONFIG_SYS_ENV_BLKDEV;

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_ENV_BLK_PARTITION
#define CONFIG_ENV_BLK_PARTITION "environment"
#endif

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
	disk_partition_t ptn;
	env_t	env_new;
	char	*res;
	loff_t	num_bytes;
	int err;

	dev = get_dev_by_name(CONFIG_SYS_ENV_BLKDEV);
	if (!dev) {
		error("Could not find environment device %s\n",
							CONFIG_SYS_ENV_BLKDEV);
		return 1;
	}

	if (get_partition_by_name(dev, CONFIG_ENV_BLK_PARTITION, &ptn)) {
		error("Could not find environment partition\n");
		return 1;
	}

	res = (char *)&env_new.data;
	if (CONFIG_ENV_SIZE > (typeof(CONFIG_ENV_SIZE))ptn.size * ptn.blksz) {
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
	num_bytes = CONFIG_ENV_SIZE;
	err = partition_write_bytes(dev, &ptn, &num_bytes, &env_new);
	if (err)
		printf("failed with error %d\n", err);
	else
		puts("done\n");

	return err;
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
	disk_partition_t ptn;
	loff_t num_bytes;
	char *buf;
	int err;

	dev = get_dev_by_name(CONFIG_SYS_ENV_BLKDEV);
	if (!dev) {
		error("Could not find environment device %s\n",
							CONFIG_SYS_ENV_BLKDEV);
		return;
	}

	if (get_partition_by_name(dev, CONFIG_ENV_BLK_PARTITION, &ptn)) {
		error("Could not find environment partition\n");
		return;
	}

	num_bytes = CONFIG_ENV_SIZE;
	buf = malloc(num_bytes);
	if (!buf) {
		error("Could not allocate memory for environment\n");
		return;
	}
	err = partition_read_bytes(dev, &ptn, &num_bytes, buf);
	if (err) {
		error("Could not read environment (error=%d)\n", err);
		free(buf);
		return;
	}

	env_import(buf, 1);
	free(buf);
	puts("Imported environment from " CONFIG_SYS_ENV_BLKDEV "\n");
#endif /* !CONFIG_ENV_NO_LOAD */
#endif /* !CONFIG_ENV_IS_EMBEDDED */
}

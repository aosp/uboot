/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Author :
 *	Aneesh V	<aneesh@ti.com>
 *	Steve Sakoman	<steve@sakoman.com>
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
#include <asm/arch/mem.h>
#include <asm/arch/sys_proto.h>

/*****************************************************************
 * dieid_num_r(void) - read and set die ID
 *****************************************************************/
#define DIE_ID_REG_BASE		(OMAP44XX_L4_CORE_BASE + 0x2000)
#define DIE_ID_REG_OFFSET		0x200
void dieid_num_r(void)
{
	unsigned int reg = DIE_ID_REG_BASE + DIE_ID_REG_OFFSET;
	char *uid_s, die_id[34];
	u32 id[4];

	memset(die_id, 0, sizeof(die_id));

	uid_s = getenv("dieid#");

	if (uid_s == NULL) {
		id[0] = readl(reg); /* bits 31:0 */
		id[1] = readl(reg + 0x8); /* bits [63:32] */
		id[2] = readl(reg + 0xC); /* bits [95:64] */
		id[3] = readl(reg + 0x10); /* bits [127:96] */

		sprintf(die_id, "%08x%08x%08x%08x", id[3], id[2], id[1], id[0]);
		setenv("dieid#", die_id);
		uid_s = die_id;
	}

	printf("Die ID #%s\n", uid_s);
}

/*
 * get_board_rev() - get board revision
 */
u32 get_board_rev(void)
{
	return 0x3;
}

/******************************************
 * get_cpu_id(void) - extract cpu id
 ******************************************/
u32 get_cpu_id(void)
{
	struct ctrl_id *ctrl_base = (struct ctrl_id *)CTRL_BASE;
	return readl(&ctrl_base->idcode);
}

/*****************************************************************
 * get_silicon_type(void) - read prod_id_1 to get info on max clock rate
 *****************************************************************/
u32 get_silicon_type(void)
{
	struct ctrl_id *ctrl_base = (struct ctrl_id *)CTRL_BASE;
	return (readl(&ctrl_base->prod_id_1) & SILICON_TYPE_MASK) >> SILICON_TYPE_SHIFT;
}

/*************************************************************
 *  get_device_type(): tell if GP/HS/EMU/TST
 *************************************************************/
u32 get_device_type(void)
{
	struct ctrl_id *ctrl_base = (struct ctrl_id *)CTRL_BASE;
	return (readl(&ctrl_base->prod_id_0) & DEVICE_TYPE_MASK);
}

#ifdef CONFIG_DISPLAY_CPUINFO
/**
 * Print CPU information
 */
int print_cpuinfo (void)
{
	char *cpu_family_s, *cpu_s, *sec_s, *max_clk;
	u32 silicon_type = get_silicon_type();
	u32 cpu_id = get_cpu_id();
	u32 device_type = get_device_type();

	cpu_family_s = "OMAP";

	switch (cpu_id) {
	case OMAP4430_ES10:
		cpu_s = "4430 ES1.0";
		goto OMAP4430_type;
	case OMAP4430_ES20:
		cpu_s = "4430 ES2.0";
		goto OMAP4430_type;
	case OMAP4430_ES21:
		cpu_s = "4430 ES2.1";
		goto OMAP4430_type;
	case OMAP4430_ES22:
		cpu_s = "4430 ES2.2";
		goto OMAP4430_type;
	case OMAP4430_ES23:
		cpu_s = "4430 ES2.3";
OMAP4430_type:
		switch (silicon_type) {
		case SILICON_TYPE_LOW_PERFORMANCE:
		  max_clk = "800 MHz";
		  break;
		case SILICON_TYPE_STANDARD_PERFORMANCE:
		  max_clk = "1.0 GHz";
		  break;
		default:
		  max_clk = "? MHz";
		  printf("silicon_type 0x%x unknown\n", silicon_type);
		  break;
		}
		break;
	case OMAP4460_ES10:
		cpu_s = "4460";
		switch (silicon_type) {
		case SILICON_TYPE_STANDARD_PERFORMANCE:
		  max_clk = "1.2 GHz";
		  break;
		case SILICON_TYPE_HIGH_PERFORMANCE:
		  max_clk = "1.5 GHz";
		  break;
		default:
		  max_clk = "? MHz";
		  printf("silicon_type 0x%x unknown\n", silicon_type);
		  break;
		}
		break;
	default:
		cpu_s = "44XX";
		max_clk = "? Mhz";
		printf("cpu_id 0x%x unknown\n", cpu_id);
		break;
	}

	switch (device_type) {
	case DEVICE_TYPE_GP:
		sec_s = "GP";
		break;
	default:
		sec_s = "?";
		printf("device type 0x%x unknown\n", device_type);
		break;
	}

	printf("OMAP%s-%s, Max CPU Clock %s\n",
	       cpu_s, sec_s, max_clk);

	return 0;
}
#endif	/* CONFIG_DISPLAY_CPUINFO */

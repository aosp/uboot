/*
 * (C) Copyright 2012
 * Texas Instruments, <www.ti.com>
 * Carlos Leija <cileija@ti.com>
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

/*
 * Test CPFROM APIs
 */
#include <common.h>
#include <command.h>
#include <i2c.h>

// HS defines
#include <asm/arch/omap4_hal.h>

#define SYS_CLK_38_4                   0x0

#define PPA_SERV_HAL_CPAUTOLOAD        0x3A
#define PPA_SERV_HAL_CPINIT            0x3B
#define PPA_SERV_HAL_CPMSV             0x3D

void issueAutld(void)
{
	printf(" Issuing Autoload\n");
	if ( (SEC_ENTRY_Std_Ppa_Call (PPA_SERV_HAL_CPAUTOLOAD, 0)) == 0 )
		printf(" ... [cpfrom autoload] done\n\n");
	else
		printf("[ERROR] [cpfrom autoload] Failed!\n\n");
}

int do_cpfrom (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	U32 value;
	U32 VAL_BCH[5];

	char *cmd = argv[1];
	unsigned char cLdata=0;

	if (argc < 2) {
		printf("Not enough ARGS!!\n");
		goto cpfrom_cmd_usage;
	} else if (argc > 4) {
		printf("Too many ARGS!!\n");
		goto cpfrom_cmd_usage;
	} else if (argc == 2) {
		if (strcmp(cmd,"autoload") == 0) {
			printf(" Calling CPFROM AutoLoad\n");
			issueAutld();
		} else if (strcmp(cmd,"init") == 0) {
			cLdata = '0';
			printf(" Initializing CPFROM  \n");
			printf("   Turning Phoenix VPP resource\n");
			cLdata = 0x1;
			i2c_set_bus_num(0);
			i2c_write(0x48, 0x9c, 1, &cLdata, 1);  // VPP_CFG_GRP
			cLdata = 0x3;
			i2c_write(0x48, 0x9d, 1, &cLdata, 1);  // VPP_CFG_TRANS
			cLdata = 0x21;
			i2c_write(0x48, 0x9e, 1, &cLdata, 1);  // VPP_CFG_STATE
			printf ("   Setting voltage : ");
			cLdata = 0x8;
			i2c_write(0x48, 0x9f, 1, &cLdata, 1);  // VPP_CFG_VOLTAGE
			printf("    ... done\n\n");
			printf( " Note: In SDP, C60 reads +0.06V of selected voltage\n\n");

			printf(" -----------------------------------------\n");
			printf("  Using default platform clock speed 38.4 MHz\n");
			printf(" Calling CPFROM_Init\n");
			if ( (SEC_ENTRY_Std_Ppa_Call (PPA_SERV_HAL_CPINIT, 1, SYS_CLK_38_4 )) == 0 ){
				printf(" ... [cpfrom %s] done\n\n", cmd);
				issueAutld();
			} else
				printf("[ERROR] [cpfrom %s] Failed!\n\n", cmd);
		}
	} else {
		/* 3 or 4 arguments */
		if (strcmp(cmd,"msv") == 0) {
			hexStringtoInteger(argv[2],&value);
			VAL_BCH[0] = cpfrom_byte_reverse32(value);
			VAL_BCH[1] = bch_enc(1, VAL_BCH);

			VAL_BCH[0] = value;

			printf("   Input:\n");
			printf("     MSV Hex value : %x\n", value);

			printf("   Passing:\n");
			printf("     MSV_ECC       : %x\n", VAL_BCH[1]);

			if (argc > 3) {
				value = simple_strtoul(argv[3], NULL, 16);
				if (value == 5){
					printf(" Calling MSV function\n");
					if ( (SEC_ENTRY_Std_Ppa_Call (PPA_SERV_HAL_CPMSV, 2, VAL_BCH[0], VAL_BCH[1])) == 0 ){
						printf(" ... [cpfrom %s] done\n\n", cmd);
						issueAutld();
					} else
						printf("[ERROR] [cpfrom %s] Failed!\n\n", cmd);
				} else
					printf(" This is a dryrun only\n");
			} else
				printf(" This is a dryrun only\n");
		} else {
			printf(" Command [cpfrom %s] not supported!\n\n", cmd);
			goto cpfrom_cmd_usage;
		}
	}
	return 0;

cpfrom_cmd_usage:
	printf("Usage:\n%s\n", cmdtp->usage);
	return 1;
}

U_BOOT_CMD(cpfrom, 4, 1, do_cpfrom,
	"    cpfrom init\n"
	"    cpfrom autoload\n"
	"    cpfrom msv <MSV_hex_no_0x> 5<--- add to program\n",
	NULL);

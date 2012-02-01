/*
 * (C) Copyright 2011
 * Google, Inc.
 *
 * Based on omap4_panda.h:
 * (C) Copyright 2010
 * Texas Instruments Incorporated.
 * Steve Sakoman  <steve@sakoman.com>
 *
 * Configuration settings for the Google OMAP4 Tungsten board.
 * Based on Panda board settings.
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

#ifndef __CONFIG_H
#define __CONFIG_H

/*
 * High Level Configuration Options
 */
#define CONFIG_ARMV7		1	/* This is an ARM V7 CPU core */
#define CONFIG_OMAP		1	/* in a TI OMAP core */
#define CONFIG_OMAP44XX		1	/* which is a 44XX */
#define CONFIG_OMAP4430		1	/* which is in a 4430 */
#define CONFIG_ARCH_CPU_INIT

#define CONFIG_TUNGSTEN		1	/* working with Tungsten */
#define	CONFIG_FASTBOOT /* Android fast boot */

/* Get CPU defs */
#include <asm/arch/cpu.h>
#include <asm/arch/omap4.h>
#include <asm/sizes.h>

/* Display CPU and Board Info */
#define CONFIG_DISPLAY_CPUINFO		1
#define CONFIG_DISPLAY_BOARDINFO	1

/* Clock Defines */
#define V_OSCK			38400000	/* Clock output from T2 */
#define V_SCLK                   V_OSCK

#undef CONFIG_USE_IRQ				/* no support for IRQs */

#define CONFIG_OF_LIBFDT		1

#define CONFIG_CMDLINE_TAG		1	/* enable passing of ATAGs */
#define CONFIG_SETUP_MEMORY_TAGS	1
#define CONFIG_INITRD_TAG		1

/*
 * Size of malloc() pool
 * Total Size Environment - 95KB - must match partition table
 * Malloc - add 128KB
 */
#define CONFIG_ENV_IS_IN_BLKDEV
#define CONFIG_SYS_ENV_BLKDEV		"mmc0"
#define CONFIG_ENV_SIZE_KB		(95)
#define CONFIG_ENV_SIZE			((CONFIG_ENV_SIZE_KB) << 10)
#define CONFIG_MALLOC_SIZE_KB		(128)
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + ((CONFIG_MALLOC_SIZE_KB) << 10))
/* Vector Base */
#define CONFIG_SYS_CA9_VECTOR_BASE	SRAM_ROM_VECT_BASE

/*
 * Hardware drivers
 */

/*
 * serial port - NS16550 compatible
 */
#define V_NS16550_CLK			48000000

#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	(-4)
#define CONFIG_SYS_NS16550_CLK		V_NS16550_CLK
#define CONFIG_CONS_INDEX		3
#define CONFIG_SYS_NS16550_COM3		UART3_BASE

/*
 * select serial console configuration
 */
#define CONFIG_SERIAL3                  3 /* UART3 on Tungsten */
#define CONFIG_BAUDRATE			115200
#define CONFIG_SYS_BAUDRATE_TABLE	{4800, 9600, 19200, 38400, 57600,\
					115200}

/* I2C  */
#define CONFIG_HARD_I2C			1
#define CONFIG_SYS_I2C_SPEED		400000
#define CONFIG_SYS_I2C_SLAVE		1
#define CONFIG_SYS_I2C_BUS		0
#define CONFIG_SYS_I2C_BUS_SELECT	1
#define CONFIG_DRIVER_OMAP34XX_I2C	1
#define CONFIG_I2C_MULTI_BUS		1

/* TWL6030 */
#undef CONFIG_TWL6030_POWER		/* disable or else musb fails */

/* MMC */
#define CONFIG_GENERIC_MMC		1
#define CONFIG_MMC			1
#define CONFIG_OMAP_HSMMC		1
#define CONFIG_EFI_PARTITION		1
#define CONFIG_MIN_PARTITION_NUM	1
#define CONFIG_MMC_DEV			0
#define CONFIG_OMAP_MMC_USE_DMA_WRITES

/* USB */
#define CONFIG_MUSB_UDC			1
#define CONFIG_USB_OMAP3		1
#define CONFIG_MUSB_RXFIFO_DOUBLE	1
#define CONFIG_MUSB_DMA_MODE1		1

/* Disable some non-essential dpll and clock setup because it
 * increases power consumption and heat significantly.  We'll let
 * the kernel initialize these.
 */
#define CONFIG_SKIP_NON_ESSENTIAL_CLOCKS 1

/* Specify a lower voltage (and resulting MPU DPLL freq) to
 * reduce power consumption and heat in the bootloader or else
 * it's way unstable with TI's default settings.
 */
#define CONFIG_FORCE_TPS62361 /* always use TPS62361, don't do runtime
				 check of omap4430 vs omap4460 */
#define CONFIG_OMAP_TPS_MPU_MV 1025
#define CONFIG_OMAP4460_MPU_DPLL mpu_dpll_params_350mhz
#define CONFIG_OMAP4430_ES1_0_MPU_DPLL mpu_dpll_params_350mhz
#define CONFIG_OMAP4430_non_ES1_0_MPU_DPLL mpu_dpll_params_350mhz

/* Make sure that the ABE is clocked off the sysclk and not the 32KHz clock.
 * Timers used for remote synchronization as well as the external fref fed to
 * the TAS5713 can only source from sysclk, and must be phase locked with the
 * McBSP output (which is fed from the ABE)
 */
#define CONFIG_SYS_OMAP4_ABE_SYSCK 1

/* Fastboot settings
 */
/* Another macro may also be used or instead used to take care of the case
 * where fastboot is started at boot (to be incorporated) based on key press
 */
#define	CONFIG_CMD_FASTBOOT
#define	CONFIG_FASTBOOT_TRANSFER_BUFFER		(OMAP44XX_DRAM_ADDR_SPACE_START + SZ_16M)
#define	CONFIG_FASTBOOT_TRANSFER_BUFFER_SIZE	(SZ_512M - SZ_16M)
/* Fastboot product name */
#define	FASTBOOT_PRODUCT_NAME	"steelhead"
/* Fastboot reboot paramenter address, it's currently put at
 * PUBLIC_SAR_RAM1_FREE
 */
#define FASTBOOT_REBOOT_PARAMETER_ADDR (0x4a326000 + 0xA0C)
/* device to use */
#define FASTBOOT_BLKDEV                 "mmc0"
/* Use HS */
#define	USB_BCD_VERSION			0x0200

#define CONFIG_USB_DEVICE		1
/* Change these to suit your needs */
#define CONFIG_USBD_VENDORID		0x18d1
#define CONFIG_USBD_PRODUCTID		0x2c10
#define CONFIG_USBD_MANUFACTURER	"Google"
#define CONFIG_USBD_PRODUCT_NAME	"Tungsten"

/* Flash */
#define CONFIG_SYS_NO_FLASH	1

/* commands to include */
#include <config_cmd_default.h>

/* Enabled commands */
#define CONFIG_CMD_I2C		/* I2C serial bus support	*/
#define CONFIG_CMD_MMC		/* MMC support                  */
#define CONFIG_CMD_ENV		/* Environment support          */

/* Disabled commands */
#undef CONFIG_CMD_EXT2		/* EXT2 Support                 */
#undef CONFIG_CMD_FAT		/* FAT support                  */
#undef CONFIG_CMD_NET
#undef CONFIG_CMD_NFS
#undef CONFIG_CMD_FPGA		/* FPGA configuration Support   */
#undef CONFIG_CMD_IMLS		/* List all found images        */
#undef CONFIG_CMD_LOADB         /* don't need loading binary over serial */
#undef CONFIG_CMD_LOADS         /* don't need loading s-record over serial */

/*
 * Environment setup
 */
#define CONFIG_BOOTDELAY	0
/* use preboot to detect key press for fastboot */
#define CONFIG_PREBOOT
#define CONFIG_BOOTCOMMAND "booti"
#ifdef CONFIG_MFG
/*
 * Manufacturing build:
 * + Force the system into fastboot mode initially, and make sure that fastboot
 *   will never load a kernel from MMC.  The test harness can then force the
 *   system into the diagnostic console by either executing "fastboot continue"
 *   from the host, or by sending a CTRL-C via the serial console.
 *
 *   Note: this was originally being done by manipulating CONFIG_BOOTCOMMAND and
 *   CONFIG_PREBOOT, but that led to unforseen issues with the MFG bootcmd and
 *   preboot settings getting saved in the environment.  Rather than attempting
 *   to layer hack after hack on top of that approach trying to get things to
 *   behave, we just introduce a new #def which gets obeyed by main.c
 *
 * + Turn on the gpio console commands.
 */
#define CONFIG_BOOTCOMMAND_FORCE_OVERRIDE "fastboot"
#define CONFIG_CMD_GPIO
#define CONFIG_MMC_SAMSUNG_SMART
#define CONFIG_CMD_BLK
#define CONFIG_SYS_LONGHELP 1
#else
#undef CONFIG_SYS_LONGHELP	/* undef to save memory */
#endif

/* overloaded in board/google/tungsten/tungsten.c */
#ifndef __ASSEMBLY__
int name_to_gpio(const char *name);
#endif
#define name_to_gpio(name) name_to_gpio(name)

#define CONFIG_ENV_OVERWRITE

#define CONFIG_EXTRA_ENV_SETTINGS \
	"console=ttyS2,115200n8\0" \
	"usbtty=cdc_acm\0" \
	"fastboot_unlocked=1\0"

#define CONFIG_AUTO_COMPLETE		1

/*
 * Miscellaneous configurable options
 */

#undef CONFIG_SYS_HUSH_PARSER	/* use "hush" command parser */
#undef CONFIG_SYS_PROMPT_HUSH_PS2
#define CONFIG_SYS_PROMPT		"Tungsten # "
#define CONFIG_SYS_CBSIZE		256
/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		16
/* Boot Argument Buffer Size */
#define CONFIG_SYS_BARGSIZE		(CONFIG_SYS_CBSIZE)

/*
 * memtest setup
 */
#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + (32 << 20))

/* Default load address */
#define CONFIG_SYS_LOAD_ADDR		0x80000000

/* Use General purpose timer 1 */
#define CONFIG_SYS_TIMERBASE		GPT2_BASE
#define CONFIG_SYS_PTV			2	/* Divisor: 2^(PTV+1) => 8 */
#define CONFIG_SYS_HZ			1000

/*
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128 << 10)	/* Regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4 << 10)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4 << 10)	/* FIQ stack */
#endif

/* which initialization functions to call for this board */
#define BOARD_LATE_INIT			1

/*
 * SDRAM Memory Map
 * Even though we use two CS all the memory
 * is mapped to one contiguous block
 */
#define CONFIG_NR_DRAM_BANKS	1

#define CONFIG_SYS_SDRAM_BASE		0x80000000
#define CONFIG_SYS_INIT_RAM_ADDR	0x4030D800
#define CONFIG_SYS_INIT_RAM_SIZE	0x800
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR + \
					 CONFIG_SYS_INIT_RAM_SIZE - \
					 GENERATED_GBL_DATA_SIZE)

#ifndef CONFIG_SYS_L2CACHE_OFF
#define CONFIG_SYS_L2_PL310		1
#define CONFIG_SYS_PL310_BASE	0x48242000
#endif

/* Defines for SDRAM init */
#ifndef CONFIG_SYS_EMIF_PRECALCULATED_TIMING_REGS
#define CONFIG_SYS_AUTOMATIC_SDRAM_DETECTION
#define CONFIG_SYS_DEFAULT_LPDDR2_TIMINGS
#endif

/* Defines for SPL */
#define CONFIG_SPL
#define CONFIG_SPL_TEXT_BASE		0x40304350
#define CONFIG_SPL_MAX_SIZE		(38 * 1024)
#define CONFIG_SPL_STACK		LOW_LEVEL_SRAM_STACK

#define CONFIG_SPL_BSS_START_ADDR	0x80000000
#define CONFIG_SPL_BSS_MAX_SIZE		0x80000		/* 512 KB */

#define CONFIG_SPL_VERIFY_IMAGE

/* These values need to match the partition table in tungsten.c */
/* emmc offset: 0x80000 */
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	                0x400
/* emmc offset: 0x180000 */
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_ALTERNATE_SECTOR	0xc00
#define CONFIG_SYS_U_BOOT_MAX_SIZE_SECTORS	0x400 /* 512 KB */

#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBDISK_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv7/omap-common/u-boot-spl.lds"

/*
 * 1MB into the SDRAM to allow for SPL's bss at the beginning of SDRAM
 * 64 bytes before this address should be set aside for u-boot.img's
 * header. That is 0x800FFFC0--0x80100000 should not be used for any
 * other needs.
 */
#define CONFIG_SYS_TEXT_BASE		0x80100000

#endif /* __CONFIG_H */

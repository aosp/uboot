/*
 * (C) Copyright 2006-2010
 * Texas Instruments, <www.ti.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef _CPU_H
#define _CPU_H

#if !(defined(__KERNEL_STRICT_NAMES) || defined(__ASSEMBLY__))
#include <asm/types.h>
#endif /* !(__KERNEL_STRICT_NAMES || __ASSEMBLY__) */

#ifndef __KERNEL_STRICT_NAMES
#ifndef __ASSEMBLY__
struct ctrl_id {
	u8 res1[0x200];
	u32 die_id_0;		/* 0x200 */
	u32 idcode;		/* 0x204 */
	u32 die_id_1;		/* 0x208 */
	u32 die_id_2;		/* 0x20C */
	u32 die_id_3;		/* 0x210 */
	u32 prod_id_0;		/* 0x214 */
	u32 prod_id_1;		/* 0x218 */
};
#endif /* __ASSEMBLY__ */
#endif /* __KERNEL_STRICT_NAMES */

/*
 * ID_CODE values
 */
#define OMAP4430_ES10	0x0b85202f
#define OMAP4430_ES20	0x1b85202f
#define OMAP4430_ES21	0x3b95c02f
#define OMAP4430_ES22	0x4b95c02f
#define OMAP4430_ES23	0x6b95c02f
#define OMAP4460_ES10	0x0b94e02f

/* device_type is bits 7:0 of prod_id_0 */
#define DEVICE_TYPE_MASK 0x000000ff
#define DEVICE_TYPE_GP   0xF0

/* silicon_type (for performance rating) is bits 17:16 of prod_id_1 */
#define SILICON_TYPE_MASK 0x00030000
#define SILICON_TYPE_SHIFT 16
#define SILICON_TYPE_LOW_PERFORMANCE       0x0
#define SILICON_TYPE_STANDARD_PERFORMANCE  0x1
#define SILICON_TYPE_HIGH_PERFORMANCE      0x2



#define GPMC_BASE		(OMAP44XX_GPMC_BASE)
#define GPMC_CONFIG_CS0		0x60
#define GPMC_CONFIG_CS0_BASE	(GPMC_BASE + GPMC_CONFIG_CS0)

#ifndef __KERNEL_STRICT_NAMES
#ifndef __ASSEMBLY__
struct gpmc_cs {
	u32 config1;		/* 0x00 */
	u32 config2;		/* 0x04 */
	u32 config3;		/* 0x08 */
	u32 config4;		/* 0x0C */
	u32 config5;		/* 0x10 */
	u32 config6;		/* 0x14 */
	u32 config7;		/* 0x18 */
	u32 nand_cmd;		/* 0x1C */
	u32 nand_adr;		/* 0x20 */
	u32 nand_dat;		/* 0x24 */
	u8 res[8];		/* blow up to 0x30 byte */
};

struct gpmc {
	u8 res1[0x10];
	u32 sysconfig;		/* 0x10 */
	u8 res2[0x4];
	u32 irqstatus;		/* 0x18 */
	u32 irqenable;		/* 0x1C */
	u8 res3[0x20];
	u32 timeout_control;	/* 0x40 */
	u8 res4[0xC];
	u32 config;		/* 0x50 */
	u32 status;		/* 0x54 */
	u8 res5[0x8];	/* 0x58 */
	struct gpmc_cs cs[8];	/* 0x60, 0x90, .. */
	u8 res6[0x14];		/* 0x1E0 */
	u32 ecc_config;		/* 0x1F4 */
	u32 ecc_control;	/* 0x1F8 */
	u32 ecc_size_config;	/* 0x1FC */
	u32 ecc1_result;	/* 0x200 */
	u32 ecc2_result;	/* 0x204 */
	u32 ecc3_result;	/* 0x208 */
	u32 ecc4_result;	/* 0x20C */
	u32 ecc5_result;	/* 0x210 */
	u32 ecc6_result;	/* 0x214 */
	u32 ecc7_result;	/* 0x218 */
	u32 ecc8_result;	/* 0x21C */
	u32 ecc9_result;	/* 0x220 */
};

/* Used for board specific gpmc initialization */
extern struct gpmc *gpmc_cfg;

struct gptimer {
	u32 tidr;		/* 0x00 r */
	u8 res[0xc];
	u32 tiocp_cfg;		/* 0x10 rw */
	u32 tistat;		/* 0x14 r */
	u32 tisr;		/* 0x18 rw */
	u32 tier;		/* 0x1c rw */
	u32 twer;		/* 0x20 rw */
	u32 tclr;		/* 0x24 rw */
	u32 tcrr;		/* 0x28 rw */
	u32 tldr;		/* 0x2c rw */
	u32 ttgr;		/* 0x30 rw */
	u32 twpc;		/* 0x34 r */
	u32 tmar;		/* 0x38 rw */
	u32 tcar1;		/* 0x3c r */
	u32 tcicr;		/* 0x40 rw */
	u32 tcar2;		/* 0x44 r */
};
#endif /* __ASSEMBLY__ */
#endif /* __KERNEL_STRICT_NAMES */

/* enable sys_clk NO-prescale /1 */
#define GPT_EN			((0x0 << 2) | (0x1 << 1) | (0x1 << 0))

/* Watchdog */
#ifndef __KERNEL_STRICT_NAMES
#ifndef __ASSEMBLY__
struct watchdog {
	u8 res1[0x34];
	u32 wwps;		/* 0x34 r */
	u8 res2[0x10];
	u32 wspr;		/* 0x48 rw */
};
#endif /* __ASSEMBLY__ */
#endif /* __KERNEL_STRICT_NAMES */

#define WD_UNLOCK1		0xAAAA
#define WD_UNLOCK2		0x5555

#define SYSCLKDIV_1		(0x1 << 6)
#define SYSCLKDIV_2		(0x1 << 7)

#define CLKSEL_GPT1		(0x1 << 0)

#define EN_GPT1			(0x1 << 0)
#define EN_32KSYNC		(0x1 << 2)

#define ST_WDT2			(0x1 << 5)

#define RESETDONE		(0x1 << 0)

#define TCLR_ST			(0x1 << 0)
#define TCLR_AR			(0x1 << 1)
#define TCLR_PRE		(0x1 << 5)

/* GPMC BASE */
#define GPMC_BASE		(OMAP44XX_GPMC_BASE)

/* I2C base */
#define I2C_BASE1		(OMAP44XX_L4_PER_BASE + 0x70000)
#define I2C_BASE2		(OMAP44XX_L4_PER_BASE + 0x72000)
#define I2C_BASE3		(OMAP44XX_L4_PER_BASE + 0x60000)

/* MUSB base */
#define MUSB_BASE		(OMAP44XX_L4_CORE_BASE + 0xAB000)



#define BIT0  (1<<0)
#define BIT1  (1<<1)
#define BIT2  (1<<2)
#define BIT3  (1<<3)
#define BIT4  (1<<4)
#define BIT5  (1<<5)
#define BIT6  (1<<6)
#define BIT7  (1<<7)
#define BIT8  (1<<8)
#define BIT9  (1<<9)
#define BIT10 (1<<10)
#define BIT11 (1<<11)
#define BIT12 (1<<12)
#define BIT13 (1<<13)
#define BIT14 (1<<14)
#define BIT15 (1<<15)
#define BIT16 (1<<16)
#define BIT17 (1<<17)
#define BIT18 (1<<18)
#define BIT19 (1<<19)
#define BIT20 (1<<20)
#define BIT21 (1<<21)
#define BIT22 (1<<22)
#define BIT23 (1<<23)
#define BIT24 (1<<24)
#define BIT25 (1<<25)
#define BIT26 (1<<26)
#define BIT27 (1<<27)
#define BIT28 (1<<28)
#define BIT29 (1<<29)
#define BIT30 (1<<30)
#define BIT31 (1<<31)

  /* PRCM */
#define CM_SYS_CLKSEL				0x4a306110

/* PRM.CKGEN module registers */
#define CM_ABE_PLL_REF_CLKSEL		0x4a30610c


/* PRM.WKUP_CM module registers */
#define CM_WKUP_CLKSTCTRL		0x4a307800
#define CM_WKUP_L4WKUP_CLKCTRL		0x4a307820
#define CM_WKUP_WDT1_CLKCTRL		0x4a307828
#define CM_WKUP_WDT2_CLKCTRL		0x4a307830
#define CM_WKUP_GPIO1_CLKCTRL		0x4a307838
#define CM_WKUP_TIMER1_CLKCTRL		0x4a307840
#define CM_WKUP_TIMER12_CLKCTRL		0x4a307848
#define CM_WKUP_SYNCTIMER_CLKCTRL	0x4a307850
#define CM_WKUP_USIM_CLKCTRL		0x4a307858
#define CM_WKUP_SARRAM_CLKCTRL		0x4a307860
#define CM_WKUP_KEYBOARD_CLKCTRL	0x4a307878
#define CM_WKUP_RTC_CLKCTRL		0x4a307880
#define CM_WKUP_BANDGAP_CLKCTRL		0x4a307888

/* CM1.CKGEN module registers */
#define CM_CLKSEL_CORE				0x4a004100
#define CM_CLKSEL_ABE				0x4a004108
#define CM_DLL_CTRL				0x4a004110
#define CM_CLKMODE_DPLL_CORE			0x4a004120
#define CM_IDLEST_DPLL_CORE			0x4a004124
#define CM_AUTOIDLE_DPLL_CORE			0x4a004128
#define CM_CLKSEL_DPLL_CORE			0x4a00412c
#define CM_DIV_M2_DPLL_CORE			0x4a004130
#define CM_DIV_M3_DPLL_CORE			0x4a004134
#define CM_DIV_M4_DPLL_CORE			0x4a004138
#define CM_DIV_M5_DPLL_CORE			0x4a00413c
#define CM_DIV_M6_DPLL_CORE			0x4a004140
#define CM_DIV_M7_DPLL_CORE			0x4a004144
#define CM_SSC_DELTAMSTEP_DPLL_CORE		0x4a004148
#define CM_SSC_MODFREQDIV_DPLL_CORE		0x4a00414c
#define CM_EMU_OVERRIDE_DPLL_CORE		0x4a004150
#define CM_CLKMODE_DPLL_MPU			0x4a004160
#define CM_IDLEST_DPLL_MPU			0x4a004164
#define CM_AUTOIDLE_DPLL_MPU			0x4a004168
#define CM_CLKSEL_DPLL_MPU			0x4a00416c
#define CM_DIV_M2_DPLL_MPU			0x4a004170
#define CM_SSC_DELTAMSTEP_DPLL_MPU		0x4a004188
#define CM_SSC_MODFREQDIV_DPLL_MPU		0x4a00418c
#define CM_BYPCLK_DPLL_MPU			0x4a00419c
#define CM_CLKMODE_DPLL_IVA			0x4a0041a0
#define CM_IDLEST_DPLL_IVA			0x4a0041a4
#define CM_AUTOIDLE_DPLL_IVA			0x4a0041a8
#define CM_CLKSEL_DPLL_IVA			0x4a0041ac
#define CM_DIV_M4_DPLL_IVA			0x4a0041b8
#define CM_DIV_M5_DPLL_IVA			0x4a0041bc
#define CM_SSC_DELTAMSTEP_DPLL_IVA		0x4a0041c8
#define CM_SSC_MODFREQDIV_DPLL_IVA		0x4a0041cc
#define CM_BYPCLK_DPLL_IVA			0x4a0041dc
#define CM_CLKMODE_DPLL_ABE			0x4a0041e0
#define CM_IDLEST_DPLL_ABE			0x4a0041e4
#define CM_AUTOIDLE_DPLL_ABE			0x4a0041e8
#define CM_CLKSEL_DPLL_ABE			0x4a0041ec
#define CM_DIV_M2_DPLL_ABE			0x4a0041f0
#define CM_DIV_M3_DPLL_ABE			0x4a0041f4
#define CM_SSC_DELTAMSTEP_DPLL_ABE		0x4a004208
#define CM_SSC_MODFREQDIV_DPLL_ABE		0x4a00420c
#define CM_CLKMODE_DPLL_DDRPHY			0x4a004220
#define CM_IDLEST_DPLL_DDRPHY			0x4a004224
#define CM_AUTOIDLE_DPLL_DDRPHY			0x4a004228
#define CM_CLKSEL_DPLL_DDRPHY			0x4a00422c
#define CM_DIV_M2_DPLL_DDRPHY			0x4a004230
#define CM_DIV_M4_DPLL_DDRPHY			0x4a004238
#define CM_DIV_M5_DPLL_DDRPHY			0x4a00423c
#define CM_DIV_M6_DPLL_DDRPHY			0x4a004240
#define CM_SSC_DELTAMSTEP_DPLL_DDRPHY		0x4a004248

/* CM1.ABE register offsets */
#define CM1_ABE_CLKSTCTRL		0x4a004500
#define CM1_ABE_L4ABE_CLKCTRL		0x4a004520
#define CM1_ABE_AESS_CLKCTRL		0x4a004528
#define CM1_ABE_PDM_CLKCTRL		0x4a004530
#define CM1_ABE_DMIC_CLKCTRL		0x4a004538
#define CM1_ABE_MCASP_CLKCTRL		0x4a004540
#define CM1_ABE_MCBSP1_CLKCTRL		0x4a004548
#define CM1_ABE_MCBSP2_CLKCTRL		0x4a004550
#define CM1_ABE_MCBSP3_CLKCTRL		0x4a004558
#define CM1_ABE_SLIMBUS_CLKCTRL		0x4a004560
#define CM1_ABE_TIMER5_CLKCTRL		0x4a004568
#define CM1_ABE_TIMER6_CLKCTRL		0x4a004570
#define CM1_ABE_TIMER7_CLKCTRL		0x4a004578
#define CM1_ABE_TIMER8_CLKCTRL		0x4a004580
#define CM1_ABE_WDT3_CLKCTRL		0x4a004588

/* CM1.DSP register offsets */
#define DSP_CLKSTCTRL			0x4a004400
#define	DSP_DSP_CLKCTRL			0x4a004420

/* CM2.CKGEN module registers */
#define CM_CLKSEL_DUCATI_ISS_ROOT		0x4a008100
#define CM_CLKSEL_USB_60MHz			0x4a008104
#define CM_SCALE_FCLK				0x4a008108
#define CM_CORE_DVFS_PERF1			0x4a008110
#define CM_CORE_DVFS_PERF2			0x4a008114
#define CM_CORE_DVFS_PERF3			0x4a008118
#define CM_CORE_DVFS_PERF4			0x4a00811c
#define CM_CORE_DVFS_CURRENT			0x4a008124
#define CM_IVA_DVFS_PERF_TESLA			0x4a008128
#define CM_IVA_DVFS_PERF_IVAHD			0x4a00812c
#define CM_IVA_DVFS_PERF_ABE			0x4a008130
#define CM_IVA_DVFS_CURRENT			0x4a008138
#define CM_CLKMODE_DPLL_PER			0x4a008140
#define CM_IDLEST_DPLL_PER			0x4a008144
#define CM_AUTOIDLE_DPLL_PER			0x4a008148
#define CM_CLKSEL_DPLL_PER			0x4a00814c
#define CM_DIV_M2_DPLL_PER			0x4a008150
#define CM_DIV_M3_DPLL_PER			0x4a008154
#define CM_DIV_M4_DPLL_PER			0x4a008158
#define CM_DIV_M5_DPLL_PER			0x4a00815c
#define CM_DIV_M6_DPLL_PER			0x4a008160
#define CM_DIV_M7_DPLL_PER			0x4a008164
#define CM_SSC_DELTAMSTEP_DPLL_PER		0x4a008168
#define CM_SSC_MODFREQDIV_DPLL_PER		0x4a00816c
#define CM_EMU_OVERRIDE_DPLL_PER		0x4a008170
#define CM_CLKMODE_DPLL_USB			0x4a008180
#define CM_IDLEST_DPLL_USB			0x4a008184
#define CM_AUTOIDLE_DPLL_USB			0x4a008188
#define CM_CLKSEL_DPLL_USB			0x4a00818c
#define CM_DIV_M2_DPLL_USB			0x4a008190
#define CM_SSC_DELTAMSTEP_DPLL_USB		0x4a0081a8
#define CM_SSC_MODFREQDIV_DPLL_USB		0x4a0081ac
#define CM_CLKDCOLDO_DPLL_USB			0x4a0081b4
#define CM_CLKMODE_DPLL_UNIPRO			0x4a0081c0
#define CM_IDLEST_DPLL_UNIPRO			0x4a0081c4
#define CM_AUTOIDLE_DPLL_UNIPRO			0x4a0081c8
#define CM_CLKSEL_DPLL_UNIPRO			0x4a0081cc
#define CM_DIV_M2_DPLL_UNIPRO			0x4a0081d0
#define CM_SSC_DELTAMSTEP_DPLL_UNIPRO		0x4a0081e8
#define CM_SSC_MODFREQDIV_DPLL_UNIPRO		0x4a0081ec

/* CM2.CORE module registers */
#define CM_L3_1_CLKSTCTRL		0x4a008700
#define CM_L3_1_DYNAMICDEP		0x4a008708
#define CM_L3_1_L3_1_CLKCTRL		0x4a008720
#define CM_L3_2_CLKSTCTRL		0x4a008800
#define CM_L3_2_DYNAMICDEP		0x4a008808
#define CM_L3_2_L3_2_CLKCTRL		0x4a008820
#define CM_L3_2_GPMC_CLKCTRL		0x4a008828
#define CM_L3_2_OCMC_RAM_CLKCTRL	0x4a008830
#define CM_DUCATI_CLKSTCTRL		0x4a008900
#define CM_DUCATI_STATICDEP		0x4a008904
#define CM_DUCATI_DYNAMICDEP		0x4a008908
#define CM_DUCATI_DUCATI_CLKCTRL	0x4a008920
#define CM_SDMA_CLKSTCTRL		0x4a008a00
#define CM_SDMA_STATICDEP		0x4a008a04
#define CM_SDMA_DYNAMICDEP		0x4a008a08
#define CM_SDMA_SDMA_CLKCTRL		0x4a008a20
#define CM_MEMIF_CLKSTCTRL		0x4a008b00
#define CM_MEMIF_DMM_CLKCTRL		0x4a008b20
#define CM_MEMIF_EMIF_FW_CLKCTRL	0x4a008b28
#define CM_MEMIF_EMIF_1_CLKCTRL		0x4a008b30
#define CM_MEMIF_EMIF_2_CLKCTRL		0x4a008b38
#define CM_MEMIF_DLL_CLKCTRL		0x4a008b40
#define CM_MEMIF_EMIF_H1_CLKCTRL	0x4a008b50
#define CM_MEMIF_EMIF_H2_CLKCTRL	0x4a008b58
#define CM_MEMIF_DLL_H_CLKCTRL		0x4a008b60
#define CM_D2D_CLKSTCTRL		0x4a008c00
#define CM_D2D_STATICDEP		0x4a008c04
#define CM_D2D_DYNAMICDEP		0x4a008c08
#define CM_D2D_SAD2D_CLKCTRL		0x4a008c20
#define CM_D2D_MODEM_ICR_CLKCTRL	0x4a008c28
#define CM_D2D_SAD2D_FW_CLKCTRL		0x4a008c30
#define CM_L4CFG_CLKSTCTRL		0x4a008d00
#define CM_L4CFG_DYNAMICDEP		0x4a008d08
#define CM_L4CFG_L4_CFG_CLKCTRL		0x4a008d20
#define CM_L4CFG_HW_SEM_CLKCTRL		0x4a008d28
#define CM_L4CFG_MAILBOX_CLKCTRL	0x4a008d30
#define CM_L4CFG_SAR_ROM_CLKCTRL	0x4a008d38
#define CM_L3INSTR_CLKSTCTRL		0x4a008e00
#define CM_L3INSTR_L3_3_CLKCTRL		0x4a008e20
#define CM_L3INSTR_L3_INSTR_CLKCTRL	0x4a008e28
#define CM_L3INSTR_OCP_WP1_CLKCTRL	0x4a008e40

/* CM2.L4PER register offsets */
#define CM_L4PER_CLKSTCTRL		0x4a009400
#define CM_L4PER_DYNAMICDEP		0x4a009408
#define CM_L4PER_ADC_CLKCTRL		0x4a009420
#define CM_L4PER_DMTIMER10_CLKCTRL	0x4a009428
#define CM_L4PER_DMTIMER11_CLKCTRL	0x4a009430
#define CM_L4PER_DMTIMER2_CLKCTRL	0x4a009438
#define CM_L4PER_DMTIMER3_CLKCTRL	0x4a009440
#define CM_L4PER_DMTIMER4_CLKCTRL	0x4a009448
#define CM_L4PER_DMTIMER9_CLKCTRL	0x4a009450
#define CM_L4PER_ELM_CLKCTRL		0x4a009458
#define CM_L4PER_GPIO2_CLKCTRL		0x4a009460
#define CM_L4PER_GPIO3_CLKCTRL		0x4a009468
#define CM_L4PER_GPIO4_CLKCTRL		0x4a009470
#define CM_L4PER_GPIO5_CLKCTRL		0x4a009478
#define CM_L4PER_GPIO6_CLKCTRL		0x4a009480
#define CM_L4PER_HDQ1W_CLKCTRL		0x4a009488
#define CM_L4PER_HECC1_CLKCTRL		0x4a009490
#define CM_L4PER_HECC2_CLKCTRL		0x4a009498
#define CM_L4PER_I2C1_CLKCTRL		0x4a0094a0
#define CM_L4PER_I2C2_CLKCTRL		0x4a0094a8
#define CM_L4PER_I2C3_CLKCTRL		0x4a0094b0
#define CM_L4PER_I2C4_CLKCTRL		0x4a0094b8
#define CM_L4PER_L4PER_CLKCTRL		0x4a0094c0
#define CM_L4PER_MCASP2_CLKCTRL		0x4a0094d0
#define CM_L4PER_MCASP3_CLKCTRL		0x4a0094d8
#define CM_L4PER_MCBSP4_CLKCTRL		0x4a0094e0
#define CM_L4PER_MGATE_CLKCTRL		0x4a0094e8
#define CM_L4PER_MCSPI1_CLKCTRL		0x4a0094f0
#define CM_L4PER_MCSPI2_CLKCTRL		0x4a0094f8
#define CM_L4PER_MCSPI3_CLKCTRL		0x4a009500
#define CM_L4PER_MCSPI4_CLKCTRL		0x4a009508
#define CM_L4PER_MMCSD3_CLKCTRL		0x4a009520
#define CM_L4PER_MMCSD4_CLKCTRL		0x4a009528
#define CM_L4PER_MSPROHG_CLKCTRL	0x4a009530
#define CM_L4PER_SLIMBUS2_CLKCTRL	0x4a009538
#define CM_L4PER_UART1_CLKCTRL		0x4a009540
#define CM_L4PER_UART2_CLKCTRL		0x4a009548
#define CM_L4PER_UART3_CLKCTRL		0x4a009550
#define CM_L4PER_UART4_CLKCTRL		0x4a009558
#define CM_L4PER_MMCSD5_CLKCTRL		0x4a009560
#define CM_L4PER_I2C5_CLKCTRL		0x4a009568
#define CM_L4SEC_CLKSTCTRL		0x4a009580
#define CM_L4SEC_STATICDEP		0x4a009584
#define CM_L4SEC_DYNAMICDEP		0x4a009588
#define CM_L4SEC_AES1_CLKCTRL		0x4a0095a0
#define CM_L4SEC_AES2_CLKCTRL		0x4a0095a8
#define CM_L4SEC_DES3DES_CLKCTRL	0x4a0095b0
#define CM_L4SEC_PKAEIP29_CLKCTRL	0x4a0095b8
#define CM_L4SEC_RNG_CLKCTRL		0x4a0095c0
#define CM_L4SEC_SHA2MD51_CLKCTRL	0x4a0095c8
#define CM_L4SEC_CRYPTODMA_CLKCTRL	0x4a0095d8

/* CM2.IVAHD */
#define IVAHD_CLKSTCTRL			0x4a008f00
#define IVAHD_IVAHD_CLKCTRL		0x4a008f20
#define IVAHD_SL2_CLKCTRL		0x4a008f28

/* CM2.L3INIT */
#define CM_L3INIT_HSMMC1_CLKCTRL	0x4a009328
#define CM_L3INIT_HSMMC2_CLKCTRL	0x4a009330
#define CM_L3INIT_HSI_CLKCTRL           0x4a009338
#define CM_L3INIT_UNIPRO1_CLKCTRL       0x4a009340
#define CM_L3INIT_HSUSBHOST_CLKCTRL     0x4a009358
#define CM_L3INIT_HSUSBOTG_CLKCTRL      0x4a009360
#define CM_L3INIT_HSUSBTLL_CLKCTRL      0x4a009368
#define CM_L3INIT_P1500_CLKCTRL         0x4a009378
#define CM_L3INIT_FSUSB_CLKCTRL         0x4a0093d0
#define CM_L3INIT_USBPHY_CLKCTRL        0x4a0093e0

#endif /* _CPU_H */

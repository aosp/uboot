/*
 * (C) Copyright 2008
 * Texas Instruments, <www.ti.com>
 * Sukumar Ghorai <s-ghorai@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation's version 2 of
 * the License.
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
#include <mmc.h>
#include <part.h>
#include <i2c.h>
#include <twl4030.h>
#include <asm/io.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>


#ifdef CONFIG_SPL_BUILD
# undef CONFIG_OMAP_MMC_USE_DMA_WRITES
#endif

#ifdef CONFIG_OMAP_MMC_USE_DMA_WRITES
# include <asm/omap_sdma.h>
#endif

/* If we fail after 1 second wait, something is really bad */
#define MAX_RETRY_MS	1000

static int mmc_read_data(hsmmc_t *mmc_base, char *buf, unsigned int size);
static int mmc_write_data(hsmmc_t *mmc_base, const char *buf, unsigned int size);
#ifdef CONFIG_OMAP_MMC_USE_DMA_WRITES
static void mmc_dma_start(struct mmc* mmc, hsmmc_t *mmc_base, const char *buf, unsigned int blkSiz, unsigned int numBlk, int write);
static int mmc_dma_wait_for_transfer_complete(struct mmc* mmc, hsmmc_t *mmc_base, const char *buf, unsigned int blkSiz, unsigned int numBlk);
#endif
static struct mmc hsmmc_dev[3];
unsigned char mmc_board_init(struct mmc *mmc)
{
#if defined(CONFIG_TWL4030_POWER)
	twl4030_power_mmc_init();
#endif

#if defined(CONFIG_OMAP34XX)
	t2_t *t2_base = (t2_t *)T2_BASE;

	writel(readl(&t2_base->pbias_lite) | PBIASLITEPWRDNZ1 |
		PBIASSPEEDCTRL0 | PBIASLITEPWRDNZ0,
		&t2_base->pbias_lite);

	writel(readl(&t2_base->devconf0) | MMCSDIO1ADPCLKISEL,
		&t2_base->devconf0);

#endif

#if defined(CONFIG_OMAP44XX)
	unsigned char data;
	unsigned int reg;
	t2_t *t2_base = (t2_t *)T2_BASE;

	switch (mmc->block_dev.dev) {
	case 0:
		/* Phoenix LDO config */
		i2c_set_bus_num(0);
		data = 0x01;
		i2c_write(0x48, 0x98, 1, &data, 1);
		data = 0x03;
		i2c_write(0x48, 0x99, 1, &data, 1);
		data = 0x21;
		i2c_write(0x48, 0x9A, 1, &data, 1);
		data = 0x15;
		i2c_write(0x48, 0x9B, 1, &data, 1);

		/* Wait for the power to stabilize before setting PWRDNZ */
		udelay(100);

		/* SLOT-0 PBIAS config - 3v IO */
		reg  = readl(&t2_base->pbias_lite);
		reg |=  MMC1_PBIASLITE_VMODE |
			MMC1_PBIASLITE_PWRDNZ |
			MMC1_PWRDNZ;
		writel(reg, &t2_base->pbias_lite);

		/* Wait for the the supply detector to tell us if there is a
		 * mismatch between the PBIAS supply and what we have configured
		 * IO for
		 */
		udelay(100);
		reg = readl(&t2_base->pbias_lite);
		if (!(reg & MMC1_PBIASLITE_SUPPLY_HI_OUT)) {
			printf("Warning: 1.8v PBIAS supply detected after"
				" configuring for 3v.  Switching to 1.8v"
				" configuration.\n");
			reg &= ~(MMC1_PBIASLITE_VMODE |
				 MMC1_PBIASLITE_PWRDNZ |
				 MMC1_PWRDNZ);
			writel(reg, &t2_base->pbias_lite);

			udelay(100);
			reg = readl(&t2_base->pbias_lite);
			reg |=  MMC1_PBIASLITE_PWRDNZ |
				MMC1_PWRDNZ;
			writel(reg, &t2_base->pbias_lite);
		}

		writel(readl(&t2_base->control_mmc1) | SDMMC1_DR2_SPEEDCTRL |
		       SDMMC1_DR1_SPEEDCTRL | SDMMC1_DR0_SPEEDCTRL |
		       SDMMC1_PUSTRENGTH_GRP1 | SDMMC1_PUSTRENGTH_GRP0,
		       &t2_base->control_mmc1);
		break;
	case 1:
		break;
	default:
		break;
	}
#endif

	return 0;
}

void mmc_init_stream(hsmmc_t *mmc_base)
{
	ulong start;

	writel(readl(&mmc_base->con) | INIT_INITSTREAM, &mmc_base->con);

	writel(MMC_CMD0, &mmc_base->cmd);
	start = get_timer(0);
	while (!(readl(&mmc_base->stat) & CC_MASK)) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for cc!\n", __func__);
			return;
		}
	}
	writel(CC_MASK, &mmc_base->stat)
		;
	writel(MMC_CMD0, &mmc_base->cmd)
		;
	start = get_timer(0);
	while (!(readl(&mmc_base->stat) & CC_MASK)) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for cc2!\n", __func__);
			return;
		}
	}
	writel(readl(&mmc_base->con) & ~INIT_INITSTREAM, &mmc_base->con);
}


static int mmc_init_setup(struct mmc *mmc)
{
	hsmmc_t *mmc_base = (hsmmc_t *)mmc->priv;
	unsigned int reg_val;
	unsigned int dsor;
	ulong start;

	mmc_board_init(mmc);

	writel(readl(&mmc_base->sysconfig) | MMC_SOFTRESET,
		&mmc_base->sysconfig);
	start = get_timer(0);
	while ((readl(&mmc_base->sysstatus) & RESETDONE) == 0) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for cc2!\n", __func__);
			return TIMEOUT;
		}
	}
	writel(readl(&mmc_base->sysctl) | SOFTRESETALL, &mmc_base->sysctl);
	start = get_timer(0);
	while ((readl(&mmc_base->sysctl) & SOFTRESETALL) != 0x0) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for softresetall!\n",
				__func__);
			return TIMEOUT;
		}
	}
	writel(DTW_1_BITMODE | SDBP_PWROFF | SDVS_3V0, &mmc_base->hctl);
	writel(readl(&mmc_base->capa) | VS30_3V0SUP | VS18_1V8SUP,
		&mmc_base->capa);

	reg_val = readl(&mmc_base->con) & RESERVED_MASK;

	writel(CTPL_MMC_SD | reg_val | WPP_ACTIVEHIGH | CDP_ACTIVEHIGH |
		MIT_CTO | DW8_1_4BITMODE | MODE_FUNC | STR_BLOCK |
		HR_NOHOSTRESP | INIT_NOINIT | NOOPENDRAIN, &mmc_base->con);

	dsor = 240;
	mmc_reg_out(&mmc_base->sysctl, (ICE_MASK | DTO_MASK | CEN_MASK),
		(ICE_STOP | DTO_15THDTO | CEN_DISABLE));
	mmc_reg_out(&mmc_base->sysctl, ICE_MASK | CLKD_MASK,
		(dsor << CLKD_OFFSET) | ICE_OSCILLATE);
	start = get_timer(0);
	while ((readl(&mmc_base->sysctl) & ICS_MASK) == ICS_NOTREADY) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for ics!\n", __func__);
			return TIMEOUT;
		}
	}
	writel(readl(&mmc_base->sysctl) | CEN_ENABLE, &mmc_base->sysctl);

	writel(readl(&mmc_base->hctl) | SDBP_PWRON, &mmc_base->hctl);

	writel(IE_BADA | IE_CERR | IE_DEB | IE_DCRC | IE_DTO | IE_CIE |
		IE_CEB | IE_CCRC | IE_CTO | IE_BRR | IE_BWR | IE_TC | IE_CC,
		&mmc_base->ie);

	mmc_init_stream(mmc_base);

	return 0;
}


static int last_start_value;
static int last_end_value;

static int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
			struct mmc_data *data)
{
	hsmmc_t *mmc_base = (hsmmc_t *)mmc->priv;
	unsigned int flags, mmc_stat;
	ulong start;
	int canUseDma = 1;

	start = get_timer(0);
	while ((readl(&mmc_base->pstate) & DATI_MASK) == DATI_CMDDIS) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for cmddis!\n", __func__);
			return TIMEOUT;
		}
	}
	writel(0xFFFFFFFF, &mmc_base->stat);
	start = get_timer(0);
	while (readl(&mmc_base->stat)) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for stat!\n", __func__);
			return TIMEOUT;
		}
	}

	if ((cmd->cmdidx == SD_CMD_ERASE_WR_BLK_START) ||
	    (cmd->cmdidx == MMC_CMD_ERASE_GROUP_START))
		last_start_value = cmd->cmdarg;
	else if ((cmd->cmdidx == SD_CMD_ERASE_WR_BLK_END) ||
		   (cmd->cmdidx == MMC_CMD_ERASE_GROUP_END))
		last_end_value = cmd->cmdarg;

	/*
	 * CMDREG
	 * CMDIDX[13:8]	: Command index
	 * DATAPRNT[5]	: Data Present Select
	 * ENCMDIDX[4]	: Command Index Check Enable
	 * ENCMDCRC[3]	: Command CRC Check Enable
	 * RSPTYP[1:0]
	 *	00 = No Response
	 *	01 = Length 136
	 *	10 = Length 48
	 *	11 = Length 48 Check busy after response
	 */
	/* Delay added before checking the status of frq change
	 * retry not supported by mmc.c(core file)
	 */
	if (cmd->cmdidx == SD_CMD_APP_SEND_SCR)
		udelay(50000); /* wait 50 ms */

	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = 0;
	else if (cmd->resp_type & MMC_RSP_136)
		flags = RSP_TYPE_LGHT136 | CICE_NOCHECK;
	else if (cmd->resp_type & MMC_RSP_BUSY)
		flags = RSP_TYPE_LGHT48B;
	else
		flags = RSP_TYPE_LGHT48;

	/* enable default flags */
	flags =	flags | (CMD_TYPE_NORMAL | CICE_NOCHECK | CCCE_NOCHECK |
			MSBS_SGLEBLK | ACEN_DISABLE | BCE_DISABLE | DE_DISABLE);

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= CCCE_CHECK;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= CICE_CHECK;

	if (data) {
		if ((cmd->cmdidx == MMC_CMD_READ_MULTIPLE_BLOCK) ||
			 (cmd->cmdidx == MMC_CMD_WRITE_MULTIPLE_BLOCK)) {
			flags |= (MSBS_MULTIBLK | BCE_ENABLE);
			data->blocksize = 512;
			writel(data->blocksize | (data->blocks << 16),
							&mmc_base->blk);
		} else
			writel(data->blocksize | NBLK_STPCNT, &mmc_base->blk);

		if (data->flags & MMC_DATA_READ)
			flags |= (DP_DATA | DDIR_READ);
		else
			flags |= (DP_DATA | DDIR_WRITE);

#ifdef CONFIG_OMAP_MMC_USE_DMA_WRITES
		if (data->flags & MMC_DATA_WRITE) {

			if ((((unsigned)data->src) & 3) || (((unsigned)data->blocksize) & 3)) {
				canUseDma = 0;
			} else {
				flags = (flags &~ DE_DISABLE) | DE_ENABLE;

				mmc_dma_start(mmc, mmc_base, data->src,
						data->blocksize, data->blocks, 1);
			}
		}
#else
		canUseDma = 0;
#endif
	}

	writel(cmd->cmdarg, &mmc_base->arg);
	writel((cmd->cmdidx << 24) | flags, &mmc_base->cmd);

	start = get_timer(0);
	do {
		mmc_stat = readl(&mmc_base->stat);
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s : timeout: No status update\n", __func__);
			return TIMEOUT;
		}
	} while (!mmc_stat);

	if ((mmc_stat & IE_CTO) != 0)
		return TIMEOUT;
	else if ((mmc_stat & ERRI_MASK) != 0)
		return -1;

	if (mmc_stat & CC_MASK) {
		writel(CC_MASK, &mmc_base->stat);
		if (cmd->resp_type & MMC_RSP_PRESENT) {
			if (cmd->resp_type & MMC_RSP_136) {
				/* response type 2 */
				cmd->response[3] = readl(&mmc_base->rsp10);
				cmd->response[2] = readl(&mmc_base->rsp32);
				cmd->response[1] = readl(&mmc_base->rsp54);
				cmd->response[0] = readl(&mmc_base->rsp76);
			} else
				/* response types 1, 1b, 3, 4, 5, 6 */
				cmd->response[0] = readl(&mmc_base->rsp10);
		}
	}

	if (data && (data->flags & MMC_DATA_READ)) {
		mmc_read_data(mmc_base,	data->dest,
				data->blocksize * data->blocks);
	} else if (data && (data->flags & MMC_DATA_WRITE)) {
	
		if (canUseDma) {
#ifdef CONFIG_OMAP_MMC_USE_DMA_WRITES
			return mmc_dma_wait_for_transfer_complete(mmc, mmc_base, data->src,
					data->blocksize, data->blocks);
#endif
		} else {
			return mmc_write_data(mmc_base, data->src,
					data->blocksize * data->blocks);
		}
	}

	/* If this is an erase, wait for it to complete.  Add a fixed
	 * additional time to the timeout to allow the device to manage its
	 * blocks (10X normal retry) plus add an additional time based on
	 * number of blocks being erased (1 ms per 1024 blocks).  In
	 * addition, when that time expires, only a warning is printed and
	 * an error will not be returned until double the time has passed.
	 * Callers should try to avoid erasing very large numbers of blocks
	 * at once to prevent timeouts so long that users can't tell if
	 * progress is being made.
	 */
	if (cmd->cmdidx == MMC_CMD_ERASE) {
		ulong timeout_ms = MAX_RETRY_MS * 10;
		ulong erase_blk_cnt = last_end_value - last_start_value + 1;
		int warned = 0;
		timeout_ms += DIV_ROUND_UP(erase_blk_cnt, 1024);
		start = get_timer(0);
		while ((readl(&mmc_base->pstate) & DATI_MASK) == DATI_CMDDIS) {
			if (get_timer(0) - start > timeout_ms) {
				printf("%s: %s erasing blocks %u to %u, "
					"timeout = %lu seconds\n",
					__func__,
					warned ? "Timed out" : "Warning",
					last_start_value, last_end_value,
					timeout_ms / 1000);
				if (warned) {
					return TIMEOUT;
				} else {
					warned = 1;
					timeout_ms *= 2;
				}
			}
		}
	}

	return 0;
}

static int mmc_read_data(hsmmc_t *mmc_base, char *buf, unsigned int size)
{
	unsigned int *output_buf = (unsigned int *)buf;
	unsigned int mmc_stat;
	unsigned int count;

	/*
	 * Start Polled Read
	 */
	count = (size > MMCSD_SECTOR_SIZE) ? MMCSD_SECTOR_SIZE : size;
	count /= 4;

	while (size) {
		ulong start = get_timer(0);
		do {
			mmc_stat = readl(&mmc_base->stat);
			if (get_timer(0) - start > MAX_RETRY_MS) {
				printf("%s: timedout waiting for status!\n",
						__func__);
				return TIMEOUT;
			}
		} while (mmc_stat == 0);

		if ((mmc_stat & ERRI_MASK) != 0)
			return 1;

		if (mmc_stat & BRR_MASK) {
			unsigned int k;

			writel(readl(&mmc_base->stat) | BRR_MASK,
				&mmc_base->stat);
			for (k = 0; k < count; k++) {
				*output_buf = readl(&mmc_base->data);
				output_buf++;
			}
			size -= (count*4);
		}

		if (mmc_stat & BWR_MASK)
			writel(readl(&mmc_base->stat) | BWR_MASK,
				&mmc_base->stat);

		if (mmc_stat & TC_MASK) {
			writel(readl(&mmc_base->stat) | TC_MASK,
				&mmc_base->stat);
			break;
		}
	}
	return 0;
}

static int mmc_write_data(hsmmc_t *mmc_base, const char *buf, unsigned int size)
{
	unsigned int *input_buf = (unsigned int *)buf;
	unsigned int mmc_stat;
	unsigned int count;

	/*
	 * Start Polled Write
	 */
	count = (size > MMCSD_SECTOR_SIZE) ? MMCSD_SECTOR_SIZE : size;
	count /= 4;

	while (size) {
		ulong start = get_timer(0);
		do {
			mmc_stat = readl(&mmc_base->stat);
			if (get_timer(0) - start > MAX_RETRY_MS) {
				printf("%s: timedout waiting for status!\n",
						__func__);
				return TIMEOUT;
			}
		} while (mmc_stat == 0);

		if ((mmc_stat & ERRI_MASK) != 0)
			return 1;

		if (mmc_stat & BWR_MASK) {
			unsigned int k;

			writel(readl(&mmc_base->stat) | BWR_MASK,
					&mmc_base->stat);
			for (k = 0; k < count; k++) {
				writel(*input_buf, &mmc_base->data);
				input_buf++;
			}
			size -= (count*4);
		}

		if (mmc_stat & BRR_MASK)
			writel(readl(&mmc_base->stat) | BRR_MASK,
				&mmc_base->stat);

		if (mmc_stat & TC_MASK) {
			writel(readl(&mmc_base->stat) | TC_MASK,
				&mmc_base->stat);
			break;
		}
	}
	return 0;
}

#ifdef CONFIG_OMAP_MMC_USE_DMA_WRITES
static void mmc_dma_start(struct mmc* mmc, hsmmc_t *mmc_base, const char *buf, unsigned int blkSiz, unsigned int numBlk, int write)
{
	/*
	We would love to use ADMA here, since it is simpler to program,and faster,
	BUT in interest of compatibility with OMAP 36xx, we cannot, since it lacks
	such a feature. Additionally not all SDMMC controllers on OMAP44xx have
	ADMA ability, while all controllers on OMAP36xx and OMAP44xx have sDMA
	abilities. This forces us to write an sDMA driver. Writing a complete
	driver, with full consideration for all possible options is most
	definitely beyound the scope an SDMMC driver. So what folows is a very
	very simple hard-coded mini-driver for OMAP's sDMA, which is functional
	enough to allow us SDMMC transfers and no more than that. **If any other
	part of uboot, at any point in time, decides that it will use sDMA too,
	we'll probably have a conflict, and a better resolution will be needed**
	(like a real driver).
	*/

	omap_sdma* sdma = OMAP_SDMA;
	omap_sdma_channel* chan = sdma->channels + OMAP_DMA_CHANNEL_NUM;
	unsigned i, dmaReqNum;
	unsigned long csdp, ccr;

	/* for large areas flushing the entire cache is likely faster */
	if(numBlk > 128){
		flush_dcache_all();
	} else {
		flush_cache((unsigned long)buf, blkSiz * numBlk);
	}

	/*  figure out what our controller and dma request num are */
	switch (mmc - hsmmc_dev) {

		default:
		case 0:
			dmaReqNum = write ? SDMA_REQ_MMC1_TX : SDMA_REQ_MMC1_RX;
			break;

		case 1:
			dmaReqNum = write ? SDMA_REQ_MMC2_TX : SDMA_REQ_MMC2_RX;
			break;

		case 2:
			dmaReqNum = write ? SDMA_REQ_MMC3_TX : SDMA_REQ_MMC3_RX;
			break;
	}
	dmaReqNum += SDMA_REQ_NUM_OFFSET;


	/*
		clear interrupt statuses for our channel and mask them.
		Why? Docs say we must clear these before enabling a channel.
	*/
	for (i = 0; i < 4; i++) {

		writel(readl(&sdma->irqstatus[i]) | (1UL << OMAP_DMA_CHANNEL_NUM), &sdma->irqstatus[i]);
	}

	writel(SYSCONFIG_MIDLEMODE_NO_STBY | SYSCONFIG_SIDLEMODE_NO_IDLE | SYSCONFIG_AUTOIDLE_NONE, &sdma->ocpsysconfig);

	/* configure the dma engine */
	csdp = CSDP_SRC_LE | CSDP_SRC_ENDIAN_LOCK | CSDP_DST_LE | CSDP_DST_ENDIAN_LOCK | CSDP_DATA_TYPE_4B;
	ccr = CCR_ENABLE | CCR_FIELDSYNC_FRAME | DMA_REQ_NUM_TO_CCR_VAL(dmaReqNum);
	if (write) {
		csdp |= CSDP_WRITE_POSTED_ALL | CSDP_SRC_BURST_64B | CSDP_DST_BURST_NONE;
		writel((unsigned long)buf, &chan->cssa);
		writel((unsigned long)&mmc_base->data, &chan->cdsa);
		ccr |= CCR_DST_SYNC | CCR_DST_AMODE_CONST_ADDR | CCR_SRC_AMODE_POSTINCR;
	} else {

		csdp |= CSDP_WRITE_POSTED_ALL_BUT_LAST | CSDP_SRC_BURST_NONE  | CSDP_DST_BURST_64B;
		writel((unsigned long)&mmc_base->data, &chan->cssa);
		writel((unsigned long)buf, &chan->cdsa);
		ccr |= CCR_SRC_AMODE_CONST_ADDR | CCR_DST_AMODE_POSTINCR;
	}
	writel(0, &chan->ccr);
	writel(0, &chan->cicr);
	writel(0xFFFFFFFF, &chan->csr);
	writel(csdp, &chan->csdp);
	writel(blkSiz >> 2, &chan->cen);
	writel(numBlk, &chan->cfn);
	writel(CDP_TRANSFER_MODE_NORMAL, &chan->cdp);
	writel(ccr, &chan->ccr);
}

static int mmc_dma_wait_for_transfer_complete(struct mmc* mmc, hsmmc_t *mmc_base, const char *buf, unsigned int blkSiz, unsigned int numBlk)
{
	omap_sdma* sdma = OMAP_SDMA;
	omap_sdma_channel* chan = sdma->channels + OMAP_DMA_CHANNEL_NUM;
	unsigned mmc_stat;
	ulong start;
	int ret = 0;

	start = get_timer(0);
	while(1) {
		mmc_stat = readl(&mmc_base->stat);

		if (get_timer(0) - start > 10 * MAX_RETRY_MS) {
			printf("%s: timed out waiting for status!\n",
					__func__);
			ret = TIMEOUT;
			break;
		}

		if ((mmc_stat & ERRI_MASK) != 0) {

			printf("%s: mmc error status reported!\n",
					__func__);
			ret = 1;
			break;
		}

		if (mmc_stat & TC_MASK) {
			writel(mmc_stat | TC_MASK, &mmc_base->stat);
			break;
		}
	}

	writel(0, &chan->ccr);
	return ret;
}
#endif

static void mmc_set_ios(struct mmc *mmc)
{
	hsmmc_t *mmc_base = (hsmmc_t *)mmc->priv;
	unsigned int dsor = 0;
	ulong start;

	/* configue bus width */
	switch (mmc->bus_width) {
	case 8:
		writel(readl(&mmc_base->con) | DTW_8_BITMODE,
			&mmc_base->con);
		break;

	case 4:
		writel(readl(&mmc_base->con) & ~DTW_8_BITMODE,
			&mmc_base->con);
		writel(readl(&mmc_base->hctl) | DTW_4_BITMODE,
			&mmc_base->hctl);
		break;

	case 1:
	default:
		writel(readl(&mmc_base->con) & ~DTW_8_BITMODE,
			&mmc_base->con);
		writel(readl(&mmc_base->hctl) & ~DTW_4_BITMODE,
			&mmc_base->hctl);
		break;
	}

	/* configure clock with 96Mhz system clock.
	 */
	if (mmc->clock != 0) {
		dsor = (MMC_CLOCK_REFERENCE * 1000000 / mmc->clock);
		if ((MMC_CLOCK_REFERENCE * 1000000) / dsor > mmc->clock)
			dsor++;
	}

	mmc_reg_out(&mmc_base->sysctl, (ICE_MASK | DTO_MASK | CEN_MASK),
				(ICE_STOP | DTO_15THDTO | CEN_DISABLE));

	mmc_reg_out(&mmc_base->sysctl, ICE_MASK | CLKD_MASK,
				(dsor << CLKD_OFFSET) | ICE_OSCILLATE);

	start = get_timer(0);
	while ((readl(&mmc_base->sysctl) & ICS_MASK) == ICS_NOTREADY) {
		if (get_timer(0) - start > MAX_RETRY_MS) {
			printf("%s: timedout waiting for ics!\n", __func__);
			return;
		}
	}
	writel(readl(&mmc_base->sysctl) | CEN_ENABLE, &mmc_base->sysctl);
}

int omap_mmc_init(int dev_index)
{
	struct mmc *mmc;

	mmc = &hsmmc_dev[dev_index];

	sprintf(mmc->name, "OMAP SD/MMC");
	mmc->send_cmd = mmc_send_cmd;
	mmc->set_ios = mmc_set_ios;
	mmc->init = mmc_init_setup;

	switch (dev_index) {
	case 0:
		mmc->priv = (hsmmc_t *)OMAP_HSMMC1_BASE;
		break;
	case 1:
		mmc->priv = (hsmmc_t *)OMAP_HSMMC2_BASE;
		break;
	case 2:
		mmc->priv = (hsmmc_t *)OMAP_HSMMC3_BASE;
		break;
	default:
		mmc->priv = (hsmmc_t *)OMAP_HSMMC1_BASE;
		return 1;
	}
	mmc->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	mmc->host_caps = MMC_MODE_4BIT | MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;

	mmc->f_min = 400000;
	mmc->f_max = 52000000;

	mmc->b_max = 0;

#if defined(CONFIG_OMAP44XX)
	mmc->host_caps |= MMC_MODE_8BIT;
#endif

#if defined(CONFIG_OMAP34XX)
	/*
	 * Silicon revs 2.1 and older do not support multiblock transfers.
	 */
	if ((get_cpu_family() == CPU_OMAP34XX) && (get_cpu_rev() <= CPU_3XX_ES21))
		mmc->b_max = 1;
#endif

	mmc_register(mmc);

	return 0;
}

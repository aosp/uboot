#ifndef	_OMAP_SDMA_H_
#define	_OMAP_SDMA_H_

/* basic module info */

#define OMAP_SDMA_NUM_CHANNELS		32
#define OMAP_SDMA			((omap_sdma*)OMAP_SDMA_BASE)


/* structures */

typedef struct omap_sdma_channel {

	unsigned int ccr;		/* 0x00 */
	unsigned int clnkctr;		/* 0x04 */
	unsigned int cicr;		/* 0x08 */
	unsigned int csr;		/* 0x0C */
	unsigned int csdp;		/* 0x10 */
	unsigned int cen;		/* 0x14 */
	unsigned int cfn;		/* 0x18 */
	unsigned int cssa;		/* 0x1C */
	unsigned int cdsa;		/* 0x20 */
	unsigned int cse;		/* 0x24 */
	unsigned int csf;		/* 0x28 */
	unsigned int cde;		/* 0x2C */
	unsigned int cdf;		/* 0x30 */
	unsigned int csac;		/* 0x34 */
	unsigned int cdac;		/* 0x38 */
	unsigned int ccen;		/* 0x3C */
	unsigned int ccfn;		/* 0x40 */
	unsigned int color;		/* 0x44 */
	unsigned int unused1[2];	/* 0x48 - 0x4F */
	unsigned int cdp;		/* 0x50 */
	unsigned int cndp;		/* 0x54 */
	unsigned int ccdn;		/* 0x58 */
	unsigned int unused2;		/* 0x5C - 0x5F */

} omap_sdma_channel;

typedef struct omap_sdma {

	unsigned int		revision;				/* 0x00 */
	unsigned int		unused1;				/* 0x04 */
	unsigned int		irqstatus[4];				/* 0x08 - 0x17 */
	unsigned int		irqenable[4];				/* 0x18 - 0x27 */
	unsigned int		sysstatus;				/* 0x28 */
	unsigned int		ocpsysconfig;				/* 0x2C */
	unsigned int		unused2[13];				/* 0x30 - 0x63 */
	unsigned int		caps[5];				/* 0x64 - 0x77 */
	unsigned int		gcr;					/* 0x78 */
	unsigned int		unused3;				/* 0x7C */
	omap_sdma_channel	channels[OMAP_SDMA_NUM_CHANNELS];	/* 0x80 */

} omap_sdma;


/* utility defines */

#define DMA_REQ_NUM_TO_CCR_VAL(reqNum)		(((reqNum) & 0x1F) | (((reqNum) & 0x60) << 14))


/* other values */

#define SDMA_REQ_MMC1_TX		60
#define SDMA_REQ_MMC1_RX		61
#define SDMA_REQ_MMC2_TX		46
#define SDMA_REQ_MMC2_RX		47
#define SDMA_REQ_MMC3_TX		76
#define SDMA_REQ_MMC3_RX		77
#define SDMA_REQ_MMC4_TX		56
#define SDMA_REQ_MMC4_RX		57
#define SDMA_REQ_MMC5_TX		58
#define SDMA_REQ_MMC5_RX		59


#define SDMA_REQ_NUM_OFFSET		1	/* OMAP dma request numbers are 1-based */

/* bits */

#define SYSCONFIG_MIDLEMODE_NO_STBY	(0x1 << 12)
#define SYSCONFIG_SIDLEMODE_NO_IDLE	(0x1 << 3)
#define SYSCONFIG_AUTOIDLE_NONE		(0x0 << 0)

#define CCR_SRC_SYNC			(0x1 << 24)
#define CCR_DST_SYNC			(0x0 << 24)
#define CCR_DST_AMODE_CONST_ADDR	(0x0 << 14)
#define CCR_DST_AMODE_POSTINCR		(0x1 << 14)
#define CCR_SRC_AMODE_CONST_ADDR	(0x0 << 12)
#define CCR_SRC_AMODE_POSTINCR		(0x1 << 12)
#define CCR_ENABLE			(0x1 << 7)
#define CCR_FIELDSYNC_ELEM		((0x0 << 17) | (0x0 << 5))
#define CCR_FIELDSYNC_BLOCK		((0x1 << 17) | (0x0 << 5))
#define CCR_FIELDSYNC_FRAME		((0x0 << 17) | (0x1 << 5))
#define CCR_FIELDSYNC_PACKET		((0x1 << 17) | (0x1 << 5))

#define CSDP_SRC_LE			(0x0 << 21)
#define CSDP_SRC_ENDIAN_LOCK		(0x1 << 20)
#define CSDP_DST_LE			(0x0 << 19)
#define CSDP_DST_ENDIAN_LOCK		(0x1 << 18)
#define CSDP_WRITE_POSTED_NONE		(0x0 << 16)
#define CSDP_WRITE_POSTED_ALL		(0x1 << 16)
#define CSDP_WRITE_POSTED_ALL_BUT_LAST	(0x2 << 16)
#define CSDP_DST_BURST_NONE		(0x0 << 14)
#define CSDP_DST_BURST_16B		(0x1 << 14)
#define CSDP_DST_BURST_32B		(0x2 << 14)
#define CSDP_DST_BURST_64B		(0x3 << 14)
#define CSDP_SRC_BURST_NONE		(0x0 << 7)
#define CSDP_SRC_BURST_16B		(0x1 << 7)
#define CSDP_SRC_BURST_32B		(0x2 << 7)
#define CSDP_SRC_BURST_64B		(0x3 << 7)
#define CSDP_DATA_TYPE_1B		(0x0 << 0)
#define CSDP_DATA_TYPE_2B		(0x1 << 0)
#define CSDP_DATA_TYPE_4B		(0x2 << 0)

#define CDP_TRANSFER_MODE_NORMAL	(0x0 << 8)


#endif


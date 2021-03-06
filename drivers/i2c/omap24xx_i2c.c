/*
 * Basic I2C functions
 *
 * Copyright (c) 2004 Texas Instruments
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Author: Jian Zhang jzhang@ti.com, Texas Instruments
 *
 * Copyright (c) 2003 Wolfgang Denk, wd@denx.de
 * Rewritten to fit into the current U-Boot framework
 *
 * Adapted for OMAP2420 I2C, r-woodruff2@ti.com
 *
 */

#include <common.h>

#include <asm/arch/i2c.h>
#include <asm/io.h>

#include "omap24xx_i2c.h"

DECLARE_GLOBAL_DATA_PTR;

#define I2C_TIMEOUT	1000

static void wait_for_bb(void);
static u16 wait_for_pin(void);
static void flush_fifo(void);

static struct i2c *i2c_base = (struct i2c *)I2C_DEFAULT_BASE;

static unsigned int bus_initialized[I2C_BUS_MAX];
static unsigned int current_bus;

void i2c_init(int speed, int slaveadd)
{
	int psc, fsscll, fssclh;
	int hsscll = 0, hssclh = 0;
	u32 scll, sclh;
	int timeout = I2C_TIMEOUT;

	/* Only handle standard, fast and high speeds */
	if ((speed != OMAP_I2C_STANDARD) &&
	    (speed != OMAP_I2C_FAST_MODE) &&
	    (speed != OMAP_I2C_HIGH_SPEED)) {
		printf("Error : I2C unsupported speed %d\n", speed);
		return;
	}

	psc = I2C_IP_CLK / I2C_INTERNAL_SAMPLING_CLK;
	psc -= 1;
	if (psc < I2C_PSC_MIN) {
		printf("Error : I2C unsupported prescalar %d\n", psc);
		return;
	}

	if (speed == OMAP_I2C_HIGH_SPEED) {
		/* High speed */

		/* For first phase of HS mode */
		fsscll = fssclh = I2C_INTERNAL_SAMPLING_CLK /
			(2 * OMAP_I2C_FAST_MODE);

		fsscll -= I2C_HIGHSPEED_PHASE_ONE_SCLL_TRIM;
		fssclh -= I2C_HIGHSPEED_PHASE_ONE_SCLH_TRIM;
		if (((fsscll < 0) || (fssclh < 0)) ||
		    ((fsscll > 255) || (fssclh > 255))) {
			printf("Error : I2C initializing first phase clock\n");
			return;
		}

		/* For second phase of HS mode */
		hsscll = hssclh = I2C_INTERNAL_SAMPLING_CLK / (2 * speed);

		hsscll -= I2C_HIGHSPEED_PHASE_TWO_SCLL_TRIM;
		hssclh -= I2C_HIGHSPEED_PHASE_TWO_SCLH_TRIM;
		if (((fsscll < 0) || (fssclh < 0)) ||
		    ((fsscll > 255) || (fssclh > 255))) {
			printf("Error : I2C initializing second phase clock\n");
			return;
		}

		scll = (unsigned int)hsscll << 8 | (unsigned int)fsscll;
		sclh = (unsigned int)hssclh << 8 | (unsigned int)fssclh;

	} else {
		/* Standard and fast speed */
		fsscll = fssclh = I2C_INTERNAL_SAMPLING_CLK / (2 * speed);

		fsscll -= I2C_FASTSPEED_SCLL_TRIM;
		fssclh -= I2C_FASTSPEED_SCLH_TRIM;
		if (((fsscll < 0) || (fssclh < 0)) ||
		    ((fsscll > 255) || (fssclh > 255))) {
			printf("Error : I2C initializing clock\n");
			return;
		}

		scll = (unsigned int)fsscll;
		sclh = (unsigned int)fssclh;
	}

	if (readw(&i2c_base->con) & I2C_CON_EN) {
		writew(0, &i2c_base->con);
		udelay(50000);
	}

	writew(0x2, &i2c_base->sysc); /* for ES2 after soft reset */
	udelay(1000);

	writew(I2C_CON_EN, &i2c_base->con);
	while (!(readw(&i2c_base->syss) & I2C_SYSS_RDONE) && timeout--) {
		if (timeout <= 0) {
			printf("ERROR: Timeout in soft-reset\n");
			return;
		}
		udelay(1000);
	}

	writew(0, &i2c_base->con);
	writew(psc, &i2c_base->psc);
	writew(scll, &i2c_base->scll);
	writew(sclh, &i2c_base->sclh);

	/* own address */
	writew(slaveadd, &i2c_base->oa);
	writew(I2C_CON_EN, &i2c_base->con);

	/* have to enable interrupts or OMAP i2c module doesn't work */
	writew(I2C_IE_XRDY_IE | I2C_IE_RRDY_IE | I2C_IE_ARDY_IE |
		I2C_IE_NACK_IE | I2C_IE_AL_IE, &i2c_base->ie);
	udelay(1000);
	flush_fifo();
	writew(0xFFFF, &i2c_base->stat);
	writew(0, &i2c_base->cnt);

	if (gd->flags & GD_FLG_RELOC)
		bus_initialized[current_bus] = 1;
}

static int i2c_read_internal(u8 devaddr, uint addr, int alen,
			     u8 *buffer, int len)
{
	int i2c_error = 0;
	u16 status;
	int i2c_timeout = I2C_TIMEOUT;

	/* wait until bus not busy */
	wait_for_bb();

	/* send address to read, if any */
	if (alen) {
		u8 *addr_p = (u8 *)&addr;

		addr = __cpu_to_le32(addr);

		/* set slave address */
		writew(devaddr, &i2c_base->sa);

		/* alen bytes */
		writew(alen, &i2c_base->cnt);

		/* clear fifo buffers */
		writew(readw(&i2c_base->buf) |
		       I2C_BUF_RXFIF_CLR | I2C_BUF_TXFIF_CLR,
		       &i2c_base->buf);

		/* no stop bit needed here */
		writew(I2C_CON_EN | I2C_CON_MST | I2C_CON_STT | I2C_CON_TRX,
		       &i2c_base->con);

		/* send addr */
		while (alen) {
			status = wait_for_pin();

			/* ack the stat except [R/X]DR and [R/X]RDY, which
			 * are done afer the data operation is complete
			 */
			writew(status & ~(I2C_STAT_RRDY | I2C_STAT_XRDY),
			       &i2c_base->stat);

			if (status == 0 || status & I2C_STAT_NACK) {
				i2c_error = 1;
				goto read_exit;
			}

			if (status & I2C_STAT_XRDY) {
				while (alen) {
					/* Important: have to use byte access */
					writeb(*addr_p, &i2c_base->data);
					addr_p++;
					alen--;
				}
				writew(I2C_STAT_XRDY, &i2c_base->stat);
			}
			/* following Linux driver implementation,
			 * clear ARDY bit twice.
			 */
			if (status & I2C_STAT_ARDY) {
				writew(I2C_STAT_ARDY, &i2c_base->stat);
				break;
			}
		}

		status = wait_for_pin();
		/* ack the stat except [R/X]DR and [R/X]RDY, which
		 * are done afer the data operation is complete
		 */
		writew(status & ~(I2C_STAT_RRDY | I2C_STAT_XRDY),
		       &i2c_base->stat);
		if (status == 0 || status & I2C_STAT_NACK) {
			i2c_error = 1;
			printf("%s: i2c error, status = 0x%x\n",
			       __func__, status);
			goto read_exit;
		}
	}

	/* set slave address */
	writew(devaddr, &i2c_base->sa);

	/* len bytes to read */
	writew(len, &i2c_base->cnt);

	/* clear fifo buffers */
	writew(readw(&i2c_base->buf) | I2C_BUF_RXFIF_CLR | I2C_BUF_TXFIF_CLR,
	       &i2c_base->buf);

	/* stop bit needed here */
	writew(I2C_CON_EN | I2C_CON_MST | I2C_CON_STT | I2C_CON_STP,
	       &i2c_base->con);

	while (len) {
		status = wait_for_pin();
		/* ack the stat except [R/X]DR and [R/X]RDY, which
		 * are done afer the data operation is complete
		 */
		writew(status & ~(I2C_STAT_RRDY | I2C_STAT_XRDY),
		       &i2c_base->stat);

		if (status == 0 || status & I2C_STAT_NACK) {
			i2c_error = 1;
			printf("%s: i2c error, status = 0x%x\n",
			       __func__, status);
			goto read_exit;
		}
		if (status & I2C_STAT_ARDY) {
			writew(I2C_STAT_ARDY, &i2c_base->stat);
			break;
		}
		if (status & I2C_STAT_RRDY) {
			u16 bufstat = readw(&i2c_base->bufstat);
			u16 rx_bytes = ((bufstat & I2C_BUFSTAT_RXSTAT_MASK) >>
					I2C_BUFSTAT_RXSTAT_SHIFT);
			if (rx_bytes > len)
				rx_bytes = len;
			len -= rx_bytes;
			while (rx_bytes) {
				/* read data */
#if defined(CONFIG_OMAP243X) || defined(CONFIG_OMAP34XX) || defined(CONFIG_OMAP44XX)
				*buffer = readb(&i2c_base->data);
#else
				*buffer = readw(&i2c_base->data);
#endif
				buffer++;
				rx_bytes--;
			}
			writew(I2C_STAT_RRDY, &i2c_base->stat);
			break;
		}
		if (i2c_timeout-- <= 0) {
			i2c_error = 2;
			printf("ERROR: Timeout in soft-reset\n");
			goto read_exit;
		}
	}

	status = readw(&i2c_base->stat);

read_exit:
	flush_fifo();
	writew(0xFFFF, &i2c_base->stat);
	writew(0, &i2c_base->cnt);
	return i2c_error;
}

static int i2c_write_internal(u8 devaddr, uint addr, int alen,
			      u8 *buffer, int len)
{
	int i2c_error = 0;
	u16 status;
	u8 *addr_p = (u8 *)&addr;

	addr = __cpu_to_le32(addr);

	/* wait until bus not busy */
	wait_for_bb();

	/* len + alen bytes */
	writew(len + alen, &i2c_base->cnt);
	/* set slave address */
	writew(devaddr, &i2c_base->sa);

	/* clear fifo buffers */
	writew(readw(&i2c_base->buf) | I2C_BUF_RXFIF_CLR | I2C_BUF_TXFIF_CLR,
	       &i2c_base->buf);

	/* stop bit needed here */
	writew(I2C_CON_EN | I2C_CON_MST | I2C_CON_STT | I2C_CON_TRX |
	       I2C_CON_STP, &i2c_base->con);

	while (len + alen) {
		status = wait_for_pin();
		if (status == 0 || status & I2C_STAT_NACK) {
			i2c_error = 1;
			printf("%s: i2c error, status = 0x%x\n",
			       __func__, status);
			goto write_exit;
		}
		if (status & I2C_STAT_XRDY) {
			if (alen) {
				/* send addr */
				/* Important: have to use byte access */
				writeb(*addr_p, &i2c_base->data);
				writew(I2C_STAT_XRDY, &i2c_base->stat);
				addr_p++;
				alen--;
			} else {
				/* send data */
				writeb(*buffer, &i2c_base->data);
				writew(I2C_STAT_XRDY, &i2c_base->stat);
				buffer++;
				len--;
			}
		}
	}

	wait_for_bb();

	status = readw(&i2c_base->stat);
	if (status & I2C_STAT_NACK)
		i2c_error = 1;

write_exit:
	flush_fifo();
	writew(0xFFFF, &i2c_base->stat);
	writew(0, &i2c_base->cnt);
	return i2c_error;
}

static void flush_fifo(void)
{	u16 stat;

	/* note: if you try and read data when its not there or ready
	 * you get a bus error
	 */
	while (1) {
		stat = readw(&i2c_base->stat);
		if (stat == I2C_STAT_RRDY) {
#if defined(CONFIG_OMAP243X) || defined(CONFIG_OMAP34XX) || \
	defined(CONFIG_OMAP44XX)
			readb(&i2c_base->data);
#else
			readw(&i2c_base->data);
#endif
			writew(I2C_STAT_RRDY, &i2c_base->stat);
			udelay(1000);
		} else
			break;
	}
}

int i2c_probe(uchar chip)
{
	u16 status;
	int res = 1; /* default = fail */

	if (chip == readw(&i2c_base->oa))
		return res;

	/* wait until bus not busy */
	wait_for_bb();

	/* try to write one byte */
	writew(1, &i2c_base->cnt);
	/* set slave address */
	writew(chip, &i2c_base->sa);
	/* stop bit needed here */
	writew(I2C_CON_EN | I2C_CON_MST | I2C_CON_STT | I2C_CON_TRX |
	       I2C_CON_STP, &i2c_base->con);

	status = wait_for_pin();

	/* check for ACK (!NAK) */
	if (!(status & I2C_STAT_NACK))
		res = 0;

	/* abort transfer (force idle state) */
	writew(0, &i2c_base->con);

	flush_fifo();
	writew(0, &i2c_base->cnt); /* don't allow any more data in...we don't want it.*/
	writew(0xFFFF, &i2c_base->stat);
	return res;
}

int i2c_read(uchar chip, uint addr, int alen, uchar *buffer, int len)
{
	if (alen > 2) {
		printf("I2C read: alen %d too large\n", alen);
		return 1;
	}

	if (i2c_read_internal(chip, addr, alen, buffer, len)) {
		printf("I2C read: I/O error\n");
		i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
		return 1;
	}

	return 0;
}

int i2c_write(uchar chip, uint addr, int alen, uchar *buffer, int len)
{
	if (alen > 2) {
		printf("I2C write: alen %d too large\n", alen);
		return 1;
	}

	if (i2c_write_internal(chip, addr, alen, buffer, len)) {
		printf("I2C write: I/O error\n");
		i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
		return 1;
	}

	return 0;
}

static void wait_for_bb (void)
{
	int timeout = I2C_TIMEOUT;
	u16 stat;

	writew(0xFFFF, &i2c_base->stat);	/* clear current interrupts...*/
	while ((stat = readw(&i2c_base->stat) & I2C_STAT_BB) && timeout--) {
		writew(stat, &i2c_base->stat);
		udelay(1000);
	}

	if (timeout <= 0) {
		printf("timed out in wait_for_bb: I2C_STAT=%x\n",
			readw(&i2c_base->stat));
	}
	writew(0xFFFF, &i2c_base->stat);	 /* clear delayed stuff*/
}

static u16 wait_for_pin(void)
{
	u16 status;
	int timeout = I2C_TIMEOUT;

	do {
		udelay(1000);
		status = readw(&i2c_base->stat);
	} while (!(status &
		   (I2C_STAT_ROVR | I2C_STAT_XUDF | I2C_STAT_XRDY |
		    I2C_STAT_RRDY | I2C_STAT_ARDY | I2C_STAT_NACK |
		    I2C_STAT_AL)) && timeout--);

	if (timeout <= 0) {
		printf("timed out in wait_for_pin: I2C_STAT=%x\n",
			readw(&i2c_base->stat));
		writew(0xFFFF, &i2c_base->stat);
		status = 0;
	}

	return status;
}

int i2c_set_bus_num(unsigned int bus)
{
	if ((bus < 0) || (bus >= I2C_BUS_MAX)) {
		printf("Bad bus: %d\n", bus);
		return -1;
	}

	switch (bus) {
		case 0: i2c_base = (struct i2c *)I2C_BASE1; break;
		case 1: i2c_base = (struct i2c *)I2C_BASE2; break;
#if I2C_BUS_MAX >= 3
		case 2: i2c_base = (struct i2c *)I2C_BASE3; break;
#endif
#if I2C_BUS_MAX >= 4
		case 3: i2c_base = (struct i2c *)I2C_BASE4; break;
#endif
	}

	current_bus = bus;

	if (!bus_initialized[current_bus])
		i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);

	return 0;
}

int i2c_get_bus_num(void)
{
	return (int) current_bus;
}

/*
 * (C) Copyright 2011
 * Google, Inc.
 *
 * Author :
 *	John Grossman <johngro@google.com>
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
 */

#include <common.h>
#include <asm/arch/mem.h>
#include <asm/arch/sys_proto.h>

#include "pseudorandom_ids.h"

#define DIE_ID_REG_BASE		(OMAP44XX_L4_CORE_BASE + 0x2000)
#define DIE_ID_REG_OFFSET	0x200

/* Implements a variation of a 64 bit Galois LFSR with the generator
 *
 * gp(x) = x^64 + x^63 + x^61 + x^60 + 1
 *
 * This variant will actually blend in the bits of data by xor'ing with the
 * output of the register before feeding back.  Returned data is taken before
 * this XOR operation.  Passing data = 0 will result in the traditional Galois
 * LFSR behavior, suitable for a PRNG.
 *
 */
static u32 hash_lfsr_data(u64* state, u32 data) {
	static const u64 taps = 0xD800000000000000ull;
	u32 i, ret = 0;

	for (i=0; i < (sizeof(ret) << 3); ++i) {
		ret <<= 1;
		ret |= (((u32)(*state)) & 0x1);

		*state >>= 1;
		if ((ret ^ data) & 0x1)
			*state ^= taps;
		data >>= 1;
	}

	return ret;
}

static void init_dieid_lfsr(u64* state, u32 salt) {
	unsigned int reg = DIE_ID_REG_BASE + DIE_ID_REG_OFFSET;
	int i;

	/* prime the core of the LFSR with the lower 64 bits of the die ID */
	*state = ((u64)readl(reg + 0x8) << 32) | /* dieid bits [63:32] */
		  (u64)readl(reg); 		 /* dieid bits [31:0] */

	/* hash in the upper 64 bits of the die ID to finish initializing the
	 * state
	 */
	hash_lfsr_data(state, readl(reg + 0xC));
	hash_lfsr_data(state, readl(reg + 0x10));

	/* Add the salt */
	hash_lfsr_data(state, salt);

	/* Blend up the core state a bunch and we are ready to go */
	for (i = 0; i < 32; ++i)
		hash_lfsr_data(state, 0);
}

void generate_default_mac_addr(u32 salt, u8* mac_out) {
	u64 lfsr_state;
	u32 rand;

	init_dieid_lfsr(&lfsr_state, salt);

	mac_out[5] = 0x00;
	mac_out[4] = 0x1A;
	mac_out[3] = 0x11;

	rand = hash_lfsr_data(&lfsr_state, 0);
	mac_out[2] = (u8)(rand & 0xFF);
	mac_out[1] = (u8)((rand >> 8)  & 0xFF);
	mac_out[0] = (u8)((rand >> 16) & 0xFF);
}

void generate_default_64bit_id(u32 salt, u64* id_64) {
	u64 lfsr_state;
	init_dieid_lfsr(&lfsr_state, salt);
	*id_64 = ((u64)hash_lfsr_data(&lfsr_state, 0) << 32) |
		  (u64)hash_lfsr_data(&lfsr_state, 0);
}

/*
 * (C) Copyright 2012
 * Texas Instruments, <www.ti.com>
 * Carlos Leija <cileija@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#ifndef _OMAP4_HAL_H
#define _OMAP4_HAL_H_

#include <asm/sizes.h>

/*
 * Constants
 */

#define FLAG_START_CRITICAL            0x7
#define PUBLIC_API_SEC_ENTRY           0x0

/* Find base address based on OMAP revision */
#define PUBLIC_API_BASE ( \
  ((*((volatile unsigned int *)(0x4A002204))>>12) & 0x00F0)== 0x0040 ? \
  0x00030400 : 0x00028400 )

/*
 * Type definitions
 */
typedef unsigned char           U8;
typedef unsigned short          U16;
typedef unsigned int            U32;
typedef volatile unsigned int   VU32;

typedef U32 (** const PUBLIC_SEC_ENTRY_Pub2SecDispatcher_pt) \
               (U32 appl_id, U32 proc_ID, U32 flag, ...);


/*
 * Function declaration
 */

/* Pointer to Public ROM Pub2SecDispatcher */
#define PUBLIC_SEC_ENTRY_Pub2SecDispatcher \
      (*(PUBLIC_SEC_ENTRY_Pub2SecDispatcher_pt) \
                                   (PUBLIC_API_BASE+PUBLIC_API_SEC_ENTRY))

U32 SEC_ENTRY_Std_Ppa_Call (U32 appl_id, U32 inNbArg, ...);

U32 bch_enc(U8 index, U32 in_v[]);
U32 cpfrom_byte_reverse32(U32 value);
U32 hexStringtoInteger(const char* hexString, U32* result);

#endif

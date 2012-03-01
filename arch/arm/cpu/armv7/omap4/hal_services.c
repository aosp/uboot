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
#include <asm/arch/omap4_hal.h>
#include <common.h>

/*
 * Abstraction API to format arguments to pass to
 * PUBLIC_API_SEC_ENTRY
 */
U32 SEC_ENTRY_Std_Ppa_Call (U32 appl_id, U32 inNbArg, ...)
{
  U32 result = 0;
  U32 Param[4];
  va_list ap;

  va_start(ap, inNbArg);

  /* we must disable cache because the ROM dispatcher will
     marshal arguments onto a stack structure before passing
     it to secure mode and secure mode has a different mapping
     of the memory and won't be able to see what was put on
     the stack from our mapping.
  */
  flush_dcache_all();
  dcache_disable();
  switch (inNbArg)
  {
    case 0:
      result = PUBLIC_SEC_ENTRY_Pub2SecDispatcher(
                   appl_id,
                   0,
                   FLAG_START_CRITICAL,
                   inNbArg);
      break;

    case 1:
      Param[0] = va_arg(ap, U32);
      result = PUBLIC_SEC_ENTRY_Pub2SecDispatcher(
                   appl_id,
                   0,
                   FLAG_START_CRITICAL,
                   inNbArg, Param[0]);
      break;

    case 2:
      Param[0] = va_arg(ap, U32);
      Param[1] = va_arg(ap, U32);
      result = PUBLIC_SEC_ENTRY_Pub2SecDispatcher(
                   appl_id,
                   0,
                   FLAG_START_CRITICAL,
                   inNbArg, Param[0], Param[1]);
      break;

    case 3:
      Param[0] = va_arg(ap, U32);
      Param[1] = va_arg(ap, U32);
      Param[2] = va_arg(ap, U32);
      result = PUBLIC_SEC_ENTRY_Pub2SecDispatcher(
                   appl_id,
                   0,
                   FLAG_START_CRITICAL,
                   inNbArg, Param[0], Param[1], Param[2]);
       break;
    case 4:
      Param[0] = va_arg(ap, U32);
      Param[1] = va_arg(ap, U32);
      Param[2] = va_arg(ap, U32);
      Param[3] = va_arg(ap, U32);
      result = PUBLIC_SEC_ENTRY_Pub2SecDispatcher(
                   appl_id,
                   0,
                   FLAG_START_CRITICAL,
                   inNbArg, Param[0], Param[1], Param[2], Param[3]);
      break;
    default:
	printf("[ERROR] [SEC_ENTRY] Number of arguments not supported \n");
	dcache_enable();
      return 1;
  }
  dcache_enable();
  va_end(ap);
	if (result != 0)
		printf("[ERROR] [SEC_ENTRY] Call to Secure HAL failed!\n");
  return result;
}

/*
 * (C) Copyright 2007 OpenMoko, Inc.
 * Written by Harald Welte <laforge@openmoko.org>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <stdio_dev.h>
#include <serial.h>

#ifdef CMD_TERMINAL_ESC_CH1
static const int esc_ch1 = CMD_TERMINAL_ESC_CH1;
#else
static const int esc_ch1 = '~';
#endif

#ifdef CMD_TERMINAL_ESC_CH2
static const int esc_ch2 = CMD_TERMINAL_ESC_CH2;
#else
static const int esc_ch2 = '.';
#endif

#ifdef CMD_TERMINAL_ESC_DELAY
static const ulong esc_dly = CMD_TERMINAL_ESC_DELAY;
#else
static const ulong esc_dly = CONFIG_SYS_HZ; /* 1 second */
#endif

int do_terminal(cmd_tbl_t * cmd, int flag, int argc, char * const argv[])
{
	int last_tilde = 0;
	struct stdio_dev *dev = NULL;
	ulong read_time, last_read_time = 0;
	int saw_esc_dly;

	if (argc < 1)
		return -1;

	/* Scan for selected output/input device */
	dev = stdio_get_by_name(argv[1]);
	if (!dev)
		return -1;

	serial_reinit_all();
	printf("Entering terminal mode for port %s.\n"
		"To leave the terminal and get back to U-Boot, "
		"send nothing for at least\n"
		"%lu milliseconds and then quickly send the "
		"two character sequence \"%c%c\".\n"
		, dev->name, esc_dly * 1000 / CONFIG_SYS_HZ, esc_ch1, esc_ch2);

	while (1) {
		int c;

		/* read from console and display on serial port */
		if (stdio_devices[0]->tstc()) {
			c = stdio_devices[0]->getc();
			read_time = get_timer(0);
			saw_esc_dly = ((read_time - last_read_time) > esc_dly);
			last_read_time = read_time;
			if (last_tilde == 1) {
				if (c == esc_ch2 && !saw_esc_dly) {
					putc('\n');
					break;
				} else {
					last_tilde = 0;
					/* write the delayed tilde */
					dev->putc(esc_ch1);
					/* and whatever just came in */
					dev->putc(c);
				}
			} else if (saw_esc_dly && (c == esc_ch1)) {
				last_tilde = 1;
			} else {
				dev->putc(c);
			}
		}

		/* read from serial port and display on console */
		if (dev->tstc()) {
			c = dev->getc();
			putc(c);
		}
	}
	return 0;
}


/***************************************************/

U_BOOT_CMD(
	terminal,	2,	0,	do_terminal,
	"start terminal emulator",
	"serial_port"
);

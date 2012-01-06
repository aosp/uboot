/* board/google/tungsten/steelhead_avr.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This is a driver that communicates with an Atmel AVR ATmega328P
 * subboard in the Android@Home device via gpios and i2c.  This subboard
 * is Arduino-Compatible and the firmware in it is developed using the
 * Arduino SDK.
 *
 * The functionality implemented by the subboard is a set of capacitive touch
 * keys and many leds.  To keep things simple for now, we have just
 * one driver that implements two input_device exposing the keys and
 * a misc_device exposing custom ioctls for controlling the leds.  We don't
 * use the Linux led driver API because we have too many leds and want
 * a more custom API to be more efficient.  Also, the subboard firmware
 * implements some macro led modes (like volume mode) which doesn't make
 * sense in the led API.
 */
#ifndef _STEELHEAD_AVR_H_
#define _STEELHEAD_AVR_H_

struct avr_led_rgb_vals {
	u8 rgb[3];
};

struct avr_led_set_range_vals {
	u8 start;
	u8 count;
	u8 rgb_triples;
	u8 rgb[32][3]; /* maximum number for now */
};

extern int detect_avr(void);
extern int avr_led_set_all(const struct avr_led_rgb_vals *req);
extern int avr_led_set_mute(const struct avr_led_rgb_vals *req);
extern int avr_led_commit_led_state(u8 val);
extern int avr_led_set_mode(u8 mode);
extern int avr_get_key(u8 *key_code);
extern int avr_set_mute_threshold(u8 mute_threshold);

#endif /* _STEELHEAD_AVR_H_ */

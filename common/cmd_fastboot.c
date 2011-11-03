/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author : Mohammed Afzal M A <afzal@ti.com>
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
 *
 *
 * Fastboot is implemented using gadget stack, many of the ideas are
 * derived from fastboot implemented in OmapZoom by
 * Tom Rix <Tom.Rix@windriver.com>, and portion of the code has been
 * ported from OmapZoom.
 *
 * Part of OmapZoom was copied from Android project, Android source
 * (legacy bootloader) was used indirectly here by using OmapZoom.
 *
 * This is Android's Copyright:
 *
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <fastboot.h>

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_FASTBOOT_VERSION_BOOTLOADER
#define CONFIG_FASTBOOT_VERSION_BOOTLOADER FASTBOOT_VERSION
#endif

#define FASTBOOT_RUN_RECOVERY_ENV_NAME "fastboot_run_recovery"
#define FASTBOOT_UNLOCKED_ENV_NAME "fastboot_unlocked"
#define FASTBOOT_UNLOCK_TIMEOUT_SECS 5

#ifndef CONFIG_ENV_BLK_PARTITION
#define CONFIG_ENV_BLK_PARTITION "environment"
#endif
#ifndef CONFIG_INFO_PARTITION
#define CONFIG_INFO_PARTITION "device_info"
#endif

#define	ERR
#define	WARN
#undef	INFO
#undef	DEBUG

#ifndef CONFIG_FASTBOOT_LOG_SIZE
#define CONFIG_FASTBOOT_LOG_SIZE 4000
#endif
static char log_buffer[CONFIG_FASTBOOT_LOG_SIZE];
static unsigned long log_position;

#ifdef DEBUG
#define FBTDBG(fmt, args...)\
	printf("DEBUG: [%s]: %d:\n"fmt, __func__, __LINE__, ##args)
#else
#define FBTDBG(fmt, args...) do {} while (0)
#endif

#ifdef INFO
#define FBTINFO(fmt, args...)\
	printf("INFO: [%s]: "fmt, __func__, ##args)
#else
#define FBTINFO(fmt, args...) do {} while (0)
#endif

#ifdef WARN
#define FBTWARN(fmt, args...)\
	printf("WARNING: [%s]: "fmt, __func__, ##args)
#else
#define FBTWARN(fmt, args...) do {} while (0)
#endif

#ifdef ERR
#define FBTERR(fmt, args...)\
	printf("ERROR: [%s]: "fmt, __func__, ##args)
#else
#define FBTERR(fmt, args...) do {} while (0)
#endif

#include <exports.h>
#include <environment.h>

/* USB specific */

#include <usb_defs.h>

#if defined(CONFIG_PPC)
#include <usb/mpc8xx_udc.h>
#elif defined(CONFIG_OMAP1510)
#include <usb/omap1510_udc.h>
#elif defined(CONFIG_MUSB_UDC)
#include <usb/musb_udc.h>
#elif defined(CONFIG_PXA27X)
#include <usb/pxa27x_udc.h>
#elif defined(CONFIG_SPEAR3XX) || defined(CONFIG_SPEAR600)
#include <usb/spr_udc.h>
#endif

#if defined (CONFIG_OMAP)
#include <asm/arch/sys_proto.h>
#endif

#define STR_LANG		0x00
#define STR_MANUFACTURER	0x01
#define STR_PRODUCT		0x02
#define STR_SERIAL		0x03
#define STR_CONFIGURATION	0x04
#define STR_INTERFACE		0x05
#define STR_COUNT		0x06

#define CONFIG_USBD_CONFIGURATION_STR	"Android Fastboot Configuration"
#define CONFIG_USBD_INTERFACE_STR	"Android Fastboot Interface"

#define USBFBT_BCD_DEVICE	0x00
#define	USBFBT_MAXPOWER		0x32

#define USB_FLUSH_DELAY_MICROSECS 1000

#define	NUM_CONFIGS	1
#define	NUM_INTERFACES	1
#define	NUM_ENDPOINTS	2

#define	RX_EP_INDEX	1
#define	TX_EP_INDEX	2

struct _fbt_config_desc {
	struct usb_configuration_descriptor configuration_desc;
	struct usb_interface_descriptor interface_desc;
	struct usb_endpoint_descriptor endpoint_desc[NUM_ENDPOINTS];
};

static void fbt_handle_response(void);
static disk_partition_t *fastboot_flash_find_ptn(const char *name);

/* defined and used by gadget/ep0.c */
extern struct usb_string_descriptor **usb_strings;

/* USB Descriptor Strings */
static char serial_number[33]; /* what should be the length ?, 33 ? */
static u8 wstr_lang[4] = {4, USB_DT_STRING, 0x9, 0x4};
static u8 wstr_manufacturer[2 + 2*(sizeof(CONFIG_USBD_MANUFACTURER)-1)];
static u8 wstr_product[2 + 2*(sizeof(CONFIG_USBD_PRODUCT_NAME)-1)];
static u8 wstr_serial[2 + 2*(sizeof(serial_number) - 1)];
static u8 wstr_configuration[2 + 2*(sizeof(CONFIG_USBD_CONFIGURATION_STR)-1)];
static u8 wstr_interface[2 + 2*(sizeof(CONFIG_USBD_INTERFACE_STR)-1)];

/* USB descriptors */
static struct usb_device_descriptor device_descriptor = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(USB_BCD_VERSION),
	.bDeviceClass =		0x00,
	.bDeviceSubClass =	0x00,
	.bDeviceProtocol =	0x00,
	.bMaxPacketSize0 =	EP0_MAX_PACKET_SIZE,
	.idVendor =		cpu_to_le16(CONFIG_USBD_VENDORID),
	.idProduct =		cpu_to_le16(CONFIG_USBD_PRODUCTID),
	.bcdDevice =		cpu_to_le16(USBFBT_BCD_DEVICE),
	.iManufacturer =	STR_MANUFACTURER,
	.iProduct =		STR_PRODUCT,
	.iSerialNumber =	STR_SERIAL,
	.bNumConfigurations =	NUM_CONFIGS
};

static struct _fbt_config_desc fbt_config_desc = {
	.configuration_desc = {
		.bLength = sizeof(struct usb_configuration_descriptor),
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength =	cpu_to_le16(sizeof(struct _fbt_config_desc)),
		.bNumInterfaces = NUM_INTERFACES,
		.bConfigurationValue = 1,
		.iConfiguration = STR_CONFIGURATION,
		.bmAttributes =	BMATTRIBUTE_SELF_POWERED | BMATTRIBUTE_RESERVED,
		.bMaxPower = USBFBT_MAXPOWER,
	},
	.interface_desc = {
		.bLength  = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0x2,
		.bInterfaceClass = FASTBOOT_INTERFACE_CLASS,
		.bInterfaceSubClass = FASTBOOT_INTERFACE_SUB_CLASS,
		.bInterfaceProtocol = FASTBOOT_INTERFACE_PROTOCOL,
		.iInterface = STR_INTERFACE,
	},
	.endpoint_desc = {
		{
			.bLength = sizeof(struct usb_endpoint_descriptor),
			.bDescriptorType = USB_DT_ENDPOINT,
			/* XXX: can't the address start from 0x1, currently
				seeing problem with "epinfo" */
			.bEndpointAddress = RX_EP_INDEX | USB_DIR_OUT,
			.bmAttributes =	USB_ENDPOINT_XFER_BULK,
			.bInterval = 0xFF,
		},
		{
			.bLength = sizeof(struct usb_endpoint_descriptor),
			.bDescriptorType = USB_DT_ENDPOINT,
			/* XXX: can't the address start from 0x1, currently
				seeing problem with "epinfo" */
			.bEndpointAddress = TX_EP_INDEX | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.bInterval = 0xFF,
		},
	},
};

static struct usb_interface_descriptor interface_descriptors[NUM_INTERFACES];
static struct usb_endpoint_descriptor *ep_descriptor_ptrs[NUM_ENDPOINTS];

static struct usb_string_descriptor *fbt_string_table[STR_COUNT];
static struct usb_device_instance device_instance[1];
static struct usb_bus_instance bus_instance[1];
static struct usb_configuration_instance config_instance[NUM_CONFIGS];
static struct usb_interface_instance interface_instance[NUM_INTERFACES];
static struct usb_alternate_instance alternate_instance[NUM_INTERFACES];
static struct usb_endpoint_instance endpoint_instance[NUM_ENDPOINTS + 1];

/* FASBOOT specific */

/* U-boot version */
extern char version_string[];

static const char info_partition_magic[] = {'I', 'n', 'f', 'o'};

static struct cmd_fastboot_interface priv = {
	.transfer_buffer       = (u8 *)CONFIG_FASTBOOT_TRANSFER_BUFFER,
	.transfer_buffer_size  = CONFIG_FASTBOOT_TRANSFER_BUFFER_SIZE,
};

static void fbt_init_endpoints(void);
static int do_booti(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);

extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);
/* Use do_bootm_linux and do_go for fastboot's 'boot' command */
extern int do_go(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);
extern int do_bootm_linux(int flag, int argc, char *argv[],
			  bootm_headers_t *images);
extern int do_env_save(cmd_tbl_t *cmdtp, int flag, int argc,
		       char *const argv[]);

/* To support the Android-style naming of flash */
#define MAX_PTN (CONFIG_MAX_PARTITION_NUM - CONFIG_MIN_PARTITION_NUM + 1)
static disk_partition_t ptable[MAX_PTN];
static unsigned int pcount;

/* USB specific */

/* utility function for converting char * to wide string used by USB */
static void str2wide(char *str, u16 * wide)
{
	int i;
	for (i = 0; i < strlen(str) && str[i]; i++) {
		#if defined(__LITTLE_ENDIAN)
			wide[i] = (u16) str[i];
		#elif defined(__BIG_ENDIAN)
			wide[i] = ((u16)(str[i])<<8);
		#else
			#error "__LITTLE_ENDIAN or __BIG_ENDIAN undefined"
		#endif
	}
}

/* fastboot_init has to be called before this fn to get correct serial string */
static void fbt_init_strings(void)
{
	struct usb_string_descriptor *string;

	fbt_string_table[STR_LANG] = (struct usb_string_descriptor *)wstr_lang;

	string = (struct usb_string_descriptor *) wstr_manufacturer;
	string->bLength = sizeof(wstr_manufacturer);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_MANUFACTURER, string->wData);
	fbt_string_table[STR_MANUFACTURER] = string;

	string = (struct usb_string_descriptor *) wstr_product;
	string->bLength = sizeof(wstr_product);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_PRODUCT_NAME, string->wData);
	fbt_string_table[STR_PRODUCT] = string;

	string = (struct usb_string_descriptor *) wstr_serial;
	string->bLength = sizeof(wstr_serial);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(serial_number, string->wData);
	fbt_string_table[STR_SERIAL] = string;

	string = (struct usb_string_descriptor *) wstr_configuration;
	string->bLength = sizeof(wstr_configuration);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_CONFIGURATION_STR, string->wData);
	fbt_string_table[STR_CONFIGURATION] = string;

	string = (struct usb_string_descriptor *) wstr_interface;
	string->bLength = sizeof(wstr_interface);
	string->bDescriptorType = USB_DT_STRING;
	str2wide(CONFIG_USBD_INTERFACE_STR, string->wData);
	fbt_string_table[STR_INTERFACE] = string;

	/* Now, initialize the string table for ep0 handling */
	usb_strings = fbt_string_table;
}

static void fbt_event_handler (struct usb_device_instance *device,
				  usb_device_event_t event, int data)
{
	switch (event) {
	case DEVICE_RESET:
	case DEVICE_BUS_INACTIVE:
		priv.configured = 0;
		break;
	case DEVICE_CONFIGURED:
		priv.configured = 1;
		break;

	case DEVICE_ADDRESS_ASSIGNED:
		fbt_init_endpoints();

	default:
		break;
	}
}

/* fastboot_init has to be called before this fn to get correct serial string */
static void fbt_init_instances(void)
{
	int i;

	/* initialize device instance */
	memset(device_instance, 0, sizeof(struct usb_device_instance));
	device_instance->device_state = STATE_INIT;
	device_instance->device_descriptor = &device_descriptor;
	device_instance->event = fbt_event_handler;
	device_instance->cdc_recv_setup = NULL;
	device_instance->bus = bus_instance;
	device_instance->configurations = NUM_CONFIGS;
	device_instance->configuration_instance_array = config_instance;

	/* XXX: what is this bus instance for ?, can't it be removed by moving
	    endpoint_array and serial_number_str is moved to device instance */
	/* initialize bus instance */
	memset(bus_instance, 0, sizeof(struct usb_bus_instance));
	bus_instance->device = device_instance;
	bus_instance->endpoint_array = endpoint_instance;
	/* XXX: what is the relevance of max_endpoints & maxpacketsize ? */
	bus_instance->max_endpoints = 1;
	bus_instance->maxpacketsize = 64;
	bus_instance->serial_number_str = serial_number;

	/* configuration instance */
	memset(config_instance, 0, sizeof(struct usb_configuration_instance));
	config_instance->interfaces = NUM_INTERFACES;
	config_instance->configuration_descriptor =
		(struct usb_configuration_descriptor *)&fbt_config_desc;
	config_instance->interface_instance_array = interface_instance;

	/* XXX: is alternate instance required in case of no alternate ? */
	/* interface instance */
	memset(interface_instance, 0, sizeof(struct usb_interface_instance));
	interface_instance->alternates = 1;
	interface_instance->alternates_instance_array = alternate_instance;

	/* alternates instance */
	memset(alternate_instance, 0, sizeof(struct usb_alternate_instance));
	alternate_instance->interface_descriptor = interface_descriptors;
	alternate_instance->endpoints = NUM_ENDPOINTS;
	alternate_instance->endpoints_descriptor_array = ep_descriptor_ptrs;

	/* endpoint instances */
	memset(endpoint_instance, 0, sizeof(endpoint_instance));
	endpoint_instance[0].endpoint_address = 0;
	endpoint_instance[0].rcv_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].rcv_attributes = USB_ENDPOINT_XFER_CONTROL;
	endpoint_instance[0].tx_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].tx_attributes = USB_ENDPOINT_XFER_CONTROL;
	/* XXX: following statement to done along with other endpoints
		at another place ? */
	udc_setup_ep(device_instance, 0, &endpoint_instance[0]);

	for (i = 1; i <= NUM_ENDPOINTS; i++) {
		endpoint_instance[i].endpoint_address =
			ep_descriptor_ptrs[i - 1]->bEndpointAddress;

		endpoint_instance[i].rcv_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		endpoint_instance[i].rcv_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);

		endpoint_instance[i].tx_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		endpoint_instance[i].tx_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);

		endpoint_instance[i].tx_attributes =
			ep_descriptor_ptrs[i - 1]->bmAttributes;

		urb_link_init(&endpoint_instance[i].rcv);
		urb_link_init(&endpoint_instance[i].rdy);
		urb_link_init(&endpoint_instance[i].tx);
		urb_link_init(&endpoint_instance[i].done);

		if (endpoint_instance[i].endpoint_address & USB_DIR_IN)
			endpoint_instance[i].tx_urb =
				usbd_alloc_urb(device_instance,
					       &endpoint_instance[i]);
		else
			endpoint_instance[i].rcv_urb =
				usbd_alloc_urb(device_instance,
					       &endpoint_instance[i]);
	}
}

/* XXX: ep_descriptor_ptrs can be removed by making better use of
	fbt_config_desc.endpoint_desc */
static void fbt_init_endpoint_ptrs(void)
{
	ep_descriptor_ptrs[0] = &fbt_config_desc.endpoint_desc[0];
	ep_descriptor_ptrs[1] = &fbt_config_desc.endpoint_desc[1];
}

static void fbt_init_endpoints(void)
{
	int i;

	/* XXX: should it be moved to some other function ? */
	bus_instance->max_endpoints = NUM_ENDPOINTS + 1;

	/* XXX: is this for loop required ?, yes for MUSB it is */
	for (i = 1; i <= NUM_ENDPOINTS; i++) {

		/* configure packetsize based on HS negotiation status */
		if (device_instance->speed == USB_SPEED_FULL) {
			FBTINFO("setting up FS USB device ep%x\n",
				endpoint_instance[i].endpoint_address);
			ep_descriptor_ptrs[i - 1]->wMaxPacketSize =
				CONFIG_USBD_FASTBOOT_BULK_PKTSIZE_FS;
		} else if (device_instance->speed == USB_SPEED_HIGH) {
			FBTINFO("setting up HS USB device ep%x\n",
				endpoint_instance[i].endpoint_address);
			ep_descriptor_ptrs[i - 1]->wMaxPacketSize =
				CONFIG_USBD_FASTBOOT_BULK_PKTSIZE_HS;
		}

		endpoint_instance[i].tx_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);
		endpoint_instance[i].rcv_packetSize =
			le16_to_cpu(ep_descriptor_ptrs[i - 1]->wMaxPacketSize);

		udc_setup_ep(device_instance, i, &endpoint_instance[i]);
	}
}

static struct urb *next_urb(struct usb_device_instance *device,
			    struct usb_endpoint_instance *endpoint)
{
	struct urb *current_urb;
	int space;

	/* If there's a queue, then we should add to the last urb */
	if (!endpoint->tx_queue)
		current_urb = endpoint->tx_urb;
	else {
		/* Last urb from tx chain */
		current_urb = p2surround(struct urb, link, endpoint->tx.prev);
	}

	/* Make sure this one has enough room */
	space = current_urb->buffer_length - current_urb->actual_length;
	if (space > 0)
		return current_urb;
	else {		/* No space here */
		/* First look at done list */
		current_urb = first_urb_detached(&endpoint->done);
		if (!current_urb)
			current_urb = usbd_alloc_urb(device, endpoint);

		urb_append(&endpoint->tx, current_urb);
		endpoint->tx_queue++;
	}
	return current_urb;
}

static void fbt_wait_usb_fifo_flush(void)
{
	/* give time to flush FIFO and remote to receive data.
	 * otherwise, USB can get hung.  someday we might actually
	 * try checking USB fifo status directly but for now, just
	 * spin for some time.
	 */
	udelay(USB_FLUSH_DELAY_MICROSECS);
}

/* FASTBOOT specific */

/*
 * Android style flash utilties
 */
static void set_serial_number(const char *serial_no)
{
	strncpy(serial_number, serial_no, sizeof(serial_number));
	serial_number[sizeof(serial_number) - 1] = '\0';
	priv.serial_no = serial_number;
	printf("fastboot serial_number = %s\n", serial_number);
}

static void create_serial_number(void)
{
	char *dieid = getenv("fbt_id#");

	if (dieid == NULL)
		dieid = getenv("dieid#");

	if (dieid == NULL) {
		printf("Setting serial number from constant (no dieid info)\n");
		set_serial_number("00123");
	} else {
		printf("Setting serial number from unique id\n");
		set_serial_number(dieid);
	}
}

static int is_env_partition(disk_partition_t *ptn)
{
	return !strcmp((char *)ptn->name, CONFIG_ENV_BLK_PARTITION);
}
static int is_info_partition(disk_partition_t *ptn)
{
	return !strcmp((char *)ptn->name, CONFIG_INFO_PARTITION);
}

void fbt_add_ptn(disk_partition_t *ptn)
{
	if (pcount < MAX_PTN) {
		memcpy(ptable + pcount, ptn, sizeof(*ptn));
		pcount++;
	}
}

static int fbt_load_partition_table(void)
{
	disk_partition_t *info_ptn;
	unsigned int i;

	if (board_fbt_load_ptbl()) {
		printf("board_fbt_load_ptbl() failed\n");
		return -1;
	}

	/* load device info partition if it exists */
	info_ptn = fastboot_flash_find_ptn(CONFIG_INFO_PARTITION);
	if (info_ptn) {
		struct info_partition_header *info_header;
		char *name, *next_name;
		char *value;

		lbaint_t num_blks = 1;
		i = partition_read_blks(priv.dev_desc, info_ptn,
					&num_blks, priv.transfer_buffer);
		if (i) {
			printf("failed to read info partition. error=%d\n", i);
			goto no_existing_info;
		}

		/* parse the info partition read from the device */
		info_header =
			(struct info_partition_header *)priv.transfer_buffer;
		name = (char *)(info_header + 1);
		value = name;

		if (memcmp(&info_header->magic, info_partition_magic,
			   sizeof(info_partition_magic)) != 0) {
			printf("info partition magic 0x%x invalid,"
			       " assuming none\n", info_header->magic);
			goto no_existing_info;
		}
		if (info_header->num_values > FASTBOOT_MAX_NUM_DEVICE_INFO) {
			printf("info partition num values %d too large "
			       " (max %d)\n", info_header->num_values,
			       FASTBOOT_MAX_NUM_DEVICE_INFO);
			goto no_existing_info;
		}
		priv.num_device_info = info_header->num_values;
		/* the name/value pairs are in the format:
		 *    name1=value1\n
		 *    name2=value2\n
		 * this makes it easier to read if we dump the partition
		 * to a file
		 */
		printf("%d device info entries read from %s partition:\n",
		       priv.num_device_info, info_ptn->name);
		for (i = 0; i < priv.num_device_info; i++) {
			while (*value != '=')
				value++;
			*value++ = '\0';
			next_name = value;
			while (*next_name != '\n')
				next_name++;
			*next_name++ = '\0';
			priv.dev_info[i].name = strdup(name);
			priv.dev_info[i].value = strdup(value);
			printf("\t%s=%s\n", priv.dev_info[i].name,
			       priv.dev_info[i].value);
			/* initialize serial number from device info */
			if (!strcmp(name, FASTBOOT_SERIALNO_BOOTARG))
				set_serial_number(value);
			name = next_name;
		}
		priv.dev_info_uninitialized = 0;
	} else {
no_existing_info:
		priv.dev_info_uninitialized = 1;
		printf("No existing device info found.\n");
	}

	if (priv.serial_no == NULL)
		create_serial_number();

	return 0;
}

static disk_partition_t *fastboot_flash_find_ptn(const char *name)
{
	unsigned int n;

	if (pcount == 0) {
		if (fbt_load_partition_table()) {
			printf("Unable to load partition table, aborting\n");
			return NULL;
		}
	}

	for (n = 0; n < pcount; n++)
		if (!strcmp((char *)ptable[n].name, name))
			return ptable + n;
	return NULL;
}

void fbt_reset_ptn(void)
{
	pcount = 0;
	if (fbt_load_partition_table())
		FBTERR("Unable to load partition table\n");
}

static void fbt_set_unlocked(int unlocked)
{
	char *unlocked_string;

	printf("Setting device to %s\n",
	       unlocked ? "unlocked" : "locked");
	priv.unlocked = unlocked;
	if (unlocked)
		unlocked_string = "1";
	else
		unlocked_string = "0";
	setenv(FASTBOOT_UNLOCKED_ENV_NAME, unlocked_string);
#if defined(CONFIG_CMD_SAVEENV)
	saveenv();
#endif
}

static void fbt_fastboot_init(void)
{
	char *fastboot_unlocked_env;
	priv.flag = 0;
	priv.d_size = 0;
	priv.d_bytes = 0;
	priv.u_size = 0;
	priv.u_bytes = 0;
	priv.exit = 0;
	priv.unlock_pending_start_time = 0;

	priv.unlocked = 1;
	fastboot_unlocked_env = getenv(FASTBOOT_UNLOCKED_ENV_NAME);
	if (fastboot_unlocked_env) {
		unsigned long unlocked;
		if (!strict_strtoul(fastboot_unlocked_env, 10, &unlocked)) {
			if (unlocked)
				priv.unlocked = 1;
			else
				priv.unlocked = 0;
		} else {
			printf("bad env setting %s of %s,"
			       " initializing to locked\n",
			       fastboot_unlocked_env,
			       FASTBOOT_UNLOCKED_ENV_NAME);
			fbt_set_unlocked(0);
		}
	} else {
		printf("no existing env setting for %s\n",
		       FASTBOOT_UNLOCKED_ENV_NAME);
		printf("creating one set to false\n");
		fbt_set_unlocked(0);
	}
	if (priv.unlocked)
		printf("Device is unlocked\n");
	else
		printf("Device is locked\n");

	/*
	 * We need to be able to run fastboot even if there isn't a partition
	 * table (so we can use "oem format") and fbt_load_partition_table
	 * already printed an error, so just ignore the error return.
	 */
	(void)fbt_load_partition_table();
}

static void fbt_handle_erase(char *cmdbuf)
{
	disk_partition_t *ptn;
	int err;
	char *partition_name = cmdbuf + 6;
	char *num_blocks_str;
	lbaint_t num_blocks;
	lbaint_t *num_blocks_p = NULL;

	/* see if there is an optional num_blocks after the partition name */
	num_blocks_str = strchr(partition_name, ' ');
	if (num_blocks_str) {
		/* null terminate the partition name */
		*num_blocks_str = 0;
		num_blocks_str++;
		num_blocks = simple_strtoull(num_blocks_str, NULL, 10);
		num_blocks_p = &num_blocks;
	}

	ptn = fastboot_flash_find_ptn(partition_name);
	if (ptn == 0) {
		printf("Partition %s does not exist\n", ptn->name);
		sprintf(priv.response, "FAILpartition does not exist");
		return;
	}

#ifndef CONFIG_MFG
	/* don't allow erasing a valid device info partition in a production
	 * u-boot */
	if (is_info_partition(ptn) && (!priv.dev_info_uninitialized)) {
		printf("Not allowed to erase %s partition\n", ptn->name);
		strcpy(priv.response, "FAILnot allowed to erase partition");
		return;
	}
#endif

	printf("Erasing partition '%s':\n", ptn->name);

	printf("\tstart blk %lu, blk_cnt %lu of %lu\n", ptn->start,
			num_blocks_p ? num_blocks : ptn->size, ptn->size);

	err = partition_erase_blks(priv.dev_desc, ptn, num_blocks_p);
	if (err) {
		printf("Erasing '%s' FAILED! error=%d\n", ptn->name, err);
		sprintf(priv.response,
				"FAILfailed to erase partition (%d)", err);
	} else {
		printf("partition '%s' erased\n", ptn->name);
		sprintf(priv.response, "OKAY");
	}
}

#define SPARSE_HEADER_MAJOR_VER 1

static int _unsparse(unsigned char *source,
					lbaint_t sector, lbaint_t num_blks)
{
	sparse_header_t *header = (void *) source;
	u32 i, outlen = 0;
	unsigned long blksz = priv.dev_desc->blksz;
	u64 section_size = (u64)num_blks * blksz;

	FBTINFO("sparse_header:\n");
	FBTINFO("\t         magic=0x%08X\n", header->magic);
	FBTINFO("\t       version=%u.%u\n", header->major_version,
						header->minor_version);
	FBTINFO("\t file_hdr_size=%u\n", header->file_hdr_sz);
	FBTINFO("\tchunk_hdr_size=%u\n", header->chunk_hdr_sz);
	FBTINFO("\t        blk_sz=%u\n", header->blk_sz);
	FBTINFO("\t    total_blks=%u\n", header->total_blks);
	FBTINFO("\t  total_chunks=%u\n", header->total_chunks);
	FBTINFO("\timage_checksum=%u\n", header->image_checksum);

	if (header->magic != SPARSE_HEADER_MAGIC) {
		printf("sparse: bad magic\n");
		return 1;
	}

	if (((u64)header->total_blks * header->blk_sz) > section_size) {
		printf("sparse: section size %llu MB limit: exceeded\n",
				section_size/(1024*1024));
		return 1;
	}

	if ((header->major_version != SPARSE_HEADER_MAJOR_VER) ||
	    (header->file_hdr_sz != sizeof(sparse_header_t)) ||
	    (header->chunk_hdr_sz != sizeof(chunk_header_t))) {
		printf("sparse: incompatible format\n");
		return 1;
	}

	/* Skip the header now */
	source += header->file_hdr_sz;

	for (i = 0; i < header->total_chunks; i++) {
		u64 clen = 0;
		lbaint_t blkcnt;
		chunk_header_t *chunk = (void *) source;

		FBTINFO("chunk_header:\n");
		FBTINFO("\t    chunk_type=%u\n", chunk->chunk_type);
		FBTINFO("\t      chunk_sz=%u\n", chunk->chunk_sz);
		FBTINFO("\t      total_sz=%u\n", chunk->total_sz);
		/* move to next chunk */
		source += sizeof(chunk_header_t);

		switch (chunk->chunk_type) {
		case CHUNK_TYPE_RAW:
			clen = (u64)chunk->chunk_sz * header->blk_sz;
			FBTINFO("sparse: RAW blk=%d bsz=%d:"
			       " write(sector=%lu,clen=%llu)\n",
			       chunk->chunk_sz, header->blk_sz, sector, clen);

			if (chunk->total_sz != (clen + sizeof(chunk_header_t))) {
				printf("sparse: bad chunk size for"
				       " chunk %d, type Raw\n", i);
				return 1;
			}

			outlen += clen;
			if (outlen > section_size) {
				printf("sparse: section size %llu MB limit:"
				       " exceeded\n", section_size/(1024*1024));
				return 1;
			}
			blkcnt = clen / blksz;
			FBTDBG("sparse: RAW blk=%d bsz=%d:"
			       " write(sector=%lu,clen=%llu)\n",
			       chunk->chunk_sz, header->blk_sz, sector, clen);
			if (priv.dev_desc->block_write(priv.dev_desc->dev,
						       sector, blkcnt, source)
						!= blkcnt) {
				printf("sparse: block write to sector %lu"
					" of %llu bytes (%ld blkcnt) failed\n",
					sector, clen, blkcnt);
				return 1;
			}

			sector += (clen / blksz);
			source += clen;
			break;

		case CHUNK_TYPE_DONT_CARE:
			if (chunk->total_sz != sizeof(chunk_header_t)) {
				printf("sparse: bogus DONT CARE chunk\n");
				return 1;
			}
			clen = chunk->chunk_sz * header->blk_sz;
			FBTDBG("sparse: DONT_CARE blk=%d bsz=%d:"
			       " skip(sector=%lu,clen=%llu)\n",
			       chunk->chunk_sz, header->blk_sz, sector, clen);

			outlen += clen;
			if (outlen > section_size) {
				printf("sparse: section size %llu MB limit:"
				       " exceeded\n", section_size/(1024*1024));
				return 1;
			}
			sector += (clen / blksz);
			break;

		default:
			printf("sparse: unknown chunk ID %04x\n",
			       chunk->chunk_type);
			return 1;
		}
	}

	printf("\nsparse: out-length-0x%d MB\n", outlen/(1024*1024));
	return 0;
}

static int do_unsparse(disk_partition_t *ptn, unsigned char *source,
					lbaint_t sector, lbaint_t num_blks)
{
	int rtn;
	if (partition_write_pre(ptn))
		return 1;

	rtn = _unsparse(source, sector, num_blks);

	if (partition_write_post(ptn))
		return 1;

	return rtn;
}

static int fbt_save_info(disk_partition_t *info_ptn)
{
	struct info_partition_header *info_header;
	char *name;
	char *value;
	int i;

	if (!priv.dev_info_uninitialized) {
		printf("%s partition already initialized, "
		       " cannot write to it again\n", info_ptn->name);
		return -1;
	}

	info_header = (struct info_partition_header *)priv.transfer_buffer;
	name = (char *)(info_header + 1);
	memset(info_header, 0, priv.dev_desc->blksz);
	memcpy(&info_header->magic, info_partition_magic,
	       sizeof(info_partition_magic));
	info_header->num_values = priv.num_device_info;

	for (i = 0; i < priv.num_device_info; i++) {
		unsigned int len = strlen(priv.dev_info[i].name);
		memcpy(name, priv.dev_info[i].name, len);
		value = name + len;
		*value++ = '=';
		if (priv.dev_info[i].value) {
			len = strlen(priv.dev_info[i].value);
			memcpy(value, priv.dev_info[i].value, len);
			name = value + len;
			*name++ = '\n';
		}
	}
	lbaint_t num_blks = 1;
	i = partition_write_blks(priv.dev_desc, info_ptn, &num_blks,
								info_header);
	if (i) {
		printf("block write to sector %lu failed, error=%d",
							info_ptn->start, i);
		return -1;
	}
	priv.dev_info_uninitialized = 0;
	return 0;
}

static void fbt_handle_flash(char *cmdbuf)
{
	disk_partition_t *ptn;

	if (!priv.unlocked) {
		sprintf(priv.response, "FAILdevice is locked");
		return;
	}

	if (!priv.d_bytes) {
		sprintf(priv.response, "FAILno image downloaded");
		return;
	}

	ptn = fastboot_flash_find_ptn(cmdbuf + 6);
	if (ptn == 0) {
		sprintf(priv.response, "FAILpartition does not exist");
		return;
	}

	/* Prevent using flash command to write to device_info partition */
	if (is_info_partition(ptn)) {
		sprintf(priv.response,
			"FAILpartition not writable using flash command");
		return;
	}

	/* Check if this is not really a flash write but rather a saveenv */
	if (is_env_partition(ptn)) {
		if (!himport_r(&env_htab,
			       (const char *)priv.transfer_buffer,
			       priv.d_bytes, '\n', H_NOCLEAR)) {
			FBTINFO("Import '%s' FAILED!\n", ptn->name);
			sprintf(priv.response, "FAIL: Import environment");
			return;
		}

#if defined(CONFIG_CMD_SAVEENV)
		if (saveenv()) {
			printf("Writing '%s' FAILED!\n", ptn->name);
			sprintf(priv.response, "FAIL: Write partition");
			return;
		}
		printf("saveenv to '%s' DONE!\n", ptn->name);
#endif
		sprintf(priv.response, "OKAY");
	} else {
		/* Normal case */
		printf("writing to partition '%s'\n", ptn->name);

		/* Check if we have sparse compressed image */
		if (((sparse_header_t *)priv.transfer_buffer)->magic
		    == SPARSE_HEADER_MAGIC) {
			printf("fastboot: %s is in sparse format\n", ptn->name);
			if (!do_unsparse(ptn, priv.transfer_buffer,
					 ptn->start, ptn->size)) {
				printf("Writing sparsed: '%s' DONE!\n",
				       ptn->name);
				sprintf(priv.response, "OKAY");
			} else {
				printf("Writing sparsed '%s' FAILED!\n",
				       ptn->name);
				sprintf(priv.response, "FAIL: Sparsed Write");
			}
		} else {
			/* Normal image: no sparse */
			int err;
			loff_t num_bytes = priv.d_bytes;

			printf("Writing %llu bytes to '%s'\n",
						num_bytes, ptn->name);
			err = partition_write_bytes(priv.dev_desc, ptn,
					&num_bytes, priv.transfer_buffer);
			if (err) {
				printf("Writing '%s' FAILED! error=%d\n",
							ptn->name, err);
				sprintf(priv.response,
					"FAILWrite partition, error=%d", err);
			} else {
				printf("Writing '%s' DONE!\n", ptn->name);
				sprintf(priv.response, "OKAY");
			}
		}
	} /* Normal Case */
}

static void fbt_handle_getvar(char *cmdbuf)
{
	strcpy(priv.response, "OKAY");
	char *subcmd = cmdbuf + strlen("getvar:");
	char *value = NULL;
	if (!strcmp(subcmd, "version"))
		value = version_string;
	else if (!strcmp(subcmd, "version-baseband"))
		value = "n/a";
	else if (!strcmp(subcmd, "version-bootloader"))
		value = CONFIG_FASTBOOT_VERSION_BOOTLOADER;
	else if (!strcmp(subcmd, "unlocked"))
		value = (priv.unlocked ? "yes" : "no");
	else if (!strcmp(subcmd, "secure")) {
		/* we use the inverse meaning of unlocked */
		value = (priv.unlocked ? "no" : "yes");
	}
#if defined (CONFIG_OMAP)
	else if (!strcmp(subcmd, "device_type")) {
		switch(get_device_type()) {
		case TST_DEVICE:
			value = "TST";
			break;
		case EMU_DEVICE:
			value = "EMU";
			break;
		case HS_DEVICE:
			value = "HS";
			break;
		case GP_DEVICE:
			value = "GP";
			break;
		default:
			value = "unknown";
			break;
		}
	}
#endif
	else if (!strcmp(subcmd, "product"))
		value = FASTBOOT_PRODUCT_NAME;
	else if (!strcmp(subcmd, "serialno"))
		value = priv.serial_no;
#ifdef CONFIG_FASTBOOT_UBOOT_GETVAR
	else {
		ENTRY e, *ep;

		e.key = subcmd;
		e.data = NULL;
		ep = NULL;
		if (hsearch_r(e, FIND, &ep, &env_htab) && ep != NULL)
			value = ep->data;
	}
#endif
	if (value) {
		/* At first I was reluctant to use strncpy because it
		 * typically pads the whole buffer with nulls, but U-Boot's
		 * strncpy does not do that.  However, I do rely on
		 * priv.null_term after priv.response in the struct
		 * cmd_fastboot_interface to ensure the strlen in
		 * fbt_response_process doesn't take a long time.
		 */
		strncpy(priv.response + 4, value, (sizeof(priv.response) - 4));
	}
}


static void fbt_handle_reboot(const char *cmdbuf)
{
	if (!strcmp(&cmdbuf[6], "-bootloader")) {
		FBTDBG("%s\n", cmdbuf);
		board_fbt_set_reboot_type(FASTBOOT_REBOOT_BOOTLOADER);
	}
	if (!strcmp(&cmdbuf[6], "-recovery")) {
		FBTDBG("%s\n", cmdbuf);
		board_fbt_set_reboot_type(FASTBOOT_REBOOT_RECOVERY);
	}

	strcpy(priv.response, "OKAY");
	priv.flag |= FASTBOOT_FLAG_RESPONSE;
	fbt_handle_response();
	udelay(1000000); /* 1 sec */

	board_fbt_end();

	do_reset(NULL, 0, 0, NULL);
}

static char tmp_buf[CONFIG_SYS_CBSIZE]; /* copy of fastboot cmdbuf */

static void fbt_handle_oem_setinfo(const char *cmdbuf)
{
	char *name, *value;
	struct device_info *di;

	FBTDBG("oem setinfo\n");

	/* this is only allowed if the device info isn't already
	 * initlialized in flash
	 */
	if (!priv.dev_info_uninitialized) {
		printf("Not allowed to change device info already in flash\n");
		strcpy(priv.response, "FAILnot allowed to change"
		       " device info already in flash");
		return;
	}

	if (priv.num_device_info == FASTBOOT_MAX_NUM_DEVICE_INFO) {
		printf("Already at maximum number of device info (%d),"
		       " no more allowed\n", FASTBOOT_MAX_NUM_DEVICE_INFO);
		strcpy(priv.response, "FAILmax device info reached");
		return;
	}

	/* copy to tmp_buf which will be modified by str_tok() */
	strcpy(tmp_buf, cmdbuf);

	name = strtok(tmp_buf, "=");
	value = strtok(NULL, "\n");
	if (!name || !value) {
		printf("Invalid format for setinfo.\n");
		printf("Syntax is "
		       "'fastboot oem setinfo <info_name>=<info_value>\n");
		strcpy(priv.response, "FAILinvalid device info");
		return;
	}

	/* we enter new value at end so last slot should be free.
	 * we don't currently allow changing a value already set.
	 */
	di = &priv.dev_info[priv.num_device_info];
	if (di->name || di->value) {
		printf("Error, device info entry not free as expected\n");
		strcpy(priv.response, "FAILinternal error");
		return;
	}

	di->name = strdup(name);
	di->value = strdup(value);
	if ((di->name == NULL) || (di->value == NULL)) {
		printf("strdup() failed, unable to set info\n");
		strcpy(priv.response, "FAILstrdup() failure\n");
		free(di->name);
		free(di->value);
		return;
	}

	printf("Set device info %s=%s\n", di->name, di->value);
	if (!strcmp(di->name, FASTBOOT_SERIALNO_BOOTARG))
		set_serial_number(di->value);
	priv.num_device_info++;

	strcpy(priv.response, "OKAY");
}

static int fbt_send_raw_info(const char *info, int bytes_left)
{
	int response_max;

	if (!priv.executing_command)
		return -1;

	/* break up info into response sized chunks */
	/* remove trailing '\n' */
	if (info[bytes_left-1] == '\n')
		bytes_left--;

	/* -4 for the INFO prefix */
	response_max = sizeof(priv.response) - 4;
	strcpy(priv.response, "INFO");
	while (1) {
		if (bytes_left >= response_max) {
			strncpy(priv.response + 4, info,
				response_max);

			/* flush any data set by command */
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
			fbt_handle_response();
			fbt_wait_usb_fifo_flush();

			info += response_max;
			bytes_left -= response_max;
		} else {
			strncpy(priv.response + 4, info,
				bytes_left);

			/* in case we stripped '\n',
			   make sure priv.response is
			   terminated */
			priv.response[4 + bytes_left] = '\0';
			break;
		}
	}

	priv.flag |= FASTBOOT_FLAG_RESPONSE;
	fbt_handle_response();
	fbt_wait_usb_fifo_flush();

	return 0;
}

static void fbt_dump_log(void)
{
	/* the log consists of a bunch of printf output, with
	 * logs of '\n' interspersed. to make it format a
	 * bit better when sending it via the INFO
	 * part of the fastboot protocol, which has a limited
	 * buffer, break the log into bits that end
	 * with '\n', like replaying the printfs.
	 */
	int bytes_left = log_position;
	char *line_start = log_buffer;
	while (bytes_left) {
		char *next_line  = strchr(line_start, '\n');
		if (next_line) {
			int len = next_line - line_start + 1;
			fbt_send_raw_info(line_start, len);
			line_start += len;
			bytes_left -= len;
		} else {
			fbt_send_raw_info(line_start, strlen(line_start));
			break;
		}
	}
}

static void fbt_handle_oem(char *cmdbuf)
{
	cmdbuf += 4;

	/* %fastboot oem recovery */
	if (strcmp(cmdbuf, "recovery") == 0) {
		FBTDBG("oem recovery\n");
		fbt_handle_reboot("reboot-recovery");
		return;
	}

	/* %fastboot oem unlock */
	if (strcmp(cmdbuf, "unlock") == 0) {
		FBTDBG("oem unlock\n");
		if (priv.unlocked) {
			printf("oem unlock ignored, device already unlocked\n");
			strcpy(priv.response, "FAILalready unlocked");
			return;
		}
		printf("oem unlock requested:\n");
		printf("\tUnlocking your device will invalidate\n");
		printf("\tyour warranty and wipe user data.\n");
		printf("\tDo 'fastboot oem unlock_accept' to accept\n");
		printf("\tthese conditions within %d seconds.\n",
		       FASTBOOT_UNLOCK_TIMEOUT_SECS);
		priv.unlock_pending_start_time = get_timer(0);
		strcpy(priv.response, "OKAY");
		return;
	}

	if (strcmp(cmdbuf, "unlock_accept") == 0) {
		int err;
		FBTDBG("oem unlock_accept\n");
		if (!priv.unlock_pending_start_time) {
			printf("oem unlock_accept ignored, not pending\n");
			strcpy(priv.response, "FAILoem unlock not requested");
			return;
		}
		priv.unlock_pending_start_time = 0;
		printf("Erasing userdata partition\n");
		err = partition_erase_blks(priv.dev_desc,
					fastboot_flash_find_ptn("userdata"),
					NULL);
		if (err) {
			printf("Erase failed with error %d\n", err);
			strcpy(priv.response, "FAILErasing userdata failed");
			return;
		}
		printf("Erasing succeeded\n");
		fbt_set_unlocked(1);
		strcpy(priv.response, "OKAY");
		return;
	}

	if (strcmp(cmdbuf, "lock") == 0) {
		FBTDBG("oem lock\n");
		if (!priv.unlocked) {
			printf("oem lock ignored, already locked\n");
			strcpy(priv.response, "FAILalready locked");
			return;
		}
		fbt_set_unlocked(0);
		strcpy(priv.response, "OKAY");
		return;
	}

	/* %fastboot oem setinfo <info_name>=<info_value> */
	if (strncmp(cmdbuf, "setinfo ", 8) == 0) {
		cmdbuf += 8;
		fbt_handle_oem_setinfo(cmdbuf);
		return;
	}

	/* %fastboot oem saveinfo */
	if (strcmp(cmdbuf, "saveinfo") == 0) {
		disk_partition_t *info_ptn;
		info_ptn = fastboot_flash_find_ptn(CONFIG_INFO_PARTITION);

		if (info_ptn == NULL) {
			sprintf(priv.response, "FAILpartition does not exist");
			return;
		}
		if (fbt_save_info(info_ptn)) {
			printf("Writing '%s' FAILED!\n", info_ptn->name);
			sprintf(priv.response, "FAIL: Write partition");
		} else {
			printf("Device info saved to partition '%s'\n",
			       info_ptn->name);
			sprintf(priv.response, "OKAY");
		}
		return;
	}

	/* %fastboot oem erase partition <numblocks>
	 * similar to 'fastboot erase' except an optional number
	 * of blocks can be passed to erase less than the
	 * full partition, for speed
	 */
	if (strncmp(cmdbuf, "erase ", 6) == 0) {
		FBTDBG("oem %s\n", cmdbuf);
		fbt_handle_erase(cmdbuf);
		return;
	}

	/* %fastboot oem log */
	if (strcmp(cmdbuf, "log") == 0) {
		FBTDBG("oem %s\n", cmdbuf);
		fbt_dump_log();
		strcpy(priv.response, "OKAY");
		return;
	}

	/* %fastboot oem ucmd ... */
	if (strncmp(cmdbuf, "ucmd ", 5) == 0) {
		FBTDBG("oem %s\n", cmdbuf);
		cmdbuf += 5;

		if (run_command(cmdbuf, 0) < 0)
			strcpy(priv.response, "FAILcommand failed");
		else
			strcpy(priv.response, "OKAY");
		return;
	}

	/* %fastboot oem [xxx] */
	FBTDBG("oem %s\n", cmdbuf);
	if (board_fbt_oem(cmdbuf) >= 0) {
		strcpy(priv.response, "OKAY");
		return;
	}

	printf("\nfastboot: unsupported oem command %s\n", cmdbuf);
	strcpy(priv.response, "FAILinvalid command");
}

static void fbt_handle_boot(const char *cmdbuf)
{
	if ((priv.d_bytes) &&
		(CONFIG_FASTBOOT_MKBOOTIMAGE_PAGE_SIZE < priv.d_bytes)) {
		char start[32];
		char *booti[] = { "booti", start };
		char *go[]    = { "go",    start };

		/*
		 * Use this later to determine if a command line was passed
		 * for the kernel.
		 */
		struct fastboot_boot_img_hdr *fb_hdr =
			(struct fastboot_boot_img_hdr *) priv.transfer_buffer;

		sprintf(start, "%p", fb_hdr);

		/* Execution should jump to kernel so send the response
		   now and wait a bit.  */
		sprintf(priv.response, "OKAY");
		priv.flag |= FASTBOOT_FLAG_RESPONSE;
		fbt_handle_response();
		udelay(1000000); /* 1 sec */

		do_booti(NULL, 0, ARRAY_SIZE(booti), booti);

		printf("do_booti() returned, trying go..\n");

		FBTINFO("Booting raw image..\n");
		do_go(NULL, 0, ARRAY_SIZE(go), go);

		FBTERR("booting failed, reset the board\n");
	}
	sprintf(priv.response, "FAILinvalid boot image");
}

static void fbt_rx_process_download(unsigned char *buffer, int length)
{
	unsigned int xfr_size;

	if (length == 0) {
		FBTWARN("empty buffer download\n");
		return;
	}

	xfr_size = priv.d_size - priv.d_bytes;
	if (xfr_size > length)
		xfr_size = length;
	memcpy(priv.transfer_buffer + priv.d_bytes, buffer, xfr_size);
	priv.d_bytes += xfr_size;

	if (priv.d_bytes >= priv.d_size) {
		priv.d_size = 0;
		strcpy(priv.response, "OKAY");
		priv.flag |= FASTBOOT_FLAG_RESPONSE;

		FBTINFO("downloaded %llu bytes\n", priv.d_bytes);
	}
}

/* XXX: Replace magic number & strings with macros */
static void fbt_rx_process(unsigned char *buffer, int length)
{
	/* Generic failed response */
	strcpy(priv.response, "FAIL");

	if (!priv.d_size) {
		/* command */
		char *cmdbuf = (char *) buffer;

		FBTDBG("command\n");

		printf("cmdbuf = (%s)\n", cmdbuf);
		priv.executing_command = 1;

		/* %fastboot getvar: <var_name> */
		if (memcmp(cmdbuf, "getvar:", 7) == 0) {
			FBTDBG("getvar\n");
			fbt_handle_getvar(cmdbuf);
		}

		/* %fastboot oem <cmd> */
		else if (memcmp(cmdbuf, "oem ", 4) == 0) {
			FBTDBG("oem\n");
			fbt_handle_oem(cmdbuf);
		}

		/* %fastboot erase <partition_name> */
		else if (memcmp(cmdbuf, "erase:", 6) == 0) {
			FBTDBG("erase\n");
			fbt_handle_erase(cmdbuf);
		}

		/* %fastboot flash:<partition_name> */
		else if (memcmp(cmdbuf, "flash:", 6) == 0) {
			FBTDBG("flash\n");
			fbt_handle_flash(cmdbuf);
		}

		/* %fastboot reboot
		 * %fastboot reboot-bootloader
		 */
		else if (memcmp(cmdbuf, "reboot", 6) == 0) {
			FBTDBG("reboot or reboot-bootloader\n");
			fbt_handle_reboot(cmdbuf);
			return;
		}

		/* %fastboot continue */
		else if (strcmp(cmdbuf, "continue") == 0) {
			FBTDBG("continue\n");
			strcpy(priv.response, "OKAY");
			priv.exit = 1;
		}

		/* %fastboot boot <kernel> [ <ramdisk> ] */
		else if (memcmp(cmdbuf, "boot", 4) == 0) {
			FBTDBG("boot\n");
			fbt_handle_boot(cmdbuf);
		}

		/* Sent as part of a '%fastboot flash <partname>' command
		 * This sends the data over with byte count:
		 * %download:<num_bytes>
		 */
		else if (memcmp(cmdbuf, "download:", 9) == 0) {
			FBTDBG("download\n");

			/* XXX: need any check for size & bytes ? */
			priv.d_size =
				simple_strtoul (cmdbuf + 9, NULL, 16);
			priv.d_bytes = 0;

			FBTINFO("starting download of %llu bytes\n",
				priv.d_size);

			if (priv.d_size == 0) {
				strcpy(priv.response, "FAILdata invalid size");
			} else if (priv.d_size >
					priv.transfer_buffer_size) {
				priv.d_size = 0;
				strcpy(priv.response, "FAILdata too large");
			} else {
				sprintf(priv.response, "DATA%08llx",
					priv.d_size);
			}
		}

		priv.flag |= FASTBOOT_FLAG_RESPONSE;
		priv.executing_command = 0;
		return;
	}

	fbt_rx_process_download(buffer, length);
}

static void fbt_handle_rx(void)
{
	struct usb_endpoint_instance *ep = &endpoint_instance[RX_EP_INDEX];

	/* XXX: Or update status field, if so,
		"usbd_rcv_complete" [gadget/core.c] also need to be modified */
	if (ep->rcv_urb->actual_length) {
		FBTDBG("rx length: %u\n", ep->rcv_urb->actual_length);
		fbt_rx_process(ep->rcv_urb->buffer, ep->rcv_urb->actual_length);
		/* Required to poison rx urb buffer as in omapzoom ?,
		   yes, as fastboot command are sent w/o NULL termination.
		   Attempt is made here to reduce poison length, may be safer
		   to posion the whole buffer, also it is assumed that at
		   the time of creation of urb it is poisoned */
		memset(ep->rcv_urb->buffer, 0, ep->rcv_urb->actual_length);
		ep->rcv_urb->actual_length = 0;
	}
}

static void fbt_response_process(void)
{
	struct usb_endpoint_instance *ep = &endpoint_instance[TX_EP_INDEX];
	struct urb *current_urb = NULL;
	unsigned char *dest = NULL;
	int n;

	current_urb = next_urb(device_instance, ep);
	if (!current_urb) {
		FBTERR("%s: current_urb NULL", __func__);
		return;
	}

	dest = current_urb->buffer + current_urb->actual_length;
	n = MIN(64, strlen(priv.response));
	memcpy(dest, priv.response, n);
	current_urb->actual_length += n;
	/*
	 * This FBTDBG appears to break communication when DEBUG
	 * is on, so comment it out.
	FBTDBG("response urb length: %u\n", current_urb->actual_length);
	 */
	if (ep->last == 0)
		udc_endpoint_write(ep);
}

static void fbt_handle_response(void)
{
	if (priv.flag & FASTBOOT_FLAG_RESPONSE) {
		fbt_response_process();
		priv.flag &= ~FASTBOOT_FLAG_RESPONSE;
	}
}

/*
 * default board-specific hooks and defaults
 */
static int __def_fbt_oem(const char *cmdbuf)
{
	return -1;
}
static void __def_fbt_set_reboot_type(enum fbt_reboot_type fre)
{
}
static enum fbt_reboot_type __def_fbt_get_reboot_type(void)
{
	return FASTBOOT_REBOOT_NORMAL;
}
static int __def_fbt_key_pressed(void)
{
	return 0;
}
static int __def_fbt_load_ptbl(void)
{
	u64 length;
	disk_partition_t ptn;
	int n;
	int res = -1;
	block_dev_desc_t *blkdev = priv.dev_desc;
	unsigned long blksz = blkdev->blksz;

	init_part(blkdev);
	if (blkdev->part_type == PART_TYPE_UNKNOWN) {
		printf("unknown partition table on %s\n", FASTBOOT_BLKDEV);
		return -1;
	}

	printf("lba size = %lu\n", blksz);
	printf("lba_start      partition_size          name\n");
	printf("=========  ======================  ==============\n");
	for (n = CONFIG_MIN_PARTITION_NUM; n <= CONFIG_MAX_PARTITION_NUM; n++) {
		if (get_partition_info(blkdev, n, &ptn))
			continue;	/* No partition <n> */
		if (!ptn.size || !ptn.blksz || !ptn.name[0])
			continue;	/* Partition <n> is empty (or sick) */
		fbt_add_ptn(&ptn);

		length = (u64)blksz * ptn.size;
		if (length > (1024 * 1024))
			printf(" %8lu  %12llu(%7lluM)  %s\n",
						ptn.start,
						length, length/(1024*1024),
						ptn.name);
		else
			printf(" %8lu  %12llu(%7lluK)  %s\n",
						ptn.start,
						length, length/1024,
						ptn.name);
		res = 0;
	}
	printf("=========  ======================  ==============\n");
	return res;
}
static void __def_fbt_start(void)
{
}
static void __def_fbt_end(void)
{
}
static void __def_board_fbt_finalize_bootargs(char* args, size_t buf_sz)
{
	return;
}

int board_fbt_oem(const char *cmdbuf)
	__attribute__((weak, alias("__def_fbt_oem")));
void board_fbt_set_reboot_type(enum fbt_reboot_type fre)
	__attribute__((weak, alias("__def_fbt_set_reboot_type")));
enum fbt_reboot_type board_fbt_get_reboot_type(void)
	__attribute__((weak, alias("__def_fbt_get_reboot_type")));
int board_fbt_key_pressed(void)
	__attribute__((weak, alias("__def_fbt_key_pressed")));
int board_fbt_load_ptbl(void)
	__attribute__((weak, alias("__def_fbt_load_ptbl")));
void board_fbt_start(void)
	__attribute__((weak, alias("__def_fbt_start")));
void board_fbt_end(void)
	__attribute__((weak, alias("__def_fbt_end")));
void board_fbt_finalize_bootargs(char* args, size_t buf_sz)
	__attribute__((weak, alias("__def_board_fbt_finalize_bootargs")));

/* command */
static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc,
							char * const argv[])
{
	int ret;

	if (!priv.dev_desc) {
		printf("fastboot was not successfully initialized\n");
		return -1;
	}

	printf("Starting fastboot protocol\n");

	board_fbt_start();

	fbt_fastboot_init();

	fbt_init_endpoint_ptrs();

	ret = udc_init();
	if (ret < 0) {
		FBTERR("%s: MUSB UDC init failure\n", __func__);
		goto out;
	}

	fbt_init_strings();
	fbt_init_instances();

	udc_startup_events(device_instance);
	udc_connect();

	FBTINFO("fastboot initialized\n");

	while (1) {
		udc_irq();
		if (priv.configured) {
			fbt_handle_rx();
			if (priv.unlock_pending_start_time) {
				/* check if unlock pending should expire */
				if (get_timer(priv.unlock_pending_start_time) >
				    (FASTBOOT_UNLOCK_TIMEOUT_SECS * 1000)) {
					printf("unlock pending expired\n");
					priv.unlock_pending_start_time = 0;
				}
			}
			fbt_handle_response();
		}
		priv.exit |= ctrlc();
		if (priv.exit) {
			FBTINFO("fastboot end\n");
			break;
		}
	}

out:
	board_fbt_end();
	return ret;
}

U_BOOT_CMD(fastboot, 1,	1, do_fastboot,
	"use USB Fastboot protocol", NULL);

/* Section for Android bootimage format support
 * Refer:
 * http://android.git.kernel.org/?p=platform/system/core.git;a=blob;f=mkbootimg/bootimg.h
 */
static void bootimg_print_image_hdr(struct fastboot_boot_img_hdr *hdr)
{
#ifdef DEBUG
	int i;
	printf("   Image magic:   %s\n", hdr->magic);

	printf("   kernel_size:   0x%x\n", hdr->kernel_size);
	printf("   kernel_addr:   0x%x\n", hdr->kernel_addr);

	printf("   rdisk_size:   0x%x\n", hdr->ramdisk_size);
	printf("   rdisk_addr:   0x%x\n", hdr->ramdisk_addr);

	printf("   second_size:   0x%x\n", hdr->second_size);
	printf("   second_addr:   0x%x\n", hdr->second_addr);

	printf("   tags_addr:   0x%x\n", hdr->tags_addr);
	printf("   page_size:   0x%x\n", hdr->page_size);

	printf("   name:      %s\n", hdr->name);
	printf("   cmdline:   %s\n", hdr->cmdline);

	for (i = 0; i < 8; i++)
		printf("   id[%d]:   0x%x\n", i, hdr->id[i]);
#endif
}

/* booti [ <addr> | <partition> ] */
static int do_booti(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *boot_source = "boot";
	block_dev_desc_t *blkdev = priv.dev_desc;
	disk_partition_t *ptn;
	struct fastboot_boot_img_hdr *hdr = NULL;
	bootm_headers_t images;
	int need_post_ran = 0;

	if (argc >= 2)
		boot_source = argv[1];

	if (!blkdev) {
		printf("fastboot was not successfully initialized\n");
		return -1;
	}

	ptn = fastboot_flash_find_ptn(boot_source);
	if (ptn) {
		unsigned long blksz;
		unsigned sector;
		unsigned blocks;

		if (partition_read_pre(ptn)) {
			printf("pre-read commands for partition '%s' failed\n",
								ptn->name);
			goto fail;
		}
		need_post_ran = 1;

		blksz = blkdev->blksz;
		hdr = malloc(blksz);
		if (hdr == NULL) {
			printf("error allocating blksz(%lu) buffer\n", blksz);
			goto fail;
		}
		if (blkdev->block_read(blkdev->dev, ptn->start,
					      1, (void *) hdr) != 1) {
			printf("booti: failed to read bootimg header\n");
			goto fail;
		}
		if (memcmp(hdr->magic, FASTBOOT_BOOT_MAGIC,
			   FASTBOOT_BOOT_MAGIC_SIZE)) {
			printf("booti: bad boot image magic\n");
			goto fail;
		}

		bootimg_print_image_hdr(hdr);

		sector = ptn->start + (hdr->page_size / blksz);
		blocks = DIV_ROUND_UP(hdr->kernel_size, blksz);
		if (blkdev->block_read(blkdev->dev, sector, blocks,
					      (void *) hdr->kernel_addr) !=
		    blocks) {
			printf("booti: failed to read kernel\n");
			goto fail;
		}

		sector += ALIGN(hdr->kernel_size, hdr->page_size) / blksz;
		blocks = DIV_ROUND_UP(hdr->ramdisk_size, blksz);
		if (blkdev->block_read(blkdev->dev, sector, blocks,
					      (void *) hdr->ramdisk_addr) !=
		    blocks) {
			printf("booti: failed to read ramdisk\n");
			goto fail;
		}
		if (need_post_ran) {
			need_post_ran = 0;
			if (partition_read_post(ptn)) {
				printf("post-read commands for partition '%s' "
							"failed\n", ptn->name);
				goto fail;
			}
		}
	} else {
		unsigned addr;
		void *kaddr, *raddr;
		char *ep;

		addr = simple_strtoul(boot_source, &ep, 16);
		if (ep == boot_source || *ep != '\0') {
			printf("'%s' does not seem to be a partition nor "
						"an address\n", boot_source);
			return cmd_usage(cmdtp);
		}

		hdr = malloc(sizeof(*hdr));
		if (hdr == NULL) {
			printf("error allocating buffer\n");
			goto fail;
		}

		/* set this aside somewhere safe */
		memcpy(hdr, (void *) addr, sizeof(*hdr));

		if (memcmp(hdr->magic, FASTBOOT_BOOT_MAGIC,
			   FASTBOOT_BOOT_MAGIC_SIZE)) {
			printf("booti: bad boot image magic\n");
			goto fail;
		}

		bootimg_print_image_hdr(hdr);

		kaddr = (void *)(addr + hdr->page_size);
		raddr = (void *)(kaddr + ALIGN(hdr->kernel_size,
					       hdr->page_size));
		memmove((void *)hdr->kernel_addr, kaddr, hdr->kernel_size);
		memmove((void *)hdr->ramdisk_addr, raddr, hdr->ramdisk_size);
	}

	printf("kernel   @ %08x (%d)\n", hdr->kernel_addr, hdr->kernel_size);
	printf("ramdisk  @ %08x (%d)\n", hdr->ramdisk_addr, hdr->ramdisk_size);

#ifdef CONFIG_CMDLINE_TAG

#ifdef CONFIG_FASTBOOT_PRESERVE_BOOTARGS
	setenv("hdr_cmdline", (char *)hdr->cmdline);
#else
	{
		/* static just to be safe when it comes to the stack */
		static char command_line[1024];
		int i, amt;

		/* Use the command line in the bootimg header instead of
		 * any hardcoded into u-boot.  Also, Android wants the
		 * serial number on the command line instead of via
		 * tags so append the serial number to the bootimg header
		 * value and set the bootargs environment variable.
		 * do_bootm_linux() will use the bootargs environment variable
		 * to pass it to the kernel.  Add the bootloader
		 * version too.
		 */
		amt = snprintf(command_line,
				sizeof(command_line),
				"%s androidboot.bootloader=%s",
				hdr->cmdline,
				CONFIG_FASTBOOT_VERSION_BOOTLOADER);

		for (i = 0; i < priv.num_device_info; i++) {
			/* Append device specific information like
			 * MAC addresses and serialno
			 */
			amt += snprintf(command_line + amt,
					sizeof(command_line) - amt,
					" %s=%s",
					priv.dev_info[i].name,
					priv.dev_info[i].value);
		}

		/* append serial number if it wasn't in device_info already */
		if (!strstr(command_line, FASTBOOT_SERIALNO_BOOTARG)) {
			snprintf(command_line + amt, sizeof(command_line) - amt,
				 " %s=%s", FASTBOOT_SERIALNO_BOOTARG,
				 priv.serial_no);
		}

		command_line[sizeof(command_line) - 1] = 0;
		board_fbt_finalize_bootargs(command_line, sizeof(command_line));

		setenv("bootargs", command_line);
	}
#endif /* CONFIG_FASTBOOT_PRESERVE_BOOTARGS */
#endif /* CONFIG_CMDLINE_TAG */

	memset(&images, 0, sizeof(images));
	images.ep = hdr->kernel_addr;
	images.rd_start = hdr->ramdisk_addr;
	images.rd_end = hdr->ramdisk_addr + hdr->ramdisk_size;
	free(hdr);
	do_bootm_linux(0, 0, NULL, &images);

	puts("booti: Control returned to monitor - resetting...\n");
	do_reset(cmdtp, flag, argc, argv);
	return 1;

fail:
	if (need_post_ran && partition_read_post(ptn))
		printf("post-read commands for partition '%s' failed\n",
								ptn->name);
	/* if booti fails, always start fastboot */
	free(hdr); /* hdr may be NULL, but that's ok. */
	return do_fastboot(NULL, 0, 0, NULL);
}

U_BOOT_CMD(
	booti,	2,	1,	do_booti,
	"boot android bootimg",
	"[ <addr> | <partition> ]\n    - boot application image\n"
	"\t'addr' should be the address of the boot image which is\n"
	"\tzImage+ramdisk.img if in memory.  'partition' is the name\n"
	"\tof the partition to boot from.  The default is to boot\n"
	"\tfrom the 'boot' partition.\n"
);

static void fbt_clear_recovery_flag(void)
{
	setenv(FASTBOOT_RUN_RECOVERY_ENV_NAME, NULL);
#if defined(CONFIG_CMD_SAVEENV)
	saveenv();
#endif
}

static void fbt_run_recovery(int do_saveenv)
{
	/* to make recovery (which processes OTAs) more failsafe,
	 * we save the fact that we were asked to boot into
	 * recovery.  if power is pulled and then restored, we
	 * will use that info to rerun recovery again and try
	 * to complete the OTA installation.
	 */
	if (do_saveenv) {
		setenv(FASTBOOT_RUN_RECOVERY_ENV_NAME, "1");
#ifdef CONFIG_CMD_SAVEENV
		saveenv();
#endif
	}

	char *const boot_recovery_cmd[] = {"booti", "recovery"};
	do_booti(NULL, 0, ARRAY_SIZE(boot_recovery_cmd), boot_recovery_cmd);

	/* returns if recovery.img is bad */
	printf("\nfastboot: Error: Invalid recovery img\n");

	/* Always clear so we don't wind up rebooting again into
	 * bad recovery img.
	 */
	fbt_clear_recovery_flag();
}

static void fbt_request_start_fastboot(void)
{
	char buf[512];
	char *old_preboot = getenv("preboot");
	printf("old preboot env = %s\n", old_preboot);

	fbt_clear_recovery_flag();

	if (old_preboot) {
		snprintf(buf, sizeof(buf),
			 "setenv preboot %s; fastboot", old_preboot);
		setenv("preboot", buf);
	} else
		setenv("preboot", "setenv preboot; fastboot");

	printf("%s: setting preboot env to %s\n", __func__, getenv("preboot"));
}

/*
 * Determine if we should
 * enter fastboot mode based on board specific key press or
 * parameter left in memory from previous boot.
 */
void fbt_preboot(void)
{
	enum fbt_reboot_type frt;


	priv.dev_desc = get_dev_by_name(FASTBOOT_BLKDEV);
	if (!priv.dev_desc) {
		FBTERR("%s: fastboot device %s not found\n",
						__func__, FASTBOOT_BLKDEV);
		return;
	}

	if (board_fbt_key_pressed()) {
		fbt_request_start_fastboot();
		return;
	}

	frt = board_fbt_get_reboot_type();
	if (frt == FASTBOOT_REBOOT_RECOVERY) {
		printf("\n%s: starting recovery img because of reboot flag\n",
		       __func__);

		return fbt_run_recovery(1);
	} else if (frt == FASTBOOT_REBOOT_BOOTLOADER) {

		/* Case: %fastboot reboot-bootloader
		 * Case: %adb reboot bootloader
		 * Case: %adb reboot-bootloader
		 */
		printf("\n%s: starting fastboot because of reboot flag\n",
		       __func__);
		fbt_request_start_fastboot();
	} else if (frt == FASTBOOT_REBOOT_NORMAL) {
		/* explicit request for a regular reboot */
		printf("\n%s: request for a normal boot\n",
		       __func__);
		fbt_clear_recovery_flag();
	} else {
		/* unkown reboot cause (typically because of a cold boot).
		 * check if we had flag set to boot recovery and it
		 * was never cleared properly (i.e. recovery didn't finish).
		 * if so, jump to recovery again.
		 */
		char *run_recovery = getenv(FASTBOOT_RUN_RECOVERY_ENV_NAME);
		if (run_recovery) {
			printf("\n%s: starting recovery because of "
			       "saved reboot flag\n", __func__);
			return fbt_run_recovery(0);
		}
		printf("\n%s: no special reboot flags, doing normal boot\n",
		       __func__);
	}
}

int fbt_send_info(const char *info)
{
	int len;
	unsigned long space_in_log = CONFIG_FASTBOOT_LOG_SIZE - log_position;
	unsigned long bytes_to_log;

	len = strlen(info);

	/* check if relocation is done before we can use globals */
	if (gd->flags & GD_FLG_RELOC) {
		if (len > space_in_log)
			bytes_to_log = space_in_log;
		else
			bytes_to_log = len;

		if (bytes_to_log) {
			strncpy(&log_buffer[log_position], info, bytes_to_log);
			log_position += bytes_to_log;
		}
	}

	return fbt_send_raw_info(info, len);
}

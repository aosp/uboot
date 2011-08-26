/*
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
#include <mmc.h>

#define	ERR
#define	WARN
//#define INFO
//#define DEBUG

#ifdef DEBUG
#define FBTDBG(fmt,args...)\
        printf("DEBUG: [%s]: %d: \n"fmt, __FUNCTION__, __LINE__,##args)
#else
#define FBTDBG(fmt,args...) do{}while(0)
#endif

#ifdef INFO
#define FBTINFO(fmt,args...)\
        printf("INFO: [%s]: "fmt, __FUNCTION__, ##args)
#else
#define FBTINFO(fmt,args...) do{}while(0)
#endif

#ifdef WARN
#define FBTWARN(fmt,args...)\
        printf("WARNING: [%s]: "fmt, __FUNCTION__, ##args)
#else
#define FBTWARN(fmt,args...) do{}while(0)
#endif

#ifdef ERR
#define FBTERR(fmt,args...)\
        printf("ERROR: [%s]: "fmt, __FUNCTION__, ##args)
#else
#define FBTERR(fmt,args...) do{}while(0)
#endif

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#include <nand.h>
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

static int fbt_handle_response(void);

/* defined and used by gadget/ep0.c */
extern struct usb_string_descriptor **usb_strings;

static struct cmd_fastboot_interface priv;

/* USB Descriptor Strings */
static char serial_number[33]; /* what should be the length ?, 33 ? */
static u8 wstr_lang[4] = {4,USB_DT_STRING,0x9,0x4};
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
	.idProduct = 		cpu_to_le16(CONFIG_USBD_PRODUCTID),
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

#define	GETVARLEN	30
#define	SECURE		"no"
/* U-boot version */
extern char version_string[];

static struct cmd_fastboot_interface priv =
{
        .transfer_buffer       = (unsigned char *)CONFIG_FASTBOOT_TRANSFER_BUFFER,
        .transfer_buffer_size  = CONFIG_FASTBOOT_TRANSFER_BUFFER_SIZE,
};

static int fbt_init_endpoints (void);

extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
/* Use do_bootm and do_go for fastboot's 'boot' command */
extern int do_bootm (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
extern int do_go (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
extern int do_bootm_linux(int flag, int argc, char *argv[], bootm_headers_t *images);
int do_booti (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
extern int do_env_save (cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
/* Use do_env_set and do_env_save to permenantly save data */
extern int do_env_set ( cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);
extern int do_switch_ecc(cmd_tbl_t * cmdtp, int flag, int argc, char *const argv[]);
extern int do_nand(cmd_tbl_t * cmdtp, int flag, int argc, char *const argv[]);

#else

extern int do_mmcops(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[]);

#endif

/* To support the Android-style naming of flash */
#define MAX_PTN 16
static fastboot_ptentry ptable[MAX_PTN];
static unsigned int pcount;
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
static int static_pcount = -1;
#endif

/* USB specific */

/* utility function for converting char* to wide string used by USB */
static void str2wide (char *str, u16 * wide)
{
	int i;
	for (i = 0; i < strlen (str) && str[i]; i++){
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
static int fbt_init_strings(void)
{
	struct usb_string_descriptor *string;

	fbt_string_table[STR_LANG] =
		(struct usb_string_descriptor*)wstr_lang;

	string = (struct usb_string_descriptor *) wstr_manufacturer;
	string->bLength = sizeof(wstr_manufacturer);
	string->bDescriptorType = USB_DT_STRING;
	str2wide (CONFIG_USBD_MANUFACTURER, string->wData);
	fbt_string_table[STR_MANUFACTURER] = string;

	string = (struct usb_string_descriptor *) wstr_product;
	string->bLength = sizeof(wstr_product);
	string->bDescriptorType = USB_DT_STRING;
	str2wide (CONFIG_USBD_PRODUCT_NAME, string->wData);
	fbt_string_table[STR_PRODUCT] = string;

	string = (struct usb_string_descriptor *) wstr_serial;
	string->bLength = sizeof(wstr_serial);
	string->bDescriptorType = USB_DT_STRING;
	str2wide (serial_number, string->wData);
	fbt_string_table[STR_SERIAL] = string;

	string = (struct usb_string_descriptor *) wstr_configuration;
	string->bLength = sizeof(wstr_configuration);
	string->bDescriptorType = USB_DT_STRING;
	str2wide (CONFIG_USBD_CONFIGURATION_STR, string->wData);
	fbt_string_table[STR_CONFIGURATION] = string;

	string = (struct usb_string_descriptor *) wstr_interface;
	string->bLength = sizeof(wstr_interface);
	string->bDescriptorType = USB_DT_STRING;
	str2wide (CONFIG_USBD_INTERFACE_STR, string->wData);
	fbt_string_table[STR_INTERFACE] = string;

	/* Now, initialize the string table for ep0 handling */
	usb_strings = fbt_string_table;

	return 0;
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
		fbt_init_endpoints ();

	default:
		break;
	}
}

/* fastboot_init has to be called before this fn to get correct serial string */
static int fbt_init_instances(void)
{
	int i;

	/* initialize device instance */
	memset (device_instance, 0, sizeof (struct usb_device_instance));
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
	memset (bus_instance, 0, sizeof (struct usb_bus_instance));
	bus_instance->device = device_instance;
	bus_instance->endpoint_array = endpoint_instance;
	/* XXX: what is the relevance of max_endpoints & maxpacketsize ? */
	bus_instance->max_endpoints = 1;
	bus_instance->maxpacketsize = 64;
	bus_instance->serial_number_str = serial_number;

	/* configuration instance */
	memset (config_instance, 0,
		sizeof (struct usb_configuration_instance));
	config_instance->interfaces = NUM_INTERFACES;
	config_instance->configuration_descriptor =
		(struct usb_configuration_descriptor *)&fbt_config_desc;
	config_instance->interface_instance_array = interface_instance;

	/* XXX: is alternate instance required in case of no alternate ? */
	/* interface instance */
	memset (interface_instance, 0,
		sizeof (struct usb_interface_instance));
	interface_instance->alternates = 1;
	interface_instance->alternates_instance_array = alternate_instance;

	/* alternates instance */
	memset (alternate_instance, 0,
		sizeof (struct usb_alternate_instance));
	alternate_instance->interface_descriptor = interface_descriptors;
	alternate_instance->endpoints = NUM_ENDPOINTS;
	alternate_instance->endpoints_descriptor_array = ep_descriptor_ptrs;

	/* endpoint instances */
	memset (&endpoint_instance[0], 0,
		sizeof (struct usb_endpoint_instance));
	endpoint_instance[0].endpoint_address = 0;
	endpoint_instance[0].rcv_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].rcv_attributes = USB_ENDPOINT_XFER_CONTROL;
	endpoint_instance[0].tx_packetSize = EP0_MAX_PACKET_SIZE;
	endpoint_instance[0].tx_attributes = USB_ENDPOINT_XFER_CONTROL;
	/* XXX: following statement to done along with other endpoints
		at another place ? */
	udc_setup_ep (device_instance, 0, &endpoint_instance[0]);

	for (i = 1; i <= NUM_ENDPOINTS; i++) {
		memset (&endpoint_instance[i], 0,
			sizeof (struct usb_endpoint_instance));

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

		urb_link_init (&endpoint_instance[i].rcv);
		urb_link_init (&endpoint_instance[i].rdy);
		urb_link_init (&endpoint_instance[i].tx);
		urb_link_init (&endpoint_instance[i].done);

		if (endpoint_instance[i].endpoint_address & USB_DIR_IN)
			endpoint_instance[i].tx_urb =
				usbd_alloc_urb (device_instance,
						&endpoint_instance[i]);
		else
			endpoint_instance[i].rcv_urb =
				usbd_alloc_urb (device_instance,
						&endpoint_instance[i]);
	}

	return 0;
}

/* XXX: ep_descriptor_ptrs can be removed by making better use of
	fbt_config_desc.endpoint_desc */
static int fbt_init_endpoint_ptrs(void)
{
	ep_descriptor_ptrs[0] = &fbt_config_desc.endpoint_desc[0];
	ep_descriptor_ptrs[1] = &fbt_config_desc.endpoint_desc[1];

	return 0;
}

static int fbt_init_endpoints(void)
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

		udc_setup_ep (device_instance, i, &endpoint_instance[i]);

	}

	return 0;
}

static struct urb *next_urb (struct usb_device_instance *device,
			     struct usb_endpoint_instance *endpoint)
{
	struct urb *current_urb = NULL;
	int space;

	/* If there's a queue, then we should add to the last urb */
	if (!endpoint->tx_queue) {
		current_urb = endpoint->tx_urb;
	} else {
		/* Last urb from tx chain */
		current_urb =
			p2surround (struct urb, link, endpoint->tx.prev);
	}

	/* Make sure this one has enough room */
	space = current_urb->buffer_length - current_urb->actual_length;
	if (space > 0) {
		return current_urb;
	} else {		/* No space here */
		/* First look at done list */
		current_urb = first_urb_detached (&endpoint->done);
		if (!current_urb) {
			current_urb = usbd_alloc_urb (device, endpoint);
		}

		urb_append (&endpoint->tx, current_urb);
		endpoint->tx_queue++;
	}
	return current_urb;
}

/* FASTBOOT specific */

/*
 * Android style flash utilties
 */
void fbt_reset_ptn(void)
{
	pcount = 0;
}

void fbt_add_ptn(fastboot_ptentry *ptn)
{
	if (pcount < MAX_PTN) {
	        memcpy(ptable + pcount, ptn, sizeof(*ptn));
	        pcount++;
	}
}

static fastboot_ptentry *fastboot_flash_find_ptn(const char *name)
{
	unsigned int n;

	if (pcount == 0) {
		board_fbt_load_ptbl();
	}

	for (n = 0; n < pcount; n++) {
		/* Make sure a substring is not accepted */
		if (strlen(name) == strlen(ptable[n].name)) {
			if(0 == strcmp(ptable[n].name, name))
				return ptable + n;
		}
	}
	return 0;
}

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
static fastboot_ptentry *fastboot_get_ptn(unsigned int n)
{
	if (n < pcount) {
		return ptable + n;
	} else {
	        return 0;
	}
}

static void save_env(struct fastboot_ptentry *ptn,
		     char *var, char *val)
{
	char ecc_type[5];
	char *saveenv[2] = { "setenv", NULL, };
	char *ecc[3]     = { "nandecc", "sw", NULL, };

	setenv (var, val);

	/* Some flashing requires the nand's ecc to be set */
	ecc[1] = ecc_type;
	if ((ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) &&
	    (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC))	{
		/* Both can not be true */
		FBTWARN("can not do hw and sw ecc for partition '%s'\n", ptn->name);
		FBTWARN("Ignoring these flags\n");
	} else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) {
		sprintf(ecc_type, "hw");
		do_switch_ecc(NULL, 0, 2, ecc);
	} else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC) {
		sprintf(ecc_type, "sw");
		do_switch_ecc(NULL, 0, 2, ecc);
	}
	do_env_save(NULL, 0, 1, saveenv);
}

static void save_block_values(struct fastboot_ptentry *ptn,
			      unsigned int offset,
			      unsigned int size)
{
	struct fastboot_ptentry *env_ptn;

	char var[64], val[32];
	char start[32], length[32];
	char ecc_type[5];
	char *setenv[4]  = { "setenv", NULL, NULL, NULL, };
	char *saveenv[2] = { "setenv", NULL, };
	char *ecc[3]     = { "nandecc", "sw", NULL, };

	setenv[1] = var;
	setenv[2] = val;

	FBTINFO ("saving it..\n");

	if (size == 0) {
		/* The error case, where the variables are being unset */

		sprintf (var, "%s_nand_offset", ptn->name);
		//		sprintf (val, "");
		do_env_set (NULL, 0, 3, setenv);

		sprintf (var, "%s_nand_size", ptn->name);
		//		sprintf (val, "");
		do_env_set (NULL, 0, 3, setenv);
	} else {
		/* Normal case */

		sprintf (var, "%s_nand_offset", ptn->name);
		sprintf (val, "0x%x", offset);

		FBTINFO("%s %s %s\n", setenv[0], setenv[1], setenv[2]);

		do_env_set (NULL, 0, 3, setenv);

		sprintf(var, "%s_nand_size", ptn->name);

		sprintf (val, "0x%x", size);

		FBTINFO("%s %s %s\n", setenv[0], setenv[1], setenv[2]);

		do_env_set (NULL, 0, 3, setenv);
	}


	/* Warning :
	   The environment is assumed to be in a partition named 'enviroment'.
	   It is very possible that your board stores the enviroment
	   someplace else. */
	env_ptn = fastboot_flash_find_ptn("environment");

	if (env_ptn)
	{
		/* Some flashing requires the nand's ecc to be set */
		ecc[1] = ecc_type;
		if ((env_ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) &&
		    (env_ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC))
		{
			/* Both can not be true */
			FBTWARN("can not do hw and sw ecc for partition '%s'\n", ptn->name);
			FBTWARN("Ignoring these flags\n");
		}
		else if (env_ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC)
		{
			sprintf (ecc_type, "hw");
			do_switch_ecc (NULL, 0, 2, ecc);
		}
		else if (env_ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC)
		{
			sprintf (ecc_type, "sw");
			do_switch_ecc (NULL, 0, 2, ecc);
		}

		sprintf (start, "0x%x", env_ptn->start);
		sprintf (length, "0x%x", env_ptn->length);

	}

	do_env_save (NULL, 0, 1, saveenv);
}

/* When save = 0, just parse.  The input is unchanged
   When save = 1, parse and do the save.  The input is changed */
static int parse_env(void *ptn, char *err_string, int save, int debug)
{
	int ret = 1;
	unsigned int sets = 0;
	unsigned int comment_start = 0;
	char *var = NULL;
	char *var_end = NULL;
	char *val = NULL;
	char *val_end = NULL;
	unsigned int i;

	char *buff = (char *)priv.transfer_buffer;
	unsigned int size = priv.download_bytes_unpadded;

	/* The input does not have to be null terminated.
	   This will cause a problem in the corner case
	   where the last line does not have a new line.
	   Put a null after the end of the input.

	   WARNING : Input buffer is assumed to be bigger
	   than the size of the input */
	if (save)
		buff[size] = 0;

	for (i = 0; i < size; i++) {

		if (NULL == var) {

			/*
			 * Check for comments, comment ok only on
			 * mostly empty lines
			 */
			if (buff[i] == '#')
				comment_start = 1;

			if (comment_start) {
				if  ((buff[i] == '\r') ||
				     (buff[i] == '\n')) {
					comment_start = 0;
				}
			} else {
				if (!((buff[i] == ' ') ||
				      (buff[i] == '\t') ||
				      (buff[i] == '\r') ||
				      (buff[i] == '\n'))) {
					/*
					 * Normal whitespace before the
					 * variable
					 */
					var = &buff[i];
				}
			}

		} else if (((NULL == var_end) || (NULL == val)) &&
			   ((buff[i] == '\r') || (buff[i] == '\n'))) {

			/* This is the case when a variable
			   is unset. */

			if (save) {
				/* Set the var end to null so the
				   normal string routines will work

				   WARNING : This changes the input */
				buff[i] = '\0';

				save_env(ptn, var, val);

				FBTDBG("Unsetting %s\n", var);
			}

			/* Clear the variable so state is parse is back
			   to initial. */
			var = NULL;
			var_end = NULL;
			sets++;
		} else if (NULL == var_end) {
			if ((buff[i] == ' ') ||
			    (buff[i] == '\t'))
				var_end = &buff[i];
		} else if (NULL == val) {
			if (!((buff[i] == ' ') ||
			      (buff[i] == '\t')))
				val = &buff[i];
		} else if (NULL == val_end) {
			if ((buff[i] == '\r') ||
			    (buff[i] == '\n')) {
				/* look for escaped cr or ln */
				if ('\\' == buff[i - 1]) {
					/* check for dos */
					if ((buff[i] == '\r') &&
					    (buff[i+1] == '\n'))
						buff[i + 1] = ' ';
					buff[i - 1] = buff[i] = ' ';
				} else {
					val_end = &buff[i];
				}
			}
		} else {
			sprintf(err_string, "Internal Error");

			FBTDBG("Internal error at %s %d\n",
				       __FILE__, __LINE__);
			return 1;
		}
		/* Check if a var / val pair is ready */
		if (NULL != val_end) {
			if (save) {
				/* Set the end's with nulls so
				   normal string routines will
				   work.

				   WARNING : This changes the input */
				*var_end = '\0';
				*val_end = '\0';

				save_env(ptn, var, val);

				FBTDBG("Setting %s %s\n", var, val);
			}

			/* Clear the variable so state is parse is back
			   to initial. */
			var = NULL;
			var_end = NULL;
			val = NULL;
			val_end = NULL;

			sets++;
		}
	}

	/* Corner case
	   Check for the case that no newline at end of the input */
	if ((NULL != var) &&
	    (NULL == val_end)) {
		if (save) {
			/* case of val / val pair */
			if (var_end)
				*var_end = '\0';
			/* else case handled by setting 0 past
			   the end of buffer.
			   Similar for val_end being null */
			save_env(ptn, var, val);

			if (var_end)
				FBTDBG("Trailing Setting %s %s\n", var, val);
			else
				FBTDBG("Trailing Unsetting %s\n", var);
		}
		sets++;
	}
	/* Did we set anything ? */
	if (0 == sets)
		sprintf(err_string, "No variables set");
	else
		ret = 0;

	return ret;
}

static int saveenv_to_ptn(struct fastboot_ptentry *ptn, char *err_string)
{
	int ret = 1;
	int save = 0;
	int debug = 0;

	/* err_string is only 32 bytes
	   Initialize with a generic error message. */
	sprintf(err_string, "%s", "Unknown Error");

	/* Parse the input twice.
	   Only save to the enviroment if the entire input if correct */
	save = 0;
	if (0 == parse_env(ptn, err_string, save, debug)) {
		save = 1;
		ret = parse_env(ptn, err_string, save, debug);
	}
	return ret;
}

static void set_ptn_ecc(struct fastboot_ptentry *ptn)
{
	char ecc_type[5];
	char ecc_layout[5];
	char *ecc[3] = {"nandecc", "sw", NULL, };

	/* Some flashing requires the nand's ecc to be set */
	ecc[1] = ecc_type;
	ecc[2] = ecc_layout;
	if ((ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) &&
	    (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC)) {
		/* Both can not be true */
		FBTERR("can not do hw and sw ecc for partition '%s'\n",
		       ptn->name);
		FBTERR("Ignoring these flags\n");
	} else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) {
		sprintf(ecc_type, "hw");
		if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_HW_ECC_LAYOUT_2) {
			sprintf(ecc_layout, "2");
			do_switch_ecc(NULL, 0, 3, ecc);
		} else {
			do_switch_ecc(NULL, 0, 2, ecc);
		}
	} else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC) {
		sprintf(ecc_type, "sw");
		do_switch_ecc(NULL, 0, 2, ecc);
	}
}

static int write_to_ptn(struct fastboot_ptentry *ptn)
{
	int ret = 1;
	char start[32], length[32];
	char wstart[32], wlength[32], addr[32];
	char write_type[32];
	int repeat, repeat_max;

	char *write[6]  = { "nand", "write",  NULL, NULL, NULL, NULL, };
	char *erase[5]  = { "nand", "erase",  NULL, NULL, NULL, };

	erase[2] = start;
	erase[3] = length;

	write[1] = write_type;
	write[2] = addr;
	write[3] = wstart;
	write[4] = wlength;

	FBTINFO("flashing '%s'\n", ptn->name);

	/* Which flavor of write to use */
	if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_I)
		sprintf(write_type, "write.i");
	else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_JFFS2)
		sprintf(write_type, "write.jffs2");
	else
		sprintf(write_type, "write");

	/* Some flashing requires writing the same data in multiple,
	   consecutive flash partitions */
	repeat_max = 1;
	if (FASTBOOT_PTENTRY_FLAGS_REPEAT_MASK(ptn->flags)) {
		if (ptn->flags &
		    FASTBOOT_PTENTRY_FLAGS_WRITE_CONTIGUOUS_BLOCK) {
			FBTWARN("can not do both 'contiguous block' and 'repeat' writes for for partition '%s'\n", ptn->name);
			FBTWARN("Ignoring repeat flag\n");
		} else {
			repeat_max = (FASTBOOT_PTENTRY_FLAGS_REPEAT_MASK(ptn->flags));
		}
	}

	sprintf(length, "0x%x", ptn->length);

	for (repeat = 0; repeat < repeat_max; repeat++) {

		set_ptn_ecc(ptn);

		sprintf(start, "0x%x", ptn->start + (repeat * ptn->length));

		do_nand(NULL, 0, 4, erase);

		if ((ptn->flags &
		     FASTBOOT_PTENTRY_FLAGS_WRITE_NEXT_GOOD_BLOCK) &&
		    (ptn->flags &
		     FASTBOOT_PTENTRY_FLAGS_WRITE_CONTIGUOUS_BLOCK)) {
			/* Both can not be true */
			FBTWARN("can not do 'next good block' and 'contiguous block' for partition '%s'\n", ptn->name);
			FBTWARN("Ignoring these flags\n");
		} else if (ptn->flags &
			   FASTBOOT_PTENTRY_FLAGS_WRITE_NEXT_GOOD_BLOCK) {
			/* Keep writing until you get a good block
			   transfer_buffer should already be aligned */
			if (priv.nand_block_size) {
				unsigned int blocks = priv.d_bytes /
					priv.nand_block_size;
				unsigned int i = 0;
				unsigned int offset = 0;

				sprintf(wlength, "0x%x",
					priv.nand_block_size);
				while (i < blocks) {
					/* Check for overflow */
					if (offset >= ptn->length)
						break;

					/* download's address only advance
					   if last write was successful */
					sprintf(addr, "%p",
						priv.transfer_buffer +
						(i * priv.nand_block_size));

					/* nand's address always advances */
					sprintf(wstart, "0x%x",
						ptn->start + (repeat * ptn->length) + offset);

					ret = do_nand(NULL, 0, 5, write);
					if (ret)
						break;
					else
						i++;

					/* Go to next nand block */
					offset += priv.nand_block_size;
				}
			} else {
				FBTWARN("nand block size can not be 0 when using 'next good block' for partition '%s'\n", ptn->name);
				FBTWARN("Ignoring write request\n");
			}
		} else if (ptn->flags &
			 FASTBOOT_PTENTRY_FLAGS_WRITE_CONTIGUOUS_BLOCK) {
			/* Keep writing until you get a good block
			   transfer_buffer should already be aligned */
			if (priv.nand_block_size) {
				if (0 == nand_curr_device) {
					nand_info_t *nand;
					unsigned long off;
					unsigned int ok_start;

					nand = &nand_info[nand_curr_device];

					FBTINFO("\nDevice %d bad blocks:\n",
					       nand_curr_device);

					/* Initialize the ok_start to the
					   start of the partition
					   Then try to find a block large
					   enough for the download */
					ok_start = ptn->start;

					/* It is assumed that the start and
					   length are multiples of block size */
					for (off = ptn->start;
					     off < ptn->start + ptn->length;
					     off += nand->erasesize) {
						if (nand_block_isbad(nand, off)) {
							/* Reset the ok_start
							   to the next block */
							ok_start = off +
								nand->erasesize;
						}

						/* Check if we have enough
						   blocks */
						if ((ok_start - off) >=
						    priv.d_bytes)
							break;
					}

					/* Check if there is enough space */
					if (ok_start + priv.d_bytes <=
					    ptn->start + ptn->length) {
						sprintf(addr,    "%p", priv.transfer_buffer);
						sprintf(wstart,  "0x%x", ok_start);
						sprintf(wlength, "0x%x", priv.d_bytes);

						ret = do_nand(NULL, 0, 5, write);

						/* Save the results into an
						   environment variable on the
						   format
						   ptn_name + 'offset'
						   ptn_name + 'size'  */
						if (ret) {
							/* failed */
							save_block_values(ptn, 0, 0);
						} else {
							/* success */
							save_block_values(ptn, ok_start, priv.d_bytes);
						}
					} else {
						FBTERR("could not find enough contiguous space in partition '%s' \n", ptn->name);
						FBTERR("Ignoring write request\n");
					}
				} else {
					/* TBD : Generalize flash handling */
					FBTERR("only handling 1 NAND per board");
					FBTERR("Ignoring write request\n");
				}
			} else {
				FBTWARN("nand block size can not be 0 when using 'continuous block' for partition '%s'\n", ptn->name);
				FBTWARN("Ignoring write request\n");
			}
		} else {
			/* Normal case */
			sprintf(addr,    "%p", priv.transfer_buffer);
			sprintf(wstart,  "0x%x", ptn->start +
				(repeat * ptn->length));
			sprintf(wlength, "0x%x", priv.d_bytes);
			if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_JFFS2)
				sprintf(wlength, "0x%x",
					priv.download_bytes_unpadded);

			ret = do_nand(NULL, 0, 5, write);

			if (0 == repeat) {
				if (ret) /* failed */
					save_block_values(ptn, 0, 0);
				else     /* success */
					save_block_values(ptn, ptn->start,
							  priv.d_bytes);
			}
		}


		if (ret)
			break;
	}

	return ret;
}

static int check_against_static_partition(struct fastboot_ptentry *ptn)
{
	int ret = 0;
	struct fastboot_ptentry *c;
	int i;

	for (i = 0; i < static_pcount; i++) {
		c = fastboot_get_ptn((unsigned int) i);

		if (0 == ptn->length)
			break;

		if ((ptn->start >= c->start) &&
		    (ptn->start < c->start + c->length))
			break;

		if ((ptn->start + ptn->length > c->start) &&
		    (ptn->start + ptn->length <= c->start + c->length))
			break;

		if ((0 == strcmp(ptn->name, c->name)) &&
		    (0 == strcmp(c->name, ptn->name)))
			break;
	}

	if (i >= static_pcount)
		ret = 1;
	return ret;
}

static unsigned long long memparse(char *ptr, char **retptr)
{
	char *endptr;	/* local pointer to end of parsed string */

	unsigned long ret = simple_strtoul(ptr, &endptr, 0);

	switch (*endptr) {
	case 'M':
	case 'm':
		ret <<= 10;
	case 'K':
	case 'k':
		ret <<= 10;
		endptr++;
	default:
		break;
	}

	if (retptr)
		*retptr = endptr;

	return ret;
}

static int add_partition_from_environment(char *s, char **retptr)
{
	unsigned long size;
	unsigned long offset = 0;
	char *name;
	int name_len;
	int delim;
	unsigned int flags;
	struct fastboot_ptentry part;

	size = memparse(s, &s);
	if (0 == size) {
		FBTERR("size of parition is 0\n");
		return 1;
	}

	/* fetch partition name and flags */
	flags = 0; /* this is going to be a regular partition */
	delim = 0;
	/* check for offset */
	if (*s == '@') {
		s++;
		offset = memparse(s, &s);
	} else {
		FBTERR("offset of parition is not given\n");
		return 1;
	}

	/* now look for name */
	if (*s == '(')
		delim = ')';

	if (delim) {
		char *p;

		name = ++s;
		p = strchr((const char *)name, delim);
		if (!p) {
			FBTERR("no closing %c found in partition name\n", delim);
			return 1;
		}
		name_len = p - name;
		s = p + 1;
	} else {
		FBTERR("no partition name for \'%s\'\n", s);
		return 1;
	}

	/* test for options */
	while (1) {
		if (strncmp(s, "i", 1) == 0) {
			flags |= FASTBOOT_PTENTRY_FLAGS_WRITE_I;
			s += 1;
		} else if (strncmp(s, "jffs2", 5) == 0) {
			/* yaffs */
			flags |= FASTBOOT_PTENTRY_FLAGS_WRITE_JFFS2;
			s += 5;
		} else if (strncmp(s, "swecc", 5) == 0) {
			/* swecc */
			flags |= FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC;
			s += 5;
		} else if (strncmp(s, "hwecc", 5) == 0) {
			/* hwecc */
			flags |= FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC;
			s += 5;
		} else {
			break;
		}
		if (strncmp(s, "|", 1) == 0)
			s += 1;
	}

	/* enter this partition (offset will be calculated later if it is zero at this point) */
	part.length = size;
	part.start = offset;
	part.flags = flags;

	if (name) {
		if (name_len >= sizeof(part.name)) {
			FBTERR("partition name is too long\n");
			return 1;
		}
		strncpy(&part.name[0], name, name_len);
		/* name is not null terminated */
		part.name[name_len] = '\0';
	} else {
		FBTERR("no name\n");
		return 1;
	}


	/* Check if this overlaps a static partition */
	if (check_against_static_partition(&part)) {
		FBTINFO("Adding: %s, offset 0x%8.8x, size 0x%8.8x, flags 0x%8.8x\n",
		       part.name, part.start, part.length, part.flags);
		fastboot_flash_add_ptn(&part);
	}

	/* return (updated) pointer command line string */
	*retptr = s;

	/* return partition table */
	return 0;
}

static int fbt_add_partitions_from_environment(void)
{
	char fbparts[4096], *env;

	/*
	 * Place the runtime partitions at the end of the
	 * static paritions.  First save the start off so
	 * it can be saved from run to run.
	 */
	if (static_pcount >= 0) {
		/* Reset */
		pcount = static_pcount;
	} else {
		/* Save */
		static_pcount = pcount;
	}
	env = getenv("fbparts");
	if (env) {
		unsigned int len;
		len = strlen(env);
		if (len && len < 4096) {
			char *s, *e;

			memcpy(&fbparts[0], env, len + 1);
			FBTINFO("Adding partitions from environment\n");
			s = &fbparts[0];
			e = s + len;
			while (s < e) {
				if (add_partition_from_environment(s, &s)) {
					FBTERR("Abort adding partitions\n");
					/* reset back to static */
					pcount = static_pcount;
					break;
				}
				/* Skip a bunch of delimiters */
				while (s < e) {
					if ((' ' == *s) ||
					    ('\t' == *s) ||
					    ('\n' == *s) ||
					    ('\r' == *s) ||
					    (',' == *s)) {
						s++;
					} else {
						break;
					}
				}
			}
		}
	}

	return 0;
}
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */

static void set_serial_number(void)
{
	char *dieid = getenv("dieid#");
	if (dieid == NULL) {
		priv.serial_no = "00123";
	} else {
		int len;

		memset(&serial_number[0], 0, sizeof(serial_number));
		len = strlen(dieid);
		if (len > sizeof(serial_number))
			len = sizeof(serial_number);

		strncpy(&serial_number[0], dieid, len);
		priv.serial_no = &serial_number[0];
		printf("fastboot serial_number = %s\n", serial_number);
	}
}

static int fbt_fastboot_init(void)
{
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	fbt_add_partitions_from_environment();
#endif
	priv.flag = 0;
	priv.d_size = 0;
	priv.d_bytes = 0;
	priv.u_size = 0;
	priv.u_bytes = 0;
	priv.exit = 0;

	priv.product_name = FASTBOOT_PRODUCT_NAME;
	set_serial_number();

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	priv.nand_block_size               = FASTBOOT_NAND_BLOCK_SIZE;
	priv.nand_oob_size                 = FASTBOOT_NAND_OOB_SIZE;
#endif

	priv.dev_desc = get_dev("mmc", FASTBOOT_MMC_DEVICE_ID);
	if (priv.dev_desc == NULL) {
		printf ("** mmc device %d not supported\n", FASTBOOT_MMC_DEVICE_ID);
		return 1;
	}
	priv.transfer_buffer_blocks = priv.transfer_buffer_size / priv.dev_desc->blksz;
	return 0;
}

static int fbt_handle_erase(char *cmdbuf)
{
	struct fastboot_ptentry *ptn;
	int status = 0;

	ptn = fastboot_flash_find_ptn(cmdbuf + 6);
	if (ptn == 0) {
		sprintf(priv.response, "FAILpartition does not exist");
	} else {
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
		char start[32], length[32];
		int status, repeat, repeat_max;
		char *erase[5]  = { "nand", "erase",  NULL, NULL, NULL, };

		FBTINFO("erasing '%s'\n", ptn->name);

		erase[2] = start;
		erase[3] = length;

		repeat_max = 1;
		if (FASTBOOT_PTENTRY_FLAGS_REPEAT_MASK(ptn->flags))
			repeat_max = FASTBOOT_PTENTRY_FLAGS_REPEAT_MASK(ptn->flags);

		sprintf (length, "0x%x", ptn->length);
		for (repeat = 0; repeat < repeat_max; repeat++) {
			sprintf (start, "0x%x", ptn->start + (repeat * ptn->length));
			status = do_nand (NULL, 0, 4, erase);
			if (status)
				break;
		}

		if (status) {
			sprintf(priv.response,"FAILfailed to erase partition");
		} else {
			FBTINFO("partition '%s' erased\n", ptn->name);
			sprintf(priv.response, "OKAY");
		}
#else
		/* MMC doesn't have a real erase function.
		 * Just simulate by writing all 0xffffffff to the partition.
		 */
		char *mmc_erase[4]  = {"mmc", "erase", NULL, NULL};
		char start_lba_hex[32], blk_cnt_hex[32];
		u64 blk_cnt = DIV_ROUND_UP(ptn->length, priv.dev_desc->blksz);

		mmc_erase[2] = start_lba_hex;
		mmc_erase[3] = blk_cnt_hex;

		sprintf(start_lba_hex, "%llx", ptn->start);
		sprintf(blk_cnt_hex, "%llx", blk_cnt);

		printf("Erasing partition '%s', start blk %llu, blk_cnt %llu\n",
		       ptn->name, ptn->start, blk_cnt);

		if (do_mmcops(NULL, 0, 4, mmc_erase)) {
			printf("Erasing '%s' FAILED!\n", ptn->name);
			sprintf(priv.response,"FAILfailed to erase partition");
		} else {
			FBTINFO("partition '%s' erased\n", ptn->name);
			sprintf(priv.response, "OKAY");
		}
#endif
	}
	return status;
}

#if !defined(FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING)
int mmc_get_env_addr(struct mmc *mmc, u32 *env_addr)
{
	if (mmc != find_mmc_device(FASTBOOT_MMC_DEVICE_ID)) {
		return -1;
	}

	fastboot_ptentry *ptn = fastboot_flash_find_ptn("environment");
	if (!ptn) {
		return -1;
	}

	*env_addr = ptn->start * mmc->write_bl_len;

	return 0;
}
#endif /* ! FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */

#define SPARSE_HEADER_MAJOR_VER 1

static int mmc_write(unsigned char *src, unsigned sector, unsigned len)
{
	lbaint_t blkcnt = len/priv.dev_desc->blksz;
	if (priv.dev_desc->block_write(priv.dev_desc->dev, sector, blkcnt, src) != blkcnt) {
		printf("mmc write to sector %d of %d bytes (%lu blkcnt) failed\n", sector, len, blkcnt);
		return -1;
	}
	return 0;
}

#ifdef DEBUG
static int mmc_compare(unsigned char *src, unsigned sector, unsigned len)
{
	u8 *data = malloc(priv.dev_desc->blksz);
	int result = -1;
	if (data == NULL) {
		printf("malloc failed for blksz %lu buffer\n",
		       priv.dev_desc->blksz);
		return -1;
	}

	while (len > 0) {
		if (priv.dev_desc->block_read(priv.dev_desc->dev, sector, 1, data) != 1) {
			printf("mmc read error sector %d\n", sector);
			goto out;
		}
		if (memcmp(data, src, priv.dev_desc->blksz)) {
			printf("mmc data mismatch sector %d\n", sector);
			goto out;
		}
		len -= priv.dev_desc->blksz;
		sector++;
		src += priv.dev_desc->blksz;
	}
	result = 0;
out:
	free(data);
	return result;
}
#endif

static int _unsparse(unsigned char *source, u32 sector, u32 section_size,
		     int (*func)(unsigned char *src, unsigned sector, unsigned len))
{
	sparse_header_t *header = (void*) source;
	u32 i, outlen = 0;

	if ((header->total_blks * header->blk_sz) > section_size) {
		printf("sparse: section size %d MB limit: exceeded\n",
				section_size/(1024*1024));
		return 1;
	}

	if (header->magic != SPARSE_HEADER_MAGIC) {
		printf("sparse: bad magic\n");
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
		unsigned int len = 0;
		int r;
		chunk_header_t *chunk = (void*) source;

		/* move to next chunk */
		source += sizeof(chunk_header_t);

		switch (chunk->chunk_type) {
		case CHUNK_TYPE_RAW:
			len = chunk->chunk_sz * header->blk_sz;

			if (chunk->total_sz != (len + sizeof(chunk_header_t))) {
				printf("sparse: bad chunk size for chunk %d, type Raw\n", i);
				return 1;
			}

			outlen += len;
			if (outlen > section_size) {
				printf("sparse: section size %d MB limit: exceeded\n", section_size/(1024*1024));
				return 1;
			}
#ifdef DEBUG
			printf("sparse: RAW blk=%d bsz=%d: write(sector=%d,len=%d)\n",
			       chunk->chunk_sz, header->blk_sz, sector, len);
#endif
			r = func(source, sector, len);
			if (r < 0) {
				printf("sparse: mmc func failed\n");
				return 1;
			}

			sector += (len / 512);
			source += len;
			break;

		case CHUNK_TYPE_DONT_CARE:
			if (chunk->total_sz != sizeof(chunk_header_t)) {
				printf("sparse: bogus DONT CARE chunk\n");
				return 1;
			}
			len = chunk->chunk_sz * header->blk_sz;
#ifdef DEBUG
			printf("sparse: DONT_CARE blk=%d bsz=%d: skip(sector=%d,len=%d)\n",
			       chunk->chunk_sz, header->blk_sz, sector, len);
#endif

			outlen += len;
			if (outlen > section_size) {
				printf("sparse: section size %d MB limit: exceeded\n", section_size/(1024*1024));
				return 1;
			}
			sector += (len / 512);
			break;

		default:
			printf("sparse: unknown chunk ID %04x\n", chunk->chunk_type);
			return 1;
		}
	}

	printf("\nsparse: out-length-0x%d MB\n", outlen/(1024*1024));
	return 0;
}

static u8 do_unsparse(unsigned char *source, u32 sector, u32 section_size)
{
	if (_unsparse(source, sector, section_size, mmc_write))
		return 1;
#ifdef DEBUG
	printf("sparse: compare mmc slot[%d] @ sector %d\n",
	       FASTBOOT_MMC_DEVICE_ID, sector);
	if (_unsparse(source, sector, section_size, mmc_compare))
		return 1;
#endif
	return 0;
}

static void fbt_handle_flash(char *cmdbuf)
{
	struct fastboot_ptentry *ptn;

	if (!priv.d_bytes) {
		sprintf(priv.response, "FAILno image downloaded");
		return;
	}

	ptn = fastboot_flash_find_ptn(cmdbuf + 6);
	if (ptn == 0) {
		sprintf(priv.response, "FAILpartition does not exist");
		return;
	}
	if ((priv.d_bytes > ptn->length) &&
		!(ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_ENV)) {
		sprintf(priv.response, "FAILimage too large for partition");
		return;
	}
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	/* Check if this is not really a flash write but rather a saveenv */
	if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_ENV) {
		/* Since the response can only be 64 bytes,
		   there is no point in having a large error message. */
		char err_string[32];

		if (saveenv_to_ptn(ptn, &err_string[0])) {
			FBTINFO("savenv '%s' failed : %s\n", ptn->name, err_string);
			sprintf(priv.response, "FAIL%s", err_string);
		} else {
			FBTINFO("partition '%s' saveenv-ed\n", ptn->name);
			sprintf(priv.response, "OKAY");
		}
	} else {
		/* Normal case */
		if (write_to_ptn(ptn)) {
			FBTINFO("flashing '%s' failed\n", ptn->name);
			sprintf(priv.response, "FAILfailed to flash partition");
		} else {
			FBTINFO("partition '%s' flashed\n", ptn->name);
			sprintf(priv.response, "OKAY");
		}
	}
#else
	/* Check if this is not really a flash write but rather a saveenv */
	if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_ENV) {
		if (!himport_r(&env_htab,
			       (const char*)priv.transfer_buffer,
			       priv.d_bytes, '\n', H_NOCLEAR)){
			FBTINFO("Import '%s' FAILED!\n", ptn->name);
			sprintf(priv.response, "FAIL: Import environment");
			return;
		}

#if defined(CONFIG_CMD_SAVEENV)
		if (do_env_save(NULL, 0, 0, NULL)) {
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
		if ( ((sparse_header_t *)priv.transfer_buffer)->magic
		     == SPARSE_HEADER_MAGIC) {
			printf("fastboot: %s is in sparse format\n", ptn->name);
			if (!do_unsparse(priv.transfer_buffer,
					 ptn->start,
					 ptn->length)) {
				printf("Writing sparsed: '%s' DONE!\n", ptn->name);
				sprintf(priv.response, "OKAY");
			} else {
				printf("Writing sparsed '%s' FAILED!\n", ptn->name);
				sprintf(priv.response, "FAIL: Sparsed Write");
			}
		} else {
			/* Normal image: no sparse */
			char *mmc_write[5]  = {"mmc", "write", NULL, NULL, NULL};
			char source[32], dest[32], blk_cnt[32];

			mmc_write[2] = source;
			mmc_write[3] = dest;
			mmc_write[4] = blk_cnt;

			sprintf(source, "%p", priv.transfer_buffer);
			sprintf(dest, "0x%llx", ptn->start);
			sprintf(blk_cnt, "0x%llx", DIV_ROUND_UP(priv.d_bytes, priv.dev_desc->blksz));

			printf("Writing '%s', %s blks (%llu bytes) at sector %s\n", ptn->name, blk_cnt, priv.d_bytes, dest);
			if (do_mmcops(NULL, 0, 5, mmc_write)) {
				printf("Writing '%s' FAILED!\n", ptn->name);
				sprintf(priv.response, "FAIL: Write partition");
			} else {
				printf("Writing '%s' DONE!\n", ptn->name);
				sprintf(priv.response, "OKAY");
			}
		}
	} /* Normal Case */
#endif
}

static int fbt_handle_getvar(char *cmdbuf)
{
	strcpy(priv.response, "OKAY");
	if(!strcmp(cmdbuf + strlen("getvar:"), "version")) {
		FBTDBG("getvar version\n");
		strcpy(priv.response + 4, FASTBOOT_VERSION);
	} else if(!strcmp(cmdbuf + strlen("getvar:"), "version-bootloader")) {
		strncpy(priv.response + 4, version_string,
			min(strlen(version_string), GETVARLEN));
	} else if(!strcmp(cmdbuf + strlen("getvar:"), "secure")) {
		strcpy(priv.response + 4, SECURE);
	} else if(!strcmp(cmdbuf + strlen("getvar:"), "product")) {
		if (priv.product_name)
			strcpy(priv.response + 4, priv.product_name);
	} else if(!strcmp(cmdbuf + strlen("getvar:"), "serialno")) {
		if (priv.serial_no)
			strcpy(priv.response + 4, priv.serial_no);
	}
	return 0;
}


static int fbt_handle_reboot(const char *cmdbuf)
{
	if (!strcmp(&cmdbuf[6], "-bootloader")) {
		FBTDBG("%s\n", cmdbuf);
		board_fbt_set_reboot_type(FASTBOOT_REBOOT_BOOTLOADER);
	}
	if (!strcmp(&cmdbuf[6], "-recovery")) {
		FBTDBG("%s\n", cmdbuf);
		board_fbt_set_reboot_type(FASTBOOT_REBOOT_RECOVERY);
	}

	strcpy(priv.response,"OKAY");
	priv.flag |= FASTBOOT_FLAG_RESPONSE;
	fbt_handle_response();
	udelay (1000000); /* 1 sec */

	do_reset (NULL, 0, 0, NULL);

	return 0;
}

static int fbt_handle_oem(const char *cmdbuf)
{
	cmdbuf += 4;

	/* %fastboot oem format */
	if (strcmp(cmdbuf, "format") == 0){
		FBTDBG("oem format\n");
		if (board_fbt_oem(cmdbuf) >= 0) {
			strcpy(priv.response,"OKAY");
		}
		return 0;
	}

	/* %fastboot oem recovery */
	if (strcmp(cmdbuf, "recovery") == 0){
		FBTDBG("oem recovery\n");
		return fbt_handle_reboot("reboot-recovery");
	}

	/* %fastboot oem unlock */
	if (strcmp(cmdbuf, "unlock") == 0){
		FBTDBG("oem unlock\n");
		printf("\nfastboot: oem unlock not implemented yet!!\n");
		return 0;
	}

	/* %fastboot oem [xxx] */
	FBTDBG("oem %s\n", cmdbuf);
	printf("\nfastboot: unsupported oem command %s\n", cmdbuf);
	return 0;
}

static int fbt_handle_boot(const char *cmdbuf)
{
	if ((priv.d_bytes) &&
		(CONFIG_FASTBOOT_MKBOOTIMAGE_PAGE_SIZE < priv.d_bytes)) {
		char start[32];
		char *booti[3] = { "booti", NULL, NULL, };
		char *go[3]    = { "go",    NULL, NULL, };

		/*
		 * Use this later to determine if a command line was passed
		 * for the kernel.
		 */
		struct fastboot_boot_img_hdr *fb_hdr =
			(struct fastboot_boot_img_hdr *) priv.transfer_buffer;


		booti[1] = go[1] = start;
		sprintf (start, "%p", fb_hdr);

		/* Execution should jump to kernel so send the response
		   now and wait a bit.  */
		sprintf(priv.response, "OKAY");
		priv.flag |= FASTBOOT_FLAG_RESPONSE;
		fbt_handle_response();
		udelay (1000000); /* 1 sec */

		do_booti(NULL, 0, 2, booti);

		printf("do_booti() returned, trying go..\n");

		FBTINFO("Booting raw image..\n");
		do_go (NULL, 0, 2, go);

		FBTERR("booting failed, reset the board\n");
	}
	sprintf(priv.response, "FAILinvalid boot image");

	return 0;
}

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#ifdef	FASTBOOT_UPLOAD
static int fbt_handle_upload(char *cmdbuf)
{
	unsigned int adv, delim_index, len;
	struct fastboot_ptentry *ptn;
	unsigned int is_raw = 0;

	/* Is this a raw read ? */
	if (memcmp(cmdbuf, "uploadraw:", 10) == 0) {
		is_raw = 1;
		adv = 10;
	} else {
		adv = 7;
	}

	/* Scan to the next ':' to find when the size starts */
	len = strlen(cmdbuf);
	for (delim_index = adv;	delim_index < len; delim_index++) {
		if (cmdbuf[delim_index] == ':') {
			/* WARNING, cmdbuf is being modified. */
			*((char *) &cmdbuf[delim_index]) = 0;
			break;
		}
	}

	ptn = fastboot_flash_find_ptn(cmdbuf + adv);
	if (ptn == 0) {
		sprintf(priv.response, "FAILpartition does not exist");
	} else {
		/* This is how much the user is expecting */
		unsigned int user_size;
		/*
		 * This is the maximum size needed for
		 * this partition
		 */
		unsigned int size;
		/* This is the length of the data */
		unsigned int length;
		/*
		 * Used to check previous write of
		 * the parition
		 */
		char env_ptn_length_var[128];
		char *env_ptn_length_val;

		user_size = 0;
		if (delim_index < len)
			user_size = simple_strtoul(cmdbuf + delim_index +
				1, NULL, 16);
		/* Make sure output is padded to block size */
		length = ptn->length;
		sprintf(env_ptn_length_var, "%s_nand_size", ptn->name);
		env_ptn_length_val = getenv(env_ptn_length_var);
		if (env_ptn_length_val) {
			length = simple_strtoul(env_ptn_length_val, NULL, 16);
			/* Catch possible problems */
			if (!length)
				length = ptn->length;
		}
		size = length / priv.nand_block_size;
		size *= priv.nand_block_size;
		if (length % priv.nand_block_size)
			size += priv.nand_block_size;
		if (is_raw)
			size += (size / priv.nand_block_size) *
				priv.nand_oob_size;
		if (size > priv.transfer_buffer_size) {
			sprintf(priv.response, "FAILdata too large");
		} else if (user_size == 0) {
			/* Send the data response */
			sprintf(priv.response, "DATA%08x", size);
		} else if (user_size != size) {
			/* This is the wrong size */
			sprintf(priv.response, "FAIL");
		} else {
			/*
			 * This is where the transfer
			 * buffer is populated
			 */
			unsigned char *buf = priv.transfer_buffer;
			char start[32], length[32], type[32], addr[32];
			char *read[6] = { "nand", NULL, NULL,
				NULL, NULL, NULL, };
			/*
			 * Setting upload_size causes
			 * transfer to happen in main loop
			 */
			priv.u_size = size;
			priv.u_bytes = 0;

			/*
			 * Poison the transfer buffer, 0xff
			 * is erase value of nand
			 */
			memset(buf, 0xff, priv.u_size);
			/* Which flavor of read to use */
			if (is_raw)
				sprintf(type, "read.raw");
			else
				sprintf(type, "read.i");

			sprintf(addr, "0x%x", priv.transfer_buffer);
			sprintf(start, "0x%x", ptn->start);
			sprintf(length, "0x%x", priv.u_size);

			read[1] = type;
			read[2] = addr;
			read[3] = start;
			read[4] = length;

			set_ptn_ecc(ptn);

			do_nand(NULL, 0, 5, read);

			/* Send the data response */
			sprintf(priv.response, "DATA%08x", size);
		}
	}

	return 0;
}
#endif /* FASTBOOT_UPLOAD */
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */

/* XXX: Replace magic number & strings with macros */
static int fbt_rx_process(unsigned char *buffer, int length)
{
	/* Generic failed response */
	strcpy(priv.response, "FAIL");

	if (!priv.d_size) {
		/* command */
		char *cmdbuf = (char *) buffer;

		FBTDBG("command\n");

		printf("cmdbuf = (%s)\n", cmdbuf);

		/* %fastboot getvar: <var_name> */
		if (memcmp(cmdbuf, "getvar:", 7) == 0) {
			FBTDBG("getvar\n");
			fbt_handle_getvar(cmdbuf);
		}

		/* %fastboot oem <cmd> */
		if (memcmp(cmdbuf, "oem ", 4) == 0) {
			FBTDBG("oem\n");
			fbt_handle_oem(cmdbuf);
		}

		/* %fastboot erase <partition_name> */
		if (memcmp(cmdbuf, "erase:", 6) == 0) {
			FBTDBG("erase\n");
			fbt_handle_erase(cmdbuf);
		}

		/* %fastboot flash:<partition_name> */
		if (memcmp(cmdbuf, "flash:", 6) == 0) {
			FBTDBG("flash\n");
			fbt_handle_flash(cmdbuf);
		}

		/* %fastboot reboot
		 * %fastboot reboot-bootloader
		 */
		if (memcmp(cmdbuf, "reboot", 6) == 0) {
			FBTDBG("reboot or reboot-bootloader\n");
			return fbt_handle_reboot(cmdbuf);
		}

		/* %fastboot continue */
		if (strcmp(cmdbuf, "continue") == 0) {
			FBTDBG("continue\n");
			strcpy(priv.response,"OKAY");
			priv.exit = 1;
		}

		/* %fastboot boot <kernel> [ <ramdisk> ] */
		if (memcmp(cmdbuf, "boot", 4) == 0) {
			FBTDBG("boot\n");
			fbt_handle_boot(cmdbuf);
		}

		/* Sent as part of a '%fastboot flash <partname>' command
		 * This sends the data over with byte count:
		 * %download:<num_bytes>
		 */
		if (memcmp(cmdbuf, "download:", 9) == 0) {
			FBTDBG("download\n");

			/* XXX: need any check for size & bytes ? */
			priv.d_size =
				simple_strtoul (cmdbuf + 9, NULL, 16);
			priv.d_bytes = 0;

			FBTINFO ("starting download of %d bytes\n",
				priv.d_size);

			if (priv.d_size == 0) {
				strcpy(priv.response, "FAILdata invalid size");
			} else if (priv.d_size >
					priv.transfer_buffer_size) {
				priv.d_size = 0;
				strcpy(priv.response, "FAILdata too large");
			} else {
				sprintf(priv.response, "DATA%08llx", priv.d_size);
			}
		}

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#ifdef	FASTBOOT_UPLOAD
		if ((memcmp(cmdbuf, "upload:", 7) == 0) ||
		    (memcmp(cmdbuf, "uploadraw", 10) == 0)) {
			FBTDBG("upload/uploadraw\n");
			fbt_handle_upload(cmdbuf);
		}
#endif
#endif
		priv.flag |= FASTBOOT_FLAG_RESPONSE;
	} else {
		if (length) {
			unsigned int xfr_size;

			xfr_size = priv.d_size - priv.d_bytes;
			if (xfr_size > length)
				xfr_size = length;
			memcpy(priv.transfer_buffer + priv.d_bytes,
				buffer, xfr_size);
			priv.d_bytes += xfr_size;

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#ifdef	INFO
			/* Inform via prompt that download is happening */
			if (! (priv.d_bytes % (16 * priv.nand_block_size)))
				printf(".");
			if (! (priv.d_bytes % (80 * 16 * priv.nand_block_size)))
				printf("\n");
#endif
#endif
			if (priv.d_bytes >= priv.d_size) {
				priv.d_size = 0;
				strcpy(priv.response, "OKAY");
				priv.flag |= FASTBOOT_FLAG_RESPONSE;
#ifdef	INFO
				printf(".\n");
#endif

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
				priv.download_bytes_unpadded = priv.d_size;
				/* XXX: Revisit padding handling */
				if (priv.nand_block_size) {
					if (priv.d_bytes % priv.nand_block_size) {
						unsigned int pad = priv.nand_block_size - (priv.d_bytes % priv.nand_block_size);
						unsigned int i;

						for (i = 0; i < pad; i++) {
							if (priv.d_bytes >= priv.transfer_buffer_size)
								break;

							priv.transfer_buffer[priv.d_bytes] = 0;
							priv.d_bytes++;
						}
					}
				}
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */
				FBTINFO("downloaded %d bytes\n", priv.d_bytes);
			}
		} else
			FBTWARN("empty buffer download\n");
	}

	return 0;
}

static int fbt_handle_rx(void)
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
		    the time of creation of urb it is poisoned 	*/
		memset(ep->rcv_urb->buffer, 0, ep->rcv_urb->actual_length);
		ep->rcv_urb->actual_length = 0;
	}

	return 0;
}

static int fbt_response_process(void)
{
	struct usb_endpoint_instance *ep = &endpoint_instance[TX_EP_INDEX];
	struct urb *current_urb = NULL;
	unsigned char *dest = NULL;
	int n, ret = 0;

	current_urb = next_urb (device_instance, ep);
	if (!current_urb) {
		FBTERR("%s: current_urb NULL", __func__);
		return -1;
	}

	dest = current_urb->buffer + current_urb->actual_length;
	n = MIN (64, strlen(priv.response));
	memcpy(dest, priv.response, n);
	current_urb->actual_length += n;
	FBTDBG("response urb length: %u\n", current_urb->actual_length);
	if (ep->last == 0) {
		ret = udc_endpoint_write (ep);
		return ret;
	}

	return ret;
}

static int fbt_handle_response(void)
{
	if (priv.flag & FASTBOOT_FLAG_RESPONSE) {
		fbt_response_process();
		priv.flag &= ~FASTBOOT_FLAG_RESPONSE;
	}

	return 0;
}

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#ifdef	FASTBOOT_UPLOAD
static int fbt_tx_process(void)
{
	struct usb_endpoint_instance *ep = &endpoint_instance[TX_EP_INDEX];
	struct urb *current_urb = NULL;
	unsigned char *dest = NULL;
	int n = 0, ret = 0;

	current_urb = next_urb (device_instance, ep);
	if (!current_urb) {
		FBTERR("%s: current_urb NULL", __func__);
		return -1;
	}

	dest = current_urb->buffer + current_urb->actual_length;
	n = MIN (64, priv.u_size - priv.u_bytes);
	memcpy(dest, priv.transfer_buffer + priv.u_bytes, n);
	current_urb->actual_length += n;
	if (ep->last == 0) {
		ret = udc_endpoint_write (ep);
		/* XXX: "ret = n" should be done iff n bytes has been
		 * transmitted, "udc_endpoint_write" to be changed for it,
		 * now it always return 0.
		 */
		return n;
	}

	return ret;
}

static int fbt_handle_tx(void)
{
	if (priv.u_size) {
		int bytes_written = fbt_tx_process();

		if (bytes_written > 0) {
			/* XXX: is this the right way to update priv.u_bytes ?,
			 * may be "udc_endpoint_write()" can be modified to
			 * return number of bytes transmitted or error and
			 * update based on hence obtained value
			 */
			priv.u_bytes += bytes_written;
#ifdef	INFO
			/* Inform via prompt that upload is happening */
			if (! (priv.d_bytes % (16 * priv.nand_block_size)))
				printf(".");
			if (! (priv.d_bytes % (80 * 16 * priv.nand_block_size)))
				printf("\n");
#endif
			if (priv.u_bytes >= priv.u_size)
#ifdef	INFO
				printf(".\n");
#endif
				priv.u_size = priv.u_bytes = 0;
				FBTINFO("data upload finished\n");
		} else {
			FBTERR("bytes_written: %d\n", bytes_written);
			return -1;
		}

	}

	return 0;
}
#endif /* FASTBOOT_UPLOAD */
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */

/* command */
int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = -1;

	ret = fbt_fastboot_init();

	ret = fbt_init_endpoint_ptrs();

	if ((ret = udc_init()) < 0) {
		FBTERR("%s: MUSB UDC init failure\n", __func__);
		return ret;
	}

	ret = fbt_init_strings();
	ret = fbt_init_instances();

	udc_startup_events (device_instance);
	udc_connect();

	FBTINFO("fastboot initialized\n");

	while(1) {
		udc_irq();
		if (priv.configured) {
			fbt_handle_rx();
			fbt_handle_response();
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
#ifdef	FASTBOOT_UPLOAD
			fbt_handle_tx();
#endif
#endif
		}
		priv.exit |= ctrlc();
		if (priv.exit) {
			FBTINFO("fastboot end\n");
			break;
		}
	}

	return ret;
}

U_BOOT_CMD(fastboot, 2,	1, do_fastboot,
	"fastboot- use USB Fastboot protocol", NULL);

  /* Section for Android bootimage format support
   * Refer:
   * http://android.git.kernel.org/?p=platform/system/core.git;a=blob;f=mkbootimg/bootimg.h
   */
void
bootimg_print_image_hdr (struct fastboot_boot_img_hdr *hdr)
{
#ifdef DEBUG
	int i;
	printf ("   Image magic:   %s\n", hdr->magic);

	printf ("   kernel_size:   0x%x\n", hdr->kernel_size);
	printf ("   kernel_addr:   0x%x\n", hdr->kernel_addr);

	printf ("   rdisk_size:   0x%x\n", hdr->ramdisk_size);
	printf ("   rdisk_addr:   0x%x\n", hdr->ramdisk_addr);

	printf ("   second_size:   0x%x\n", hdr->second_size);
	printf ("   second_addr:   0x%x\n", hdr->second_addr);

	printf ("   tags_addr:   0x%x\n", hdr->tags_addr);
	printf ("   page_size:   0x%x\n", hdr->page_size);

	printf ("   name:      %s\n", hdr->name);
	printf ("   cmdline:   %s\n", hdr->cmdline);

	for (i=0;i<8;i++)
		printf ("   id[%d]:   0x%x\n", i, hdr->id[i]);
#endif
}

/* booti [<addr> | mmc0 | mmc1] [ <partition> ] */
int do_booti (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned addr;
	char *ptn = "boot";
	int mmcc = -1;
	struct fastboot_boot_img_hdr *hdr = NULL;
	bootm_headers_t images;

	if (argc < 2)
		return -1;

	if (!strcmp(argv[1],"mmc0")) {
		mmcc = 0;
	} else if (!strcmp(argv[1],"mmc1")) {
		mmcc = 1;
	} else {
		addr = simple_strtoul(argv[1], NULL, 16);
	}

	if (argc > 2)
		ptn = argv[2];

	if (mmcc != -1) {
#if (CONFIG_MMC)
		struct fastboot_ptentry *pte;
		unsigned sector;
		unsigned blocks;
		struct mmc *mmc = find_mmc_device(mmcc);
		if (mmc == NULL) {
			printf("error finding mmc device %d\n", mmcc);
			return -1;
		}
		pte = fastboot_flash_find_ptn(ptn);
		if (!pte) {
			printf("booti: cannot find '%s' partition\n", ptn);
			goto fail;
		}
		if (mmc_init(mmc)) {
			printf("mmc%d init failed\n", mmcc);
			goto fail;
		}
		hdr = malloc(mmc->block_dev.blksz);
		if (hdr == NULL) {
			printf("error allocating blksz(%lu) buffer\n", mmc->block_dev.blksz);
			goto fail;
		}
		if (mmc->block_dev.block_read(mmcc, pte->start, 1, (void*) hdr) != 1) {
			printf("booti: mmc failed to read bootimg header\n");
			goto fail;
		}
		if (memcmp(hdr->magic, FASTBOOT_BOOT_MAGIC, FASTBOOT_BOOT_MAGIC_SIZE)) {
			printf("booti: bad boot image magic\n");
			goto fail;
		}

		sector = pte->start + (hdr->page_size / 512);
		blocks = DIV_ROUND_UP(hdr->kernel_size, mmc->block_dev.blksz);
		if (mmc->block_dev.block_read(mmcc, sector, blocks,
					      (void*) hdr->kernel_addr) != blocks) {
			printf("booti: failed to read kernel\n");
			goto fail;
		}

		sector += ALIGN(hdr->kernel_size, hdr->page_size) / 512;
		blocks = DIV_ROUND_UP(hdr->ramdisk_size, mmc->block_dev.blksz);
		if (mmc->block_dev.block_read(mmcc, sector, blocks,
					      (void*) hdr->ramdisk_addr) != blocks) {
			printf("booti: failed to read ramdisk\n");
			goto fail;
		}
#else
		printf("booti: mmc support not enabled\n");
		return -1;
#endif
	} else {
		void *kaddr, *raddr;

		hdr = malloc(sizeof(*hdr));
		if (hdr == NULL) {
			printf("error allocating buffer\n");
			return -1;
		}

		/* set this aside somewhere safe */
		memcpy(hdr, (void*) addr, sizeof(*hdr));

		if (memcmp(hdr->magic, FASTBOOT_BOOT_MAGIC, FASTBOOT_BOOT_MAGIC_SIZE)) {
			printf("booti: bad boot image magic\n");
			return 1;
		}

		bootimg_print_image_hdr(hdr);

		kaddr = (void*)(addr + hdr->page_size);
		raddr = (void*)(kaddr + ALIGN(hdr->kernel_size, hdr->page_size));

		memmove((void*) hdr->kernel_addr, kaddr, hdr->kernel_size);
		memmove((void*) hdr->ramdisk_addr, raddr, hdr->ramdisk_size);
	}

	printf("kernel   @ %08x (%d)\n", hdr->kernel_addr, hdr->kernel_size);
	printf("ramdisk  @ %08x (%d)\n", hdr->ramdisk_addr, hdr->ramdisk_size);

#ifdef CONFIG_CMDLINE_TAG
	if (priv.serial_no == NULL) {
		set_serial_number();
	}
	{
		/* Use the command line in the bootimg header instead of
		 * any hardcoded into u-boot.  Also, Android wants the
		 * serial number on the command line instead of via
		 * tags so append the serial number to the bootimg header
		 * value and set the bootargs environment variable.
		 * do_bootm_linux() will use the bootargs environment variable
		 * to pass it to the kernel.
		 */
		char command_line[255];
		sprintf(command_line, "%s androidboot.serialno=%s",
			hdr->cmdline, priv.serial_no);
		setenv("bootargs", command_line);
	}
#endif

	memset(&images, 0, sizeof(images));
	images.ep = hdr->kernel_addr;
	images.rd_start = hdr->ramdisk_addr;
	images.rd_end = hdr->ramdisk_addr + hdr->ramdisk_size;
	free(hdr);
	do_bootm_linux(0, 0, NULL, &images);

	puts ("booti: Control returned to monitor - resetting...\n");
	do_reset (cmdtp, flag, argc, argv);
	return 1;

fail:
	/* if booti fails, always start fastboot */
	free(hdr);
	return do_fastboot(NULL, 0, 0, NULL);
}

U_BOOT_CMD(
	booti,	3,	1,	do_booti,
	"booti   - boot android bootimg",
	"<addr>\n    - boot application image\n"
	"\t'addr' should be the address of boot image which is zImage+ramdisk.img if in memory or the mmc0 or mmc1 to load from 'boot' partition in mmc"
);

/*
 * Determine if we should
 * enter fastboot mode based on board specific key press or
 * parameter left in memory from previous boot.
 */
void fbt_preboot(void)
{
	enum fbt_reboot_type frt;

	if (board_fbt_key_pressed()) {
		setenv ("preboot", "fastboot");
		return;
	}

	frt = board_fbt_get_reboot_type();
	if (frt == FASTBOOT_REBOOT_RECOVERY) {
		extern int do_booti (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
		char *boot_cmd[3] = {"booti", NULL, "recovery"};
		char boot_device[64];

		printf("\n%s: starting recovery img because of reboot flag\n", __func__);

		/* pass: booti mmci<N> recovery */
		sprintf(boot_device, "mmc%d", FASTBOOT_MMC_DEVICE_ID);
		boot_cmd[1] = boot_device;

		do_booti(NULL, 0, 3, boot_cmd);

		/* returns if recovery.img is bad
		 * Default to normal boot
		 */

		printf("\nfastboot: Error: Invalid recovery img\n");
		return;
	} else if (frt == FASTBOOT_REBOOT_BOOTLOADER) {

		/* Case: %fastboot reboot-bootloader
		 * Case: %adb reboot bootloader
		 * Case: %adb reboot-bootloader
		 */
		printf("\n%s: starting fastboot because of reboot flag\n", __func__);
		setenv ("preboot", "fastboot");
	} else {
		printf("\n%s: no special reboot flags, doing normal boot\n", __func__);
	}
}

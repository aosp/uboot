/*
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

#include <common.h>
#include <command.h>
#include <fastboot.h>

#define	ERR
#define	WARN
#define	INFO
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
#include <environment.h>
#endif

/* USB specific */

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

#define	CONFIG_USBD_FASTBOOT_BULK_PKTSIZE	64

struct _fbt_config_desc {
	struct usb_configuration_descriptor configuration_desc;
	struct usb_interface_descriptor interface_desc;
	struct usb_endpoint_descriptor endpoint_desc[NUM_ENDPOINTS];
};

/* defined and used by gadget/ep0.c */
extern struct usb_string_descriptor **usb_strings;

/* USB Descriptor Strings */
static char serial_number[16] = "TEMP";

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
	.bDeviceClass =		0xFF,
	.bDeviceSubClass =	0x00,
	.bDeviceProtocol =	0x00,
	.bMaxPacketSize0 =	EP0_MAX_PACKET_SIZE,
	.idVendor =		cpu_to_le16(CONFIG_USBD_VENDORID),
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
			.bEndpointAddress = 0x1 | USB_DIR_OUT,
			.bmAttributes =	USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize	=
				cpu_to_le16(CONFIG_USBD_FASTBOOT_BULK_PKTSIZE),
			.bInterval = 0xFF,
		},
		{
			.bLength = sizeof(struct usb_endpoint_descriptor),
			.bDescriptorType = USB_DT_ENDPOINT,
			/* XXX: can't the address start from 0x1, currently
				seeing problem with "epinfo" */
			.bEndpointAddress = 0x2 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize	=
				cpu_to_le16(CONFIG_USBD_FASTBOOT_BULK_PKTSIZE),
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

static struct cmd_fastboot_interface priv =
{
        .transfer_buffer       = CONFIG_FASTBOOT_TRANSFER_BUFFER,
        .transfer_buffer_size  = CONFIG_FASTBOOT_TRANSFER_BUFFER_SIZE,
};

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING

/* Use do_setenv and do_saveenv to permenantly save data */
extern int do_saveenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_setenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_switch_ecc(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[]);
extern int do_nand(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[]);
extern fastboot_ptentry ptn[];

/* To support the Android-style naming of flash */
#define MAX_PTN 16
static fastboot_ptentry ptable[MAX_PTN];
static unsigned int pcount;
static int static_pcount = -1;
#endif

/* USB specific */

/* utility function for converting char* to wide string used by USB */
static void str2wide (char *str, u16 * wide)
{
	int i;
	FBTDBG();
	for (i = 0; i < strlen (str) && str[i]; i++){
		#if defined(__LITTLE_ENDIAN)
			wide[i] = (u16) str[i];
		#elif defined(__BIG_ENDIAN)
			wide[i] = ((u16)(str[i])<<8);
		#else
			#error "__LITTLE_ENDIAN or __BIG_ENDIAN undefined"
		#endif
	}
	FBTDBG();
}

static int fbt_init_strings(void)
{
	struct usb_string_descriptor *string;

	FBTDBG();
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
	string->bLength = sizeof(serial_number);
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
	FBTDBG();

	/* Now, initialize the string table for ep0 handling */
	usb_strings = fbt_string_table;

	return 0;
}

static int fbt_init_instances(void)
{
	int i;

	FBTDBG();
	/* initialize device instance */
	memset (device_instance, 0, sizeof (struct usb_device_instance));
	device_instance->device_state = STATE_INIT;
	device_instance->device_descriptor = &device_descriptor;
	device_instance->event = NULL;
	device_instance->cdc_recv_setup = NULL;
	device_instance->bus = bus_instance;
	device_instance->configurations = NUM_CONFIGS;
	device_instance->configuration_instance_array = config_instance;

	FBTDBG();
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

	FBTDBG();
	/* configuration instance */
	memset (config_instance, 0,
		sizeof (struct usb_configuration_instance));
	config_instance->interfaces = NUM_INTERFACES;
	config_instance->configuration_descriptor =
		(struct usb_configuration_descriptor *)&fbt_config_desc;
	config_instance->interface_instance_array = interface_instance;

	FBTDBG();
	/* XXX: is alternate instance required in case of no alternate ? */
	/* interface instance */
	memset (interface_instance, 0,
		sizeof (struct usb_interface_instance));
	interface_instance->alternates = 1;
	interface_instance->alternates_instance_array = alternate_instance;

	FBTDBG();
	/* alternates instance */
	memset (alternate_instance, 0,
		sizeof (struct usb_alternate_instance));
	alternate_instance->interface_descriptor = interface_descriptors;
	alternate_instance->endpoints = NUM_ENDPOINTS;
	alternate_instance->endpoints_descriptor_array = ep_descriptor_ptrs;

	FBTDBG();
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

	FBTDBG();
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
	FBTDBG();

	return 0;
}

/* XXX: ep_descriptor_ptrs can be removed by making better use of
	fbt_config_desc.endpoint_desc */
static int fbt_init_endpoint_ptrs(void)
{
	FBTDBG();
	ep_descriptor_ptrs[0] = &fbt_config_desc.endpoint_desc[0];
	ep_descriptor_ptrs[1] = &fbt_config_desc.endpoint_desc[1];

	return 0;
}

static int fbt_init_endpoints (void)
{
	int i;

	FBTDBG();
	bus_instance->max_endpoints = NUM_ENDPOINTS + 1;
	/* XXX: is this for loop required ? */
	for (i = 1; i <= NUM_ENDPOINTS; i++) {
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

/* FASBOOT specific */

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
/*
 * Android style flash utilties */
void fastboot_flash_add_ptn(fastboot_ptentry *ptn)
{
    if(pcount < MAX_PTN){
        memcpy(ptable + pcount, ptn, sizeof(*ptn));
        pcount++;
    }
}

void fastboot_flash_dump_ptn(void)
{
    unsigned int n;
    for(n = 0; n < pcount; n++) {
        fastboot_ptentry *ptn = ptable + n;
        FBTINFO("ptn %d name='%s' start=%d len=%d\n",
                n, ptn->name, ptn->start, ptn->length);
    }
}


fastboot_ptentry *fastboot_flash_find_ptn(const char *name)
{
    unsigned int n;

    for(n = 0; n < pcount; n++) {
	    /* Make sure a substring is not accepted */
	    if (strlen(name) == strlen(ptable[n].name))
	    {
		    if(0 == strcmp(ptable[n].name, name))
			    return ptable + n;
	    }
    }
    return 0;
}

fastboot_ptentry *fastboot_flash_get_ptn(unsigned int n)
{
    if(n < pcount) {
        return ptable + n;
    } else {
        return 0;
    }
}

unsigned int fastboot_flash_get_ptn_count(void)
{
    return pcount;
}
static void set_env(char *var, char *val)
{
	char *setenv[4]  = { "setenv", NULL, NULL, NULL, };

	setenv[1] = var;
	setenv[2] = val;

	do_setenv(NULL, 0, 3, setenv);
}

static void save_env(struct fastboot_ptentry *ptn,
		     char *var, char *val)
{
	char ecc_type[5];
	char *saveenv[2] = { "setenv", NULL, };
	char *ecc[3]     = { "nandecc", "sw", NULL, };

	set_env (var, val);

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
	do_saveenv(NULL, 0, 1, saveenv);
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
		sprintf (val, "");
		do_setenv (NULL, 0, 3, setenv);

		sprintf (var, "%s_nand_size", ptn->name);
		sprintf (val, "");
		do_setenv (NULL, 0, 3, setenv);
	} else {
		/* Normal case */

		sprintf (var, "%s_nand_offset", ptn->name);
		sprintf (val, "0x%x", offset);

		FBTINFO("%s %s %s\n", setenv[0], setenv[1], setenv[2]);

		do_setenv (NULL, 0, 3, setenv);

		sprintf(var, "%s_nand_size", ptn->name);

		sprintf (val, "0x%x", size);

		FBTINFO("%s %s %s\n", setenv[0], setenv[1], setenv[2]);

		do_setenv (NULL, 0, 3, setenv);
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

	do_saveenv (NULL, 0, 1, saveenv);
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
	char *ecc[3] = {"nandecc", "sw", NULL, };

	/* Some flashing requires the nand's ecc to be set */
	ecc[1] = ecc_type;
	if ((ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) &&
	    (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_SW_ECC)) {
		/* Both can not be true */
		printf("Warning can not do hw and sw ecc for partition '%s'\n",
		       ptn->name);
		printf("Ignoring these flags\n");
	} else if (ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC) {
		sprintf(ecc_type, "hw");
		do_switch_ecc(NULL, 0, 2, ecc);
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

	printf("flashing '%s'\n", ptn->name);

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
					sprintf(addr, "0x%x",
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

					printf("\nDevice %d bad blocks:\n",
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
						sprintf(addr,    "0x%x", priv.transfer_buffer);
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
			sprintf(addr,    "0x%x", priv.transfer_buffer);
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
#endif

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
static int check_against_static_partition(struct fastboot_ptentry *ptn)
{
	int ret = 0;
	struct fastboot_ptentry *c;
	int i;

	for (i = 0; i < static_pcount; i++) {
		c = fastboot_flash_get_ptn((unsigned int) i);

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
#endif

static int fbt_fastboot_init(void)
{
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	fbt_add_partitions_from_environment();
#endif
	priv.d_size = 0;
	priv.flag = 0;
	priv.d_size = 0;
	priv.d_bytes = 0;

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
	priv.nand_block_size               = FASTBOOT_NAND_BLOCK_SIZE;
	priv.nand_oob_size                 = FASTBOOT_NAND_OOB_SIZE;
#endif

	return 0;
}

#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
static int fbt_handle_erase(char *cmdbuf)
{
	struct fastboot_ptentry *ptn;
	int status = 0;

	ptn = fastboot_flash_find_ptn(cmdbuf + 6);
	if (ptn == 0) {
		sprintf(priv.response, "FAILpartition does not exist");
	} else {
		char start[32], length[32];
		int status, repeat, repeat_max;
		char *erase[5]  = { "nand", "erase",  NULL, NULL, NULL, };

		printf("erasing '%s'\n", ptn->name);

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
	}
	return status;
}

static int fbt_handle_flash(char *cmdbuf)
{
	int status = 0;

	if (priv.d_bytes) {
		struct fastboot_ptentry *ptn;

		ptn = fastboot_flash_find_ptn(cmdbuf + 6);
		if (ptn == 0) {
			sprintf(priv.response, "FAILpartition does not exist");
		} else if ((priv.d_bytes > ptn->length) &&
			!(ptn->flags & FASTBOOT_PTENTRY_FLAGS_WRITE_ENV)) {
			sprintf(priv.response, "FAILimage too large for partition");
		} else {
			/* Check if this is not really a flash write
			   but rather a saveenv */
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
		}
	} else {
		sprintf(priv.response, "FAILno image downloaded");
	}

	return status;
}
#endif

/* XXX: Replace magic number & strings with macros */
static int fbt_rx_process(unsigned char *buffer, int length)
{
	FBTDBG();
	/* Generic failed response */
	strcpy(priv.response, "FAIL");

	if (!priv.d_size) {
		/* command */
		char *cmdbuf = (char *) buffer;

		FBTDBG("%c%c%c%c%c%c%c\n", cmdbuf[0], cmdbuf[1], cmdbuf[2],
			cmdbuf[3], cmdbuf[4], cmdbuf[5], cmdbuf[6]);

		FBTDBG();
		if(memcmp(cmdbuf, "getvar:", 7) == 0) {
			FBTINFO("getvar\n");
			strcpy(priv.response, "OKAY");
			if(!strcmp(cmdbuf + strlen("getvar:"), "version")) {
				FBTDBG("getvar version\n");
				strcpy(priv.response + 4,
					FASTBOOT_VERSION);
			}
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
		}

		if(memcmp(cmdbuf, "erase:", 6) == 0) {
			FBTINFO("erase\n");
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
			fbt_handle_erase(cmdbuf);
#endif
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
		}

		if(memcmp(cmdbuf, "flash:", 6) == 0) {
			FBTINFO("flash\n");
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
			fbt_handle_flash(cmdbuf);
#endif
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
		}

		if(memcmp(cmdbuf, "flash:", 6) == 0) {
			FBTINFO("flash\n");
			strcpy(priv.response, "OKAY");
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
		}

		if(memcmp(cmdbuf, "download:", 9) == 0) {
			FBTINFO("download\n");

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
				sprintf(priv.response, "DATA%08x", priv.d_size);
			}
			priv.flag |= FASTBOOT_FLAG_RESPONSE;
		}
	} else {
		if (length) {
			unsigned int xfr_size;

			xfr_size = priv.d_size - priv.d_bytes;
			if (xfr_size > length)
				xfr_size = length;
			memcpy(priv.transfer_buffer + priv.d_bytes,
				buffer, xfr_size);
			priv.d_bytes += xfr_size;

#ifdef	INFO
			/* Inform via prompt that download is happening */
			if (! (priv.d_bytes % (16 * priv.nand_block_size)))
				printf(".");
			if (! (priv.d_bytes % (80 * 16 * priv.nand_block_size)))
				printf("\n");
#endif
			if (priv.d_bytes >= priv.d_size) {
				priv.d_size = 0;
				strcpy(priv.response, "OKAY");
				priv.flag |= FASTBOOT_FLAG_RESPONSE;
#ifdef	INFO
				printf(".\n");
#endif
				priv.download_bytes_unpadded = priv.d_size;
				/* XXX: Revisit padding handling */
#ifdef	FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING
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
#endif
				FBTINFO("downloaded %d bytes\n", priv.d_bytes);
			}
		} else
			FBTWARN("empty buffer download\n");
	}

	FBTDBG();

	return 0;
}

static int fbt_handle_recieve(void)
{
	struct usb_endpoint_instance *ep = &endpoint_instance[RX_EP_INDEX];

	/* XXX: Or update status field, if so,
		"usbd_rcv_complete" [gadget/core.c] also need to be modified */
	if (ep->rcv_urb->actual_length) {
		FBTDBG("rx length: %u\n", ep->rcv_urb->actual_length);
		fbt_rx_process(ep->rcv_urb->buffer, ep->rcv_urb->actual_length);
		FBTDBG();
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

	FBTDBG();
	current_urb = next_urb (device_instance, ep);
	FBTDBG();
	if (!current_urb) {
		printf("error: %s: current_urb NULL", __func__);
		return -1;
	}
	FBTDBG();

	dest = current_urb->buffer + current_urb->actual_length;
	n = MIN (64, strlen(priv.response));
	FBTDBG();
	memcpy(dest, priv.response, n);
	FBTDBG();
	current_urb->actual_length += n;
	FBTDBG();
	if (ep->last == 0) {
		FBTDBG();
		ret = udc_endpoint_write (ep);
		return ret;
	}

	FBTDBG();
	return ret;
}

static int fbt_handle_response(void)
{
	if (priv.flag & FASTBOOT_FLAG_RESPONSE) {
		FBTDBG();
		fbt_response_process();
		FBTDBG();
		priv.flag &= ~FASTBOOT_FLAG_RESPONSE;
	}

	return 0;
}

/* command */
int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = -1;

	ret = fbt_fastboot_init();

	FBTDBG();
	ret = fbt_init_endpoint_ptrs();

	if ((ret = udc_init()) < 0) {
		printf("error: %s: MUSB UDC init failure\n", __func__);
		return ret;
	}

	FBTDBG();
	ret = fbt_init_strings();
	FBTDBG();
	ret = fbt_init_instances();
	FBTDBG();

	udc_startup_events (device_instance);
	FBTDBG();
	udc_connect();
	FBTDBG();
	ret = fbt_init_endpoints();

	FBTINFO("fastboot initialized\n");

	while(1) {
		udc_irq();
		fbt_handle_recieve();
		fbt_handle_response();
		if (ctrlc()) {
			FBTDBG();
			printf("fastboot ended by user\n");
			break;
		}
	}

	FBTDBG();
	return ret;
}

U_BOOT_CMD(fastboot, 2,	1, do_fastboot,
	"fastboot- use USB Fastboot protocol\n", NULL);

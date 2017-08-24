#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <usb.h>
#include <string.h>
#include <ncurses.h>
#include <stdlib.h>

/**
* Device vendor definition
*/
//@{

#define DEVICE_VENDOR_VID 0x03eb
#define DEVICE_VENDOR_PID 0x2423

// the device's endpoints
static unsigned char udi_vendor_ep_interrupt_in;
static unsigned char udi_vendor_ep_interrupt_out;

char string_usb[100];

struct usb_bus *bus;
struct usb_device *device;
usb_dev_handle *device_handle = NULL; // the device handle

#define  UDI_VENDOR_LOOPBACK_SIZE    12

uint8_t udi_vendor_buf_out[UDI_VENDOR_LOOPBACK_SIZE] = "hello world";
uint8_t udi_vendor_buf_in[UDI_VENDOR_LOOPBACK_SIZE];

//@}

static void init_buffers(void);
static int loop_back_interrupt(usb_dev_handle *device_handle);

int opendevice(void);

int x=0,y=5;

void findendpoint(void)
{
	//if (opendevice())
	//{
		//clear();
		mvprintw(y,x,"Searching endpoints\n");
		refresh();
		unsigned char nb_ep = 0;
		struct usb_endpoint_descriptor *endpoints;

		if (1 == device->config->interface->num_altsetting) {
			// Old firmwares have no alternate setting
			nb_ep = device->config->interface->altsetting[0].bNumEndpoints;
			endpoints = device->config->interface->altsetting[0].endpoint;
		}
		else {
			// The alternate setting has been added to be USB compliance:
			// 1.2.40 An Isochronous endpoint present in alternate interface 0x00 must have a MaxPacketSize of 0x00
			// Reference document: Universal Serial Bus Specification, Revision 2.0, Section 5.6.3.
			nb_ep = device->config->interface->altsetting[1].bNumEndpoints;
			endpoints = device->config->interface->altsetting[1].endpoint;
		}
		while (nb_ep) {
			nb_ep--;

			unsigned char ep_type = endpoints[nb_ep].bmAttributes	& USB_ENDPOINT_TYPE_MASK;
			unsigned char ep_add = endpoints[nb_ep].bEndpointAddress;
			unsigned char dir_in = (ep_add & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN;
			unsigned short ep_size = endpoints[nb_ep].wMaxPacketSize;

			switch (ep_type) {
			case USB_ENDPOINT_TYPE_INTERRUPT:
				if (dir_in) {
					udi_vendor_ep_interrupt_in = ep_add;
				}
				else {
					udi_vendor_ep_interrupt_out = ep_add;
				}
				break;
			}
		}
		//clear();
		mvprintw(4,0,"Endpoint in: %02X, out: %02X\n", udi_vendor_ep_interrupt_in, udi_vendor_ep_interrupt_out);
		refresh();
	//}
}

int openinterface(void)
{
	//if (opendevice())
	//{
		//clear();
		mvprintw(y,x,"Initialization device\n");
		refresh();
		// Open interface vendor
		if (usb_set_configuration(device_handle, 1) < 0) {
			clear();
			mvprintw(y,x,"error: setting config 1 failed\n");
			refresh();
			usb_close(device_handle);
			return 0;
		}
		if (usb_claim_interface(device_handle, 0) < 0) {
			clear();
			mvprintw(y,x,"error: claiming interface 0 failed\n");
			refresh();
			usb_close(device_handle);
			return 0;
		}
		if (1 != device->config->interface->num_altsetting) {

			if (usb_set_altinterface(device_handle, 1) < 0) {
				clear();
				mvprintw(y,x,"error: set alternate 1 interface 0 failed\n");
				refresh();
				usb_close(device_handle);
				return 0;
			}
		}
		//clear();
		mvprintw(y,x,"Device ready\n");
		refresh();
		findendpoint();
		return 1;
	//}
	//else
		//return 0;
}

int opendevice(void)
{
	if (device_handle == NULL)
	{
		clear();
		mvprintw(y,x,"Opening\n");
		refresh();
		usb_find_devices(); // find all connected devices
		// Search and open device
		for (bus = usb_get_busses(); bus; bus = bus->next)
		{
			for (device = bus->devices; device; device = device->next)
			{
				if (device->descriptor.idVendor == DEVICE_VENDOR_VID && device->descriptor.idProduct == DEVICE_VENDOR_PID)
				{
					device_handle = usb_open(device);
					clear();
					mvprintw(y,x,"Device open\n");
					mvprintw(0,0,"- Device version: %d.%d\n", device->descriptor.bcdDevice >> 8, (device->descriptor.bcdDevice & 0xFF));
					if (0 != device->descriptor.iManufacturer) {
						usb_get_string_simple(device_handle, device->descriptor.iManufacturer, string_usb, sizeof(string_usb));
						mvprintw(1,0,"- Manufacturer name: %s\n", string_usb);
					}
					if (0 != device->descriptor.iProduct) {
						usb_get_string_simple(device_handle, device->descriptor.iProduct, string_usb, sizeof(string_usb));
						mvprintw(2,0,"- Product name: %s\n", string_usb);
					}
					if (0 != device->descriptor.iSerialNumber) {
						usb_get_string_simple(device_handle, device->descriptor.iSerialNumber, string_usb, sizeof(string_usb));
						mvprintw(3,0,"- Serial number: %s\n\n", string_usb);
					}
					refresh();
					openinterface();
					return 1;
				}
			}
		}
		if (device_handle == NULL)
		{
			clear();
			mvprintw(y,x,"Device not found\n");
			refresh();
			return 0;
		}
	}
	return 1;
}

void transfer(void)
{
	if (device_handle != NULL)
	{
		if (udi_vendor_ep_interrupt_in && udi_vendor_ep_interrupt_out)
		{
			//printf("Interrupt enpoint loop back...\n");
			if (loop_back_interrupt(device_handle)) {
				clear();
				mvprintw(y,x,"Error during interrupt endpoint transfer\n");
				refresh();
				usb_close(device_handle);
				device_handle = NULL;
				udi_vendor_ep_interrupt_in = 0;
				udi_vendor_ep_interrupt_out = 0;
				return;
			}
			//clear();
			mvprintw(y,x,"data: %02X %02X\n", udi_vendor_buf_in[0], udi_vendor_buf_in[1]);
			refresh();
		}
	}
	else
		opendevice();
}

/// The main entry-point function.
int main(int argc, char *argv[])
{
	// Ncurses initialization
	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	atexit(endwin);

	// Libusb initialization
	clear();
	mvprintw(y,x,"Initialization library \"libusb\"...\n");
	refresh();
	usb_init();         // initialize the library
	usb_find_busses();  // find all busses
	clear();
	mvprintw(y,x,"Search device...\n");
	refresh();

	while (1)
	{
		transfer();
		sleep(1);
	}

}

static int loop_back_interrupt(usb_dev_handle *device_handle)
{
	if (0> usb_interrupt_write(device_handle,
		udi_vendor_ep_interrupt_out,
		udi_vendor_buf_out,
		sizeof(udi_vendor_buf_out),
		1000)) {
		return -1;
	}
	if (0> usb_interrupt_read(device_handle,
		udi_vendor_ep_interrupt_in,
		udi_vendor_buf_in,
		sizeof(udi_vendor_buf_in),
		1000)) {
		return -1;
	}
	return 0;
}

/*******************************************************************************\
 * Program Name		:	libcyusb.c					*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )		*
 * License		:	GPL Ver 2.0					*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS		*
 * Date written		:	March 12, 2012					*
 * Modification Notes	:							*
 * 										*
 * This program is the main library for all cyusb applications to use.		*
 * This is a thin wrapper around libusb						*
\*******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>
#include "../include/cyusb.h"

/* Maximum length of a string read from the Configuration file (/etc/cyusb.conf) for the library. */
#define MAX_CFG_LINE_LENGTH                     (120)

static struct cydev cydev[MAXDEVICES];
static int nid;		/* Number of Interesting Devices	*/

struct VPD {
	unsigned short	vid;
	unsigned short	pid;
	char		desc[MAX_STR_LEN];
};

static struct VPD vpd[MAX_ID_PAIRS];

static int maxdevices;
static int numdev;
static libusb_device **list;
static unsigned int checksum = 0;

char pidfile[256];
char logfile[256];
int logfd;
int pidfd;

static int isempty(char *buf, int L)
{
	int flag = 1;
	int i;

	if ( L == 0 ) return 1;

	for (i = 0; i < L; ++i ) {
	    if ( (buf[i] == ' ') || ( buf[i] == '\t' ) )
		 continue;
	    else {
		 flag = 0;
		 break;
	    }
	}
	return flag;
}

static void parse_configfile(void)
{
	FILE *inp;
	char buf[MAX_CFG_LINE_LENGTH];
	char *cp1, *cp2, *cp3;
	int i;
	
	inp = fopen("/etc/cyusb.conf", "r");
        if (inp == NULL)
            return;

	memset(buf,'\0',MAX_CFG_LINE_LENGTH);
	while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
		if ( buf[0] == '#' ) 			/* Any line starting with a # is a comment 	*/
		   continue;
		if ( buf[0] == '\n' )
		   continue;
		if ( isempty(buf,strlen(buf)) )		/* Any blank line is also ignored		*/
		   continue;

		cp1 = strtok(buf," =\t\n");
		if ( !strcmp(cp1,"LogFile") ) {
		   cp2 = strtok(NULL," \t\n");
		   strcpy(logfile,cp2);
		}
		else if ( !strcmp(cp1,"PIDFile") ) {
			cp2 = strtok(NULL," \t\n");
			strcpy(pidfile,cp2);
		}
		else if ( !strcmp(cp1,"<VPD>") ) {
			while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
				if ( buf[0] == '#' ) 		/* Any line starting with a # is a comment 	*/
				   continue;
				if ( buf[0] == '\n' )
				   continue;
				if ( isempty(buf,strlen(buf)) )	/* Any blank line is also ignored		*/
				   continue;
				if ( maxdevices == (MAX_ID_PAIRS - 1) )
				   continue;
				cp1 = strtok(buf," \t\n");
				if ( !strcmp(cp1,"</VPD>") )
				   break;
				cp2 = strtok(NULL, " \t");
				cp3 = strtok(NULL, " \t\n");

				vpd[maxdevices].vid = strtol(cp1,NULL,16);
				vpd[maxdevices].pid = strtol(cp2,NULL,16);
				strncpy(vpd[maxdevices].desc,cp3,MAX_STR_LEN);
                                vpd[maxdevices].desc[MAX_STR_LEN - 1] = '\0';   /* Make sure of NULL-termination. */

				++maxdevices;
			}
		}
		else {
		     printf("Error in config file /etc/cyusb.conf: %s \n",buf);
		     exit(1);
		}
	}

	fclose(inp);
}

static int device_is_of_interest(cyusb_device *d)
{
	int i;
	int found = 0;
	struct libusb_device_descriptor desc;
	int vid;
	int pid;

	libusb_get_device_descriptor(d, &desc);
	vid = desc.idProduct;
	pid = desc.idProduct;
	
	for ( i = 0; i < maxdevices; ++i ) {
	    if ( (vpd[i].vid == desc.idVendor) && (vpd[i].pid == desc.idProduct) ) {
	       	found = 1;
		break;
	    }
	}
	return found;
}

unsigned short cyusb_getvendor(cyusb_handle *h)
{
	struct libusb_device_descriptor d;
	cyusb_get_device_descriptor(h, &d);
	return d.idVendor;
}

unsigned short cyusb_getproduct(cyusb_handle *h)
{
	struct libusb_device_descriptor d;
	cyusb_get_device_descriptor(h, &d);
	return d.idProduct;
} 

static int renumerate(void)
{
	cyusb_device *dev = NULL;
	cyusb_handle *handle = NULL;
	int found = 0;
	int i;
	int r;

	numdev = libusb_get_device_list(NULL, &list);
	if ( numdev < 0 ) {
	   printf("Library: Error in enumerating devices...\n");
	   return -4;
	}

	nid = 0;

	for ( i = 0; i < numdev; ++i ) {
		cyusb_device *tdev = list[i];
		if ( device_is_of_interest(tdev) ) {
		   cydev[nid].dev = tdev;
		   r = libusb_open(tdev, &cydev[nid].handle);
		   if ( r ) {
		      printf("Error in opening device\n");
		      return -5;
		   }
		   else handle = cydev[nid].handle;
		   cydev[nid].vid     = cyusb_getvendor(handle);
		   cydev[nid].pid     = cyusb_getproduct(handle);
		   cydev[nid].is_open = 1;
		   cydev[nid].busnum  = cyusb_get_busnumber(handle);
		   cydev[nid].devaddr = cyusb_get_devaddr(handle);
		   ++nid;
		}
	}
	return nid;
}

int cyusb_open(void)
{
	int fd1;
	int r;

	fd1 = open("/etc/cyusb.conf", O_RDONLY);
	if ( fd1 < 0 ) {
	   printf("/etc/cyusb.conf file not found. Exiting\n");
	   return -1;
	}
	else {
	   close(fd1);
	   parse_configfile();	/* Parses the file and stores critical information inside exported data structures */
	}

	r = libusb_init(NULL);
	if (r) {
	      printf("Error in initializing libusb library...\n");
	      return -2;
	}

	r = renumerate();
	return r;
}

int cyusb_open(unsigned short vid, unsigned short pid)
{
	int r;
	cyusb_handle *h = NULL;
	
	r = libusb_init(NULL);
	if (r) {
	      printf("Error in initializing libusb library...\n");
	      return -1;
	}
	h = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if ( !h ) {
	   printf("Device not found\n");
	   return -2;
	}
	cydev[0].dev     = libusb_get_device(h);
	cydev[0].handle  = h;
  	cydev[nid].vid     = cyusb_getvendor(h);
	cydev[nid].pid     = cyusb_getproduct(h);
	cydev[nid].is_open = 1;
	cydev[nid].busnum  = cyusb_get_busnumber(h);
	cydev[nid].devaddr = cyusb_get_devaddr(h);
	nid = 1;
	return 1;
}

void cyusb_error(int err)
{	if ( err == -1 )
	   fprintf(stderr, "Input/output error\n");
	else if ( err == -2 )
	   	fprintf(stderr, "Invalid parameter\n");
	else if ( err == -3 )
		fprintf(stderr, "Access denied (insufficient permissions)\n");
	else if ( err == -4 )
		fprintf(stderr, "No such device. Disconnected...?\n");
	else if ( err == -5 )
		fprintf(stderr, "Entity not found\n");
	else if ( err == -6 )
		fprintf(stderr, "Resource busy\n");
	else if ( err == -7 )
		fprintf(stderr, "Operation timed out\n");
	else if ( err == -8 )
		fprintf(stderr, "Overflow\n");
	else if ( err == -9 )
		fprintf(stderr, "Pipe error\n");
	else if ( err == -10 )
		fprintf(stderr, "System call interrupted, ( due to signal ? )\n");
	else if ( err == -11 )
		fprintf(stderr, "Insufficient memory\n");
	else if ( err == 12 )
		fprintf(stderr, "Operation not supported/implemented\n");
	else fprintf(stderr, "Unknown internal error\n");
}
	

cyusb_handle * cyusb_gethandle(int index)	
{
	return cydev[index].handle;
}

void cyusb_close(void)
{
	int i;

	for ( i = 0; i < nid; ++i ) {
		libusb_close(cydev[i].handle);
	}
	libusb_free_device_list(list, 1);
	libusb_exit(NULL);
}

int cyusb_get_busnumber(cyusb_handle *h)
{
	cyusb_device *tdev = libusb_get_device(h);
	return libusb_get_bus_number( tdev );
}

int cyusb_get_devaddr(cyusb_handle *h)
{
	cyusb_device *tdev = libusb_get_device(h);
	return libusb_get_device_address( tdev );
}

int cyusb_get_max_packet_size(cyusb_handle *h, unsigned char endpoint)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_max_packet_size(tdev, endpoint) );
}

int cyusb_get_max_iso_packet_size(cyusb_handle *h, unsigned char endpoint)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_max_iso_packet_size(tdev, endpoint) );
}

int cyusb_get_configuration(cyusb_handle *h, int *config)
{
	return ( libusb_get_configuration(h, config) );
}

int cyusb_set_configuration(cyusb_handle *h, int config)
{
	return ( libusb_set_configuration(h, config) );
}

int cyusb_claim_interface(cyusb_handle *h, int interface)
{
	return ( libusb_claim_interface(h, interface) );
}

int cyusb_release_interface(cyusb_handle *h, int interface)
{
	return ( libusb_release_interface(h, interface) );
}

int cyusb_set_interface_alt_setting(cyusb_handle *h, int interface, int altsetting)
{
	return ( libusb_set_interface_alt_setting(h, interface, altsetting) );
}

int cyusb_clear_halt(cyusb_handle *h, unsigned char endpoint)
{
	return ( libusb_clear_halt(h, endpoint) );
}

int cyusb_reset_device(cyusb_handle *h)
{
	return ( libusb_reset_device(h) );
}

int cyusb_kernel_driver_active(cyusb_handle *h, int interface)
{
	return ( libusb_kernel_driver_active(h, interface) );
}

int cyusb_detach_kernel_driver(cyusb_handle *h, int interface)
{
	return ( libusb_detach_kernel_driver(h, interface) );
}

int cyusb_attach_kernel_driver(cyusb_handle *h, int interface)
{
	return ( libusb_attach_kernel_driver(h, interface) );
}

int cyusb_get_device_descriptor(cyusb_handle *h, struct libusb_device_descriptor *desc)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_device_descriptor(tdev, desc ) );
}

int cyusb_get_active_config_descriptor(cyusb_handle *h, struct libusb_config_descriptor **config)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_active_config_descriptor(tdev, config) );
}

int cyusb_get_config_descriptor(cyusb_handle *h, unsigned char config_index, struct libusb_config_descriptor **config)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_config_descriptor(tdev, config_index, config) );
}

int cyusb_get_config_descriptor_by_value(cyusb_handle *h, unsigned char bConfigurationValue, 
									struct usb_config_descriptor **config)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_config_descriptor_by_value(tdev, bConfigurationValue,
                    (struct libusb_config_descriptor **)config) );
}

void cyusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
	libusb_free_config_descriptor( (libusb_config_descriptor *)config );
}

int cyusb_get_string_descriptor_ascii(cyusb_handle *h, unsigned char index, unsigned char *data,  int length)
{
	cyusb_device *tdev = libusb_get_device(h);
	return ( libusb_get_string_descriptor_ascii(h, index, data, length) );
}

int cyusb_get_descriptor(cyusb_handle *h, unsigned char desc_type, unsigned char desc_index, unsigned char *data,
        int len)
{
	return ( libusb_get_descriptor(h, desc_type, desc_index, data, len) );
}

int cyusb_get_string_descriptor(cyusb_handle *h, unsigned char desc_index, unsigned short langid, 
								unsigned char *data, int len)
{
	return ( libusb_get_string_descriptor(h, desc_index, langid, data, len) );
}

int cyusb_control_transfer(cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout)
{
	return ( libusb_control_transfer(h, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout) );
}

int cyusb_control_read (cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout)
{
	/* Set the direction bit to indicate a read transfer. */
	return ( libusb_control_transfer(h, bmRequestType | 0x80, bRequest, wValue, wIndex, data, wLength, timeout) );
}

int cyusb_control_write (cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout)
{
	/* Clear the direction bit to indicate a write transfer. */
	return ( libusb_control_transfer(h, bmRequestType & 0x7F, bRequest, wValue, wIndex, data, wLength, timeout) );
}

int cyusb_bulk_transfer(cyusb_handle *h, unsigned char endpoint, unsigned char *data, int length, 
				int *transferred, int timeout)
{
	return ( libusb_bulk_transfer(h, endpoint, data, length, transferred, timeout) );
}

int cyusb_interrupt_transfer(cyusb_handle *h, unsigned char endpoint, unsigned char *data, int length,
				int *transferred, unsigned int timeout)
{
	return ( libusb_interrupt_transfer(h, endpoint, data, length, transferred, timeout) );
}

int cyusb_download_fx2(cyusb_handle *h, char *filename, unsigned char vendor_command)
{
	FILE *fp = NULL;
	char buf[256];
	char tbuf1[3];
	char tbuf2[5];
	char tbuf3[3];
	unsigned char reset = 0;
	int r;
	int count = 0;
	unsigned char num_bytes = 0;
	unsigned short address = 0;
	unsigned char *dbuf = NULL;
	int i;


	fp = fopen(filename, "r" );
	tbuf1[2] ='\0';
	tbuf2[4] = '\0';
	tbuf3[2] = '\0';

	reset = 1;
	r = cyusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
	if ( !r ) {
	   printf("Error in control_transfer\n");
	   return r;
	}
	sleep(1);
	
	count = 0;

	while ( fgets(buf, 256, fp) != NULL ) {
		if ( buf[8] == '1' )
		   break;
		strncpy(tbuf1,buf+1,2);
		num_bytes = strtoul(tbuf1,NULL,16);
		strncpy(tbuf2,buf+3,4);
		address = strtoul(tbuf2,NULL,16);
		dbuf = (unsigned char *)malloc(num_bytes);
		for ( i = 0; i < num_bytes; ++i ) {
		    strncpy(tbuf3,&buf[9+i*2],2);
		    dbuf[i] = strtoul(tbuf3,NULL,16);
		}
		r = cyusb_control_transfer(h, 0x40, vendor_command, address, 0x00, dbuf, num_bytes, 1000);
		if ( !r ) {
	   	   printf("Error in control_transfer\n");
		   free(dbuf);
	   	   return r;
		}
	     	count += num_bytes;
		free(dbuf);
	}
	printf("Total bytes downloaded = %d\n", count);
	sleep(1);
	reset = 0;
	r = cyusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
	fclose(fp);
	return 0;
}


static void control_transfer(cyusb_handle *h, unsigned int address, unsigned char *dbuf, int len)
{
	int j;
	int r;
	int b;
	unsigned int *pint;
	int index;

	int balance = len;
	pint = (unsigned int *)dbuf;

	index = 0;
	while ( balance > 0 ) {
	      if ( balance > 4096 )
	 	 b = 4096;
	      else b = balance;
	      r = cyusb_control_transfer(h, 0x40, 0xA0, ( address & 0x0000ffff ), address >> 16, &dbuf[index], b, 1000);
	      if ( r != b ) {
	   	   printf("Error in control_transfer\n");
	      }
	      address += b ;
	      balance -= b;
	      index += b;
	}
	for ( j = 0; j < len/4; ++j )
	    checksum += pint[j];
}

int cyusb_download_fx3(cyusb_handle *h, const char *filename)
{
	int fd;
	unsigned char buf[1000000];
	int nbr;
	int dlen;
	int count;
	unsigned int *pdbuf = NULL;
	unsigned int address;
	unsigned int *pint;
	unsigned int program_entry;
	int r;

	fd = open(filename, O_RDONLY);
	if ( fd < 0 ) { 
	   printf("File not found\n");
	   return -1;
	}
	else printf("File successfully opened\n");
	count = 0;
	checksum = 0;
	nbr = read(fd, buf, 2);		/* Read first 2 bytes, must be equal to 'CY'	*/
	if ( strncmp((char *)buf,"CY",2) ) {
	   printf("Image does not have 'CY' at start. aborting\n");
	   return -2;
	}
	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageCTL	*/
	if ( buf[0] & 0x01 ) {
	   printf("Image does not contain executable code\n");
	   return -3;
	}
	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageType	*/
	if ( !(buf[0] == 0xB0) ) {
	   printf("Not a normal FW binary with checksum\n");
	   return -4;
	}
	while (1) {
	   nbr = read(fd, buf, 4);	/* Read Length of section 1,2,3, ...	*/
	   pdbuf = (unsigned int *)buf;
	   dlen = *pdbuf;
	   nbr = read(fd,buf,4);	/* Read Address of section 1,2,3,...	*/
	   pint = (unsigned int *)buf;   
	   address = *pint;
	   if ( dlen != 0 ) {
	      nbr = read(fd, buf, dlen*4);	/* Read data bytes	*/
	      control_transfer(h, address, buf, dlen*4);
	   }
	   else {
	      program_entry = address;
	      break;
	   }
	}
	nbr = read(fd, buf, 4);			/* Read checksum	*/
	pdbuf = (unsigned int *)buf;
	if ( *pdbuf != checksum ) {
	   printf("Error in checksum\n");
	   return -5;
	}
	sleep(1);
	r = cyusb_control_transfer(h, 0x40, 0xA0, (program_entry & 0x0000ffff ) , program_entry >> 16, NULL, 0, 1000);
	if ( r ) {
	   printf("Ignored error in control_transfer: %d\n", r);
	}
	close(fd);
	return 0;
}


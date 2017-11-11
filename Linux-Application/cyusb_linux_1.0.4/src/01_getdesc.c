/***********************************************************************************************************************\
 * Program Name		:	01_getdesc.c										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	March 13, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is a CLI program to dump out the device descriptor for any USB device given its VID/PID		        *
 * The program itself accepts options : Either vid/pid on the command line OR first VID/PID of interest in cyusb.conf   *
\***********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "../include/cyusb.h"

/********** Cut and paste the following & modify as required  **********/
static const char * program_name;
static const char *out_filename = "stdout";
static const char *const short_options = "hvV:P:o:";
static const struct option long_options[] = {
		{ "help",	0,	NULL,	'h'	},
		{ "version",	0,	NULL,	'v'	},
		{ "Vendor",     1,      NULL,   'V'     },
		{ "Product",    1,      NULL,   'P'     },
		{ "Output",     1,      NULL,   'O',    },
		{ NULL,		0,	NULL,	 0	}
};

static int next_option;

static void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s options\n", program_name);
	fprintf(stream, 
		"  -h  --help           Display this usage information.\n"
		"  -v  --version        Print version.\n"
		"  -V  --Vendor		Input Vendor  ID in hexadecimal.\n"
		"  -P  --Product	Input Product ID in hexadecimal.\n"
		"  -o  --output	 	Output file name.\n");

	exit(exit_code);
}
/***********************************************************************/

static FILE *fp = stdout;
static int vendor_provided;
static unsigned short vid;
static int product_provided;
static unsigned short pid;

static void validate_inputs(void)
{
	if ( ((vendor_provided) && (!product_provided)) || ((!vendor_provided) && (product_provided)) ) {
	   fprintf(stderr,"Must provide BOTH VID and PID or leave BOTH blank to pick up from /etc/cyusb.conf\n");
	   print_usage(stdout, 1);
	}   
}

int main(int argc, char **argv)
{
	int r;
	struct libusb_device_descriptor desc;
	char user_input = 'n';

	program_name = argv[0];
	
	while ( (next_option = getopt_long(argc, argv, short_options, 
					   long_options, NULL) ) != -1 ) {
		switch ( next_option ) {
			case 'h': /* -h or --help  */
				  print_usage(stdout, 0);
			case 'v': /* -v or --version */
				  printf("%s (Ver 1.0)\n",program_name);
				  printf("Copyright (C) 2012 Cypress Semiconductors Inc. / ATR-LABS\n");
				  exit(0);
			case 'o': /* -o or --output */
				  out_filename = optarg;
				  fp = fopen(out_filename,"a");
				  if ( !fp ) 
				     print_usage(stdout, 1);
				  break;
			case 'V': /* -V or --vendor  */
				  vendor_provided = 1;
				  vid = strtoul(optarg,NULL,16);
				  break;
			case 'P': /* -P or --Product  */
				  product_provided = 1;
				  pid = strtoul(optarg,NULL,16);
				  break;
			case '?': /* Invalid option */
				  print_usage(stdout, 1);
			default : /* Something else, unexpected */
				  abort();
		}
	} 

	validate_inputs();

	if ( (vendor_provided) && (product_provided) )
	   user_input = 'y';
	else user_input = 'n';
	if ( user_input == 'y' ) {
	   r = cyusb_open(vid, pid);
	   if ( r < 0 ) {
	      printf("Error opening library\n");
	      return -1;
	   }
	   else if ( r == 0 ) {
		   printf("No device found\n");
		   return 0;
	   }
	}
	else {
	   r = cyusb_open();
	   if ( r < 0 ) {
	      printf("Error opening library\n");
	      return -1;
	   }
	   else if ( r == 0 ) {
		   printf("No device found\n");
		   return 0;
	   }
	}
	r = cyusb_get_device_descriptor(cyusb_gethandle(0), &desc);
	if ( r ) {
	   printf("error getting device descriptor\n");
	   return -2;
	}
	fprintf(fp,"bLength             = %d\n",       desc.bLength);
	fprintf(fp,"bDescriptorType     = %d\n",       desc.bDescriptorType);
	fprintf(fp,"bcdUSB              = 0x%04x\n",   desc.bcdUSB);
	fprintf(fp,"bDeviceClass        = 0x%02x\n",   desc.bDeviceClass);
	fprintf(fp,"bDeviceSubClass     = 0x%02x\n",   desc.bDeviceSubClass);
	fprintf(fp,"bDeviceProtocol     = 0x%02x\n",   desc.bDeviceProtocol);
	fprintf(fp,"bMaxPacketSize      = %d\n",       desc.bMaxPacketSize0);
	fprintf(fp,"idVendor            = 0x%04x\n",   desc.idVendor);
	fprintf(fp,"idProduct           = 0x%04x\n",   desc.idProduct);
	fprintf(fp,"bcdDevice           = 0x%04x\n",   desc.bcdDevice);
	fprintf(fp,"iManufacturer       = %d\n",       desc.iManufacturer);
	fprintf(fp,"iProduct            = %d\n",       desc.iProduct);
	fprintf(fp,"iSerialNumber       = %d\n",       desc.iSerialNumber);
	fprintf(fp,"bNumConfigurations  = %d\n",       desc.bNumConfigurations);

	cyusb_close();
	return 0;
}

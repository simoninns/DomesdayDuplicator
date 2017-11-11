/***********************************************************************************************************************\
 * Program Name		:	08_cybulk.c										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	April 3, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is a CLI program that does a bulk transfer using the bulkloop.hex file downloaded to the FX2 device	*
\***********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

#include "../include/cyusb.h"

/********** Cut and paste the following & modify as required  **********/
static const char * program_name;
static const char *const short_options = "hvt:";
static const struct option long_options[] = {
		{ "help",	0,	NULL,	'h'	},
		{ "version",	0,	NULL,	'v'	},
		{ "timeout",    1,      NULL,   't',	},
		{ NULL,		0,	NULL,	 0	}
};

static int next_option;

static void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s options\n", program_name);
	fprintf(stream, 
		"  -h  --help           Display this usage information.\n"
		"  -v  --version        Print version.\n"
		"  -t  --timeout	timeout in seconds, 0 for indefinite wait.\n");

	exit(exit_code);
}
/***********************************************************************/

static FILE *fp = stdout;
static int timeout_provided;
static int timeout = 0;
static cyusb_handle *h1 = NULL;

static void validate_inputs(void)
{
	if ( (timeout_provided) && (timeout < 0) ) {
	   fprintf(stderr,"Must provide a positive value for timeout in seconds\n");
	   print_usage(stdout, 1);
	}   
}

static void *reader(void *arg1)
{
	int r;
	unsigned char buf[64];
	int transferred = 0;

	memset(buf,'\0',64);
	while (1) {
		r = cyusb_bulk_transfer(h1, 0x86, buf, 64, &transferred, timeout * 1000);
		if ( r == 0 ) {
		   printf("%s", buf);
		   memset(buf,'\0',64);
		   continue;
		}
		else {
			cyusb_error(r);
			cyusb_close();
			return NULL;
		}
	} 

}

static void * writer(void *arg2)
{
	int r, nbr;
	unsigned char buf[64];
	int transferred = 0;

	memset(buf,'\0',64);
	while ( nbr = read(0,buf,64) ) {
		r = cyusb_bulk_transfer(h1, 0x02, buf, nbr, &transferred, timeout * 1000);
		if ( r == 0 ) {
			memset(buf,'\0',64);
			continue;
		}
		else {
			cyusb_error(r);
			cyusb_close();
			return NULL;
		}
	} 

}


int main(int argc, char **argv)
{
	int r;
	char user_input = 'n';
	pthread_t tid1, tid2;

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
			case 't': /* -t or --timeout  */
				  timeout_provided = 1;
				  timeout = atoi(optarg);
				  break;
			case '?': /* Invalid option */
				  print_usage(stdout, 1);
			default : /* Something else, unexpected */
				  abort();
		}
	} 

	validate_inputs();

	r = cyusb_open();
	if ( r < 0 ) {
	   printf("Error opening library\n");
	   return -1;
	}
	else if ( r == 0 ) {
		printf("No device found\n");
		return 0;
	}
	if ( r > 1 ) {
		printf("More than 1 devices of interest found. Disconnect unwanted devices\n");
		return 0;
	}
	h1 = cyusb_gethandle(0);
	if ( cyusb_getvendor(h1) != 0x04b4 ) {
	  	printf("Cypress chipset not detected\n");
		cyusb_close();
	  	return 0;
	}
	r = cyusb_kernel_driver_active(h1, 0);
	if ( r != 0 ) {
	   printf("kernel driver active. Exitting\n");
	   cyusb_close();
	   return 0;
	}
	r = cyusb_claim_interface(h1, 0);
	if ( r != 0 ) {
	   printf("Error in claiming interface\n");
	   cyusb_close();
	   return 0;
	}
	else printf("Successfully claimed interface\n");
	r = pthread_create(&tid1, NULL, reader, NULL);
	r = pthread_create(&tid2, NULL, writer, NULL);
	while ( 1) {
		pause();
	}
	cyusb_close();
	return 0;
}

/***********************************************************************************************************************\
 * Program Name		:	05_claiminterface.c									*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	March 22, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is a CLI program to claim an interface for a device which has an unclaimed interface			*
\***********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include "../include/cyusb.h"

/********** Cut and paste the following & modify as required  **********/
static const char * program_name;
static const char *out_filename = "stdout";
static const char *const short_options = "hv";
static const struct option long_options[] = {
		{ "help",	0,	NULL,	'h'	},
		{ "version",	0,	NULL,	'v'	},
		{ NULL,		0,	NULL,	 0	}
};

static int next_option;

static void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s options\n", program_name);
	fprintf(stream, 
		"  -h  --help           Display this usage information.\n"
		"  -v  --version        Print version.\n");

	exit(exit_code);
}
/***********************************************************************/

static FILE *fp = stdout;
static int interface;

static void validate_inputs(void)
{
}

static void sighandler(int signo)
{
	printf("Signal to quit received\n");
	cyusb_release_interface(cyusb_gethandle(0), interface);
	cyusb_close();
	exit(0);
}

int main(int argc, char **argv)
{
	int r;
	int kernel_attached = 0;
	int opt;

	program_name = argv[0];

	signal(SIGINT, sighandler);
	
	while ( (next_option = getopt_long(argc, argv, short_options, 
					   long_options, NULL) ) != -1 ) {
		switch ( next_option ) {
			case 'h': /* -h or --help  */
				  print_usage(stdout, 0);
			case 'v': /* -v or --version */
				  printf("%s (Ver 1.0)\n",program_name);
				  printf("Copyright (C) 2012 Cypress Semiconductors Inc. / ATR-LABS\n");
				  exit(0);
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
	printf("Enter interface number you wish to claim : ");
	scanf("%d",&interface);

	r = cyusb_kernel_driver_active(cyusb_gethandle(0), interface);
	if ( r == 1 ) {
	   printf("A kernel driver has already claimed this interface\n");
	   kernel_attached = 1;
	}
	else if ( r ) {
	   cyusb_error(r);
	   cyusb_close();
	   return r;
	}
	if ( kernel_attached ) {
	      	r = cyusb_detach_kernel_driver(cyusb_gethandle(0), interface);
	  	if ( r == 0 ) {
		   printf("Successfully detached kernel driver for this interface\n");
		}
		else {
			cyusb_error(r);
			cyusb_close();
			return r;
		}
	}
	r = cyusb_claim_interface(cyusb_gethandle(0), interface);
	if ( r == 0 ) {
	   printf("Interface %d claimed successfully\n",interface);
	}
	else {
	     cyusb_error(r);
	     cyusb_close();
	     return r;
	}
	while (1) {
		pause();
	}
	cyusb_close();
	return 0;
}

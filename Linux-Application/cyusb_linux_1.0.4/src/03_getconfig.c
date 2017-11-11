/***********************************************************************************************************************\
 * Program Name		:	01_getconfig.c										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	March 22, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is a CLI program to extract the current configuration of a device.					*
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

static void validate_inputs(void)
{
}

int main(int argc, char **argv)
{
	int r;
	int config = 0;

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
	r = cyusb_get_configuration(cyusb_gethandle(0),&config); 
	if ( r ) {
	   cyusb_error(r);
	   cyusb_close();
	   return r;
	}
	if ( config == 0 ) 
	   printf("The device is currently unconfigured\n");
	else printf("Device configured. Current configuration = %d\n", config);

	cyusb_close();
	return 0;
}

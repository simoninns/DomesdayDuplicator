/***********************************************************************************************************************\
 * Program Name		:	0_fwload.c										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	March 27, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program is a CLI program to download a firmware file ( .hex format ) into the chip using bRequest 0xA0		*
\***********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>

#include "../include/cyusb.h"

/********** Cut and paste the following & modify as required  **********/
static const char * program_name;
static const char *const short_options = "hvf:e:";
static const struct option long_options[] = {
		{ "help",	0,	NULL,	'h'	},
		{ "version",	0,	NULL,	'v'	},
		{ "file",       1,      NULL,   'f',    },
		{ "extension",  1,      NULL,   'e',	},
		{ NULL,		0,	NULL,	 0	}
};

static int next_option;

static void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s options\n", program_name);
	fprintf(stream, 
		"  -h  --help           Display this usage information.\n"
		"  -v  --version        Print version.\n"
		"  -f  --file           firmware file name (.hex) format\n"
		"  -e  --extension	Vendor Specific Command extensions, eg A3 or A0 etc\n");

	exit(exit_code);
}
/***********************************************************************/

static int filename_provided;
static char *filename;
static unsigned char extension;

static void validate_inputs(void)
{
	if ( !filename_provided ) {
	   fprintf(stderr,"Please provide full path to firmware image file\n");
	   print_usage(stdout, 1);
	}   
}

static void dumpbuf(unsigned short, unsigned char *buf, int len)
{

}
	
int main(int argc, char **argv)
{
	int r;
	cyusb_handle *h = NULL;
	FILE *fp = NULL;
	struct stat statbuf;
	unsigned char reset = 0;
	char buf[256];
	int i;
	int count = 0;
	int fd;
	unsigned short address = 0;
	char tbuf1[3];
	char tbuf2[5];
	char tbuf3[3];
	unsigned char num_bytes = 0;
	unsigned char *dbuf;
	int linecount = 0;
	

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
			case 'f': /* -f or --file */
				  filename = optarg;
				  fp = fopen(filename,"r");
				  if ( !fp ) {
				     fprintf(stderr,"File not found!\n");
				     print_usage(stdout, 1);
				  }
				  filename_provided = 1;
				  fclose(fp);
				  break;
			case 'e': /* -e or --extension  */
				  extension = strtoul(optarg, NULL, 16);
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
	else if ( r > 1 ) {
		printf("Only 1 device supported in this example\n");
		return 0;
	}
	h = cyusb_gethandle(0);
	r = stat(filename, &statbuf);
	printf("File size = %d\n",(int)statbuf.st_size);
	r = cyusb_download_fx2(h, filename, extension);
	cyusb_close();
	close(fd);
	return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>

#include "../include/cyusb.h"
static unsigned long checksum = 0;

static cyusb_handle *h;

static void control_transfer(unsigned int address, unsigned char *dbuf, int len)
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
	      printf("Wrote %d bytes at %08x\n", b, address);
	      address += b ;
	      balance -= b;
	      index += b;
	}
	for ( j = 0; j < len/4; ++j )
	    checksum += pint[j];
}

int main(int argc, char **argv)
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
	int N;
	
	N = cyusb_open();
	if ( N < 0 ) {
	   printf("Error in opening library\n");
	   return -1;
	}
	else if ( N == 0 ) {
		printf("No device of interest found\n");
		return 0;
	}
	else  printf("No of devices of interest found = %d\n",N);

	h = cyusb_gethandle(0);
	if ( !h ) {
	   printf("Error obtaining handle\n");
	   return -1;
	}
	else printf("Successfully obtained handle\n");

	fd = open(argv[1], O_RDONLY);
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
	printf("Image type = %02x\n",buf[0]);
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
	   printf("Length = %d, Address = %08x\n",dlen, address);
	   if ( dlen != 0 ) {
	      nbr = read(fd, buf, dlen*4);	/* Read data bytes	*/
	      control_transfer(address, buf, dlen*4);
	   }
	   else {
	      program_entry = address;
	      printf("Program entry = %08x\n",program_entry);
	      break;
	   }
	}
	nbr = read(fd, buf, 4);			/* Read checksum	*/
	pdbuf = (unsigned int *)buf;
	if ( *pdbuf != checksum ) {
	   printf("Error in checksum\n");
	   return -5;
	}
	else printf("Valid checksum\n");

        /* Note: The control request to transfer control to the downloaded firmware fails on some host controllers,
         * because the FX3 disconnects from the host as soon as this request is processed. It is therefore okay
         * to ignore the error return from this request.
         */
	r = cyusb_control_transfer(h, 0x40, 0xA0, ( program_entry & 0x0000ffff ) , program_entry >> 16, NULL, 0, 1000);
	if ( r ) {
	   printf("Ignored error in control_transfer: %d\n", r);
	}
	close(fd);
	return 0;
}

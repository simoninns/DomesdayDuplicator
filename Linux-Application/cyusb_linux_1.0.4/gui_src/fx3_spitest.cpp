#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>

#include "../include/cyusb.h"

static cyusb_handle *h;

static unsigned char buf[512*1024];
static int filsiz;
static int fd;

static void control_transfer_read(int start, int len)
{
	int r = 0;
	int index = 0;
	unsigned short page_address = 0;
	int balance = len;

	while ( balance > 0 ) {
	      r = cyusb_control_transfer(h, 0xC0, 0xC3, 0, page_address, &buf[start+index], 256, 1000);
	      if ( r != 256 ) {
	   	   printf("Error in control_transfer\n");
	      }
	      write(fd, &buf[start+index], 256);
	      page_address += 1 ;
	      balance -= 256;
	      index += 256;
	}
}

static void control_transfer_write(int start, int len)
{
	int r = 0;
	int index = 0;
	unsigned short page_address = 0;
	int balance = len;
	unsigned char status;

	while ( balance > 0 ) {
	      r = cyusb_control_transfer(h, 0x40, 0xC2, 0x0, page_address, &buf[start+index], 256, 1000);
	      if ( r != 256 ) {
	   	   printf("Error in control_transfer\n");
	      }
	      page_address += 1 ;
	      balance -= 256;
	      index += 256;
	      
	      status = 1;
	      while ( cyusb_control_transfer(h, 0xC0, 0xC4, 0, 0, &status, 1, 1000 ) ) {
			if ( status == 0 ) break;
	      }
	}
}

static int perform_basic_validation(const char *filename)
{
	int nbr;
	int index;
	int quotient, reminder;

	fd = open(filename, O_RDONLY);
	if ( fd < 0 ) { 
	   printf("File not found\n");
	   return -1;
	}
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
	close(fd);
	
	fd = open(filename, O_RDONLY);
	filsiz = 0;
	index = 0;

	memset(buf,'\0',512 * 1024);

	while ( (nbr = read(fd, &buf[index], 4096)) ) {
		filsiz += nbr;
		index  += nbr;
	}
	printf("Actual size of file = %d\n", filsiz);
	quotient = filsiz / 256;
	reminder = filsiz % 256;
	if ( reminder ) 
	   filsiz = 256 * (quotient + 1);
	printf("Adjusted value of file size (multiple of 256) = %d\n",filsiz);
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
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

	if ( argc != 3 ) {
	   printf("Usage: fx3_spitest <file> <R|W\n");
	   printf("Image file for write, any other name to create for read\n");
	   return 0;
	}
	if ( argv[2][0] == 'W' ) {
	   if ( perform_basic_validation(argv[1]) ) {
	      printf("Basic file validation failed\n");
	      return -1;
	   }
	   control_transfer_write(0, filsiz);
	   printf("Completed writing into spi device\n");
	}
	else {
	   fd = open(argv[1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	   if ( fd < 0 ) {
	      printf("Error creating file\n");
	      return -1;
	   }
	   memset(buf,'\0',512 * 1024); 
	   printf("Enter num of bytes to read : ");
	   scanf("%d", &filsiz);
	   printf("Number of bytes to read = %d\n",filsiz);
	   control_transfer_read(0, filsiz);
	   printf("Completed reading from spi device\n");
	   close(fd);
	}
	cyusb_close();
	return 0;
}

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

int main(void)
{
	int num;
	char option[5];
	char filename[256];
	int fd;
	char value_buf[5];
	unsigned char hexval;
	char tbuf[5];
	int i;
	struct timeval tv;

	printf("This program will generate test data as per your specifications...\n");
	printf("It can generate FIXED data, RANDOM data or Auto-incrementing data.\n");
	printf("(C) V. Radhakrishnan, ATR-LABs (www.atr-labs.com) \n");

	printf("Enter number of bytes of data to be generated : ");
	scanf("%d", &num);

	printf("Enter (F) for fixed, or (v) for variable/random data, or (a) for auto-incrementing data : ");
	scanf("%s",option);

	if ( !  (  (option[0] == 'f') || (option[0] == 'F') 
		|| (option[0] == 'v') || (option[0] == 'V') 
		|| (option[0] == 'a') || (option[0] == 'A') ) ) {
	   printf("Invalid option entered. Enter only 'f', 'v' or 'a'\n");
	   return -1;
	}
	
	printf("Enter output file name : ");
	scanf("%s", filename);

	fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );

	if ( fd < 0 ) {
	   printf("Error %s. \n",strerror(errno));
	   return -1;
	}
	
	if ( (option[0] == 'f' || option[0] == 'F') ) {
	   printf("Enter 1-byte value ( in hex ) : ");
	   scanf("%s",value_buf);
	   hexval = strtoul(value_buf, NULL, 16);
	   sprintf(tbuf, "%c", hexval);
	   for ( i = 0; i < num; ++i )
		write(fd, tbuf, 1);
	} 
	else if ( (option[0] == 'v') || (option[0] == 'V') ) {
	   gettimeofday(&tv, NULL);
	   srandom((int)tv.tv_sec);
	   for ( i = 0; i < num; ++i ) {
		hexval = random() % 256;
	   	sprintf(tbuf, "%c", hexval);
		write(fd, tbuf, 1);
	   }
	}
	else {
		printf("Enter start value (in hex) : ");
		scanf("%s",value_buf);
		hexval = strtoul(value_buf, NULL, 16);
	   	for ( i = 0; i < num; ++i ) {
	   	    sprintf(tbuf, "%c", hexval);
		    write(fd, tbuf, 1);
		    ++hexval;
		}
	}
		
	close(fd);
}

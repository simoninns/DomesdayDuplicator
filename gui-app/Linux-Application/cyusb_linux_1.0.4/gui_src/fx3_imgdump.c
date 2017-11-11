#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	int fd;
	unsigned char buf[1000000];
	int nbr;
	int dlen;
	int j;
	int count;
	int *pdbuf = NULL;
	unsigned long checksum = 0;

	if ( argc != 2 ) {
	   printf("Usage: fx3_imgdump <filename>\n");
	   return -1;
	}
	fd = open(argv[1], O_RDONLY);
	if ( fd < 0 ) {
	   printf("File %s not found\n", argv[1]);
	   return -2;
	}
	count = 0;
	nbr = read(fd, buf, 2);
	printf("Signature at beginning of file = %c%c\n",buf[0],buf[1]);
	nbr = read(fd, buf, 1);
	printf("ImageCTL = %02x\n",buf[0]);
	if ( buf[0] & 0x01 ) 
	   printf("File contains pure data ( non-executable )\n");
	else printf("File contains executable content\n");
	nbr = read(fd, buf, 1);
	printf("Image type = %02x\n",buf[0]);
	if ( buf[0] == 0xB0 )
	   printf("Normal FW binary image with checksum\n");
	else if ( buf[0] == 0xB1 )
	  	printf("Security Image\n");
	else if ( buf[0] == 0xB2 )
		printf("SPI Boot with new VID/PID\n");
	else printf("Unknown Image Type\n");
	if ( buf[0] == 0xB2 ) {
	   nbr = read(fd, buf, 4);
	   printf("PID/VID = %08x\n",buf);
	}
	if ( !(buf[0] == 0xB0) ) {
	   close(fd);
	   printf("Over\n");
	   return 0;
	}
	while (1) {
	   nbr = read(fd, buf, 4);
	   pdbuf = (unsigned int *)buf;
	   dlen = *pdbuf;
	   printf("Length of section %d in 32-bit words = %d : ",++count,dlen);
	   nbr = read(fd,buf,4);
	   if ( dlen != 0 ) {
	      pdbuf = (unsigned int *)buf;
	      printf("Address = %08x : ",*pdbuf);
	      nbr = read(fd, buf, dlen*4);
	      if ( dlen > 1024 ) 
		 printf("Larger than 4096 bytes !!\n");
	      pdbuf = (unsigned int *)buf;
	      for ( j = 0; j < dlen; ++j ) {
//		   printf("%08x ",pdbuf[j]);
		   checksum += pdbuf[j];
	      }
	      printf("\n");
	   }
	   else {
	      printf("Program Entry = %08x\n",*((unsigned int *)(buf)));
	      break;
	   }
	}
	nbr = read(fd, buf, 4);
	pdbuf = (unsigned int *)buf;
	printf("Checksum in image file = %08x\n",*pdbuf);	  
	printf("Checksum as computed   = %08x\n",checksum);
	close(fd);
	return 0;
}

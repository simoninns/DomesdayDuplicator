#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "../include/cyusb.h"

int main(int argc, char **argv)
{
	int r;
	int config = 0;
	cyusb_handle *h = NULL;
	char tbuf[60];

	struct libusb_config_descriptor *desc = NULL;

	r = cyusb_open();
	if ( r < 0 ) {
	      printf("Error opening library\n");
	      return -1;
	}
	else if ( r == 0 ) {
		   printf("No device found\n");
		   return 0;
	}
	h = cyusb_gethandle(0);
	r = cyusb_get_configuration(h,&config); 
	if ( r ) {
	   cyusb_error(r);
	   cyusb_close();
	   return r;
	}
	if ( config == 0 ) 
	   printf("The device is currently unconfigured\n");
	else printf("Device configured. Current configuration = %d\n", config);

	r = cyusb_get_active_config_descriptor(h, &desc);
	if ( r ) {
	   printf("Error retrieving config descriptor\n");
	   return r;
	}
	sprintf(tbuf,"bLength             = %d\n",   desc->bLength);
	printf("%s",tbuf);
	sprintf(tbuf,"bDescriptorType     = %d\n",   desc->bDescriptorType);
	printf("%s",tbuf);
	sprintf(tbuf,"TotalLength         = %d\n",   desc->wTotalLength);
	printf("%s",tbuf);
	sprintf(tbuf,"Num. of interfaces  = %d\n",   desc->bNumInterfaces);
	printf("%s",tbuf);
	sprintf(tbuf,"bConfigurationValue = %d\n",   desc->bConfigurationValue);
	printf("%s",tbuf);
	sprintf(tbuf,"iConfiguration     = %d\n",    desc->iConfiguration);
	printf("%s",tbuf);
	sprintf(tbuf,"bmAttributes       = %d\n",    desc->bmAttributes);
	printf("%s",tbuf);
	sprintf(tbuf,"Max Power          = %04d\n",  desc->MaxPower);
	printf("%s",tbuf);

	cyusb_close();
	return 0;
}

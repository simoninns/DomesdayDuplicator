#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "../include/cyusb.h"


static struct libusb_transfer *transfer = NULL;
uint8_t ep_in, ep_out;
cyusb_handle *h1 = NULL;
static unsigned char *isoc_databuf = NULL;
int	num_pkts;
unsigned short gl_ep_out, gl_ep_in;

/*
    This is the call back function called by libusb upon completion 
    of transfer.
*/
static void out_callback( struct libusb_transfer *transfer)
{
    int act_len;
    static int total_in, pkts_success, pkts_failure;
    unsigned char *ptr;
    int pkts_out;

    //Transfer complete call back got.Check if transfer succeeded
    if ( transfer->status != LIBUSB_TRANSFER_COMPLETED ) 
    {
        printf ("Transfer not completed \n");
    }

    total_in = pkts_success = pkts_failure = 0;
    pkts_out = num_pkts;

    //Loop through all the packets and check the status of each 
    //packet transfer
    for ( int i = 0; i < pkts_out; ++i ) {
        ptr = libusb_get_iso_packet_buffer_simple(transfer, i);
        act_len = transfer->iso_packet_desc[i].actual_length;
        total_in += 1;
        if ( transfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED ) {
            pkts_success += 1;
        }
        else pkts_failure += 1;
    }
    //Info on how many packets succeeded and failed
    printf("\n***************************\n Total Packets sent: %d \n", total_in);
    printf(" Pkts Succedded    : %d \n", pkts_success);
    printf(" Pkts Failed       : %d \n****************************\n", pkts_failure);

}
//This function selects the alternate setting of interface
//which has ISO out
const libusb_interface_descriptor * selectInterface (struct libusb_config_descriptor *configDesc)
{
    const struct libusb_interface_descriptor *interfaceDesc;
    int numAltsetting, altsetting = 0;
    const struct libusb_endpoint_descriptor *endpoint;

    interfaceDesc = configDesc->interface->altsetting;    
    numAltsetting = configDesc->interface->num_altsetting;
    endpoint = interfaceDesc->endpoint;

    if (numAltsetting == 1)
        return interfaceDesc;

    while (numAltsetting){
        if (endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_ISOCHRONOUS){
            if (!(endpoint->bEndpointAddress & 0x80)){
                cyusb_set_interface_alt_setting (h1,0, altsetting); 
                return interfaceDesc;
            }
        }
        numAltsetting--;
        altsetting++;
        interfaceDesc = (interfaceDesc + 1);
        endpoint = interfaceDesc->endpoint;
    }
    return NULL;
}

int main(int argc, char **argv)

{
    int rStatus, pktsize_out, bufsize_out;
    libusb_config_descriptor *configDesc;
    int num_bytes, numEndpoints, ep_index_out = 0, ep_index_in = 0;
    unsigned short int temp, ep_in[8], ep_out[8];
    const struct libusb_interface_descriptor *interfaceDesc ;
    const struct libusb_endpoint_descriptor *endpoint;
    struct timeval tv;

    if (argc != 2){
        printf ("Usage of binary ./isoread_test <num packets> \n Ex: ./isoread_test 64 \n");
        return -1;
    }

    num_pkts = atoi (argv[1]);

    // Opens the libusb library and does all the
    // Initialization.

    rStatus = cyusb_open();
    if ( rStatus < 0 ) {
        printf("Error opening library\n");
        return -1;
    }
    else if ( rStatus == 0 ) {
        printf("No device found\n");
        cyusb_close();
        return 0;
    }

    //Get the device handle
    h1 = cyusb_gethandle(0);
    if (h1 == NULL){
        printf (" handle is null \n");
        cyusb_close();
        return -1;
    }

    rStatus = cyusb_get_config_descriptor (h1, 0, &configDesc);
    if (rStatus != 0){
        printf ("Could not get Config descriptor \n");
        cyusb_close();
        return -1;
    }
    
    rStatus = cyusb_claim_interface(h1, 0);
    if ( rStatus != 0 ) {
        printf("Error in claiming interface\n");
        cyusb_close();
        return 0;
    }

    //Finding the proper interface
    
    interfaceDesc = selectInterface (configDesc);
    if (interfaceDesc == NULL){
        printf ("Device does not have any interface with IN endpoint..Cannot do ISO Read \n");
        cyusb_free_config_descriptor (configDesc);
        cyusb_close();
    }
    
    numEndpoints = interfaceDesc->bNumEndpoints;
    endpoint = interfaceDesc->endpoint;

    //Finding one out endpoint in the interface
    
    while (numEndpoints){
        printf (" %x \n", endpoint->bEndpointAddress);
        if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN){
            ep_in [ep_index_in] = endpoint->bEndpointAddress; 
            ep_index_in++;
        }
        else{
            ep_out [ep_index_out] = endpoint->bEndpointAddress;
            ep_index_out++;
        }
        numEndpoints --;
        endpoint = ((endpoint) + 1);
    }
    //Choosing 1 endpoint for read and one for write
   
    if (ep_out[0] == 0){
        printf ("No OUT endpoint in the device ... Cannot do iso write\n");
        cyusb_free_config_descriptor (configDesc);
        cyusb_close();
        return -1;    
    }

    gl_ep_in = ep_in [0];
    gl_ep_out = ep_out [0];

    transfer = libusb_alloc_transfer(num_pkts);
    if (transfer == NULL){
        printf ("Error in Transfer allocation \n");
        cyusb_free_config_descriptor (configDesc);
        cyusb_close();
        return -1;
    }

    // This section of the code fills the ISO URB and 
    // submits the transfer.
    
    pktsize_out = cyusb_get_max_iso_packet_size(h1, gl_ep_out);
    printf ("\nPacket size is %d \n", pktsize_out);
    bufsize_out = pktsize_out * num_pkts;
    isoc_databuf = (unsigned char *)(malloc(bufsize_out));
    libusb_fill_iso_transfer(transfer, h1, gl_ep_out, isoc_databuf, bufsize_out, num_pkts, out_callback, NULL, 10000 );
    libusb_set_iso_packet_lengths(transfer, pktsize_out);
    transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_submit_transfer(transfer);

    //This section of code waits till all the transfer 
    //have been completed : 
    //Refer to www.libusb.org for further details of event handling 
    //functions

    tv.tv_sec = 0;
    tv.tv_usec = 500;
    for ( int i = 0; i < 1000; ++i ) {
        libusb_handle_events_timeout(NULL,&tv);
    }

    cyusb_close();
    return 0;
}

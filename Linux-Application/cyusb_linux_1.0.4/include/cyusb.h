#ifndef __CYUSB_H
#define __CYUSB_H

/*********************************************************************************\
 * This is the main header file for the cyusb suite for Linux/Mac, called cyusb.h *
 *                                                                                *
 * Author              :        V. Radhakrishnan ( rk@atr-labs.com )              *
 * License             :        GPL Ver 2.0                                       *
 * Copyright           :        Cypress Semiconductors Inc. / ATR-LABS            *
 * Date written        :        March 12, 2012                                    *
 * Modification Notes  :                                                          *
 *    1. Cypress Semiconductor, January 23, 2013                                  *
 *       Added function documentation.                                            *
 *       Added new constant to specify number of device ID entries.               *
 *                                                                                *
 \********************************************************************************/

#include <libusb-1.0/libusb.h>
#include <linux/types.h>

typedef struct libusb_device               cyusb_device;
typedef struct libusb_device_handle        cyusb_handle;

/* This is the maximum number of 'devices of interest' we are willing to store as default. */
/* These are the maximum number of devices we will communicate with simultaneously */
#define MAXDEVICES        10

/* This is the maximum number of VID/PID pairs that this library will consider. This limits
   the number of valid VID/PID entries in the configuration file.
 */
#define MAX_ID_PAIRS    100

/* This is the maximum length for the description string for a device in the configuration
   file. If the actual string in the file is longer, only the first MAX_STR_LEN characters
   will be considered.
 */
#define MAX_STR_LEN     30

struct cydev {
    cyusb_device *dev;          /* as above ... */
    cyusb_handle *handle;       /* as above ... */
    unsigned short vid;         /* Vendor ID */
    unsigned short pid;         /* Product ID */
    unsigned char is_open;      /* When device is opened, val = 1 */
    unsigned char busnum;       /* The bus number of this device */
    unsigned char devaddr;      /* The device address*/
    unsigned char filler;       /* Padding to make struct = 16 bytes */
};

/* Function prototypes */

/*******************************************************************************************
  Prototype    : int cyusb_error(int err);
  Description  : Print out a verbose message corresponding to an error code, to the stderr
                 stream.
  Parameters   :
                 int err : Error code
  Return Value : none
 *******************************************************************************************/
extern void cyusb_error(int err);

/*******************************************************************************************
  Prototype    : int cyusb_open(void);
  Description  : This initializes the underlying libusb library, populates the cydev[]
                 array, and returns the number of devices of interest detected. A
                 'device of interest' is a device which appears in the /etc/cyusb.conf file.
  Parameters   : None
  Return Value : Returns an integer, equal to number of devices of interest detected.
 *******************************************************************************************/
extern int cyusb_open(void);

/*******************************************************************************************
  Prototype    : int cyusb_open(unsigned short vid, unsigned short pid);
  Description  : This is an overloaded function that populates the cydev[] array with
                 just one device that matches the provided vendor ID and Product ID.
                 This function is only useful if you know in advance that there is only
                 one device with the given VID and PID attached to the host system.
  Parameters   :
                 unsigned short vid : Vendor ID
                 unsigned short pid : Product ID
  Return Value : Returns 1 if a device of interest exists, else returns 0.
 *******************************************************************************************/
extern int cyusb_open(unsigned short vid, unsigned short pid);

/*******************************************************************************************
  Prototype    : cyusb_handle * cyusb_gethandle(int index);
  Description  : This function returns a libusb_device_handle given an index from the cydev[] array.
  Parameters   :
                 int index : Equal to the index in the cydev[] array that gets populated
                             during the cyusb_open() call described above.
  Return Value : Returns the pointer to a struct of type cyusb_handle, also called as
                 libusb_device_handle.
 *******************************************************************************************/
extern cyusb_handle * cyusb_gethandle(int index);

/*******************************************************************************************
  Prototype    : unsigned short cyusb_getvendor(cyusb_handle *);
  Description  : This function returns a 16-bit value corresponding to the vendor ID given
                 a device's handle.
  Parameters   :
                 cyusb_handle *handle : Pointer to a struct of type cyusb_handle.
  Return Value : Returns the 16-bit unique vendor ID of the given device.
 *******************************************************************************************/
extern unsigned short cyusb_getvendor(cyusb_handle *);

/*******************************************************************************************
  Prototype    : unsigned short cyusb_getproduct(cyusb_handle *);
  Description  : This function returns a 16-bit value corresponding to the device ID given
                 a device's handle.
  Parameters   :
                 cyusb_handle *handle : Pointer to a struct of type cyusb_handle.
  Return Value : Returns the 16-bit product ID of the given device.
 *******************************************************************************************/
extern unsigned short cyusb_getproduct(cyusb_handle *);

/*******************************************************************************************
  Prototype    : void cyusb_close(void);
  Description  : This function closes the libusb library and releases memory allocated to cydev[].
  Parameters   : none.
  Return Value : none.
 *******************************************************************************************/
extern void cyusb_close(void);

/*******************************************************************************************
  Prototype    : int cyusb_get_busnumber(cyusb_handle * handle);
  Description  : This function returns the Bus Number pertaining to a given device handle.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
  Return Value : An integer value corresponding to the Bus Number on which the device resides.
                 This is also the same value present in the cydev[] array.
 *******************************************************************************************/
extern int cyusb_get_busnumber(cyusb_handle *);

/*******************************************************************************************
  Prototype    : int cyusb_get_devaddr(cyusb_handle * handle);
  Description  : This function returns the device address pertaining to a given device handle
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
  Return Value : An integer value corresponding to the device address (between 1 and 127).
                 This is also the same value present in the cydev[] array.
 *******************************************************************************************/
extern int cyusb_get_devaddr(cyusb_handle *);

/*******************************************************************************************
  Prototype     : int cyusb_get_max_packet_size(cyusb_handle * handle,unsigned char endpoint);
  Description   : This function returns the max packet size that an endpoint can handle, without
                  taking into account high-bandwidth capabiity. It is therefore only useful
                  for Bulk, not Isochronous endpoints.
  Parameters    :
                  cyusb_handle *handle   : The libusb device handle
                  unsigned char endpoint : The endpoint number
  Return Value  : Max packet size in bytes for the endpoint.
 *******************************************************************************************/
extern int cyusb_get_max_packet_size(cyusb_handle *, unsigned char endpoint);

/*******************************************************************************************
  Prototype    : int cyusb_get_max_iso_packet_size(cyusb_handle * handle,unsigned char endpoint);
  Description  : This function returns the max packet size that an isochronous endpoint can
                 handle, after considering multiple transactions per microframe if present.
  Parameters   :
                 cyusb_handle *handle   : The libusb device handle
                 unsigned char endpoint : The endpoint number
  Return Value : Maximum amount of data that an isochronous endpoint can transfer per
                 microframe.
 *******************************************************************************************/
extern int cyusb_get_max_iso_packet_size(cyusb_handle *, unsigned char endpoint);

/*******************************************************************************************
  Prototype    : int cyusb_get_configuration(cyusb_handle * handle,int *config);
  Description  : This function determines the bConfiguration value of the active configuration.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int * config         : Address of an integer variable that will store the
                                        currently active configuration number.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *******************************************************************************************/
extern int cyusb_get_configuration(cyusb_handle *, int *config);

/*******************************************************************************************
  Prototype    : int cyusb_set_configuration(cyusb_handle * handle,int config);
  Description  : This function sets the device's active configuration (standard request).
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int config           : Configuration number required to be made active.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *******************************************************************************************/
extern int cyusb_set_configuration(cyusb_handle *, int config);

/*******************************************************************************************
  Prototype    : int cyusb_claim_interface(cyusb_handle * handle,int interface);
  Description  : This function claims an interface for a given device handle.
                 You must claim an interface before performing I/O operations on the device.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int interface        : The bInterfaceNumber of the interface you wish to claim.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *******************************************************************************************/
extern int cyusb_claim_interface(cyusb_handle *, int interface);

/*********************************************************************************************
  Prototype    : int cyusb_release_interface(cyusb_handle * handle,int interface);
  Description  : This function releases an interface previously claimed for a given device handle.
                 You must release all claimed interfaces before closing a device handle.
                 This is a blocking funcion, where a standard SET_INTERFACE control request is
                 sent to the device, resetting interface state to the first alternate setting.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int interface        : The bInterfaceNumber of the interface you wish to release
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *********************************************************************************************/
extern int cyusb_release_interface(cyusb_handle *, int interface);

/*******************************************************************************************
  Prototype    : int cyusb_set_interface_alt_setting(cyusb_handle * handle,
                     int interface,int altsetting);
  Description  : This function activates an alternate setting for an interface.
                 The interface itself must have been previously claimed using
                 cyusb_claim_interface. This is a blocking funcion, where a standard
                 control request is sent to the device.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int  interface       : The bInterfaceNumber of the interface you wish to set,
                 int altsetting       : The bAlternateSetting number to activate
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *****************************************************************************************/
extern int cyusb_set_interface_alt_setting(cyusb_handle *, int interface, int altsetting);

/*******************************************************************************************
  Prototype    : int cyusb_clear_halt(cyusb_handle * handle, unsigned char endpoint);
  Description  : This function clears a halt condition on an endpoint.
                 Endpoints with a halt condition are unable to send/receive data unless
                 the condition is specifically cleared by the Host.
                 This is a blocking funcion.
  Parameters   :
                 cyusb_handle *handle   : The libusb device handle
                 unsigned char endpoint : The endpoint for which the clear request is sent.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *****************************************************************************************/
extern int cyusb_clear_halt(cyusb_handle *, unsigned char endpoint);

/******************************************************************************************
  Prototype    : int cyusb_reset_device(cyusb_handle * handle);
  Description  : This function performs a USB port reset to the device.
                 This is a blocking funcion.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR
 *****************************************************************************************/
extern int cyusb_reset_device(cyusb_handle *);

/******************************************************************************************
  Prototype    : int cyusb_kernel_driver_active(cyusb_handle * handle, int interface);
  Description  : This function returns whether a kernel driver has already claimed an
                 interface. If a kernel driver is active and has claimed an interface,
                 cyusb cannot perform I/O operations on that interface unless the interface
                 is first released.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int interface        : The interface which you are testing.
  Return Value : 0 if no kernel driver is active, 1 if a kernel driver is active;
                 LIBUSB error code in case of error.
 *****************************************************************************************/
extern int cyusb_kernel_driver_active(cyusb_handle *, int interface);

/******************************************************************************************
  Prototype    : int cyusb_detach_kernel_driver(cyusb_handle * handle, int interface);
  Description  : This function detaches a kernel mode driver in order for cyusb to claim
                 the interface. If a kernel driver is active and has claimed an interface,
                 cyusb cannot perform I/O operations on that interface unless the interface
                 is first released.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int interface        : The interface which you want to be detached.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 *****************************************************************************************/
extern int cyusb_detach_kernel_driver(cyusb_handle *, int interface);

/******************************************************************************************
  Prototype    : int cyusb_attach_kernel_driver(cyusb_handle * handle, int interface);
  Description  : This function reattaches a kernel mode driver which was previously detached.
  Parameters   :
                 cyusb_handle *handle : The libusb device handle
                 int interface        : The interface which you want to be reattached.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 *****************************************************************************************/
extern int cyusb_attach_kernel_driver(cyusb_handle *, int interface);

/******************************************************************************************
  Prototype    : int cyusb_get_device_descriptot(cyusb_handle * handle,
                     struct libusb_device_descriptor *);
  Description  : This function returns the usb device descriptor for the given device.
  Parameters   :
                 cyusb_handle *handle                  : The libusb device handle
                 struct libusb_device_descriptor *desc : Address of a device_desc structure
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 *****************************************************************************************/
extern int cyusb_get_device_descriptor(cyusb_handle *, struct libusb_device_descriptor *desc);

/******************************************************************************************
  Prototype    : int cyusb_get_active_config_descriptor(cyusb_handle * handle,
                     struct libusb_config_descriptor **);
  Description  : This function returns the usb configuration descriptor for the given device.
                 The descriptor structure must be freed with cyusb_free_config_descriptor()
                 explained below.
  Parameters   :
                 cyusb_handle *handle                          : The libusb device handle
                 struct libusb_configuration_descriptor **desc : Address of a config_descriptor
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ******************************************************************************************/
extern int cyusb_get_active_config_descriptor(cyusb_handle *, struct libusb_config_descriptor **config);

/*****************************************************************************************
  Prototype    : int cyusb_get_config_descriptor(cyusb_handle * handle, unsigned char index,
                     struct libusb_config_descriptor **);
  Description  : This function returns the usb configuration descriptor with the specified
                 index for the given device. The descriptor structure must be freed using
                 the cyusb_free_config_descriptor() call later.
  Parameters   :
                 cyusb_handle *handle                          : The libusb device handle
                 unsigned char index                           : Index of configuration you wish to retrieve.
                 struct libusb_configuration_descriptor **desc : Address of a config_descriptor
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 *****************************************************************************************/
extern int cyusb_get_config_descriptor(cyusb_handle *, unsigned char index, struct libusb_config_descriptor **config);

/*****************************************************************************************
  Prototype    : void cyusb_free_config_descriptor(struct libusb_config_descriptor *);
  Description  : Frees the configuration descriptor obtained earlier.
  Parameters   :
                 struct libusb_config_descriptor *config : The config descriptor you wish to free.
  Return Value : NIL.
 *****************************************************************************************/
extern void cyusb_free_config_descriptor(struct libusb_config_descriptor *config);

/*****************************************************************************************
  Prototype    : void cyusb_control_transfer(cyusb_handle *h, unsigned char bmRequestType,
                     unsigned char bRequest, unsigned short wValue, unsigned short wIndex,
                     unsigned char *data, unsigned short wLength, unsigned int timeout);
  Description  : Performs a USB Control Transfer. Please note that this is a generic transfer
                 function which can be used for READ (IN) and WRITE (OUT) data transfers, as
                 well as transfers with no data. The direction bit (MSB) in the bmRequestType
                 parameter should be set in the case of READ requests, and cleared in the case
                 of WRITE requests.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 unsigned char bmRequestType  : The request type field for the setup packet
                 unsigned char bRequest       : The request field of the setup packet
                 unsigned short wValue        : The value field of the setup packet
                 unsigned short wIndex        : The index field of the setup packet
                 unsigned char *data          : Data Buffer ( for input or output )
                 unsigned short wLength       : The length field of the setup packet.
                 unsigned int timeout         : Timeout in milliseconds. 0 means no Timeout.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_control_transfer(cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout);

/*****************************************************************************************
  Prototype    : void cyusb_control_read(cyusb_handle *h, unsigned char bmRequestType,
                     unsigned char bRequest, unsigned short wValue, unsigned short wIndex,
                     unsigned char *data, unsigned short wLength, unsigned int timeout);
  Description  : Performs a USB control transfer including a READ (IN) data phase. Please
                 note that it is not advisable to use this function with wLength=0, because
                 most USB hosts/devices do not handle this case properly.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 unsigned char bmRequestType  : The request type field for the setup packet
                 unsigned char bRequest       : The request field of the setup packet
                 unsigned short wValue        : The value field of the setup packet
                 unsigned short wIndex        : The index field of the setup packet
                 unsigned char *data          : Data Buffer ( for input or output )
                 unsigned short wLength       : The length field of the setup packet.
                 unsigned int timeout         : Timeout in milliseconds. 0 means no Timeout.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_control_read(cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout);

/*****************************************************************************************
  Prototype    : void cyusb_control_write(cyusb_handle *h, unsigned char bmRequestType,
                     unsigned char bRequest, unsigned short wValue, unsigned short wIndex,
                     unsigned char *data, unsigned short wLength, unsigned int timeout);
  Description  : Performs a USB control transfer including a WRITE (OUT) data phase.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 unsigned char bmRequestType  : The request type field for the setup packet
                 unsigned char bRequest       : The request field of the setup packet
                 unsigned short wValue        : The value field of the setup packet
                 unsigned short wIndex        : The index field of the setup packet
                 unsigned char *data          : Data Buffer ( for input or output )
                 unsigned short wLength       : The length field of the setup packet.
                 unsigned int timeout         : Timeout in milliseconds. 0 means no Timeout.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_control_write(cyusb_handle *h, unsigned char bmRequestType, unsigned char bRequest,
        unsigned short wValue, unsigned short wIndex, unsigned char *data, unsigned short wLength,
        unsigned int timeout);

/****************************************************************************************
  Prototype    : void cyusb_bulk_transfer(cyusb_handle *h, unsigned char endpoint,
                     unsigned char *data, int length, int *transferred, int timeout);
  Description  : Performs a USB Bulk Transfer.
  Parameters   :
                 cyusb_handle *h        : Device handle
                 unsigned char endpoint : Address of endpoint to comunicate with
                 unsigned char *data    : Data Buffer ( for input or output )
                 unsigned short wLength : The length field of the data buffer for read or write
                 int * transferred      : Output location of bytes actually transferred
                 unsigned int timeout   : Timeout in milliseconds. 0 means no Timeout.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_bulk_transfer(cyusb_handle *h, unsigned char endpoint, unsigned char *data,
        int length, int *transferred, int timeout);

/****************************************************************************************
  Prototype    : void cyusb_interrupt_transfer(cyusb_handle *h, unsigned char endpoint,
                     unsigned char *data, int length, int *transferred, int timeout);
  Description  : Performs a USB Interrupt Transfer.
  Parameters   :
                 cyusb_handle *h        : Device handle
                 unsigned char endpoint : Address of endpoint to comunicate with
                 unsigned char *data    : Data Buffer ( for input or output )
                 unsigned short wLength : The length field of the data buffer for read or write
                 int * transferred      : Output location of bytes actually transferred
                 unsigned int timeout   : Timeout in milliseconds. 0 means no Timeout.
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_interrupt_transfer(cyusb_handle *h, unsigned char endpoint, unsigned char *data,
        int length, int *transferred, unsigned int timeout);

/****************************************************************************************
  Prototype    : void cyusb_download_fx2(cyusb_handle *h, char *filename,
                     unsigned char vendor_command);
  Description  : Performs firmware download on FX2.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 char * filename              : Path where the firmware file is stored
                 unsigned char vendor_command : Vendor command that needs to be passed during download
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
extern int cyusb_download_fx2(cyusb_handle *h, char *filename, unsigned char vendor_command);


/****************************************************************************************
  Prototype    : void cyusb_download_fx3(cyusb_handle *h, char *filename);
  Description  : Performs firmware download on FX3.
  Parameters   :
                 cyusb_handle *h : Device handle
                 char *filename  : Path where the firmware file is stored
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ***************************************************************************************/
extern int cyusb_download_fx3(cyusb_handle *h, char *filename);

#endif

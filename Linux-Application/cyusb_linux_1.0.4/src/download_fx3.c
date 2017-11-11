/*
 * Filename             : download_fx3.cpp
 * Description          : Downloads FX3 firmware to RAM, I2C EEPROM or SPI Flash.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <libusb-1.0/libusb.h>
#include "../include/cyusb.h"

#define FLASHPROG_VID		(0x04b4)	// USB VID for the FX3 flash programmer.

#define MAX_FWIMG_SIZE		(512 * 1024)	// Maximum size of the firmware binary.
#define MAX_WRITE_SIZE		(2 * 1024)	// Max. size of data that can be written through one vendor command.

#define I2C_PAGE_SIZE		(64)		// Page size for I2C EEPROM.
#define I2C_SLAVE_SIZE		(64 * 1024)	// Max. size of data that can fit on one EEPROM address.

#define SPI_PAGE_SIZE		(256)		// Page size for SPI flash memory.
#define SPI_SECTOR_SIZE		(64 * 1024)	// Sector size for SPI flash memory.

#define VENDORCMD_TIMEOUT	(5000)		// Timeout (in milliseconds) for each vendor command.
#define GETHANDLE_TIMEOUT	(5)		// Timeout (in seconds) for getting a FX3 flash programmer handle.

/* Utility macros. */
#define ROUND_UP(n,v)	((((n) + ((v) - 1)) / (v)) * (v))	// Round n upto a multiple of v.
#define GET_LSW(v)	((unsigned short)((v) & 0xFFFF))	// Get Least Significant Word part of an integer.
#define GET_MSW(v)	((unsigned short)((v) >> 16))		// Get Most Significant Word part of an integer.

/* Enumeration representing the FX3 firmware target. */
typedef enum {
	FW_TARGET_NONE = 0,	// Invalid value
	FW_TARGET_RAM,		// Program to device RAM
	FW_TARGET_I2C,		// Program to I2C EEPROM
	FW_TARGET_SPI		// Program to SPI Flash
} fx3_fw_target;

/* Array representing physical size of EEPROM corresponding to each size encoding. */
const int i2c_eeprom_size[] =
{
	1024,		// bImageCtl[2:0] = 'b000
	2048,		// bImageCtl[2:0] = 'b001
	4096,		// bImageCtl[2:0] = 'b010
	8192,		// bImageCtl[2:0] = 'b011
	16384,		// bImageCtl[2:0] = 'b100
	32768,		// bImageCtl[2:0] = 'b101
	65536,		// bImageCtl[2:0] = 'b110
	131072		// bImageCtl[2:0] = 'b111
};

static int
fx3_ram_write (
		cyusb_handle  *h,
		unsigned char *buf,
		unsigned int   ramAddress,
		int            len)
{
	int r;
	int index = 0;
	int size;

	while (len > 0) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer (h, 0x40, 0xA0, GET_LSW(ramAddress), GET_MSW(ramAddress),
				&buf[index], size, VENDORCMD_TIMEOUT);
		if (r != size) {
			fprintf (stderr, "Error: Vendor write to FX3 RAM failed\n");
			return -1;
		}

		ramAddress += size;
		index      += size;
		len        -= size;
	}

	return 0;
}

/* Read the firmware image from the file into a buffer. */
static int
read_firmware_image (
		const char    *filename,
		unsigned char *buf,
		int           *romsize,
		int           *filesize)
{
	int fd;
	int nbr;
	struct stat filestat;

	if (stat (filename, &filestat) != 0) {
		fprintf (stderr, "Error: Failed to stat file %s\n", filename);
		return -1;
	}

	// Verify that the file size does not exceed our limits.
	*filesize = filestat.st_size;
	if (*filesize > MAX_FWIMG_SIZE) {
		fprintf (stderr, "Error: File size exceeds maximum firmware image size\n");
		return -2;
	}

	fd = open (filename, O_RDONLY);
	if (fd < 0) { 
		fprintf (stderr, "Error: File not found\n");
		return -3;
	}
	nbr = read (fd, buf, 2);		/* Read first 2 bytes, must be equal to 'CY'	*/
	if (strncmp ((char *)buf,"CY",2)) {
		fprintf (stderr, "Error: Image does not have 'CY' at start.\n");
		return -4;
	}
	nbr = read (fd, buf, 1);		/* Read 1 byte. bImageCTL	*/
	if (buf[0] & 0x01) {
		fprintf (stderr, "Error: Image does not contain executable code\n");
		return -5;
	}
	if (romsize != 0)
		*romsize = i2c_eeprom_size[(buf[0] >> 1) & 0x07];

	nbr = read (fd, buf, 1);		/* Read 1 byte. bImageType	*/
	if (!(buf[0] == 0xB0)) {
		fprintf (stderr, "Error: Not a normal FW binary with checksum\n");
		return -6;
	}

	// Read the complete firmware binary into a local buffer.
	lseek (fd, 0, SEEK_SET);
	nbr = read (fd, buf, *filesize);

	close (fd);
	return 0;
}

static int
fx3_usbboot_download (
		cyusb_handle *h,
		const char   *filename)
{
	unsigned char *fwBuf;
	unsigned int  *data_p;
	unsigned int i, checksum;
	unsigned int address, length;
	int r, index, filesize;

	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);
	if (fwBuf == 0) {
		fprintf (stderr, "Error: Failed to allocate buffer to store firmware binary\n");
		return -1;
	}

	// Read the firmware image into the local RAM buffer.
	r = read_firmware_image (filename, fwBuf, NULL, &filesize);
	if (r != 0) {
		fprintf (stderr, "Error: Failed to read firmware file %s\n", filename);
		free (fwBuf);
		return -2;
	}

	// Run through each section of code, and use vendor commands to download them to RAM.
	index    = 4;
	checksum = 0;
	while (index < filesize) {
		data_p  = (unsigned int *)(fwBuf + index);
		length  = data_p[0];
		address = data_p[1];
		if (length != 0) {
			for (i = 0; i < length; i++)
				checksum += data_p[2 + i];
			r = fx3_ram_write (h, fwBuf + index + 8, address, length * 4);
			if (r != 0) {
				fprintf (stderr, "Error: Failed to download data to FX3 RAM\n");
				free (fwBuf);
				return -3;
			}
		} else {
			if (checksum != data_p[2]) {
				fprintf (stderr, "Error: Checksum error in firmware binary\n");
				free (fwBuf);
				return -4;
			}

			r = cyusb_control_transfer (h, 0x40, 0xA0, GET_LSW(address), GET_MSW(address), NULL,
					0, VENDORCMD_TIMEOUT);
			if (r != 0)
				printf ("Info: Ignored error in control transfer: %d\n", r);
			break;
		}

		index += (8 + length * 4);
	}

	free (fwBuf);
	return 0;
}

/* Check if the current device handle corresponds to the FX3 flash programmer. */
static int check_fx3_flashprog (cyusb_handle *handle)
{
	int r;
	char local[8];

	r = cyusb_control_transfer (handle, 0xC0, 0xB0, 0, 0, (unsigned char *)local, 8, VENDORCMD_TIMEOUT);
	if ((r != 8) || (strncasecmp (local, "FX3PROG", 7) != 0)) {
		printf ("Info: Current device is not the FX3 flash programmer\n");
		return -1;
	}
	
	printf ("Info: Found FX3 flash programmer\n");
	return 0;
}

/* Get the handle to the FX3 flash programmer device, if found. */
static int
get_fx3_prog_handle (
		cyusb_handle **h)
{
	char *progfile_p, *tmp;
	cyusb_handle *handle;
	int i, j, r;
	struct stat filestat;

	handle = *h;
	r = check_fx3_flashprog (handle);
	if (r == 0)
		return 0;

	printf ("Info: Trying to download flash programmer to RAM\n");

	tmp = getenv ("CYUSB_ROOT");
	if (tmp != NULL) {
		i = strlen (tmp);
		progfile_p = (char *)malloc (i + 32);
		strcpy (progfile_p, tmp);
		strcat (progfile_p, "/fx3_images/cyfxflashprog.img");
	}
	else {
		progfile_p = (char *)malloc (32);
		strcpy (progfile_p, "fx3_images/cyfxflashprog.img");
	}

	r = stat (progfile_p, &filestat);
	if (r != 0) {
		fprintf (stderr, "Error: Failed to find cyfxflashprog.img file\n");
		return -1;
	}

	r = fx3_usbboot_download (handle, progfile_p);
	free (progfile_p);
	if (r != 0) {
		fprintf (stderr, "Error: Failed to download flash prog utility\n");
		return -1;
	}

	cyusb_close ();
	*h = NULL;

	// Now wait for the flash programmer to enumerate, and get a handle to it.
	for (j = 0; j < GETHANDLE_TIMEOUT; j++) {
		sleep (1);
		r = cyusb_open ();
		if (r > 0) {
			for (i = 0; i < r; i++) {
				handle = cyusb_gethandle (i);
				if (cyusb_getvendor (handle) == FLASHPROG_VID) {
					r = check_fx3_flashprog (handle);
					if (r == 0) {
						printf ("Info: Got handle to FX3 flash programmer\n");
						*h = handle;
						return 0;
					}
				}
			}
			cyusb_close ();
		}
	}

	fprintf (stderr, "Error: Failed to get handle to flash programmer\n");
	return -2;
}

static int
fx3_i2c_write (
		cyusb_handle  *h,
		unsigned char *buf,
		int            devAddr,
		int            start,
		int            len)
{
	int r = 0;
	int index = start;
	unsigned short address = 0;
	int size;

	while (len > 0) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer (h, 0x40, 0xBA, devAddr, address, &buf[index], size, VENDORCMD_TIMEOUT);
		if (r != size) {
			fprintf (stderr, "Error: I2C write failed\n");
			return -1;
		}

		address += size ;
		index   += size;
		len     -= size;
	}

	return 0;
}

/* Function to read I2C data and compare against the expected value. */
static int
fx3_i2c_read_verify (
		cyusb_handle  *h,
		unsigned char *expData,
		int            devAddr,
		int            len)
{
	int r = 0;
	int index = 0;
	unsigned short address = 0;
	int size;
	unsigned char tmpBuf[MAX_WRITE_SIZE];

	while (len > 0) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer (h, 0xC0, 0xBB, devAddr, address, tmpBuf, size, VENDORCMD_TIMEOUT);
		if (r != size) {
			fprintf (stderr, "Error: I2C read failed\n");
			return -1;
		}

		if (memcmp (expData + index, tmpBuf, size) != 0) {
			fprintf (stderr, "Error: Failed to read expected data from I2C EEPROM\n");
			return -2;
		}

		address += size ;
		index   += size;
		len     -= size;
	}

	return 0;
}

int
fx3_i2cboot_download (
		cyusb_handle *h,
		const char   *filename)
{
	int romsize, size;
	int address = 0, offset = 0;
	int r, filesize;
	unsigned char *fwBuf = 0;

	// Check if we have a handle to the FX3 flash programmer.
	r = get_fx3_prog_handle (&h);
	if (r != 0) {
		fprintf (stderr, "Error: FX3 flash programmer not found\n");
		return -1;
	}

	// Allocate memory for holding the firmware binary.
	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);

	if (read_firmware_image (filename, fwBuf, &romsize, &filesize)) {
		fprintf (stderr, "Error: File %s does not contain valid FX3 firmware image\n", filename);
		free (fwBuf);
		return -2;
	}

        printf ("Info: Writing firmware image to I2C EEPROM\n");

	filesize = ROUND_UP(filesize, I2C_PAGE_SIZE);
	while (filesize != 0) {

		size = (filesize <= romsize) ? filesize : romsize;
		if (size > I2C_SLAVE_SIZE) {
			r = fx3_i2c_write (h, fwBuf, address, offset, I2C_SLAVE_SIZE);
			if (r == 0) {
				r = fx3_i2c_read_verify (h, fwBuf + offset, address, I2C_SLAVE_SIZE);
				if (r != 0) {
					fprintf (stderr, "Error: Read-verify from I2C EEPROM failed\n");
					free (fwBuf);
					return -3;
				}

				r = fx3_i2c_write (h, fwBuf, address + 4, offset + I2C_SLAVE_SIZE, size - I2C_SLAVE_SIZE);
				if (r == 0) {
					r = fx3_i2c_read_verify (h, fwBuf + offset + I2C_SLAVE_SIZE,
							address + 4, size - I2C_SLAVE_SIZE);
					if (r != 0) {
						fprintf (stderr, "Error: Read-verify from I2C EEPROM failed\n");
						free (fwBuf);
						return -3;
					}
				}
			}
		} else {
			r = fx3_i2c_write (h, fwBuf, address, offset, size);
			if (r == 0) {
				r = fx3_i2c_read_verify (h, fwBuf + offset, address, size);
				if (r != 0) {
					fprintf (stderr, "Error: Read-verify from I2C EEPROM failed\n");
					free (fwBuf);
					return -3;
				}
			}
		}

		if (r != 0) {
			fprintf (stderr, "Error: Write to I2C EEPROM failed\n");
			free (fwBuf);
			return -4;
		}

		/* Move to the next slave address. */
		offset += size;
		filesize -= size;
		address++;
	}

	free (fwBuf);
        printf ("Info: I2C programming completed\n");
	return 0;
}

static int
fx3_spi_write (
		cyusb_handle  *h,
		unsigned char *buf,
		int            len)
{
	int r = 0;
	int index = 0;
	int size;
	unsigned short page_address = 0;

	while (len > 0) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer (h, 0x40, 0xC2, 0, page_address, &buf[index], size, VENDORCMD_TIMEOUT);
		if (r != size) {
			fprintf (stderr, "Error: Write to SPI flash failed\n");
			return -1;
		}
		index += size;
		len   -= size;
		page_address += (size / SPI_PAGE_SIZE);
	}

	return 0;
}

static int
fx3_spi_erase_sector (
		cyusb_handle   *h,
		unsigned short  nsector)
{
	unsigned char stat;
	int           timeout = 10;
	int r;

	r = cyusb_control_transfer (h, 0x40, 0xC4, 1, nsector, NULL, 0, VENDORCMD_TIMEOUT);
	if (r != 0) {
		fprintf (stderr, "Error: SPI sector erase failed\n");
		return -1;
	}

	// Wait for the SPI flash to become ready again.
	do {
		r = cyusb_control_transfer (h, 0xC0, 0xC4, 0, 0, &stat, 1, VENDORCMD_TIMEOUT);
		if (r != 1) {
			fprintf (stderr, "Error: SPI status read failed\n");
			return -2;
		}
		sleep (1);
		timeout--;
	} while ((stat != 0) && (timeout > 0));

	if (stat != 0) {
		fprintf (stderr, "Error: Timed out on SPI status read\n");
		return -3;
	}

	printf ("Info: Erased sector %d of SPI flash\n", nsector);
	return 0;
}

int
fx3_spiboot_download (
		cyusb_handle *h,
		const char   *filename)
{
	unsigned char *fwBuf;
	int r, i, filesize;

	// Check if we have a handle to the FX3 flash programmer.
	r = get_fx3_prog_handle (&h);
	if (r != 0) {
		fprintf (stderr, "Error: FX3 flash programmer not found\n");
		return -1;
	}

	// Allocate memory for holding the firmware binary.
	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);
	if (fwBuf == 0) {
		fprintf (stderr, "Error: Failed to allocate buffer to store firmware binary\n");
		return -2;
	}

	if (read_firmware_image (filename, fwBuf, NULL, &filesize)) {
		fprintf (stderr, "Error: File %s does not contain valid FX3 firmware image\n", filename);
		free (fwBuf);
		return -3;
	}

	filesize = ROUND_UP(filesize, SPI_PAGE_SIZE);

	// Erase as many SPI sectors as are required to hold the firmware binary.
	for (i = 0; i < ((filesize + SPI_SECTOR_SIZE - 1) / SPI_SECTOR_SIZE); i++) {
		r = fx3_spi_erase_sector (h, i);
		if (r != 0) {
			fprintf (stderr, "Error: Failed to erase SPI flash\n");
			free (fwBuf);
			return -4;
		}
	}

	r = fx3_spi_write (h, fwBuf, filesize);
	if (r != 0) {
		fprintf (stderr, "Error: SPI write failed\n");
	} else {
		printf ("Info: SPI flash programming completed\n");
	}

	free (fwBuf);
	return r;
}

void
print_usage_info (
		const char *arg0)
{
	printf ("%s: FX3 firmware programmer\n\n", arg0);
	printf ("Usage:\n");
	printf ("\t%s -h: Print this help message\n\n", arg0);
	printf ("\t%s -t <target> -i <img filename>: Program firmware binary from <img filename>\n", arg0);
	printf ("\t\tto <target>\n");
	printf ("\t\t\twhere <target> is one of\n");
	printf ("\t\t\t\t\"RAM\": Program to FX3 RAM\n");
	printf ("\t\t\t\t\"I2C\": Program to I2C EEPROM\n");
	printf ("\t\t\t\t\"SPI\": Program to SPI FLASH\n");
	printf ("\n\n");
}

int main (
		int    argc,
		char **argv)
{
	cyusb_handle *h;
	char         *filename = NULL;
	char         *tgt_str  = NULL;
	fx3_fw_target tgt = FW_TARGET_NONE;
	int r, i;

	/* Parse command line arguments. */
	for (i = 1; i < argc; i++) {
		if ((strcasecmp (argv[i], "-h") == 0) || (strcasecmp (argv[i], "--help") == 0)) {
			print_usage_info (argv[0]);
			return 0;
		} else {
			if ((strcasecmp (argv[i], "-t") == 0) || (strcasecmp (argv[i], "--target") == 0)) {
				if (argc > (i + 1)) {
					tgt_str = argv[i + 1];
					if (strcasecmp (argv[i + 1], "ram") == 0)
						tgt = FW_TARGET_RAM;
					if (strcasecmp (argv[i + 1], "i2c") == 0)
						tgt = FW_TARGET_I2C;
					if (strcasecmp (argv[i + 1], "spi") == 0)
						tgt = FW_TARGET_SPI;
					if (tgt == FW_TARGET_NONE) {
						fprintf (stderr, "Error: Unknown target %s\n", argv[i + 1]);
						print_usage_info (argv[0]);
						return -EINVAL;
					}
				}
				i++;
			} else {
				if ((strcmp (argv[i], "-i") == 0) || (strcmp (argv[i], "--image") == 0)) {
					if (argc > (i + 1))
						filename = argv[i + 1];
					i++;
				} else {
					fprintf (stderr, "Error: Unknown parameter %s\n", argv[i]);
					print_usage_info (argv[0]);
					return -EINVAL;
				}
			}
		}
	}
	if ((filename == NULL) || (tgt == FW_TARGET_NONE)) {
		fprintf (stderr, "Error: Firmware binary or target not specified\n");
		print_usage_info (argv[0]);
		return -EINVAL;
	}

	r = cyusb_open ();
	if (r < 0) {
	     fprintf (stderr, "Error opening library\n");
	     return -ENODEV;
	}
	else if (r == 0) {
	        fprintf (stderr, "Error: No FX3 device found\n");
		return -ENODEV;
	}
	else if (r > 1) {
		fprintf (stderr, "Error: More than one Cypress device found\n");
		return -EINVAL;
	}

	h = cyusb_gethandle (0);

	switch (tgt) {
		case FW_TARGET_RAM:
			r = fx3_usbboot_download (h, filename);
			break;
		case FW_TARGET_I2C:
			r = fx3_i2cboot_download (h, filename);
			break;
		case FW_TARGET_SPI:
			r = fx3_spiboot_download (h, filename);
			break;
		default:
			break;
	}
	if (r != 0) {
		fprintf (stderr, "Error: FX3 firmware programming failed\n");
	} else {
		printf ("FX3 firmware programming to %s completed\n", tgt_str);
	}

	return r;
}

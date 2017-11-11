/*
 * Filename             : fx3_download.cpp
 * Description          : Provides functions to download FX3 firmware to RAM, I2C EEPROM
 *                        or SPI Flash.
 */

#include <QtCore>
#include <QtGui>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <libusb-1.0/libusb.h>

#include "../include/cyusb.h"
#include "../include/controlcenter.h"

extern ControlCenter *mainwin;
extern QStatusBar *sb;
extern QProgressBar *mbar;

extern cyusb_handle *h;
extern int num_devices_detected;

#define FLASHPROG_VID	(0x04b4)		// USB VID for the FX3 flash programmer.

#define MAX_FWIMG_SIZE	(512 * 1024)		// Maximum size of the firmware binary.
#define MAX_WRITE_SIZE	(2 * 1024)		// Max. size of data that can be written through one vendor command.

#define I2C_PAGE_SIZE	(64)			// Page size for I2C EEPROM.
#define I2C_SLAVE_SIZE	(64 * 1024)		// Max. size of data that can fit on one EEPROM address.

#define SPI_PAGE_SIZE	(256)			// Page size for SPI flash memory.
#define SPI_SECTOR_SIZE	(64 * 1024)		// Sector size for SPI flash memory.

#define VENDORCMD_TIMEOUT	(5000)		// Timeout for each vendor command is set to 5 seconds.
#define GETHANDLE_TIMEOUT	(5)		// Timeout (in seconds) for getting a FX3 flash programmer handle.

// Round n up to a multiple of v.
#define ROUND_UP(n,v)	((((n) + ((v) - 1)) / (v)) * (v))

#define GET_LSW(v)	((unsigned short)((v) & 0xFFFF))
#define GET_MSW(v)	((unsigned short)((v) >> 16))

const int i2c_eeprom_size[] =
{
	1024,
	2048,
	4096,
	8192,
	16384,
	32768,
	65536,
	131072
};

static int filesize;
static int current_count;

static int ram_write(unsigned char *buf, unsigned int ramAddress, int len)
{
	int r;
	int index = 0;
	int size;

	while ( len > 0 ) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer(h, 0x40, 0xA0, GET_LSW(ramAddress), GET_MSW(ramAddress),
				&buf[index], size, VENDORCMD_TIMEOUT);
		if ( r != size ) {
			printf("Vendor write to FX3 RAM failed\n");
			return -1;
		}

		ramAddress += size;
		index      += size;
		len        -= size;
	}

	return 0;
}

static int read_firmware_image(const char *filename, unsigned char *buf, int *romsize)
{
	int fd;
	int nbr;
	struct stat filestat;

	// Verify that the file size does not exceed our limits.
	if ( stat (filename, &filestat) != 0 ) {
		printf("Failed to stat file %s\n", filename);
		return -1;
	}

	filesize = filestat.st_size;
	if ( filesize > MAX_FWIMG_SIZE ) {
		printf("File size exceeds maximum firmware image size\n");
		return -2;
	}

	fd = open(filename, O_RDONLY);
	if ( fd < 0 ) { 
		printf("File not found\n");
		return -3;
	}
	nbr = read(fd, buf, 2);		/* Read first 2 bytes, must be equal to 'CY'	*/
	if ( strncmp((char *)buf,"CY",2) ) {
		printf("Image does not have 'CY' at start. aborting\n");
		return -4;
	}
	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageCTL	*/
	if ( buf[0] & 0x01 ) {
		printf("Image does not contain executable code\n");
		return -5;
	}
	if (romsize != 0)
		*romsize = i2c_eeprom_size[(buf[0] >> 1) & 0x07];

	nbr = read(fd, buf, 1);		/* Read 1 byte. bImageType	*/
	if ( !(buf[0] == 0xB0) ) {
		printf("Not a normal FW binary with checksum\n");
		return -6;
	}

	// Read the complete firmware binary into a local buffer.
	lseek(fd, 0, SEEK_SET);
	nbr = read(fd, buf, filesize);

	close(fd);
	return 0;
}

int fx3_usbboot_download(const char *filename)
{
	unsigned char *fwBuf;
	unsigned int  *data_p;
	unsigned int i, checksum;
	unsigned int address, length;
	int r, index;

	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);
	if ( fwBuf == 0 ) {
		printf("Failed to allocate buffer to store firmware binary\n");
		sb->showMessage("Error: Failed to get memory for download\n", 5000);
		return -1;
	}

	// Read the firmware image into the local RAM buffer.
	r = read_firmware_image(filename, fwBuf, NULL);
	if ( r != 0 ) {
		printf("Failed to read firmware file %s\n", filename);
		sb->showMessage("Error: Failed to read firmware binary\n", 5000);
		free(fwBuf);
		return -2;
	}

	// Run through each section of code, and use vendor commands to download them to RAM.
	index    = 4;
	checksum = 0;
	while ( index < filesize ) {
		data_p  = (unsigned int *)(fwBuf + index);
		length  = data_p[0];
		address = data_p[1];
		if (length != 0) {
			for (i = 0; i < length; i++)
				checksum += data_p[2 + i];
			r = ram_write(fwBuf + index + 8, address, length * 4);
			if (r != 0) {
				printf("Failed to download data to FX3 RAM\n");
				sb->showMessage("Error: Write to FX3 RAM failed", 5000);
				free(fwBuf);
				return -3;
			}
		} else {
			if (checksum != data_p[2]) {
				printf ("Checksum error in firmware binary\n");
				sb->showMessage("Error: Firmware checksum error", 5000);
				free(fwBuf);
				return -4;
			}

			r = cyusb_control_transfer(h, 0x40, 0xA0, GET_LSW(address), GET_MSW(address), NULL,
					0, VENDORCMD_TIMEOUT);
			if ( r != 0 )
				printf("Ignored error in control transfer: %d\n", r);
			break;
		}

		index += (8 + length * 4);
	}

	free(fwBuf);
	return 0;
}

/* Check if the current device handle corresponds to the FX3 flash programmer. */
static int check_fx3_flashprog(cyusb_handle *handle)
{
	int r;
	char local[8];

	r = cyusb_control_transfer(handle, 0xC0, 0xB0, 0, 0, (unsigned char *)local, 8, VENDORCMD_TIMEOUT);
	if ( ( r != 8 ) || ( strncasecmp(local, "FX3PROG", 7) != 0 ) ) {
		printf("Current device is not the FX3 flash programmer\n");
		return -1;
	}
	
	printf("Found FX3 flash programmer\n");
	return 0;
}

/* Get the handle to the FX3 flash programmer device, if found. */
static int get_fx3_prog_handle(void)
{
	char *progfile_p, *tmp;
	cyusb_handle *handle;
	int i, j, r;
	struct stat filestat;

	r = check_fx3_flashprog(h);
	if ( r == 0 )
		return 0;

	printf("Failed to find FX3 flash programmer\n");
	printf("Trying to download flash programmer to RAM\n");

	tmp = getenv("CYUSB_ROOT");
	if (tmp != NULL) {
		i = strlen(tmp);
		progfile_p = (char *)malloc(i + 32);
		strcpy(progfile_p, tmp);
		strcat(progfile_p, "/fx3_images/cyfxflashprog.img");
	}
	else {
		progfile_p = (char *)malloc (32);
		strcpy (progfile_p, "fx3_images/cyfxflashprog.img");
	}

	r = stat(progfile_p, &filestat);
	if ( r != 0 ) {
		printf("Failed to find cyfxflashprog.img file\n");
		return -1;
	}

	r = fx3_usbboot_download(progfile_p);
	free (progfile_p);
	if ( r != 0 ) {
		printf("Failed to download flash prog utility\n");
		return -1;
	}

	// Now wait for the flash programmer to enumerate, and get a handle to it.
	for ( j = 0; j < GETHANDLE_TIMEOUT; j++ ) {
		sleep (1);
		for ( i = 0; i < num_devices_detected; i++ ) {
			handle = cyusb_gethandle(i);
			if ( cyusb_getvendor(handle) == FLASHPROG_VID ) {
				r = check_fx3_flashprog(handle);
				if ( r == 0 ) {
					h = handle;
					return 0;
				}
			}
		}
	}

	printf("Failed to get handle to flash programmer\n");
	return -2;
}

static int i2c_write(unsigned char *buf, int devAddr, int start, int len)
{
	int r = 0;
	int index = start;
	unsigned short address = 0;
	int size;

	while ( len > 0 ) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer(h, 0x40, 0xBA, devAddr, address, &buf[index], size, VENDORCMD_TIMEOUT);
		if ( r != size ) {
			printf("Error in i2c_write\n");
			return -1;
		}

		address += size ;
		index   += size;
		len     -= size;

		current_count += size;
		mbar->setValue(current_count);
	}

	return 0;
}

/* Function to read I2C data and compare against the expected value. */
static int i2c_read_verify(unsigned char *expData, int devAddr, int len)
{
	int r = 0;
	int index = 0;
	unsigned short address = 0;
	int size;
	unsigned char tmpBuf[MAX_WRITE_SIZE];

	while ( len > 0 ) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer(h, 0xC0, 0xBB, devAddr, address, tmpBuf, size, VENDORCMD_TIMEOUT);
		if ( r != size ) {
			printf("Error in i2c_read\n");
			return -1;
		}

		if ( memcmp(expData + index, tmpBuf, size) != 0 ) {
			printf("Failed to read expected data from I2C EEPROM\n");
			return -2;
		}

		address += size ;
		index   += size;
		len     -= size;
	}

	return 0;
}

int fx3_i2cboot_download(const char *filename)
{
	int romsize, size;
	int address = 0, offset = 0;
	int r;
	unsigned char *fwBuf = 0;

	// Check if we have a handle to the FX3 flash programmer.
	r = get_fx3_prog_handle();
	if ( r != 0 ) {
		printf("FX3 flash programmer not found\n");
		sb->showMessage("Error: Could not find target device", 5000);
		return -1;
	}

	// Allocate memory for holding the firmware binary.
	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);

	if ( read_firmware_image(filename, fwBuf, &romsize) ) {
		printf("File %s does not contain valid FX3 firmware image\n", filename);
		sb->showMessage("Error: Failed to find valid FX3 firmware image", 5000);
		free(fwBuf);
		return -2;
	}

        printf ("Writing firmware image to I2C EEPROM\n");
	mbar = new QProgressBar();
	mbar->setRange(0, filesize);
	sb->addWidget(mbar);
	current_count = 0;

	filesize = ROUND_UP(filesize, I2C_PAGE_SIZE);
	while ( filesize != 0 ) {

		size = (filesize <= romsize) ? filesize : romsize;
		if ( size > I2C_SLAVE_SIZE ) {
			r = i2c_write(fwBuf, address, offset, I2C_SLAVE_SIZE);
			if ( r == 0 ) {
				r = i2c_read_verify (fwBuf + offset, address, I2C_SLAVE_SIZE);
				if ( r != 0 ) {
					printf("Read-verify from I2C EEPROM failed\n");
                                        sb->removeWidget(mbar);
					sb->showMessage("Error: Failed to read correct data back from EEPROM", 5000);
					free(fwBuf);
					return -3;
				}

				r = i2c_write(fwBuf, address + 4, offset + I2C_SLAVE_SIZE, size - I2C_SLAVE_SIZE);
				if ( r == 0 ) {
					r = i2c_read_verify(fwBuf + offset + I2C_SLAVE_SIZE, address + 4, size - I2C_SLAVE_SIZE);
					if ( r != 0 ) {
						printf("Read-verify from I2C EEPROM failed\n");
						sb->removeWidget(mbar);
						sb->showMessage("Error: Failed to read correct data back from EEPROM", 5000);
						free(fwBuf);
						return -3;
					}
				}
			}
		} else {
			r = i2c_write(fwBuf, address, offset, size);
			if ( r == 0 ) {
				r = i2c_read_verify (fwBuf + offset, address, size);
				if ( r != 0 ) {
					printf("Read-verify from I2C EEPROM failed\n");
                                        sb->removeWidget(mbar);
					sb->showMessage("Error: Failed to read correct data back from EEPROM", 5000);
					free(fwBuf);
					return -3;
				}
			}
		}

		if ( r != 0 ) {
			printf("Write to I2C EEPROM failed\n");
                        sb->removeWidget(mbar);
			sb->showMessage("Error: I2C EEPROM programming failed", 5000);
			free(fwBuf);
			return -4;
		}

		/* Move to the next slave address. */
		offset += size;
		filesize -= size;
		address++;
	}

	free(fwBuf);
	sb->removeWidget(mbar);
        printf("I2C programming completed\n");
	sb->showMessage("Completed programming EEPROM device", 5000);
	return 0;
}

static int spi_write(unsigned char *buf, int len)
{
	int r = 0;
	int index = 0;
	int size;
	unsigned short page_address = 0;

	while ( len > 0 ) {
		size = ( len > MAX_WRITE_SIZE ) ? MAX_WRITE_SIZE : len;
		r = cyusb_control_transfer(h, 0x40, 0xC2, 0, page_address, &buf[index], size, VENDORCMD_TIMEOUT);
		if ( r != size ) {
			printf("Write to SPI flash failed\n");
			return -1;
		}
		index += size;
		len   -= size;
		page_address += (size / SPI_PAGE_SIZE);
	}

	return 0;
}

static int spi_erase_sector(unsigned short nsector)
{
	unsigned char stat;
	int           timeout = 10;
	int r;

	r = cyusb_control_transfer(h, 0x40, 0xC4, 1, nsector, NULL, 0, VENDORCMD_TIMEOUT);
	if (r != 0) {
		printf("SPI sector erase failed\n");
		return -1;
	}

	// Wait for the SPI flash to become ready again.
	do {
		r = cyusb_control_transfer(h, 0xC0, 0xC4, 0, 0, &stat, 1, VENDORCMD_TIMEOUT);
		if (r != 1) {
			printf("SPI status read failed\n");
			return -2;
		}
		sleep (1);
		timeout--;
	} while ( (stat != 0) && (timeout > 0) );

	if (stat != 0) {
		printf("Timed out on SPI status read\n");
		return -3;
	}

	printf("Erased sector %d of SPI flash\n", nsector);
	return 0;
}

int fx3_spiboot_download(const char *filename)
{
	unsigned char *fwBuf;
	int r, i;

	// Check if we have a handle to the FX3 flash programmer.
	r = get_fx3_prog_handle();
	if ( r != 0 ) {
		printf("FX3 flash programmer not found\n");
		sb->showMessage("Error: Could not find target device", 5000);
		return -1;
	}

	// Allocate memory for holding the firmware binary.
	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);
	if ( fwBuf == 0 ) {
		printf("Failed to allocate buffer to store firmware binary\n");
		sb->showMessage("Error: Failed to get memory for download\n", 5000);
		return -2;
	}

	if ( read_firmware_image(filename, fwBuf, NULL) ) {
		printf("File %s does not contain valid FX3 firmware image\n", filename);
		sb->showMessage("Error: Failed to find valid FX3 firmware image", 5000);
		free(fwBuf);
		return -3;
	}

	filesize = ROUND_UP(filesize, SPI_PAGE_SIZE);

	// Erase as many SPI sectors as are required to hold the firmware binary.
	for (i = 0; i < ((filesize + SPI_SECTOR_SIZE - 1) / SPI_SECTOR_SIZE); i++) {
		r = spi_erase_sector(i);
		if (r != 0) {
			printf("Failed to erase SPI flash\n");
			sb->showMessage("Error: Failed to erase SPI flash device", 5000);
			free(fwBuf);
			return -4;
		}
	}

	r = spi_write(fwBuf, filesize);
	if (r != 0) {
		printf("SPI write failed\n");
		sb->showMessage("SPI Flash programming failed", 5000);
	} else {
		sb->showMessage("Completed writing into SPI FLASH", 5000);
	}

	free(fwBuf);
	return r;
}


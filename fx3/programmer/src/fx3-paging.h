/*
 * Paging arithmetic for FX3 EEPROM and SPI flash programming.
 *
 * Split out of fx3-programmer.c so it can be unit tested. Every function here is pure —
 * no libusb, no I/O, no globals — which matters because an off-by-one in this arithmetic
 * writes past a page or slave boundary and bricks the device, and that is not something
 * you want to discover on real hardware.
 *
 * Domesday Duplicator - FX3 programmer
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FX3_PAGING_H
#define FX3_PAGING_H

/* Largest single vendor-command transfer the flash programmer accepts. */
#define FX3_MAX_WRITE_SIZE 2048

/* SPI flash geometry. */
#define FX3_SPI_FLASH_PAGE_SIZE 256
#define FX3_SPI_FLASH_SECTOR_SIZE (64 * 1024)

/* I2C EEPROM geometry. Writes must be a whole number of pages, and each I2C slave
 * device covers 64 KiB — past that the slave address increments. */
#define FX3_I2C_PAGE_SIZE 64
#define FX3_I2C_SLAVE_SIZE (64 * 1024)

/*
 * Round an image size up to a whole number of I2C pages.
 *
 * The EEPROM is written a page at a time, so a firmware image that does not end on a page
 * boundary is zero-padded. Returns 0 for a size of 0, and is a no-op for sizes that are
 * already page-aligned.
 */
static inline int fx3_pad_to_i2c_page(int size)
{
    if (size <= 0) {
        return 0;
    }
    return ((size + FX3_I2C_PAGE_SIZE - 1) / FX3_I2C_PAGE_SIZE) * FX3_I2C_PAGE_SIZE;
}

/*
 * Size of the next chunk to send to a single I2C slave device.
 *
 * The caller walks the padded image in chunks of this size, incrementing the slave address
 * after each one. Returns 0 when nothing remains.
 */
static inline int fx3_i2c_slave_chunk(int remaining)
{
    if (remaining <= 0) {
        return 0;
    }
    return (remaining > FX3_I2C_SLAVE_SIZE) ? FX3_I2C_SLAVE_SIZE : remaining;
}

/*
 * Size of the next USB control transfer within a chunk.
 *
 * Returns 0 when nothing remains.
 */
static inline int fx3_transfer_size(int remaining)
{
    if (remaining <= 0) {
        return 0;
    }
    return (remaining > FX3_MAX_WRITE_SIZE) ? FX3_MAX_WRITE_SIZE : remaining;
}

/*
 * Number of I2C slave devices a padded image spans.
 *
 * This is how many times the slave address increments over a full programming run, so it
 * is the loop-count the caller's while() must agree with.
 */
static inline int fx3_i2c_slave_count(int padded_size)
{
    if (padded_size <= 0) {
        return 0;
    }
    return (padded_size + FX3_I2C_SLAVE_SIZE - 1) / FX3_I2C_SLAVE_SIZE;
}

/*
 * Number of SPI flash sectors that must be erased to hold an image of this size.
 *
 * SPI flash erases a whole sector at a time, so an image one byte into a new sector still
 * costs a full sector erase.
 */
static inline int fx3_spi_sector_count(int size)
{
    if (size <= 0) {
        return 0;
    }
    return (size + FX3_SPI_FLASH_SECTOR_SIZE - 1) / FX3_SPI_FLASH_SECTOR_SIZE;
}

#endif /* FX3_PAGING_H */

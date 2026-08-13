/*
 * test_paging.cpp
 *
 * Domesday Duplicator - FX3 programmer tests
 *
 * T1 (unit) coverage for the EEPROM and SPI flash paging arithmetic.
 *
 * These are four short functions, and it would be easy to dismiss them as too simple to
 * test. They are not. An off-by-one in the padding sends a partial page to the EEPROM; an
 * off-by-one in the chunking rolls the I2C slave address at the wrong offset and writes
 * firmware bytes over the wrong device. Either bricks the FX3, and the only way to recover
 * is the PMODE jumper — assuming the board has one exposed.
 *
 * SPDX-FileCopyrightText: 2026 Simon Inns
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

extern "C" {
#include "fx3-paging.h"
}

namespace
{

// --- Geometry ---------------------------------------------------------------------------

TEST(Paging, GeometryMatchesTheFx3)
{
    // These come from the FX3 datasheet and the Cypress flash programmer protocol. If any
    // of them changes, every function below changes with it, so pin them explicitly.
    EXPECT_EQ(FX3_I2C_PAGE_SIZE, 64);
    EXPECT_EQ(FX3_I2C_SLAVE_SIZE, 65536);
    EXPECT_EQ(FX3_MAX_WRITE_SIZE, 2048);
    EXPECT_EQ(FX3_SPI_FLASH_PAGE_SIZE, 256);
    EXPECT_EQ(FX3_SPI_FLASH_SECTOR_SIZE, 65536);
}

TEST(Paging, TransferSizeDividesSlaveSizeExactly)
{
    // The chunk loop assumes a slave-sized chunk splits into whole transfers with no
    // remainder. If this ever stopped holding, the final transfer of each chunk would be
    // short and the address stepping would drift.
    EXPECT_EQ(FX3_I2C_SLAVE_SIZE % FX3_MAX_WRITE_SIZE, 0);
}

// --- Padding to a page boundary -----------------------------------------------------------

TEST(Paging, PadToPageRoundsUp)
{
    EXPECT_EQ(fx3_pad_to_i2c_page(1), 64);
    EXPECT_EQ(fx3_pad_to_i2c_page(63), 64);
    EXPECT_EQ(fx3_pad_to_i2c_page(65), 128);
    EXPECT_EQ(fx3_pad_to_i2c_page(129), 192);
}

TEST(Paging, PadToPageIsIdempotentOnAlignedSizes)
{
    // Already-aligned sizes must not gain a spurious extra page — that would write 64
    // bytes of zeros past the end of the image.
    EXPECT_EQ(fx3_pad_to_i2c_page(64), 64);
    EXPECT_EQ(fx3_pad_to_i2c_page(128), 128);
    EXPECT_EQ(fx3_pad_to_i2c_page(65536), 65536);

    for (int size = 64; size <= 8192; size += 64) {
        EXPECT_EQ(fx3_pad_to_i2c_page(size), size) << "at aligned size " << size;
    }
}

TEST(Paging, PadToPageHandlesDegenerateSizes)
{
    EXPECT_EQ(fx3_pad_to_i2c_page(0), 0);
    EXPECT_EQ(fx3_pad_to_i2c_page(-1), 0);
}

TEST(Paging, PadToPageNeverShrinksAndNeverOverpads)
{
    // The two properties that matter: the padded size holds the whole image, and it wastes
    // less than one page doing so.
    for (int size = 1; size <= 4096; ++size) {
        const int padded = fx3_pad_to_i2c_page(size);
        EXPECT_GE(padded, size) << "at size " << size;
        EXPECT_LT(padded - size, FX3_I2C_PAGE_SIZE) << "at size " << size;
        EXPECT_EQ(padded % FX3_I2C_PAGE_SIZE, 0) << "at size " << size;
    }
}

// --- Chunking across I2C slave devices ------------------------------------------------------

TEST(Paging, SlaveChunkCapsAtSlaveSize)
{
    EXPECT_EQ(fx3_i2c_slave_chunk(100), 100);
    EXPECT_EQ(fx3_i2c_slave_chunk(65536), 65536);
    EXPECT_EQ(fx3_i2c_slave_chunk(65537), 65536);
    EXPECT_EQ(fx3_i2c_slave_chunk(200000), 65536);
}

TEST(Paging, SlaveChunkHandlesDegenerateSizes)
{
    EXPECT_EQ(fx3_i2c_slave_chunk(0), 0);
    EXPECT_EQ(fx3_i2c_slave_chunk(-1), 0);
}

TEST(Paging, SlaveChunkLoopTerminatesAndCoversExactly)
{
    // Walk the loop the programmer actually runs, for a range of image sizes including
    // the boundary cases either side of a slave rollover. The loop must consume the image
    // exactly — never overshooting, never stalling.
    const int sizes[] = { 1, 63, 64, 65535, 65536, 65537, 131072, 131073, 200000 };

    for (int size : sizes) {
        const int padded = fx3_pad_to_i2c_page(size);
        int remaining = padded;
        int offset = 0;
        int address = 0;
        int iterations = 0;

        while (remaining > 0) {
            const int chunk = fx3_i2c_slave_chunk(remaining);
            ASSERT_GT(chunk, 0) << "loop stalled at size " << size;

            offset += chunk;
            remaining -= chunk;
            address++;

            ASSERT_LT(++iterations, 1000) << "loop ran away at size " << size;
        }

        EXPECT_EQ(offset, padded) << "at size " << size;
        EXPECT_EQ(remaining, 0) << "at size " << size;
        EXPECT_EQ(address, fx3_i2c_slave_count(padded)) << "at size " << size;
    }
}

TEST(Paging, SlaveCountMatchesRollovers)
{
    EXPECT_EQ(fx3_i2c_slave_count(0), 0);
    EXPECT_EQ(fx3_i2c_slave_count(1), 1);
    EXPECT_EQ(fx3_i2c_slave_count(65536), 1);
    // One byte into the second slave still needs a second slave
    EXPECT_EQ(fx3_i2c_slave_count(65537), 2);
    EXPECT_EQ(fx3_i2c_slave_count(131072), 2);
    EXPECT_EQ(fx3_i2c_slave_count(131073), 3);
}

// --- Transfer sizes within a chunk ----------------------------------------------------------

TEST(Paging, TransferSizeCapsAtMaxWrite)
{
    EXPECT_EQ(fx3_transfer_size(1), 1);
    EXPECT_EQ(fx3_transfer_size(2047), 2047);
    EXPECT_EQ(fx3_transfer_size(2048), 2048);
    EXPECT_EQ(fx3_transfer_size(2049), 2048);
    EXPECT_EQ(fx3_transfer_size(65536), 2048);
}

TEST(Paging, TransferSizeHandlesDegenerateSizes)
{
    EXPECT_EQ(fx3_transfer_size(0), 0);
    EXPECT_EQ(fx3_transfer_size(-1), 0);
}

TEST(Paging, TransferLoopAddressStaysWithinSixteenBits)
{
    // fx3_i2c_write tracks the offset within a chunk in an `unsigned short`. A full
    // 64 KiB chunk walks that variable right up to its limit, so check the last increment
    // lands exactly at 65536 and the loop exits rather than wrapping to 0 and rewriting
    // the start of the chunk.
    int remaining = FX3_I2C_SLAVE_SIZE;
    long address = 0;
    int transfers = 0;

    while (remaining > 0) {
        const int size = fx3_transfer_size(remaining);
        ASSERT_GT(size, 0);

        // Every address used to issue a transfer must be representable
        EXPECT_LE(address, 0xFFFF) << "address escaped 16 bits before transfer " << transfers;

        address += size;
        remaining -= size;
        transfers++;
    }

    EXPECT_EQ(transfers, 32);
    EXPECT_EQ(address, 65536);
    EXPECT_EQ(remaining, 0);
}

// --- SPI flash sectors ------------------------------------------------------------------

TEST(Paging, SpiSectorCountRoundsUp)
{
    EXPECT_EQ(fx3_spi_sector_count(0), 0);
    EXPECT_EQ(fx3_spi_sector_count(1), 1);
    EXPECT_EQ(fx3_spi_sector_count(65536), 1);
    EXPECT_EQ(fx3_spi_sector_count(65537), 2);
    EXPECT_EQ(fx3_spi_sector_count(196608), 3);
}

TEST(Paging, SpiSectorCountCoversTheWholeImage)
{
    for (int size = 1; size <= 300000; size += 997) {
        const int sectors = fx3_spi_sector_count(size);
        EXPECT_GE(static_cast<long>(sectors) * FX3_SPI_FLASH_SECTOR_SIZE, size)
            << "at size " << size;
        EXPECT_LT(static_cast<long>(sectors - 1) * FX3_SPI_FLASH_SECTOR_SIZE, size)
            << "at size " << size;
    }
}

} // namespace

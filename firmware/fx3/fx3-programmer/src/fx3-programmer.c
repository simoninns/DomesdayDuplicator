/*
 * fx3-programmer.c - Minimal FX3 firmware programmer
 * 
    int fd, ret, bytes_sent = 0;
 * A minimal, libusb-based tool to:
 * - Discover connected FX3 devices
 * - Upload firmware to FX3 RAM/EEPROM/Flash
 * - Verify firmware upload
 * 
 * This implementation is based on the Cypress cyusb_linux project:
 * https://github.com/Cypress-Semiconductor/cyusb_linux
 * 
 * Simplified and streamlined for the Domesday Duplicator project,
 * removing Qt GUI, FX2 support, and unnecessary dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libusb-1.0/libusb.h>

#define FX3_VENDOR_ID       0x04b4
#define FX3_BOOTLOADER_ID   0x0080
#define FX3_PROD_ID         0x00f3

#define DOMESDAY_VENDOR_ID  0x1d50
#define DOMESDAY_PROD_ID    0x603b

#define USB_TIMEOUT_MS      5000

#define FX3_DL_CMD           0xA0  // RAM download command
#define FX3_SPI_FLASH_CMD    0xC2  // Flash programmer write command
#define FX3_SPI_FLASH_ERASE  0xC4  // Flash programmer erase/status command
#define FX3_I2C_WRITE_CMD    0xBA  // Flash programmer I2C write
#define FX3_I2C_READ_CMD     0xBB  // Flash programmer I2C read/verify
#define MAX_WRITE_SIZE       2048
#define SPI_FLASH_PAGE_SIZE  256   // SPI flash page size for FX3
#define SPI_FLASH_SECTOR_SIZE (64 * 1024)
#define I2C_PAGE_SIZE        64
#define I2C_SLAVE_SIZE       (64 * 1024)
#define FLASH_PROG_MAGIC     "FX3PROG"
#define GET_LSW(x)           ((x) & 0xFFFF)
#define GET_MSW(x)           ((x) >> 16)

typedef struct {
    libusb_device_handle *handle;
    uint16_t vid;
    uint16_t pid;
    uint8_t bus;
    uint8_t addr;
    uint8_t dev_class;
    int is_bootloader;
    int index;
} fx3_device_t;

static fx3_device_t fx3_devices[16];
static int num_devices = 0;

/* Forward declarations */
int fx3_download_firmware(int device_idx, const char *filename);
int fx3_discover_devices(void);

/* I2C write via flash programmer */
static int fx3_i2c_write(libusb_device_handle *h, unsigned char *buf, int devAddr, int start, int len) {
    int r = 0;
    int index = start;
    unsigned short address = 0;
    int size;

    while (len > 0) {
        size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
        r = libusb_control_transfer(h, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
                                    FX3_I2C_WRITE_CMD, devAddr, address, &buf[index], size, USB_TIMEOUT_MS);
        if (r != size) {
            fprintf(stderr, "Error: I2C write failed\n");
            return -1;
        }

        address += size;
        index   += size;
        len     -= size;
    }

    return 0;
}

/* I2C read/verify via flash programmer */
static int fx3_i2c_read_verify(libusb_device_handle *h, unsigned char *expData, int devAddr, int len) {
    int r = 0;
    int index = 0;
    unsigned short address = 0;
    int size;
    unsigned char tmpBuf[MAX_WRITE_SIZE];

    while (len > 0) {
        size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
        r = libusb_control_transfer(h, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
                                    FX3_I2C_READ_CMD, devAddr, address, tmpBuf, size, USB_TIMEOUT_MS);
        if (r != size) {
            fprintf(stderr, "Error: I2C read failed\n");
            return -1;
        }

        if (memcmp(expData + index, tmpBuf, size) != 0) {
            fprintf(stderr, "Error: Failed to read expected data from I2C EEPROM\n");
            return -2;
        }

        address += size;
        index   += size;
        len     -= size;
    }

    return 0;
}
/* Check if a handle is the Cypress flash programmer (secondary loader). */
static int is_flash_programmer(libusb_device_handle *handle) {
    unsigned char buf[8];
    int r = libusb_control_transfer(handle,
                                    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
                                    0xB0,
                                    0,
                                    0,
                                    buf,
                                    sizeof(buf),
                                    USB_TIMEOUT_MS);
    if (r == (int)sizeof(buf) && strncmp((char *)buf, FLASH_PROG_MAGIC, 7) == 0) {
        return 1;
    }
    return 0;
}

/* Locate cyfxflashprog.img: env override then common relative paths. */
static char *find_flashprog_image(void) {
    const char *env = getenv("FX3_FLASH_PROG");
    const char *candidates[] = {
        env,
        "cyfxflashprog.img",
        "../cyfxflashprog.img",
        "../../../../../cyusb_linux/fx3_images/cyfxflashprog.img",
        "../../cyusb_linux/fx3_images/cyfxflashprog.img",
        "../fx3_images/cyfxflashprog.img",
        "../../fx3_images/cyfxflashprog.img",
    };
    struct stat st;

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        const char *path = candidates[i];
        if (!path) {
            continue;
        }
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            return strdup(path);
        }
    }

    return NULL;
}

/* Download flash programmer to RAM (from bootloader) and return handle to it. */
static int load_flash_programmer(int device_idx, libusb_device_handle **out_handle) {
    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    libusb_device_handle *h = fx3_devices[device_idx].handle;

    if (is_flash_programmer(h)) {
        *out_handle = h;
        return 0;
    }

    if (!fx3_devices[device_idx].is_bootloader) {
        fprintf(stderr, "Error: Device must be in bootloader mode to launch flash programmer\n");
        fprintf(stderr, "Please set PMODE jumper (J4) then power cycle\n");
        return -1;
    }

    char *img = find_flashprog_image();
    if (!img) {
        fprintf(stderr, "Error: cyfxflashprog.img not found. Set FX3_FLASH_PROG or place it near the binary.\n");
        return -1;
    }

    printf("Downloading flash programmer %s to device %d...\n", img, device_idx);
    int r = fx3_download_firmware(device_idx, img);
    free(img);
    if (r != 0) {
        fprintf(stderr, "Error: Failed to load flash programmer into RAM\n");
        return r;
    }

    /* Device will disconnect/re-enumerate as flash programmer. Refresh handles. */
    for (int i = 0; i < num_devices; i++) {
        if (fx3_devices[i].handle) {
            libusb_close(fx3_devices[i].handle);
            fx3_devices[i].handle = NULL;
        }
    }

    for (int attempt = 0; attempt < 10; attempt++) {
        sleep(1);
        if (fx3_discover_devices() < 0) {
            continue;
        }
        for (int i = 0; i < num_devices; i++) {
            if (fx3_devices[i].vid == FX3_VENDOR_ID && is_flash_programmer(fx3_devices[i].handle)) {
                *out_handle = fx3_devices[i].handle;
                printf("Found FX3 flash programmer (device %d)\n", i);
                return 0;
            }
        }
    }

    fprintf(stderr, "Error: Flash programmer did not enumerate\n");
    return -1;
}

/* Detect if device is in bootloader mode by reading product string */
int is_fx3_bootloader(libusb_device_handle *handle) {
    unsigned char product_string[256];
    int ret;

    /* Try to read the product string descriptor (index 2) */
    ret = libusb_get_string_descriptor_ascii(handle, 2, product_string, sizeof(product_string));
    
    if (ret > 0) {
        /* Bootloader uses "WestBridge" as product name */
        if (strncmp((char *)product_string, "WestBridge", 10) == 0) {
            return 1;
        }
    }
    
    return 0;
}

/* Discover all FX3 devices connected to the system */
int fx3_discover_devices(void) {
    libusb_device **devices;
    libusb_device_handle *handle = NULL;
    struct libusb_device_descriptor desc;
    ssize_t num, i;

    num = libusb_get_device_list(NULL, &devices);
    if (num < 0) {
        fprintf(stderr, "Failed to get USB device list\n");
        return -1;
    }

    num_devices = 0;

    for (i = 0; i < num && num_devices < 16; i++) {
        libusb_get_device_descriptor(devices[i], &desc);

        /* Look for any Cypress FX3 device (bootloader/app/flashprog) or Domesday Duplicator firmware */
        if ((desc.idVendor == FX3_VENDOR_ID) ||
            (desc.idVendor == DOMESDAY_VENDOR_ID && desc.idProduct == DOMESDAY_PROD_ID)) {
            
            if (libusb_open(devices[i], &handle) == 0) {
                fx3_devices[num_devices].handle = handle;
                fx3_devices[num_devices].vid = desc.idVendor;
                fx3_devices[num_devices].pid = desc.idProduct;
                fx3_devices[num_devices].bus = libusb_get_bus_number(devices[i]);
                fx3_devices[num_devices].addr = libusb_get_device_address(devices[i]);
                fx3_devices[num_devices].dev_class = desc.bDeviceClass;
                fx3_devices[num_devices].is_bootloader = is_fx3_bootloader(handle);
                fx3_devices[num_devices].index = num_devices;
                num_devices++;
            }
        }
    }

    libusb_free_device_list(devices, 1);
    return num_devices;
}

/* List all discovered FX3 devices */
void fx3_list_devices(void) {
    int i;

    if (num_devices == 0) {
        printf("No FX3 devices found\n");
        return;
    }

    printf("Found %d FX3 device(s):\n\n", num_devices);
    for (i = 0; i < num_devices; i++) {
        /* Detect mode based on bootloader/flashprog response */
        const char *mode = fx3_devices[i].is_bootloader ? "Bootloader" : "Application";
        if (!fx3_devices[i].is_bootloader && is_flash_programmer(fx3_devices[i].handle)) {
            mode = "FlashProgrammer";
        }
        const char *product = "FX3";
        if (fx3_devices[i].vid == DOMESDAY_VENDOR_ID && fx3_devices[i].pid == DOMESDAY_PROD_ID) {
            product = "Domesday Duplicator";
        }
        printf("[%d] VID:PID=%04x:%04x Bus=%03d Device=%03d Mode=%s (%s)\n",
               i,
               fx3_devices[i].vid,
               fx3_devices[i].pid,
               fx3_devices[i].bus,
               fx3_devices[i].addr,
               mode,
               product);
    }
    printf("\n");
}

/* Download firmware to FX3 device */
int fx3_download_firmware(int device_idx, const char *filename) {
    int fd, ret, bytes_sent = 0;
    struct stat st;
    libusb_device_handle *handle;
    int size, len, chunk_size;
    uint32_t address;
    uint8_t *pdata;
    uint8_t *firmware_buf;
    ssize_t bytes_read;
    int remaining;

    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    handle = fx3_devices[device_idx].handle;

    /* Open firmware file */
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open firmware file");
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        perror("Failed to stat firmware file");
        close(fd);
        return -1;
    }

    size = st.st_size;
    printf("Uploading %s (%d bytes) to FX3 device %d...\n", filename, size, device_idx);
    printf("Target device: VID:PID=%04x:%04x\n", fx3_devices[device_idx].vid, fx3_devices[device_idx].pid);

    /* Allocate buffer for entire firmware */
    firmware_buf = malloc(size);
    if (!firmware_buf) {
        fprintf(stderr, "Failed to allocate memory for firmware\n");
        close(fd);
        return -1;
    }

    /* Read entire file into buffer */
    bytes_read = read(fd, firmware_buf, size);
    close(fd);
    
    if (bytes_read != size) {
        fprintf(stderr, "Failed to read entire firmware file (read %ld of %d bytes)\n", bytes_read, size);
        free(firmware_buf);
        return -1;
    }

    /* Parse firmware sections and download each one */
    /* Firmware format:  CY header (2 bytes) + bImageCTL (1) + bImageType (1) + sections with address and data */
    if (size < 4 || firmware_buf[0] != 'C' || firmware_buf[1] != 'Y') {
        fprintf(stderr, "Invalid firmware file: missing CY header\n");
        free(firmware_buf);
        return -1;
    }

    /* Check image control byte (bit 0 = executable code flag) */
    if (firmware_buf[2] & 0x01) {
        fprintf(stderr, "Invalid firmware: image does not contain executable code\n");
        free(firmware_buf);
        return -1;
    }

    /* Check image type byte (0xB0 = normal firmware with checksum) */
    if (firmware_buf[3] != 0xB0) {
        fprintf(stderr, "Invalid firmware: not a normal FW binary with checksum (got 0x%02x)\n", firmware_buf[3]);
        free(firmware_buf);
        return -1;
    }

    pdata = firmware_buf + 4;
    remaining = size - 4;
    bytes_sent = 0;

    while (remaining >= 4) {
        /* Read length (4 bytes, little-endian) */
        uint32_t *plen = (uint32_t *)pdata;
        len = *plen;
        pdata += 4;
        remaining -= 4;

        if (len == 0) {
            /* End marker, read program entry address */
            if (remaining >= 4) {
                uint32_t *paddr = (uint32_t *)pdata;
                address = *paddr;
                printf("\nProgram entry address: 0x%08x\n", address);
                pdata += 4;
                remaining -= 4;

                /* Send entry address with no data to start execution */
                ret = libusb_control_transfer(handle,
                                             LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
                                             FX3_DL_CMD,
                                             GET_LSW(address),
                                             GET_MSW(address),
                                             NULL,
                                             0,
                                             USB_TIMEOUT_MS);
                if (ret < 0) {
                    fprintf(stderr, "\nError sending program entry: %s\n", libusb_error_name(ret));
                }
            }
            break;
        }

        /* Read address (4 bytes, little-endian) */
        if (remaining < 4) break;
        uint32_t *paddr = (uint32_t *)pdata;
        address = *paddr;
        pdata += 4;
        remaining -= 4;

        /* Read data section */
        if (remaining < len * 4) break;
        uint8_t *section_data = pdata;
        pdata += len * 4;
        remaining -= len * 4;

        /* Send data in chunks */
        int section_bytes = len * 4;
        int section_offset = 0;

        while (section_offset < section_bytes) {
            chunk_size = (section_bytes - section_offset > MAX_WRITE_SIZE) ?
                        MAX_WRITE_SIZE : (section_bytes - section_offset);

            ret = libusb_control_transfer(handle,
                                         LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
                                         FX3_DL_CMD,
                                         GET_LSW(address + section_offset),
                                         GET_MSW(address + section_offset),
                                         section_data + section_offset,
                                         chunk_size,
                                         USB_TIMEOUT_MS);

            if (ret < 0) {
                fprintf(stderr, "\nUSB transfer failed at offset %d (0x%x): %s\n",
                       bytes_sent + section_offset, bytes_sent + section_offset,
                       libusb_error_name(ret));
                return -1;
            }

            section_offset += ret;
            bytes_sent += ret;
            printf(".");
            fflush(stdout);
        }
    }

    printf("\n");
    printf("Successfully uploaded %d bytes to FX3 device %d\n", bytes_sent, device_idx);
    return 0;
}

/* Program firmware to I2C EEPROM on FX3 device using the flash programmer */
int fx3_program_prom(int device_idx, const char *filename) {
    int fd, ret;
    struct stat st;
    libusb_device_handle *prog_handle = NULL;
    int size;
    uint8_t *firmware_buf;
    ssize_t bytes_read;
    int bytes_to_write;
    int bytes_sent = 0;
    int address = 0; /* I2C slave base address increments every 64KB */

    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    /* Ensure flash programmer is running (loads cyfxflashprog.img if needed) */
    if (load_flash_programmer(device_idx, &prog_handle) != 0) {
        return -1;
    }

    /* Open firmware file */
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open firmware file");
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        perror("Failed to stat firmware file");
        close(fd);
        return -1;
    }

    size = st.st_size;
    bytes_to_write = ((size + I2C_PAGE_SIZE - 1) / I2C_PAGE_SIZE) * I2C_PAGE_SIZE;
    printf("Programming %s (%d bytes, padded to %d) to FX3 I2C EEPROM...\n", filename, size, bytes_to_write);

    firmware_buf = calloc(1, bytes_to_write);
    if (!firmware_buf) {
        fprintf(stderr, "Failed to allocate memory for firmware\n");
        close(fd);
        return -1;
    }

    bytes_read = read(fd, firmware_buf, size);
    close(fd);
    if (bytes_read != size) {
        fprintf(stderr, "Failed to read entire firmware file (read %ld of %d bytes)\n", bytes_read, size);
        free(firmware_buf);
        return -1;
    }

    /* Program firmware to I2C EEPROM in chunks, rolling the I2C slave address every 64KB */
    int remaining = bytes_to_write;
    int offset = 0;
    while (remaining > 0) {
        int chunk = (remaining > I2C_SLAVE_SIZE) ? I2C_SLAVE_SIZE : remaining;
        int devAddr = address; /* flash programmer handles address translation */

        ret = fx3_i2c_write(prog_handle, firmware_buf, devAddr, offset, chunk);
        if (ret != 0) {
            fprintf(stderr, "Error: I2C write failed at devAddr %d offset %d\n", devAddr, offset);
            free(firmware_buf);
            return -1;
        }

        ret = fx3_i2c_read_verify(prog_handle, firmware_buf + offset, devAddr, chunk);
        if (ret != 0) {
            fprintf(stderr, "Error: I2C verify failed at devAddr %d offset %d\n", devAddr, offset);
            free(firmware_buf);
            return -1;
        }

        offset += chunk;
        remaining -= chunk;
        address++;
        bytes_sent += chunk;
        printf(".");
        fflush(stdout);
    }

    free(firmware_buf);
    printf("\nSuccessfully programmed %d bytes to FX3 I2C EEPROM\n", bytes_sent);
    return 0;
}

/* Verify firmware on FX3 device */
int fx3_verify_firmware(int device_idx, const char *filename) {
    int fd, ret;
    struct stat st;
    libusb_device_handle *prog_handle = NULL;
    int size;
    uint8_t *firmware_buf;
    ssize_t bytes_read;
    int bytes_to_verify;
    int address = 0; /* I2C slave base address increments every 64KB */

    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    if (filename == NULL) {
        fprintf(stderr, "Verify requested but no firmware file provided. Use -p <file> -v.\n");
        return -1;
    }

    /* Ensure flash programmer is running (loads cyfxflashprog.img if needed) */
    if (load_flash_programmer(device_idx, &prog_handle) != 0) {
        return -1;
    }

    /* Open firmware file */
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open firmware file");
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        perror("Failed to stat firmware file");
        close(fd);
        return -1;
    }

    size = st.st_size;
    bytes_to_verify = ((size + I2C_PAGE_SIZE - 1) / I2C_PAGE_SIZE) * I2C_PAGE_SIZE;
    printf("Verifying %s against FX3 I2C EEPROM (%d bytes, padded to %d)...\n", filename, size, bytes_to_verify);

    firmware_buf = calloc(1, bytes_to_verify);
    if (!firmware_buf) {
        fprintf(stderr, "Failed to allocate memory for firmware\n");
        close(fd);
        return -1;
    }

    bytes_read = read(fd, firmware_buf, size);
    close(fd);
    if (bytes_read != size) {
        fprintf(stderr, "Failed to read entire firmware file (read %ld of %d bytes)\n", bytes_read, size);
        free(firmware_buf);
        return -1;
    }

    /* Verify EEPROM contents in 64KB chunks to match address rollover */
    int remaining = bytes_to_verify;
    int offset = 0;
    while (remaining > 0) {
        int chunk = (remaining > I2C_SLAVE_SIZE) ? I2C_SLAVE_SIZE : remaining;
        int devAddr = address; /* flash programmer handles address translation */

        ret = fx3_i2c_read_verify(prog_handle, firmware_buf + offset, devAddr, chunk);
        if (ret != 0) {
            fprintf(stderr, "Error: Verify failed at devAddr %d offset %d\n", devAddr, offset);
            free(firmware_buf);
            return -1;
        }

        offset += chunk;
        remaining -= chunk;
        address++;
        printf(".");
        fflush(stdout);
    }

    free(firmware_buf);
    printf("\nVerification successful: EEPROM matches %s\n", filename);
    return 0;
}

/* Reset FX3 device */
int fx3_reset_device(int device_idx) {
    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    printf("Device will reset automatically after firmware download completes\n");
    sleep(2);
    return 0;
}

/* Print usage information */
void print_usage(const char *prog) {
    printf("FX3 Firmware Programmer\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -l                 List connected FX3 devices\n");
    printf("  -d DEVICE_IDX      Target device index (default: 0)\n");
    printf("  -u FIRMWARE_FILE   Upload firmware to device RAM\n");
    printf("  -p FIRMWARE_FILE   Program firmware to SPI flash (persistent)\n");
    printf("  -v                 Verify EEPROM contents against firmware file (use with -p)\n");
    printf("  -r                 Reset device\n");
    printf("  -h                 Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -l                          List devices\n", prog);
    printf("  %s -u firmware.img             Upload firmware to RAM on device 0\n", prog);
    printf("  %s -p firmware.img             Program firmware to SPI flash on device 0\n", prog);
    printf("  %s -d 1 -u firmware.img        Upload firmware to RAM on device 1\n", prog);
    printf("  %s -d 0 -v                     Verify device 0 firmware\n", prog);
    printf("  %s -d 0 -r                     Reset device 0\n", prog);
    printf("\n");
    printf("Notes:\n");
    printf("  - SPI flash programming requires device to be in bootloader mode\n");
    printf("  - Set the PMODE jumper (J4) and power cycle to enter bootloader\n");
    printf("  - SPI flash-programmed firmware persists across power cycles\n");
}

int main(int argc, char *argv[]) {
    int opt, device_idx = 0, ret = 0;
    const char *firmware_file = NULL;
    const char *prom_file = NULL;
    int list_devices_flag = 0;
    int verify_flag = 0;
    int reset_flag = 0;
    int upload_flag = 0;
    int prom_flag = 0;

    /* Initialize libusb */
    if (libusb_init(NULL) < 0) {
        fprintf(stderr, "Failed to initialize libusb\n");
        return 1;
    }

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "ld:u:p:vrh")) != -1) {
        switch (opt) {
        case 'l':
            list_devices_flag = 1;
            break;
        case 'd':
            device_idx = atoi(optarg);
            break;
        case 'u':
            firmware_file = optarg;
            upload_flag = 1;
            break;
        case 'p':
            prom_file = optarg;
            prom_flag = 1;
            break;
        case 'v':
            verify_flag = 1;
            break;
        case 'r':
            reset_flag = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            libusb_exit(NULL);
            return 0;
        default:
            fprintf(stderr, "Unknown option: -%c\n", opt);
            print_usage(argv[0]);
            libusb_exit(NULL);
            return 1;
        }
    }

    /* Show help if no options provided */
    if (argc == 1) {
        print_usage(argv[0]);
        libusb_exit(NULL);
        return 0;
    }

    /* Discover devices */
    if (fx3_discover_devices() < 0) {
        fprintf(stderr, "Failed to discover devices\n");
        libusb_exit(NULL);
        return 1;
    }

    /* Execute operations */
    if (list_devices_flag) {
        fx3_list_devices();
    }

    if (upload_flag && firmware_file) {
        ret = fx3_download_firmware(device_idx, firmware_file);
    }

    if (prom_flag && prom_file) {
        ret = fx3_program_prom(device_idx, prom_file);
        if (verify_flag && ret == 0) {
            ret = fx3_verify_firmware(device_idx, prom_file);
        }
        if (ret == 0) {
            printf("Power cycle the device (remove J4/PMODE to boot from EEPROM)\n");
        }
    } else if (verify_flag) {
        fprintf(stderr, "Verify requires a firmware file. Use -p <file> -v to program and verify.\n");
        ret = -1;
    }

    if (reset_flag && ret == 0) {
        ret = fx3_reset_device(device_idx);
    }

    /* Cleanup */
    for (int i = 0; i < num_devices; i++) {
        if (fx3_devices[i].handle) {
            libusb_close(fx3_devices[i].handle);
            fx3_devices[i].handle = NULL;
        }
    }

    libusb_exit(NULL);
    return ret;
}

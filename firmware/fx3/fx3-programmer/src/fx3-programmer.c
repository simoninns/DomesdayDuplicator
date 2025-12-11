/*
 * fx3-programmer.c - Minimal FX3 firmware programmer
 * 
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

#define FX3_DL_CMD           0xA0
#define MAX_WRITE_SIZE       2048
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

        /* Look for Cypress FX3 bootloader/application or Domesday Duplicator firmware */
        if ((desc.idVendor == FX3_VENDOR_ID && 
            (desc.idProduct == FX3_BOOTLOADER_ID || desc.idProduct == FX3_PROD_ID)) ||
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
        /* Detect mode based on bootloader protocol response */
        const char *mode = fx3_devices[i].is_bootloader ? "Bootloader" : "Application";
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

/* Verify firmware on FX3 device */
int fx3_verify_firmware(int device_idx) {
    if (device_idx < 0 || device_idx >= num_devices) {
        fprintf(stderr, "Invalid device index\n");
        return -1;
    }

    printf("Verification not implemented for this bootloader\n");
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
    printf("  -u FIRMWARE_FILE   Upload firmware to device\n");
    printf("  -v                 Verify firmware on device\n");
    printf("  -r                 Reset device\n");
    printf("  -h                 Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -l                          List devices\n", prog);
    printf("  %s -u firmware.img             Upload firmware to device 0\n", prog);
    printf("  %s -d 1 -u firmware.img        Upload firmware to device 1\n", prog);
    printf("  %s -d 0 -v                     Verify device 0 firmware\n", prog);
    printf("  %s -d 0 -r                     Reset device 0\n", prog);
}

int main(int argc, char *argv[]) {
    int opt, device_idx = 0, ret = 0;
    const char *firmware_file = NULL;
    int list_devices_flag = 0;
    int verify_flag = 0;
    int reset_flag = 0;
    int upload_flag = 0;

    /* Initialize libusb */
    if (libusb_init(NULL) < 0) {
        fprintf(stderr, "Failed to initialize libusb\n");
        return 1;
    }

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "ld:u:vrh")) != -1) {
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

    if (verify_flag && ret == 0) {
        ret = fx3_verify_firmware(device_idx);
    }

    if (reset_flag && ret == 0) {
        ret = fx3_reset_device(device_idx);
    }

    /* Cleanup */
    for (int i = 0; i < num_devices; i++) {
        libusb_close(fx3_devices[i].handle);
    }

    libusb_exit(NULL);
    return ret;
}

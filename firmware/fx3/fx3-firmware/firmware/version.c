/*
 * version.c - Firmware version information
 * 
 * This file generates the product descriptor string that includes
 * the git commit hash and build date, allowing identification of the
 * firmware version running on the device.
 */

#include <stdint.h>
#include "version.h"

/* Maximum length for product string descriptor (in UTF-16 bytes) */
#define MAX_PRODUCT_STRING_LEN 64

/*
 * Generate product descriptor string with version information
 * Format: "Domesday Duplicator (commit-date)"
 */
static uint8_t product_string_buffer[MAX_PRODUCT_STRING_LEN] __attribute__((aligned(2)));

const uint8_t* get_product_descriptor_string(uint16_t* length) {
    /* Base product name */
    const char base[] = "Domesday Duplicator ";
    const char version_prefix = '(';
    const char version_suffix = ')';
    
    int pos = 0;
    int i;
    
    /* Copy base product name in UTF-16 format */
    for (i = 0; base[i] && pos < MAX_PRODUCT_STRING_LEN - 2; i++) {
        product_string_buffer[pos++] = base[i];
        product_string_buffer[pos++] = 0x00;
    }
    
    /* Add opening parenthesis */
    if (pos < MAX_PRODUCT_STRING_LEN - 2) {
        product_string_buffer[pos++] = version_prefix;
        product_string_buffer[pos++] = 0x00;
    }
    
    /* Add git commit string */
    const char* commit = FIRMWARE_GIT_COMMIT;
    for (i = 0; commit[i] && pos < MAX_PRODUCT_STRING_LEN - 2; i++) {
        product_string_buffer[pos++] = commit[i];
        product_string_buffer[pos++] = 0x00;
    }
    
    /* Add closing parenthesis */
    if (pos < MAX_PRODUCT_STRING_LEN - 2) {
        product_string_buffer[pos++] = version_suffix;
        product_string_buffer[pos++] = 0x00;
    }
    
    /* Set the descriptor size (includes length byte and descriptor type) */
    *length = pos + 2;  /* +2 for length and type bytes */
    
    return product_string_buffer;
}

/*
 * Generate the complete USB string descriptor with length and type
 */
void generate_product_descriptor(uint8_t* descriptor) {
    uint16_t string_length = 0;
    const uint8_t* string_data = get_product_descriptor_string(&string_length);
    
    /* Descriptor format: [size][type][data...] */
    descriptor[0] = string_length;
    descriptor[1] = 0x03;  /* String descriptor type */
    
    /* Copy the string data */
    int i;
    for (i = 0; i < string_length - 2 && i < MAX_PRODUCT_STRING_LEN; i++) {
        descriptor[2 + i] = string_data[i];
    }
}

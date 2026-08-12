/************************************************************************

    main.c

    fx3-mkimage - FX3 boot image builder for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2026 Simon Inns

    This file is part of Domesday Duplicator.

    Domesday Duplicator is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: simon.inns@gmail.com

************************************************************************/

#include "fx3-bootimage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FX3_MKIMAGE_VERSION
#define FX3_MKIMAGE_VERSION "unknown"
#endif

static void usage(FILE *f, const char *argv0)
{
    fprintf(f,
            "fx3-mkimage %s - build an FX3 boot image from a firmware ELF\n"
            "\n"
            "Usage:\n"
            "  %s -i <input.elf> -o <output.img> [options]\n"
            "\n"
            "Options:\n"
            "  -i <file>          input ELF file (required)\n"
            "  -o <file>          output boot image (required)\n"
            "  -i2cconf <hex>     bImageCTL byte, EEPROM size and bus speed (default 0x%02X)\n"
            "  -imgtype <hex>     bImageType byte (default 0x%02X, normal firmware)\n"
            "  -vectorload <y|n>  retain data in the 0x00-0x100 vector area (default no)\n"
            "  -v                 verbose: describe each section written\n"
            "  -h, --help         this help\n"
            "  --version          print the version and exit\n"
            "\n"
            "The image format is specified in Infineon AN76405, \"EZ-USB FX3/FX3S boot\n"
            "options\", section 4.4. This is a from-scratch GPLv3 implementation; it is\n"
            "not derived from the Cypress SDK tool it replaces.\n",
            FX3_MKIMAGE_VERSION, argv0, FX3_IMAGE_CTL_DEFAULT, FX3_IMAGE_TYPE_NORMAL);
}

/* Parse a byte written with or without a 0x prefix. Returns 0 on success. */
static int parse_byte(const char *s, uint8_t *out)
{
    char *end = NULL;
    unsigned long v;

    errno = 0;
    v = strtoul(s, &end, 16);

    if (errno != 0 || end == s || *end != '\0' || v > 0xFFu) return -1;

    *out = (uint8_t)v;
    return 0;
}

/* Read a whole file into memory. Returns NULL and reports on failure. */
static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "fx3-mkimage: cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "fx3-mkimage: cannot seek %s: %s\n", path, strerror(errno));
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len < 0) {
        fprintf(stderr, "fx3-mkimage: cannot size %s: %s\n", path, strerror(errno));
        fclose(f);
        return NULL;
    }
    rewind(f);

    uint8_t *buf = malloc((size_t)len ? (size_t)len : 1);
    if (buf == NULL) {
        fprintf(stderr, "fx3-mkimage: out of memory reading %s\n", path);
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "fx3-mkimage: short read on %s\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *size = (size_t)len;
    return buf;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "fx3-mkimage: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fwrite(data, 1, size, f) != size) {
        fprintf(stderr, "fx3-mkimage: short write on %s: %s\n", path, strerror(errno));
        fclose(f);
        return -1;
    }

    /* An image truncated by a full disk that is only noticed at close time
     * would be written to a device and brick it. Check. */
    if (fclose(f) != 0) {
        fprintf(stderr, "fx3-mkimage: cannot close %s: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

/* Re-walk the finished image to report it. Reading back what was written keeps
 * the verbose output honest rather than describing what was intended. */
static void describe(const uint8_t *img, size_t size)
{
    size_t pos = 4;
    unsigned n = 0;

    printf("  header: signature %c%c, bImageCTL 0x%02X, bImageType 0x%02X\n", img[0], img[1], img[2],
           img[3]);

    while (pos + 8 <= size) {
        uint32_t words = (uint32_t)img[pos] | ((uint32_t)img[pos + 1] << 8) |
                         ((uint32_t)img[pos + 2] << 16) | ((uint32_t)img[pos + 3] << 24);
        uint32_t addr = (uint32_t)img[pos + 4] | ((uint32_t)img[pos + 5] << 8) |
                        ((uint32_t)img[pos + 6] << 16) | ((uint32_t)img[pos + 7] << 24);
        pos += 8;

        if (words == 0) {
            printf("  termination: program entry 0x%08X\n", addr);
            break;
        }

        n++;
        printf("  section %u: %u words (%u bytes) at 0x%08X\n", n, words, words * 4u, addr);
        pos += (size_t)words * 4u;
    }

    printf("  %u section(s), %zu bytes total\n", n, size);
}

int main(int argc, char *argv[])
{
    const char *in_path = NULL;
    const char *out_path = NULL;
    uint8_t image_ctl = FX3_IMAGE_CTL_DEFAULT;
    uint8_t image_type = FX3_IMAGE_TYPE_NORMAL;
    int keep_vectors = 0;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            printf("fx3-mkimage %s\n", FX3_MKIMAGE_VERSION);
            return 0;
        }
        if (strcmp(a, "-v") == 0) {
            verbose = 1;
            continue;
        }

        /* Everything below takes a value. */
        if (i + 1 >= argc) {
            fprintf(stderr, "fx3-mkimage: %s needs a value\n", a);
            return 1;
        }
        const char *val = argv[++i];

        if (strcmp(a, "-i") == 0) {
            in_path = val;
        } else if (strcmp(a, "-o") == 0) {
            out_path = val;
        } else if (strcmp(a, "-i2cconf") == 0) {
            if (parse_byte(val, &image_ctl) != 0) {
                fprintf(stderr, "fx3-mkimage: -i2cconf wants a hex byte, got '%s'\n", val);
                return 1;
            }
        } else if (strcmp(a, "-imgtype") == 0) {
            if (parse_byte(val, &image_type) != 0) {
                fprintf(stderr, "fx3-mkimage: -imgtype wants a hex byte, got '%s'\n", val);
                return 1;
            }
        } else if (strcmp(a, "-vectorload") == 0) {
            if (strcmp(val, "yes") == 0 || strcmp(val, "y") == 0) {
                keep_vectors = 1;
            } else if (strcmp(val, "no") == 0 || strcmp(val, "n") == 0) {
                keep_vectors = 0;
            } else {
                fprintf(stderr, "fx3-mkimage: -vectorload wants yes or no, got '%s'\n", val);
                return 1;
            }
        } else {
            fprintf(stderr, "fx3-mkimage: unknown option '%s'\n", a);
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if (in_path == NULL || out_path == NULL) {
        fprintf(stderr, "fx3-mkimage: both -i and -o are required\n\n");
        usage(stderr, argv[0]);
        return 1;
    }

    size_t elf_size = 0;
    uint8_t *elf = read_file(in_path, &elf_size);
    if (elf == NULL) return 1;

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n_segs = 0;
    uint32_t entry = 0;

    fx3_status st = fx3_elf_parse(elf, elf_size, segs, FX3_MAX_SEGMENTS, &n_segs, &entry);
    if (st != FX3_OK) {
        fprintf(stderr, "fx3-mkimage: %s: %s\n", in_path, fx3_strerror(st));
        free(elf);
        return 1;
    }

    uint8_t *img = NULL;
    size_t img_size = 0;

    st = fx3_build_image(segs, n_segs, entry, image_ctl, image_type, keep_vectors, &img, &img_size);
    if (st != FX3_OK) {
        fprintf(stderr, "fx3-mkimage: %s: %s\n", in_path, fx3_strerror(st));
        free(elf);
        return 1;
    }

    if (verbose) {
        printf("fx3-mkimage: %s -> %s\n", in_path, out_path);
        describe(img, img_size);
    }

    int rc = write_file(out_path, img, img_size);

    free(img);
    free(elf);

    return rc == 0 ? 0 : 1;
}

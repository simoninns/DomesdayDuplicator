/************************************************************************

    fx3-bootimage.c

    FX3 boot image construction for the Domesday Duplicator
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

#include <stdlib.h>
#include <string.h>

/* ELF32 header and program header offsets, from the System V ABI. Spelled out
 * as offsets rather than as a struct so the parser does not depend on the host
 * compiler's padding, and works the same on any host. */
#define EI_NIDENT 16
#define ELF_OFF_TYPE 0x10    /* Elf32_Half  e_type      */
#define ELF_OFF_MACHINE 0x12 /* Elf32_Half  e_machine   */
#define ELF_OFF_ENTRY 0x18   /* Elf32_Addr  e_entry     */
#define ELF_OFF_PHOFF 0x1C   /* Elf32_Off   e_phoff     */
#define ELF_OFF_PHENTSIZE 0x2A
#define ELF_OFF_PHNUM 0x2C
#define ELF_HEADER_SIZE 0x34

#define PH_OFF_TYPE 0x00   /* Elf32_Word p_type   */
#define PH_OFF_OFFSET 0x04 /* Elf32_Off  p_offset */
#define PH_OFF_PADDR 0x0C  /* Elf32_Addr p_paddr  */
#define PH_OFF_FILESZ 0x10 /* Elf32_Word p_filesz */
#define PH_OFF_MEMSZ 0x14  /* Elf32_Word p_memsz  */
#define PH_MIN_SIZE 0x20

#define PT_LOAD 1
#define ET_EXEC 2
#define EM_ARM 40

#define ELFCLASS32 1
#define ELFDATA2LSB 1

/* Explicit little-endian accessors. The image format is little-endian
 * regardless of the host, so nothing here casts a pointer to a wider type --
 * that would be both an alignment fault waiting to happen and wrong on a
 * big-endian build host. */
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

const char *fx3_strerror(fx3_status status)
{
    switch (status) {
    case FX3_OK:
        return "success";
    case FX3_E_TRUNCATED:
        return "the ELF file is truncated, or a program header points outside it";
    case FX3_E_NOT_ELF:
        return "not an ELF file (bad magic)";
    case FX3_E_NOT_ELF32_LSB:
        return "not a 32-bit little-endian ELF file";
    case FX3_E_NOT_ARM:
        return "not an ARM executable";
    case FX3_E_NOT_EXEC:
        return "not an executable ELF file (e_type is not ET_EXEC)";
    case FX3_E_TOO_MANY_SEGMENTS:
        return "the ELF file declares more loadable segments than this tool supports";
    case FX3_E_NO_SEGMENTS:
        return "the ELF file contains nothing to load";
    case FX3_E_NOMEM:
        return "out of memory";
    }
    return "unknown error";
}

fx3_status fx3_elf_parse(const uint8_t *elf, size_t elf_size, fx3_segment *segs, size_t max_segs,
                         size_t *n_segs, uint32_t *entry)
{
    *n_segs = 0;

    if (elf_size < ELF_HEADER_SIZE) return FX3_E_TRUNCATED;
    if (elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') return FX3_E_NOT_ELF;
    if (elf[4] != ELFCLASS32 || elf[5] != ELFDATA2LSB) return FX3_E_NOT_ELF32_LSB;
    if (rd16(elf + ELF_OFF_TYPE) != ET_EXEC) return FX3_E_NOT_EXEC;
    if (rd16(elf + ELF_OFF_MACHINE) != EM_ARM) return FX3_E_NOT_ARM;

    *entry = rd32(elf + ELF_OFF_ENTRY);

    uint32_t phoff = rd32(elf + ELF_OFF_PHOFF);
    uint16_t phentsize = rd16(elf + ELF_OFF_PHENTSIZE);
    uint16_t phnum = rd16(elf + ELF_OFF_PHNUM);

    if (phnum == 0 || phentsize < PH_MIN_SIZE) return FX3_E_TRUNCATED;

    /* Bounds-check the whole program header table before walking it. Done in
     * size_t arithmetic so a hostile phoff/phnum cannot wrap. */
    if ((size_t)phoff > elf_size) return FX3_E_TRUNCATED;
    if ((size_t)phnum > (elf_size - phoff) / phentsize) return FX3_E_TRUNCATED;

    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *ph = elf + phoff + (size_t)i * phentsize;

        if (rd32(ph + PH_OFF_TYPE) != PT_LOAD) continue;

        uint32_t p_offset = rd32(ph + PH_OFF_OFFSET);
        uint32_t p_paddr = rd32(ph + PH_OFF_PADDR);
        uint32_t p_filesz = rd32(ph + PH_OFF_FILESZ);
        uint32_t p_memsz = rd32(ph + PH_OFF_MEMSZ);

        /* A segment with no memory footprint contributes nothing. */
        if (p_memsz == 0) continue;

        /* .bss is the legitimate case for memsz > filesz; the reverse is a
         * malformed file. */
        if (p_filesz > p_memsz) return FX3_E_TRUNCATED;

        if ((size_t)p_offset > elf_size) return FX3_E_TRUNCATED;
        if ((size_t)p_filesz > elf_size - p_offset) return FX3_E_TRUNCATED;

        if (*n_segs >= max_segs) return FX3_E_TOO_MANY_SEGMENTS;

        segs[*n_segs].address = p_paddr;
        segs[*n_segs].data = elf + p_offset;
        segs[*n_segs].file_size = p_filesz;
        segs[*n_segs].mem_size = p_memsz;
        (*n_segs)++;
    }

    if (*n_segs == 0) return FX3_E_NO_SEGMENTS;

    return FX3_OK;
}

/* Round up to the next multiple of four. The format counts sections in 32-bit
 * words, so a section is always a whole number of words. */
static size_t round_up4(size_t n)
{
    return (n + 3u) & ~(size_t)3u;
}

/*
 * Drop whatever part of a segment falls inside the vector area. Returns 0 if
 * nothing of the segment survives.
 */
static int trim_vectors(fx3_segment *seg)
{
    if (seg->address >= FX3_VECTOR_AREA_END) return 1;

    uint32_t skip = FX3_VECTOR_AREA_END - seg->address;

    if ((size_t)skip >= seg->mem_size) return 0;

    seg->data += (seg->file_size < skip) ? seg->file_size : skip;
    seg->file_size = (seg->file_size > skip) ? seg->file_size - skip : 0;
    seg->mem_size -= skip;
    seg->address = FX3_VECTOR_AREA_END;

    return 1;
}

fx3_status fx3_build_image(const fx3_segment *segs, size_t n_segs, uint32_t entry,
                           uint8_t image_ctl, uint8_t image_type, int keep_vectors, uint8_t **out,
                           size_t *out_size)
{
    fx3_segment work[FX3_MAX_SEGMENTS];
    size_t n_work = 0;

    if (n_segs > FX3_MAX_SEGMENTS) return FX3_E_TOO_MANY_SEGMENTS;

    for (size_t i = 0; i < n_segs; i++) {
        fx3_segment seg = segs[i];
        if (!keep_vectors && !trim_vectors(&seg)) continue;
        work[n_work++] = seg;
    }

    if (n_work == 0) return FX3_E_NO_SEGMENTS;

    /* Size the output: 4-byte header, then per section 8 bytes of length and
     * address plus its word-aligned payload, then the 8-byte termination
     * record and the 4-byte checksum. */
    size_t total = 4;
    for (size_t i = 0; i < n_work; i++) {
        size_t payload = round_up4(work[i].mem_size);
        size_t sections = (payload + FX3_MAX_SECTION_BYTES - 1) / FX3_MAX_SECTION_BYTES;
        total += sections * 8 + payload;
    }
    total += 8 + 4;

    uint8_t *img = calloc(1, total);
    if (img == NULL) return FX3_E_NOMEM;

    img[0] = FX3_SIGNATURE_0;
    img[1] = FX3_SIGNATURE_1;
    img[2] = image_ctl;
    img[3] = image_type;

    size_t pos = 4;
    uint32_t checksum = 0;

    for (size_t i = 0; i < n_work; i++) {
        uint32_t address = work[i].address;
        size_t remaining = round_up4(work[i].mem_size);
        size_t copied = 0;

        while (remaining > 0) {
            size_t chunk = (remaining > FX3_MAX_SECTION_BYTES) ? FX3_MAX_SECTION_BYTES : remaining;

            wr32(img + pos, (uint32_t)(chunk / 4));
            wr32(img + pos + 4, address);
            pos += 8;

            /* The buffer came from calloc, so the tail beyond file_size --
             * .bss, and any padding up to the word boundary -- is already
             * zero. Only the bytes actually present in the ELF are copied. */
            if (copied < work[i].file_size) {
                size_t avail = work[i].file_size - copied;
                size_t n = (avail < chunk) ? avail : chunk;
                memcpy(img + pos, work[i].data + copied, n);
            }

            for (size_t w = 0; w < chunk; w += 4)
                checksum += rd32(img + pos + w);

            pos += chunk;
            copied += chunk;
            address += (uint32_t)chunk;
            remaining -= chunk;
        }
    }

    /* Termination record: a zero-length section whose address field carries the
     * program entry point (AN76405 Table 14). */
    wr32(img + pos, 0);
    wr32(img + pos + 4, entry);
    pos += 8;

    /* "The checksum will not include the dLength, dAddress, and Image Header"
     * -- AN76405 section 4.4. Sum of the section payload words only, 32-bit
     * unsigned, wrapping. */
    wr32(img + pos, checksum);
    pos += 4;

    *out = img;
    *out_size = pos;

    return FX3_OK;
}

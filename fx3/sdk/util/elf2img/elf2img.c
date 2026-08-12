/*
## ===========================
##
##  Copyright Cypress Semiconductor Corporation, 2010-2023,
##  All Rights Reserved
##  UNPUBLISHED, LICENSED SOFTWARE.
##
##  CONFIDENTIAL AND PROPRIETARY INFORMATION
##  WHICH IS THE PROPERTY OF CYPRESS.
##
##  Use of this file is governed
##  by the license agreement included in the file
##
##     <install>/license/license.txt
##
##  where <install> is the Cypress software
##  installation root directory path.
##
## ===========================
 */

/* Summary
   Program to convert an ARM specific executable ELF image into the img
   format accepted by the USB 3.0 device boot loader.
   Invoke "elf2img -h" for usage syntax.

   Note
   This program currently works only on 32-bit little endian architectures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>

#define CY_INTVECTOR_AREA_SIZE  (0x100)

typedef struct ElfHeader
{
    unsigned char  ident[16];
    unsigned short type;
    unsigned short machine;
    unsigned int   version;
    unsigned int   entry;
    unsigned int   phoff;
    unsigned int   shoff;
    unsigned int   flags;
    unsigned short ehsize;
    unsigned short phentsize;
    unsigned short phnum;
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
} ElfHeader;

typedef struct ProgramHeader
{
    unsigned int   type;
    unsigned int   offset;
    unsigned int   vaddr;
    unsigned int   paddr;
    unsigned int   filesize;
    unsigned int   memsize;
    unsigned int   flags;
    unsigned int   align;
} ProgramHeader;

#define PT_LOAD         (1)

#define ERREXIT(...)                                    \
{                                                       \
    fprintf (stderr, __VA_ARGS__);                      \
    return (-1);                                        \
}

const unsigned int I2cDeviceSizes[8] = {
    0x0100,     /* Dev size encoding of 0 => 1 KB. */
    0x0200,     /* Dev size encoding of 1 => 2 KB. */
    0x0400,     /* Dev size encoding of 2 => 4 KB. */
    0x0800,     /* Dev size encoding of 3 => 8 KB. */
    0x1000,     /* Dev size encoding of 4 => 16 KB. */
    0x2000,     /* Dev size encoding of 5 => 32 KB. */
    0x4000,     /* Dev size encoding of 6 => 64 KB. */
    0x4000      /* Maximum segment size is 64 KB. */
};

/* Global variables. */
int          verbose        = 0;
unsigned int checksum       = 0;
unsigned int i2cDevSize     = 0x4000;  /* 64 KB by default. */
int          loadIntVectors = 0;

int
CheckElfHeader (
        ElfHeader    *elfHdr)
{
    /* Decode the ELF header and print file information. */
    if (elfHdr)
    {
        if ((elfHdr->ident[0] != 0x7F) || (elfHdr->ident[1] != 'E') || (elfHdr->ident[2] != 'L') ||
                (elfHdr->ident[3] != 'F'))
            ERREXIT ("Invalid ELF header\n");

        if (elfHdr->ident[4] != 1)
            ERREXIT ("Unknown object type\n");

        if (verbose)
        {
            if (elfHdr->ident[5] == 1)
                fprintf (stderr, "Endian-ness    : Little\n");
            else if (elfHdr->ident[5] == 2)
                fprintf (stderr, "Endian-ness    : Big\n");
            else
                ERREXIT ("Bad endian setting\n");
        }
        else
        {
            if ((elfHdr->ident[5] != 1) && (elfHdr->ident[5] != 2))
                ERREXIT ("Bad endian setting\n");
        }

        if ((elfHdr->version != 1) || (elfHdr->ident[6] != 1))
            ERREXIT ("Invalid ELF version\n");

        if (elfHdr->type != 2)
            ERREXIT ("Not an executable image\n");

        if (elfHdr->machine != 40)
            ERREXIT ("Machine type is not ARM\n");

        if (verbose)
        {
            fprintf (stderr, "Entry point    : 0x%08x\n", elfHdr->entry);
            fprintf (stderr, "Program header table offset: 0x%08x\n", elfHdr->phoff);
            fprintf (stderr, "Program header count       : %d\n", elfHdr->phnum);
            fprintf (stderr, "Program header entry size  : 0x%04x\n", elfHdr->phentsize);
        }
    }
    else
        ERREXIT ("Bad parameter\n");

    return (0);
}

int
ProcessProgHeader (
        FILE          *fpElf,
        ProgramHeader *progHdr,
        FILE          *fpImg)
{
    unsigned int buffer[2048];
    unsigned int memSz, fileSz, secStart;
    unsigned int loadSz, validSz;
    unsigned int offset;

    if (progHdr)
    {
        if (progHdr->type == PT_LOAD)
        {
            if (((progHdr->memsize & 0x03) != 0) || ((progHdr->filesize & 0x03) != 0))
            {
                printf ("Warning: Size of the section starting at address 0x%08x is a non-multiple of 4 bytes.\n",
                        progHdr->vaddr);
                printf ("         Please verify the settings in your linker script.\n");
            }

            memSz    = (progHdr->memsize + 3) / 4;
            fileSz   = (progHdr->filesize + 3) / 4;
            secStart = progHdr->vaddr;

            /* Write the segment header to the img file. */
            if ((secStart < CY_INTVECTOR_AREA_SIZE) && (loadIntVectors == 0))
            {
                printf ("Note: %d bytes of interrupt vector code have been removed from the image.\n",
                        (CY_INTVECTOR_AREA_SIZE - secStart));
                printf ("      Use the \"-vectorload yes\" option to retain this code.\n\n");

                if (fileSz > (CY_INTVECTOR_AREA_SIZE - secStart) / 4)
                {
                    progHdr->offset += (CY_INTVECTOR_AREA_SIZE - secStart);
                    memSz           -= (CY_INTVECTOR_AREA_SIZE - secStart) / 4;
                    fileSz          -= (CY_INTVECTOR_AREA_SIZE - secStart) / 4;
                    secStart         = CY_INTVECTOR_AREA_SIZE;
                }
                else
                {
                    /* This section can be skipped as it is completely contained in the initial 256 bytes. */
                    return 0;
                }
            }

            offset = progHdr->offset;
            fseek (fpElf, offset, SEEK_SET);

            while (memSz)
            {
                if (memSz > i2cDevSize)
                {
                    loadSz = i2cDevSize;
                    memSz  -= loadSz;
                }
                else
                {
                    loadSz = memSz;
                    memSz  = 0;
                }

                if (loadSz > fileSz)
                    validSz = fileSz;
                else
                {
                    validSz = loadSz;
                    fileSz -= validSz;
                }

                if (verbose)
                {
                    fprintf (stderr, "Copying data from program header:\n");
                    fprintf (stderr, "\tAddr=0x%08x Size(words)=0x%08x FileSize=0x%08x Offset=0x%08x Flags=0x%08x\n",
                            secStart, loadSz, validSz, offset, progHdr->flags);
                }

                fwrite (&loadSz,  4, 1, fpImg);
                fwrite (&secStart, 4, 1, fpImg);
                secStart += (loadSz * 4);
                offset   += (loadSz * 4);

                /* Read and copy the segment contents. */
                while (loadSz)
                {
                    unsigned int readlen  = (validSz < 2048) ? (validSz) : 2048;
                    unsigned int writelen = (loadSz < 2048) ? (loadSz) : 2048;
                    unsigned int i, actual_rd = 0, actual_wr = 0;

                    /* Zero out the buffer to start with. */
                    memset (buffer, 0, sizeof (buffer));

                    /* Read the valid data length from the file. */
                    if (readlen != 0)
                    {
                        actual_rd  = fread (&buffer, 4, readlen, fpElf);
                        for (i = 0; i < actual_rd; i++)
                            checksum += buffer[i];
                    }

                    actual_wr = fwrite (&buffer, 4, writelen, fpImg);
                    if (actual_wr < actual_rd)
                        actual_rd = actual_wr;

                    validSz   -= actual_rd;
                    loadSz    -= actual_wr;
                }
            }
        }
        else
        {
            if (verbose)
            {
                fprintf (stderr, "Skipping program header with type %d\n", progHdr->type);
            }
        }
    }
    else
        ERREXIT ("Bad parameter\n");

    return 0;
}

/* Function to retrieve parameter values from command line arguments. */
int
GetParameter (
        int    argc,
        char  *argv[],
        char  *option,
        char **parameter)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp (argv[i], option) == 0)
        {
            if (parameter)
            {
                if ((argc > (i + 1)) && (*argv[i + 1] != '-'))
                    *parameter = argv[i + 1];
                else
                    return -1;
            }
            return 0;   /* Option found. Parameter is returned alongside. */
        }
    }

    return -1;          /* Option not found. */
}

void
PrintUsageInfo (
        char *progName)
{
    printf ("Usage:\n______\n");
    printf ("%s -i <elf filename> -o <image filename> [-i2cconf <eeprom control>]\n", progName);
    printf ("        [-imgtype <image type>] [-vectorload <vecload>] [-v] [-h]\n");
    printf ("    where\n");
    printf ("    <elf filename> is the input ELF file name with path\n");
    printf ("    <image filename> is the output file name with path\n");
    printf ("    <eeprom control> is the I2C/SPI EEPROM control word in hexadecimal form\n");
    printf ("    <image type> is the image type byte in hexadecimal\n");
    printf ("    <vecload> can be set to \"yes\" to force the interrupt vectors to be exported into the image\n");
    printf ("    -v is used for verbose logs during the conversion process\n");
    printf ("    -h is used to print this help information\n");
}

int
main (
        int   argc,
        char *argv[])
{
    char *inFilename = NULL, *outFilename = NULL, *tmp;
    FILE *fpIn, *fpOut;

    ElfHeader     elfHdr  = {{0}, 0};
    ProgramHeader progHdr = {0};

    unsigned char i2cConf = 0x1C;        /* Default value of I2C config: 64 KB+ Atmel EEPROM at 400 KHz. */
    unsigned char imgType = 0xB0;        /* Default value of image type: Binary. */

    unsigned int val, entryAddr;
    int i, stat = 0;

    /* Parse and check the command line arguments. */
    GetParameter (argc, argv, "-i", &inFilename);
    GetParameter (argc, argv, "-o", &outFilename);
    if (GetParameter (argc, argv, "-i2cconf", &tmp) == 0)
    {
        sscanf (tmp, "%x", &val);
        i2cConf = (unsigned char)val;
        i2cDevSize = I2cDeviceSizes[((i2cConf >> 1) & 7)];
    }
    if (GetParameter (argc, argv, "-imgtype", &tmp) == 0)
    {
        sscanf (tmp, "%x", &val);
        imgType = (unsigned char)val;
    }
    if (GetParameter (argc, argv, "-v", 0) == 0)
        verbose = 1;
    if (GetParameter (argc, argv, "-vectorload", &tmp) == 0)
    {
        if (strcmp (tmp, "yes") == 0)
            loadIntVectors = 1;
    }
    if (GetParameter (argc, argv, "-h", 0) == 0)
    {
        PrintUsageInfo (argv[0]);
        return (0);
    }

    if ((inFilename == NULL) || (outFilename == NULL))
    {
        PrintUsageInfo (argv[0]);
        ERREXIT ("Input or output file not specified\n");
    }

    /* Open the input ELF file. */
    fpIn = fopen (inFilename, "rb");
    if (fpIn == NULL)
    {
        ERREXIT ("Failed to open file %s\n", inFilename);
    }

    if (fread (&elfHdr, sizeof (ElfHeader), 1, fpIn) == 1)
    {
        stat = CheckElfHeader (&elfHdr);
        if (stat)
            return (stat);
    }
    else
        ERREXIT ("Failed to read ELF header\n");

    /* Open the output file. */
    fpOut = fopen (outFilename, "wb");
    if (fpOut == NULL)
        ERREXIT ("Failed to open output file %s\n", outFilename);

    /* Write the image header to the output file. */
    fprintf (fpOut, "CY%c%c", i2cConf, imgType);

    /* Read each program header and process. */
    for (i = 0; i < elfHdr.phnum; i++)
    {
        fseek (fpIn, (elfHdr.phoff + i * elfHdr.phentsize), SEEK_SET);
        if (fread (&progHdr, sizeof (ProgramHeader), 1, fpIn) == 1)
        {
            stat = ProcessProgHeader (fpIn, &progHdr, fpOut);
            if (stat)
                ERREXIT ("Failed in ProcessProgHeader\n");
        }
    }

    /* Print the termination section containing the image entry address. */
    val       = 0;
    entryAddr = elfHdr.entry;

    /* Write the trailer with the entry section and checksum to the img file. */
    fwrite (&val,       4, 1, fpOut);
    fwrite (&entryAddr, 4, 1, fpOut);
    fwrite (&checksum,  4, 1, fpOut);

    fclose (fpOut);
    fclose (fpIn);

    return (0);
}

/*[]*/


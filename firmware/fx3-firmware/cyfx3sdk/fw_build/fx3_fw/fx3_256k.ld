/*
   Cypress USB 3.0 Platform linker script file (fx3_256k.ld)
 
   Copyright Cypress Semiconductor Corporation, 2010-2023,
   All Rights Reserved
   UNPUBLISHED, LICENSED SOFTWARE.

   CONFIDENTIAL AND PROPRIETARY INFORMATION
   WHICH IS THE PROPERTY OF CYPRESS.

   Use of this file is governed
   by the license agreement included in the file
 
      <install>/license/license.txt

   where <install> is the Cypress software
   installation root directory path.
*/

/*
   This is the GNU linker file for FX3 devices such as CYUSB3011 or CYUSB3012 which
   only have 256 KB of System RAM. This file sets up a memory map based on the
   assumption that only 256 KB or memory is available, and therefore allocates a
   small memory region for the code (text) section.

   If a device with 512 KB of RAM (CYUSB3014 or CYUSB3035) is being used, the fx3_512k.ld
   linker file can be used instead.

   The CYUSB3011/CYUSB3012 devices have 16 KB of I-TCM memory which can be used for
   code (typically ISRs) and 512 KB of SYSTEM RAM which is shared between code, data
   and DMA buffers.

   The default memory map used for these devices is as follows:

   Descriptor area    Base: 0x40000000 Size: 12KB
   Code area          Base: 0x40003000 Size: 128KB
   Data area          Base: 0x40023000 Size: 24KB
   Driver heap        Base: 0x40029000 Size: 28KB  (Update cyfxtx.c to change this.)
   Buffer area        Base: 0x40030000 Size: 32KB  (Update cyfxtx.c to change this.)
   2-stage boot area  Base: 0x40038000 Size: 32KB  (Update cyfxtx.c to change this.)

   Interrupt handlers are placed in I-TCM (16KB). The first 256 bytes of ITCM are
   reserved for Exception Vectors and will be loaded during firmware initialization.
   The next 256 bytes of I-TCM are reserved for device configuration functions.

   Kernel stacks are be placed in the D-TCM (8KB).
   This is done internal to the library as part of the CyU3PFirmwareEntry() function,
   and is not expected to be modified by the FX3 application.

   SYS_STACK       Base: 0x10000000 Size 2KB    (Used by ISR bottom-halves.)
   ABT_STACK       Base: 0x10000800 Size 256B   (Unused except in error cases.)
   UND_STACK       Base: 0x10000900 Size 256B   (Unused except in error cases.)
   FIQ_STACK       Base: 0x10000A00 Size 512B   (Unused as FIQ is not registered.)
   IRQ_STACK       Base: 0x10000C00 Size 1KB    (Used by IST top halves.)
   SVC_STACK       Base: 0x10001000 Size 4KB    (Used by the RTOS kernel and scheduler.)
*/

ENTRY(CyU3PFirmwareEntry);

MEMORY
{
	I-TCM	: ORIGIN = 0x200	LENGTH = 0x3E00
	SYS_MEM	: ORIGIN = 0x40003000	LENGTH = 0x20000
	DATA	: ORIGIN = 0x40023000	LENGTH = 0x6000
}

SECTIONS
{
	.vectors :
	{
		*(CYU3P_ITCM_SECTION)
                tx_thread_irq_nesting*(.text)
                tx_thread_context*(.text)
                tx_thread_vectored*(.text)
		. = ALIGN(4);
	} >I-TCM

	.text :
	{
		*(.text*)
		*(.rodata*)
		*(.constdata)
		*(.emb_text)
		*(CYU3P_EXCEPTION_VECTORS);
		_etext = .;
		. = ALIGN(4);
	} > SYS_MEM

	.data :
	{
		_data = .;
		*(.data*)
		* (+RW, +ZI)
		_edata = .;
		. = ALIGN(4);
	} > DATA

	.bss :
	{
		_bss_start = .;
		*(.bss*)
		. = ALIGN(4);
	} >DATA 
	_bss_end = . ;

	.ARM.extab :
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} > DATA

	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
		. = ALIGN(4);
	} > DATA
	__exidx_end = .;
}


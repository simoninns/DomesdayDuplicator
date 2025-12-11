/*
   Cypress USB 3.0 Platform linker script file (fx3cpp.ld)
 
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
   This is the GNU linker file used by FX3 applications using the C++ compiler.
   The memory map used here is designed for devices with 512 KB of System RAM,
   such as the CYUSB3014 and CYUSB3035 devices.

   It is not advisable to use C++ based applications on FX3 devices with only
   256 KB of RAM (CYUSB3011/CYUSB3012). This is because a typical C++ firmware
   application will use up about 200 KB of the RAM, and not leave sufficient
   space for data buffers.

   The full FX3/FX3S device has 16 KB of I-TCM memory which can be used for
   code (typically ISRs) and 512 KB of SYSTEM RAM which is shared between
   code, data and DMA buffers.

   The default memory map used for C++ based FX3 applications is as follows:

   Descriptor area              Base: 0x40000000 Size: 12KB
   Code area                    Base: 0x40003000 Size: 256KB
   Data area                    Base: 0x40043000 Size: 20KB
   C++ Exception Handling       Base: 0x40048000 Size: 32KB
   Runtime Compiler heap        Base: 0x40050000 Size: 32KB
   Driver heap                  Base: 0x40058000 Size: 32KB  (Update cyfxtx.cpp when changing this)
   Buffer area                  Base: 0x40060000 Size: 128KB (Update cyfxtx.cpp when changing this)

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
	I-TCM		: ORIGIN = 0x200	LENGTH = 0x3E00
	SYS_MEM	        : ORIGIN = 0x40003000	LENGTH = 0x40000
	DATA		: ORIGIN = 0x40043000	LENGTH = 0x5000
	ARM		: ORIGIN = 0x40048000	LENGTH = 0x8000
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
		*(.text)
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

	.ARM.extab   : 
        { 
            *(.ARM.extab* .gnu.linkonce.armextab.*) 
            . = ALIGN(4);
        } > ARM

        __exidx_start = .;
        PROVIDE(__exidx_start = __exidx_start);
	.ARM.exidx   : 
        { 
            *(.ARM.exidx* .gnu.linkonce.armexidx.*) 
            . = ALIGN(4);
        } >ARM
        __exidx_end = .;

        PROVIDE(__exidx_end = __exidx_end);
        
        . = ALIGN(4);
        __heap_start = 0x40050000;
        PROVIDE(__heap_start = __heap_start);
        
        . = ALIGN(4);
        __heap_end = 0x40058000;
        PROVIDE(__heap_end = __heap_end);
	
	PROVIDE(__heap_size = __heap_end - __heap_start);
}

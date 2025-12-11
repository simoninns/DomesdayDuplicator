/*
   Cypress USB 3.0 Platform linker script file (cyfx3.ld)
 
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
  The last 32KB of the available memory is reserved for Fx3 Boot-loader.

  On the CYUSB3014 part, this memory range is 0x40078000 - 0x4007FFFF.

  If a part with less memory (such as CYUSB3011 or CYUSB3012) is being
  used, this memory range needs to be dropped to 0x40038000 - 0x4003FFFF.

  Descriptor area Base: 0x40000000 Size: 8KB
  ITCM area       Base: 0x00000100 Size: 256B
  Code area       Base: 0x40078000 Size: 28KB
  Data area       Base: 0x4007F000 Size: 4KB

  The first 256 bytes of ITCM are reserved for Exception Vectors.
  These vectors will be loaded to ITCM 0x0 as part of the Device Init.

*/

ENTRY(__vectors);


MEMORY
{
	I-TCM  	: ORIGIN = 0x100     	LENGTH = 0x100
	SYS_MEM	: ORIGIN = 0x40078000	LENGTH = 0x7000
	DATA	: ORIGIN = 0x4007F000	LENGTH = 0x1000
}

SECTIONS
{
	.vectors :
	{
		*(CYU3P_ITCM_SECTION)
		. = ALIGN(4);
	} >I-TCM

	.text :
	{
		*(RESET)
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
		*(+RW, +ZI)
		*(Heap)
		*(ISR_Stacks)
		*(Stacks)
		_edata = .;
		. = ALIGN(4);
	} > DATA

	.bss :
	{
		_bss_start = .;
		*(.bss)
		. = ALIGN(4);
	} >DATA 
	_bss_end = . ;

	.ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) }
		 __exidx_start = .;
	.ARM.exidx   : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) }
		 __exidx_end = .;

}

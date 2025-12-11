##
## Copyright Cypress Semiconductor Corporation, 2010-2023,
## All Rights Reserved
## UNPUBLISHED, LICENSED SOFTWARE.
##
## CONFIDENTIAL AND PROPRIETARY INFORMATION
## WHICH IS THE PROPERTY OF CYPRESS.
##
## Use of this file is governed
## by the license agreement included in the file
##
##	<install>/license/license.txt
##
## where <install> is the Cypress software
## installation root directory path.
##

###
### GNU toolchain Firmware configuration
###

# Tools
CC	= arm-none-eabi-g++
AS	= arm-none-eabi-g++
LD	= arm-none-eabi-ld
AR	= arm-none-eabi-ar

# Arguments
ASMFLAGS += -Wall -c -mcpu=arm926ej-s -mthumb-interwork -Wno-write-strings	

CCFLAGS += -Wall -mcpu=arm926ej-s -mthumb-interwork -Wno-write-strings

ifeq ($(CYLINKSCR), local)

# Use application specific linker script.
LDFLAGS += -T app_specific.ld -d --gc-sections \
	   --no-wchar-size-warning -Map $(MODULE).map

else

# No support for the CYUSB3011 device.
LDFLAGS += -T $(FX3FWROOT)/fw_build/fx3_fw/fx3cpp.ld -d --gc-sections \
	   --no-wchar-size-warning -Map $(MODULE).map

endif

# The ARM toolchain location and the version are taken from environment variables
LDLIBS  += \
	"$$ARMGCC_INSTALL_PATH"/arm-none-eabi/lib/libstdc++.a	\
	"$$ARMGCC_INSTALL_PATH"/arm-none-eabi/lib/libc.a	\
	"$$ARMGCC_INSTALL_PATH"/lib/gcc/arm-none-eabi/$(ARMGCC_VERSION)/libgcc.a

EXEEXT = elf

# Command Shortcuts
COMPILE			= $(CC) $(CCFLAGS) -c -o $@ $< 
ASSEMBLE		= $(AS) $(ASMFLAGS) -o $@ $<
LINK			= $(LD) $+ $(LDFLAGS) -o $@
BDLIB			= $(AR) -r $@ $+

# []


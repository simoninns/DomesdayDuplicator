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
CC	= arm-none-eabi-gcc
AS	= arm-none-eabi-gcc
LD	= arm-none-eabi-ld
AR	= arm-none-eabi-ar

# Arguments
ASMFLAGS += -Wall -c -mcpu=arm926ej-s -mthumb-interwork
CCFLAGS  += -Wall -mcpu=arm926ej-s -mthumb-interwork -ffunction-sections -fdata-sections

ifeq ($(CYLINKSCR), local)

# Use application specific linker script.
    LDFLAGS += -T app_specific.ld -d --gc-sections \
	       --no-warn-mismatch --no-wchar-size-warning -Map $(MODULE).map

else

# Select the linker script based on the chosen FX3 device.
ifeq ($(CYDEVICE), CYUSB3011)
    LDFLAGS += -T $(FX3FWROOT)/fw_build/fx3_fw/fx3_256k.ld -d --gc-sections \
	       --no-warn-mismatch --no-wchar-size-warning -Map $(MODULE).map
else
    LDFLAGS += -T $(FX3FWROOT)/fw_build/fx3_fw/fx3_512k.ld -d --gc-sections \
	       --no-warn-mismatch --no-wchar-size-warning -Map $(MODULE).map
endif

endif

# The ARM toolchain location and the version are taken from environment variables
LDLIBS  += \
	"$$ARMGCC_INSTALL_PATH"/arm-none-eabi/lib/libc.a	\
	"$$ARMGCC_INSTALL_PATH"/lib/gcc/arm-none-eabi/$(ARMGCC_VERSION)/libgcc.a

EXEEXT		= elf

# Command Shortcuts
COMPILE		= $(CC) $(CCFLAGS) -c -o $@ $< 
ASSEMBLE	= $(AS) $(ASMFLAGS) -o $@ $<
LINK		= $(LD) $+ $(LDFLAGS) -o $@
BDLIB		= $(AR) -r $@ $+

# []

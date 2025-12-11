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

# Set the default SDK version
ifeq ($(CYSDKVERSION),)
	CYSDKVERSION=1_3_5
endif

# the common include path
Include	=-I. \
	 -I$(FX3FWROOT)/boot_lib/$(CYSDKVERSION)/include

# the common compiler options
ifeq ($(CYFXBUILD), arm)
CCFLAGS	= -g -Ospace $(Include)
else
CCFLAGS	= -g -Os $(Include)
endif

ifeq ($(CYDEVICE), CYUSB3011)
	CCFLAGS += -DCYMEM_256K
endif

# FX3 Boot library
ifeq ($(CYFXBUILD), arm)
	LDLIBS = $(FX3FWROOT)/boot_lib/$(CYSDKVERSION)/lib/libcyfx3boot.a
else
	LDLIBS = -L $(FX3FWROOT)/boot_lib/$(CYSDKVERSION)/lib -lcyfx3boot
endif

# the common linker options
LDFLAGS	= --entry Reset_Handler $(LDLIBS)

# now include the compile specific build options
# gcc is the default compiler
ifeq ($(CYFXBUILD), arm)
	include $(FX3FWROOT)/fw_build/boot_fw/fx3_armrvds_config.mak
else
	include $(FX3FWROOT)/fw_build/boot_fw/fx3_armgcc_config.mak
endif	

#[]

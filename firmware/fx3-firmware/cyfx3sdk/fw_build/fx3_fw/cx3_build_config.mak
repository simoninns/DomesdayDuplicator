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

# how to build, select the toolchain and include the correct files
# NOTE: the FX3FWROOT has to be defined in each makefile before
# this file is included !

# Default option for Config is fx3_debug
ifeq ($(CYCONFOPT),)
    CYCONFOPT=fx3_debug
endif

# Default library version is 1.3.5
ifeq ($(CYSDKVERSION),)
    CYSDKVERSION=1_3_5
endif

# Add any user provided ASMFLAGS
ASMFLAGS = $(USER_ASMFLAGS)

# The common include path
Include	=-I. \
	 -I$(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/inc

# The common compiler options
CCFLAGS	= -g -DTX_ENABLE_EVENT_TRACE -DDEBUG -DCYU3P_FX3=1	\
	  -D__CYU3P_TX__=1 $(Include)

# Add any user provided CFLAGS
CCFLAGS += $(USER_CFLAGS)

# The optimization level depends on the build configuration.
ifeq ($(findstring release,$(CYCONFOPT)),release)
ifeq ($(CYFXBUILD),arm)
    CCFLAGS += -Ospace
else
    CCFLAGS += -Os
endif
else
    CCFLAGS += -O0
endif

# Set the performance profiling enable flag where required.
ifeq ($(findstring profile,$(CYCONFOPT)),profile)
    CCFLAGS += -DCYU3P_PROFILE_EN
endif

# If we are building for a lower memory FX3 device, update the CCFLAGS accordingly.
ifeq ($(CYDEVICE), CYUSB3011)
    CCFLAGS += -DCYMEM_256K
endif

# The common libraries
# NOTE: This order is important for GNU linker. Do not change
ifeq ($(CYFXBUILD),arm)
LDLIBS += \
	$(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/$(CYCONFOPT)/libcyu3mipicsi.a	\
	$(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/$(CYCONFOPT)/libcyu3lpp.a		\
	$(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/$(CYCONFOPT)/libcyfxapi.a		\
	$(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/$(CYCONFOPT)/libcyu3threadx.a
else
LDLIBS += \
	-L $(FX3FWROOT)/fw_lib/$(CYSDKVERSION)/$(CYCONFOPT)	\
	-lcyu3mipicsi						\
	-lcyu3lpp						\
	-lcyfxapi						\
	-lcyu3threadx
endif

# The common linker options
LDFLAGS	= --entry CyU3PFirmwareEntry $(LDLIBS)

# Add any user provided LDFLAGS
LDFLAGS += $(USER_LDFLAGS)

# Now include the compile specific build options
# The GNU compiler is default
ifeq ($(CYFXBUILD), gcc)
	include $(FX3FWROOT)/fw_build/fx3_fw/fx3_armgcc_config.mak
endif
ifeq ($(CYFXBUILD), g++)
	include $(FX3FWROOT)/fw_build/fx3_fw/fx3_armg++_config.mak
endif
ifeq ($(CYFXBUILD), arm)
	include $(FX3FWROOT)/fw_build/fx3_fw/fx3_armrvds_config.mak
endif	
ifeq ($(CYFXBUILD),)
	include $(FX3FWROOT)/fw_build/fx3_fw/fx3_armgcc_config.mak
endif

#[]

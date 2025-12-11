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
### ARM Firmware configuration
###

# Tools
CY.ASM      = armasm
CY.CC       = armcc
CY.AR       = armar
CY.LD	    = armlink

# Arguments
ASMFLAGS	+= -g --cpu ARM926EJ-S --apcs \interwork		\
			--pd "CYU3P_FX3 SETA (1)"

CCFLAGS		+= --cpu ARM926EJ-S --apcs interwork

EXEEXT		= axf

LDFLAGS		+= -d --elf --remove						\
		   --map --symbols --list $(MODULE).map 			\
		   --no_strict_wchar_size --diag_suppress L6436W		\
		   --datacompressor off

ifeq ($(findstring local,$(CYFXSCAT)),local)
    LDFLAGS += --scatter local.scat
else
    LDFLAGS += --scatter $(FX3FWROOT)/fw_build/fx3_fw/cyfx3.scat
endif

# Command shortcuts
COMPILE		= $(CY.CC) $(CCFLAGS) -c -o $@ $<
ASSEMBLE	= $(CY.ASM) $(ASMFLAGS) -o $@ $<
LINK		= $(CY.LD) $(LDFLAGS) -o $@ $+
BDLIB		= $(CY.AR) --create $@ $+

#[]#

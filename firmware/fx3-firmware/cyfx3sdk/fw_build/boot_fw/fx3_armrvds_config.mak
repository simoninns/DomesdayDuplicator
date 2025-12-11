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
CY.CC       = armcc
CY.LD	    = armlink

# Arguments

CCFLAGS	+= --cpu ARM926EJ-S			\
	   --apcs /interwork --strict_warnings	\
	   --signed-chars 

EXEEXT	= axf

LDFLAGS	+= -d --elf --remove						\
	   --scatter $(FX3FWROOT)/fw_build/boot_fw/cyfx3.scat 		\
	   --datacompressor off						\
	   --map --symbols --list $(MODULE).map


# Command shortcuts
COMPILE		= $(CY.CC) $(CCFLAGS) -c -o $@ $<
LINK		= $(CY.LD) $(LDFLAGS) -o $@ $+

#[]#

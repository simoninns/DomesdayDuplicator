#************************************************************************
#	
#	DomesdayDuplicator.SDC
#	SDC file
#	
#	Domesday Duplicator - LaserDisc RF sampler
#	SPDX-FileCopyrightText: 2018-2025 Simon Inns
#	SPDX-License-Identifier: GPL-3.0-or-later
#	
#************************************************************************/


# Create 50MHz system Clock
create_clock -period 20.000ns [get_ports CLOCK_50]


# Create Generated Clocks
derive_pll_clocks
derive_clock_uncertainty

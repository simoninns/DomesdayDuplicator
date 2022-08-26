# Domesday Duplicator

## Synopsis

The Domesday Duplicator is a USB3 based DAQ capable of 40 million samples per second acquisition of analogue RF data.

The hardware is a USB3 based 10-bit analogue to digital converter designed to allow the backup of Domesday AIV LaserDiscs (as well as generic laserdiscs) through the direct sampling of the RF data from the optical head (laser) of a LaserDisc player.

The hardware/software solution is designed to act as a sampling front-end to the ld-decode (software decode of laserdiscs) project https://github.com/happycube/ld-decode and replaces the generic TV capture card to provide high-frequency sampling with 4 times the sample resolution.

This project is currently work-in-progress and should be considered beta (until the decoding is proven modifications may be needed to the capture set-up).

Please see http://www.domesday86.com/?page_id=978 for detailed documentation on the Domesday Duplicator and overall information about the Domesday86 project.

There are 3 main components that make up the Domesday Duplicator:

A custom ADC board based around the Texas Instruments ADS825 10-Bit, 40MSPS analogue to digital converter.  This board contains an RF amplifier and conditioner (to amplify the output from the laserdisc player RF tap and condition it for the single-ended ADC chip) as well as headers for both the DE0-Nano FPGA development board and Cypress FX3 SuperSpeed Explorer board.  RF input is physically provided by a BNC connector.

Terasic DE0-Nano FPGA development board - The DE0-Nano is a low-cost FPGA development board containing an Altera Cyclone IV FPGA.  The DE0-Nano is used to process the raw 10-bit ADC data stream and provides FIFO buffering (towards the FX3) and sample conversion from unsigned 10-bit to scaled 16-bit signed data.  The FPGA provides a dual-clock FIFO; receiving ADC data at a maximum of 40 MSPS (million samples per second) and transmitting the data to the FX3 at a maximum of 60 MSPS.  This is to ensure that no data is lost during sustained transfers (as RF sampling of a disc can take more than 40 minutes).

Cypress FX3 SuperSpeed Explorer board - The FX3 is a low-cost USB3 development board from Cypress.  The FX3 is used to buffer data from the FPGA and provide it to a host computer using USB3.  USB3 is required for this application due to the high-data bandwidth necessary for high-speed/high-resolution sampling.

## Motivation

The Domesday86 project is involved in the documentation and preservation of the BBC Domesday Project from 1986.  The Domesday project provided a set of analogue laserdiscs in a proprietary format (like standard laserdiscs but with some specific extensions only supported by the Philips VP415 laserdisc player).

The Domesday Duplicator is intended to allow high-quality back-ups of the analogue information contained on the laserdiscs by bypassing most of the 30-year-old electronics in the VP415 player.  Direct RF sampling also allows all information on the laserdiscs to be stored (unlike conventional RGB sampling of the video output).  Since AIV laserdiscs are a combination of video, pictures, sound and data (as well as numerous VBI streams), direct RF sampling is the preferred method of preservation.

## Installation

Please see https://www.domesday86.com/?page_id=1312#Installation for instructions on how to install the Domesday Duplicator software

## Author

Domesday Duplicator is written and maintained by Simon Inns.

## Software License (GPLv3)

    Domesday Duplicator is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Domesday Duplicator is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Hardware License (Creative Commons BY-SA 4.0)

Please see the following link for details: https://creativecommons.org/licenses/by-sa/4.0/

You are free to:

Share - copy and redistribute the material in any medium or format
Adapt - remix, transform, and build upon the material
for any purpose, even commercially.

This license is acceptable for Free Cultural Works.

The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

Attribution - You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.

ShareAlike - If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

No additional restrictions - You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.


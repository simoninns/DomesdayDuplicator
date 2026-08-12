# The Domesday Duplicator (DdD) Documentation


The Domesday Duplicator was purpose built to allow high-quality back-ups of the analogue information contained on the LaserDiscs by bypassing most of the 40-year-old electronics in LaserDisc players.  

This leverages the simplistic method of direct FM RF capture (FM RF Archival) allowing all original signals information on the LaserDiscs (or any single FM RF channel media format (such as Video8/Hi8 or Betamax NTSC)) to be stored (unlike conventional RGB sampling of the video output).  

Since LaserDiscs are a combination of video, pictures, sound and data (as well as numerous VBI streams), direct RF sampling is the preferred method of preservation, as it is for most analog media formats in the signal domain such as videotape.

The Domesday Duplicator is a USB3-based DAQ capable of 40 million samples per second acquisition of analogue RF data.

![](assets/domesday_duplicator_3_photo.jpg)

>The Domesday Duplicator (DdD) Rev 3.0

The hardware is a USB 3.0 based 10-bit analogue to digital converter designed to allow the backup of Domesday AIV LaserDiscs (as well as generic LaserDiscs) through the direct sampling of the RF data from the optical head (laser) of a LaserDisc player.

The hardware/software solution is designed to act as a sampling front-end to the [ld-decode](https://github.com/happycube/ld-decode) (software decoder of LaserDiscs) project, which replaces the generic TV capture card to provide the entire 4fsc sampled picture frame and any data contained above its active "picture" area.

There are 3 main components that make up the Domesday Duplicator:

A custom ADC board based around the Texas Instruments ADS825 10-Bit, 40MSPS analogue to digital converter.  This board contains an RF amplifier and conditioner (to amplify the output from the LaserDiscs player RF tap and condition it for the single-ended ADC chip) as well as headers for both the DE0-Nano FPGA development board and Cypress FX3 SuperSpeed Explorer board.  RF input is physically provided by a BNC connector.

Terasic DE0-Nano FPGA development board - The DE0-Nano is a low-cost FPGA development board containing an Altera Cyclone IV FPGA.  The DE0-Nano is used to process the raw 10-bit ADC data stream and provides FIFO buffering (towards the FX3) and sample conversion from unsigned 10-bit to scaled 16-bit signed data.  The FPGA provides a dual-clock FIFO; receiving ADC data at a maximum of 40 MSPS (million samples per second) and transmitting the data to the FX3 at a maximum of 60 MSPS.  This is to ensure that no data is lost during sustained transfers (as RF sampling of a disc can take more than 40 minutes).

Cypress FX3 SuperSpeed Explorer board - The FX3 is a low-cost USB3 development board from Cypress.  The FX3 is used to buffer data from the FPGA and provide it to a host computer using USB3.  USB3 is required for this application due to the high-data bandwidth necessary for high-speed/high-resolution sampling.

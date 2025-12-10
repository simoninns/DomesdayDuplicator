# Domesday Duplicator Firmware

> **Please note:** The primary repository is [simoninns/DomesdayDuplicator](https://github.com/simoninns/DomesdayDuplicator) which includes this as a submodule.

This directory contains the firmware for the Domesday Duplicator hardware.

## Components

### DE0-NANO
FPGA firmware for the Terasic DE0-NANO board. This handles the high-speed data acquisition and buffering.

### FX3-Firmware
Firmware for the Cypress FX3 USB 3.0 controller. This manages the USB communication between the FPGA and the host computer.

## Building

Please refer to the individual component directories for specific build instructions:
- [DE0-NANO](DE0-NANO/)
- [FX3-Firmware](FX3-Firmware/)

## Documentation

For detailed documentation, please see the [main project documentation](https://simoninns.github.io/DomesdayDuplicator-docs).

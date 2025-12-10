# Domesday Duplicator GUI Application

> **Please note:** The primary repository is [simoninns/DomesdayDuplicator](https://github.com/simoninns/DomesdayDuplicator) which includes this as a submodule.

This directory contains the graphical user interface application for controlling the Domesday Duplicator hardware and managing RF data capture.

## Components

### Linux-Application
Cross-platform GUI application built with Qt. Includes:
- **DomesdayDuplicator** - Main capture application for controlling the hardware
- **dddutil** - Utility for analyzing and converting captured data
- **dddconv** - Command-line data conversion tool

## Building

The application can be built using either CMake or Qt Creator:

```bash
cd Linux-Application
mkdir build && cd build
cmake ..
make
```

Or open the `.pro` files in Qt Creator.

## Requirements

- Qt 5 or later
- LibUSB
- C++11 compatible compiler

## Documentation

For detailed documentation, please see the [main project documentation](https://simoninns.github.io/DomesdayDuplicator-docs).

# FX3 Firmware Programmer

A minimal, libusb-based command-line tool for programming Cypress FX3 devices. No Qt, no legacy code, just what you need.

## Origin

This project is based on the [Cypress cyusb_linux](https://github.com/Cypress-Semiconductor/cyusb_linux) project, which provides tools for programming FX3 and FX2 devices. For the Domesday Duplicator project, we have:

- Removed the Qt GUI dependency
- Eliminated FX2 (legacy) support
- Stripped unnecessary utilities and examples
- Streamlined to a single 22KB CLI binary using pure libusb-1.0
- Maintained FX3 firmware upload to RAM, EEPROM programming and verification

## Table of Contents

- [Prerequisites](#prerequisites)
- [Building](#building)
- [Installation](#installation)
- [Usage](#usage)
- [Programming the FX3 with Domesday Duplicator Firmware](#programming-the-fx3-with-domesday-duplicator-firmware)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Tools

**On Ubuntu/Debian:**
```bash
sudo apt-get install build-essential cmake pkg-config libusb-1.0-0-dev
```

**On Fedora/RHEL:**
```bash
sudo dnf install cmake pkg-config libusb1-devel gcc
```

## Building

From the repository root:

```bash
cd fx3/programmer
mkdir build
cd build
cmake ..
make
```

### Build Output

The build process generates:
- `fx3-programmer` - Command-line FX3 firmware programmer (22KB)

## Installation

To install the tool system-wide:

```bash
cd build
sudo make install
```

This installs:
- `/usr/local/bin/fx3-programmer` - Programming tool
- `/usr/local/share/domesday-duplicator/cyfxflashprog.img` - Cypress secondary loader
- `/usr/local/lib/udev/rules.d/70-domesday-duplicator.rules` - udev rules for USB device access

### Activate the udev Rules

The rules install under the CMake prefix rather than directly into `/etc`, so that a build
never writes outside the directory it was told to install into. Link them into place:

```bash
sudo ln -sf /usr/local/lib/udev/rules.d/70-domesday-duplicator.rules /etc/udev/rules.d/70-domesday-duplicator.rules
sudo udevadm control --reload
sudo udevadm trigger
```

Then disconnect and reconnect your FX3 device.

Distribution packagers should copy or symlink from `$PREFIX/lib/udev/rules.d` in the package
post-install step rather than having CMake write to an absolute path.

### The Cypress secondary loader

Permanent (I2C EEPROM) programming needs `cyfxflashprog.img`, a Cypress-supplied
secondary loader that runs on the FX3 and performs the flash write. RAM-only programming
does not need it.

A copy is vendored in this directory — see [VENDOR.md](VENDOR.md) for its provenance — and
`make install` places it alongside the binary. `fx3-programmer` looks for it in this order:

1. `$FX3_FLASH_PROG`, if set
2. the compiled-in install path (`$PREFIX/share/domesday-duplicator/cyfxflashprog.img`)
3. `cyfxflashprog.img` and a few paths relative to the current working directory

So an installed `fx3-programmer` finds it from any directory. If you are running from a
build tree and it cannot be found, point `FX3_FLASH_PROG` at the copy in this directory:

```bash
export FX3_FLASH_PROG=/path/to/DomesdayDuplicator/fx3/programmer/cyfxflashprog.img
```

## Usage

### List Connected FX3 Devices

```bash
fx3-programmer -l
```

Output example (Bootloader mode):
```
Found 1 FX3 device(s):

[0] VID:PID=04b4:00f3 Bus=007 Device=013 Mode=Bootloader (FX3)
```

Output example (Application mode):
```
Found 1 FX3 device(s):

[0] VID:PID=1d50:603b Bus=007 Device=014 Mode=Application (Domesday Duplicator)
```

### Upload Firmware

**To device 0:**
```bash
fx3-programmer -u firmware.img
```

**To specific device:**
```bash
fx3-programmer -d 1 -u firmware.img
```

The firmware is loaded into RAM, parsed, and executed: the bootloader transfers control to the image's entry point once the download completes. The device then re-enumerates in application mode — for this project's firmware, as `1d50:603b`. This is not a reset; the FX3 is never rebooted.

After successful upload, the device may enumerate with a new VID:PID pair (e.g., 1d50:603b for Domesday Duplicator).

### Verify Firmware Upload

```bash
fx3-programmer -d 0 -p firmware.img -v
```

`-v` is a modifier for `-p`, not a standalone operation: verification compares the EEPROM
against a firmware file, so it needs one. Running `-v` on its own reports that.

### No software reset

There is no `-r`, and there is no way to reset the FX3 from the host in this project.

The option used to exist and did nothing: `fx3_reset_device()` printed "Device will reset
automatically after firmware download completes", slept for two seconds and returned success
(D25). Nothing was ever sent to the device.

A real implementation is not available either. The FX3 boot ROM offers no reset vendor
command, and this project's firmware implements only `0xB5` and `0xB6` — capture control.
A `libusb_reset_device()` would re-enumerate the USB device without rebooting the FX3 or
changing its boot mode, which would look like a reset while not being one.

**Changing boot mode requires a physical power cycle**, with J4 fitted or removed to choose
where the device boots from. Running `-r` now fails and says so.

### Complete workflow

```bash
# List devices — confirm it is in bootloader mode
fx3-programmer -l

# Volatile: load into RAM, lost on power down
fx3-programmer -d 0 -u firmware.img

# Permanent: write the I2C EEPROM and verify it
fx3-programmer -d 0 -p firmware.img -v
```

Then remove J4 and power cycle to boot the programmed firmware.

## Programming the FX3 with Domesday Duplicator Firmware

This section describes how to program the FX3 device with firmware built from the Domesday Duplicator firmware project.

### Prerequisites

1. **Build the firmware** - Follow the instructions in `../firmware/README.md` to compile the firmware:
   ```bash
   cd ../firmware
   mkdir build
   cd build
   cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake ..
   make
   ```
   
   This produces `firmware.img` in the build directory.

2. **Build fx3-programmer** - Follow the [Building](#building) section above.

3. **FX3 Device Requirements**:
   - Cypress FX3 development board or compatible hardware
   - USB cable (USB 3.0 recommended)
   - Access to the PMODE (Program Mode) jumper, J4, for any programming operation

### Step-by-Step Programming

#### 1. Prepare the FX3 Device

The FX3 bootloader behavior depends on what firmware is already installed:

**If the device shows as Bootloader (VID:PID=04b4:00f3):**
- Device is ready to program immediately
- Proceed to step 2

**If the device shows as Application (VID:PID=1d50:603b):**
- The current Domesday Duplicator firmware is running
- **Programming in this mode writes to RAM only** and will be lost on power cycle
- To make firmware changes permanent:
  1. Locate jumper **J4 (PMODE)** on your FX3 board
  2. **Close/short the jumper** to force boot-to-bootloader mode
  3. **Power cycle** the FX3 (disconnect USB, then reconnect)
  4. Run `fx3-programmer -l` again - device should now show as Bootloader
  5. Then proceed with programming

**If no device appears:**
- Ensure USB cable is connected
- Try different USB ports (preferably USB 3.0)
- Check `lsusb | grep "04b4"` to see if any Cypress device is detected

#### 2. Verify Device Detection

```bash
fx3-programmer -l
```

You should see one of:
```
Found 1 FX3 device(s):

[0] VID:PID=04b4:00f3 Bus=007 Device=013 Mode=Bootloader (FX3)
```

or

```
Found 1 FX3 device(s):

[0] VID:PID=1d50:603b Bus=007 Device=014 Mode=Application (Domesday Duplicator)
```

- **Bootloader mode**: the only mode in which the device can be programmed at all. `-u` writes RAM, `-p` writes the I2C EEPROM. Shows as `VID:PID=04b4:00f3`.
- **Application mode**: running firmware — the Domesday Duplicator shows as `VID:PID=1d50:603b`. **Neither `-u` nor `-p` works here.** Both rely on the FX3 boot ROM, which is no longer in control once firmware is running. Fit J4 and power cycle to get back to the bootloader.
- **Flash programmer**: a transient mode, `VID:PID=04b4:4720`, entered automatically while `-p` runs. Nothing to do with it; it goes away on the next power cycle.

Note the device index (usually 0 if you have one device).

#### 3. Upload Firmware

```bash
fx3-programmer -d 0 -u /path/to/firmware.img
```

This:
- `-d 0` - Targets device 0
- `-u /path/to/firmware.img` - Uploads the firmware image

**Note:** the bootloader executes the firmware automatically once the upload completes — there is no separate "start" step. `-v` verifies an EEPROM write and is only meaningful alongside `-p`. There is no `-r`: see *No software reset* below.

#### 4. Complete Workflow Example

```bash
# Build the firmware
cd ../firmware/build
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake ..
make

# Navigate to programmer
cd ../../programmer/build

# List devices
fx3-programmer -l

# Program device
fx3-programmer -d 0 -u ../../firmware/build/firmware.img
```

#### 5. Verify Firmware is Running

After successful program:
- The FX3 device should enumerate with the application's VID:PID
- For Domesday Duplicator: VID:PID should change to `1d50:603b`
- Check your system logs: `dmesg | tail -20`
- Run the list command again to see the new device state: `fx3-programmer -l`

### Troubleshooting

#### Permission Denied When Accessing USB Device

If you get permission errors:

```bash
sudo udevadm control --reload
sudo udevadm trigger
```

Then disconnect and reconnect the FX3 device.

#### Device Not Found

Ensure:
1. Device is connected via USB
2. Device is in bootloader mode (PMODE jumper set if applicable)
3. Run `fx3-programmer -l` to verify device detection

If still not detected:
```bash
# Check if device appears in lsusb
lsusb | grep "04b4"

# If you see 04b4:0080, device is in bootloader - ready to program
# If you see 04b4:00f3, device is in application mode
```

#### Firmware Upload Fails

1. Verify the firmware file exists and is readable:
   ```bash
   ls -lh firmware.img
   ```

2. Try re-entering bootloader mode:
   - Disconnect USB
   - Close PMODE jumper (if applicable)
   - Reconnect USB
   - Run `fx3-programmer -l` to verify

3. Check for USB errors:
   ```bash
   dmesg | grep -i usb | tail -10
   ```

#### Device Not Responding After Programming

If the device doesn't respond after programming:

1. Disconnect USB cable
2. Wait 5 seconds
3. Reconnect USB cable
4. Verify with `fx3-programmer -l`

If in bootloader mode still, try programming again. If stuck in bootloader, you may need to:
- Open PMODE jumper
- Power cycle the board
- Check the firmware image is valid

### Key Features

- ✅ Discover connected FX3 devices
- ✅ Upload firmware via USB
- ✅ Verify an EEPROM write against the source image
- ✅ No Qt dependency
- ✅ No legacy code
- ✅ Single binary, ~22KB
- ✅ Pure libusb-1.0 implementation

### Additional Resources

- **FX3 API Documentation**: Refer to Cypress FX3 SDK documentation (available online at Cypress website)
- **Firmware Source**: See `../firmware/src/` for Domesday Duplicator firmware source
- **Cypress FX3 SDK**: Included in `../sdk/` (version 1.3.5 headers and libraries)

### Support

For issues with:
- **Firmware building**: See `../firmware/README.md`
- **Firmware source code**: See `../firmware/src/`
- **Hardware setup**: Refer to your FX3 board documentation

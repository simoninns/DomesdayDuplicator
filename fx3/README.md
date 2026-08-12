# Cypress FX3 USB 3.0 Controller

Everything to do with the FX3, which moves sampled data from the FPGA to the host over
USB 3.0.

| Path | Contents |
| --- | --- |
| [firmware/](firmware/) | The firmware that runs on the FX3 — build it with `arm-none-eabi-gcc` and CMake |
| [firmware/gpif/](firmware/gpif/) | GPIF II Designer project for the parallel interface state machine |
| [programmer/](programmer/) | `fx3-programmer`, the host-side libusb tool that loads firmware onto the device |
| [sdk/](sdk/) | Vendored subset of the Cypress EZ-USB FX3 SDK 1.3.5 that the firmware links against |

## Typical workflow

```bash
# Build the firmware image
cmake -B firmware/build -S firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
cmake --build firmware/build

# Build the programmer
cmake -B programmer/build -S programmer
cmake --build programmer/build

# Load it onto a connected device
./programmer/build/fx3-programmer -l
./programmer/build/fx3-programmer -d 0 -u firmware/build/firmware.img
```

See [firmware/README.md](firmware/README.md) and [programmer/README.md](programmer/README.md)
for prerequisites, permanent (EEPROM) programming and troubleshooting.

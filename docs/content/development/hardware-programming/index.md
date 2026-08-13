# Hardware programming

A Domesday Duplicator has **two programmable devices**, and both must be programmed before
the unit will capture anything.

![](../../assets/domesday_duplicator_3_photo.jpg)
_The Domesday Duplicator 3_0_

| Device | Carries | Programmed with | Guide |
| --- | --- | --- | --- |
| **Cypress FX3** SuperSpeed Explorer Kit | The USB 3.0 firmware that streams samples to the host | `fx3-programmer` over USB | [FX3 firmware](fx3-firmware.md) |
| **Terasic DE0-NANO** (Cyclone IV FPGA) | The gateware that drives the ADC and the GPIF II bus | Quartus over the onboard USB-Blaster | [FPGA bitstream](fpga-bitstream.md) |

Neither board is part of the Domesday Duplicator PCB itself. Both are off-the-shelf
development boards that plug into it.

## Which one do I need to program?

| Situation | FX3 | FPGA |
| --- | --- | --- |
| Brand new build | Yes | Yes |
| Updating to a new release | Usually | Only if the release says so |
| Capture starts but samples are corrupt | Maybe | Maybe — the two must be in step |
| Device does not appear on USB at all | Yes | No |

The FX3 firmware and the FPGA gateware talk to each other over a parallel bus with a
protocol defined in both. **If you update one across a release that changed that protocol,
update the other too.** A mismatched pair enumerates happily and captures nothing but
garbage.

## Before you start, on Linux

Both the FX3 programmer and the capture application talk to the device over USB, and a USB
device is owned by `root` unless something says otherwise. Set that up once:

**→ [Linux device access](linux-device-access.md)**

Skipping it produces confusing failures — the device is plainly visible in `lsusb`, but every
tool reports that it cannot find it.

## Windows and macOS

Due to various restrictions around non-free tooling building and programming the hardware on 
non-linux platforms is not recommended nor supported.
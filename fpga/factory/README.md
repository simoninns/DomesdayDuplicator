# The factory image

**This image is written into a unit's flash by JTAG when the unit is provisioned, and
never again. Changing it means re-provisioning every fielded Duplicator with a cable.
Read this whole file before editing anything in this directory.**

It is the resident half of the two-image gateware described on the
[EPCS layout and boot flow](../../docs/content/development/epcs-layout-and-boot-flow.md)
page. The other half, the capture gateware, is in [../application/](../application/) and
is field-updatable over the single USB cable the unit already has. The half they share is
in [../common/](../common/).

## The freeze policy

A change here is not like a change anywhere else in this repository, and the difference is
not one of degree:

| | application image | factory image |
| --- | --- | --- |
| How it reaches a unit | a device update over USB | a JTAG cable, per unit, by hand |
| Who can install it | the user, from the capture application | somebody holding the board |
| If it is wrong | reinstall it | the unit is in recovery until somebody opens it up |
| How often it should change | freely | after its first release, essentially never |

So the policy is:

- **Assume every change here is permanent.** Not because something prevents a change, but
  because a change is an operation nobody can perform remotely on hardware that is already
  in somebody's house.
- **A change here is a re-provisioning event** and has to be described as one in the pull
  request: what changed, why it could not wait, and what happens to units that are never
  re-provisioned.
- **`../common/` counts.** A change under `common/` rebuilds both images, so it inherits
  this policy. That is a cost, and it is the right one: the alternative is two copies of
  the register bank drifting apart, which would be a protocol split inside one device.
- **The bench soak is disproportionate to the size of the code**, deliberately. This is the
  one component a field update can never repair.

## What is in here

| File | Holds |
| --- | --- |
| `DomesdayDuplicatorFactory.v` | Top level: pin mapping, safe GPIF idle, the two resets |
| `bootLoader.v` | The boot decision - read the boot block, check it, check the image it names, hand over or stay |
| `crc32.v` | The checksum the boot block is validated with |
| `DomesdayDuplicatorFactory.qsf` | Quartus settings. Pin assignments are the application image's verbatim |
| `DomesdayDuplicatorFactory.SDC` | Timing constraints |
| `DomesdayDuplicatorFactory_write_sof.cdf` | Volatile JTAG configuration, for development |

What it deliberately does **not** contain is the capture path. There is no ADC interface,
no FIFO, no GPIF state machine and no test generator in this image. A unit that falls back
to it cannot capture and says so through `IMAGE_ROLE`, which is a better failure than
silently running whatever the capture logic looked like on the day the unit was
provisioned.

## Two resets, which is the one structural surprise

The register bank runs from the FX3's `nReset`, as the register interface specification
says it must. The boot logic, the flash bridge and the reconfiguration control run from a
power-on reset of this image's own.

That is deliberate: the boot decision must not be interruptible by the other chip's reset
line. The FX3 is booting at the same moment, and a device that only entered its
application image when the FX3 happened to release reset early enough would fail
intermittently, in the field, for a reason nobody could see.

## What still has to be confirmed on the bench

Two things in this image are written from the documentation of the device rather than from
a measurement, and **both must be confirmed before this image is frozen into fielded
hardware**. Both are marked in the source where they appear.

1. **The remote update parameter encoding** — `ParamBootAddress`, `ParamWatchdogValue` and
   `ParamWatchdogEnable` in [../common/remoteUpdate.v](../common/remoteUpdate.v). They say
   how the application's start address and the watchdog timeout are presented to the
   device's reconfiguration block. Getting them wrong fails safely - the application image
   is never entered and the unit stays here, which is the state it is in before it has ever
   been provisioned - but it fails.

2. **The watchdog period.** `WatchdogTimeout` is currently the largest value the field
   holds, which is the deliberately generous end of the range. The period has to sit
   comfortably above the worst case of an FX3 boot plus its identity read; that figure is a
   measurement nobody has taken yet, and a period set too short means a device that drops
   into recovery whenever the FX3 boots slowly.

What *is* established, by Quartus rather than by assumption: the reconfiguration block's
clock is specified for a 25 ns minimum period and 10.1 ns minimum high and low times on
this device, so it is given a divide-by-four of the system clock rather than the system
clock itself.

## What the simulation covers

`../tests/tb_bootLoader.v` builds this image's boot path exactly as the top level wires it
- boot logic, flash bridge, active serial block, reconfiguration control - with a model of
the EPCS64 behind it, and drives the four cases the boot flow documents: a valid boot
block, the wrong magic, a block whose own checksum is wrong, and an intact block
describing a damaged image. `../tests/tb_crc32.v` checks the checksum against the
published CRC-32 check value, so that what this image computes is what a host computes.

What simulation cannot cover is the handover itself. A simulated device cannot
reconfigure, so the testbench checks that the right thing was asked for at the right
moment, and the bench checks that asking for it works.

## Building it

Both images are built together, because they are provisioned together:

```bash
nix develop .#fpga-quartus -c ./fpga/build-local.sh
```

or hermetically, `nix build .#bitstream`. See [../README.md](../README.md).

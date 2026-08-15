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

## What the bench has settled, and what it has not

The handover ran on hardware for the first time on 2026-08-15 (TESTING.md §6, G0 and G1).
A unit provisioned with a dual-image flash now updates over USB and cold-boots into the
application image with no host attached.

**The remote update parameter encoding is confirmed.** All three parameter numbers were
wrong - the boot address was written to a read-only register and the watchdog timeout to
the early-CONF_DONE bit - and each is now checked against Table 17 of the Remote Update IP
User Guide, read back out of the block on hardware through registers `0x30`-`0x37`, and
pinned by `../tests/tb_bootLoader.v`.

That entry used to claim getting them wrong *fails safely*, and it is worth recording that
the claim was false. The unit did not stay here. It reconfigured, failed, reverted, and
reconfigured again, about three seconds a lap, indefinitely - a state that is neither
recovery nor operation and that announces itself only as a blinking LED. **A failure in
this image is not automatically a fall-back to this image**, and nothing about the design
makes it one.

Three things still have to be settled before this image is frozen, and all three are
listed in TESTING.md §7:

1. **The watchdog period.** `WatchdogTimeout` is still the largest value the field holds,
   which is the deliberately generous end of the range. The mechanism is proven - an
   application image ran well past the ~54 s timeout with the watchdog enabled, which it
   could only do if the FX3's register traffic were resetting the timer - but the period
   has to be narrowed to a measured margin over the worst case of an FX3 boot plus its
   identity read. A period set too short means a device that drops into recovery whenever
   the FX3 boots slowly.

2. **The double-configuration time** (V5). Power-on is now two configurations rather than
   one, and the FX3 firmware assumes the FPGA is ready well under a second. Nobody has
   timed it against that assumption.

3. **The refusal to make a second attempt.** The boot logic has no guard against an
   application image that validates, configures, and is nonetheless dead - which is the
   cycling case above. `Cd_early` now has the device check the image before committing to
   it, which narrows the window to exactly that case, but the deliberate parking in
   recovery after a failed attempt is still owed.

## What the handover has to do, and why each part is there

Four things, in this order, and the first bench session had to correct every one of them:

1. **Relock the flash bridge.** Reading the boot block leaves the fabric driving the
   flash's pins, and those are the pins the configuration engine is about to use. The
   bridge is relocked on both paths out of the boot decision, before anything is armed.
2. **Write `Osc_int` and `Cd_early`.** The device handbook requires the factory
   configuration to set both, and this design set neither.
3. **Stage the boot address** as a full 24-bit byte address; the block stores bits 23:2
   and appends two zeros at boot.
4. **Hold every strobe for at least 250 ns.** They were 200 ns. They are 800 ns now.

The first three are asserted by `../tests/tb_bootLoader.v` - it fails if the bridge still
drives the flash pins at handover, if either option bit is clear, or if the staged address
is not the application's.

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

# Domesday Duplicator FPGA Gateware

Verilog for the Terasic DE0-NANO board (Altera/Intel Cyclone IV, `EP4CE22F17C6`). It samples
the ADC, buffers the data and feeds it to the FX3 USB 3.0 controller over the GPIF II bus.

## Contents

| Path | Contents |
| --- | --- |
| [src/](src/) | Quartus project and all Verilog sources |

Inside `src/`:

| File | Role |
| --- | --- |
| `DomesdayDuplicator.v` | Top level: pin mapping and module wiring |
| `DomesdayDuplicator.qsf` | Quartus settings — device, pin assignments, source list |
| `DomesdayDuplicator.qpf` | Quartus project file |
| `DomesdayDuplicator.SDC` | Timing constraints |
| `dataGenerator.v` | ADC sampling and the built-in test-data generator |
| `buffer.v` | Sample buffering between the ADC and FX3 clock domains |
| `fx3StateMachine.v` | GPIF II handshake with the FX3 |
| `statusLED.v` | Front-panel status LED behaviour |
| `IPfifo.v`, `IPpllGenerator.v` | Instantiations of the Altera `dcfifo` and `altpll` primitives |

`IPfifo.v` and `IPpllGenerator.v` are committed as plain Verilog with explicit `defparam`
values. They instantiate Altera primitives, but they are ordinary source files — nothing in
the build regenerates them, so **MegaWizard is not needed to build the project**.

## Building

Quartus Prime Lite is required (the design targets a Cyclone IV, which Lite supports). From
this directory:

```bash
cd src
quartus_sh --flow compile DomesdayDuplicator
```

That produces `DomesdayDuplicator.sof` for volatile JTAG configuration.

To produce the `.jic` image used for permanent EPCS64 configuration:

```bash
quartus_cpf -c DomesdayDuplicator.cof
```

## Programming the board

Both configuration description files expect to be run from `src/`:

```bash
cd src
quartus_pgm DomesdayDuplicator_write_sof.cdf   # volatile, lost on power cycle
quartus_pgm DomesdayDuplicator_write_jic.cdf   # permanent, into the EPCS64 flash
```

## Notes

- The Quartus GUI is never required — every step above is a command-line tool.
- The project files were last written by Quartus 16.0.2/18.0.0. Newer versions will offer to
  upgrade them in place; the resulting diff is large and should be a deliberate change rather
  than a side effect of opening the project.

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/DomesdayDuplicator-docs).

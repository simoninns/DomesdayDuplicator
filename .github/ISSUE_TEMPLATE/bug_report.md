---
name: Bug report
about: Report a reproducible problem in Domesday Duplicator
title: "[Bug] "
labels: bug
assignees: ''
---

## Summary

Describe the problem clearly.

## Component

Which part of the project is affected?

- [ ] Capture application (`ddd-gui`, `ddd-update`, `ddd-jtag`)
- [ ] FX3 firmware (`fx3/firmware`)
- [ ] FX3 programmer (`fx3/programmer`)
- [ ] FPGA gateware (`fpga/`)
- [ ] Hardware / PCB (`hardware/`)
- [ ] Documentation (`docs/`)
- [ ] Build, packaging or Nix flake
- [ ] Not sure

## Steps to reproduce

1. 
2. 
3. 

## Expected behavior

What should have happened?

## Actual behavior

What happened instead?

## Environment

- OS and version:
- Install method: (Flatpak / macOS DMG / Windows MSI / built from source with Nix)
- Application commit: (Help → About)
- FX3 firmware commit: (Tools → Firmware → Update firmware…)
- FPGA gateware commit: (same dialog)
- Board revision, and DE0-NANO or other carrier:
- ADC clock / sample rate in use, if relevant:

All three are short hex hashes. The firmware and the gateware are installed together from
one update and come from one commit, so they normally match each other; the application is
released separately and is not expected to match them. **Please give all three even when
two of them look the same** — whether they are identical is most of what they tell us.

Leave any field blank that does not apply — a documentation or build report does not need
the device details.

## Additional context

Attach logs, screenshots, or a short capture sample if available. For capture problems the
application's log panel (View → Panels → Log) is usually the most useful thing you can
include. For gateware or firmware problems, say whether the same board worked with an
earlier build, and name that build if you can.

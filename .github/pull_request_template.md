## Summary

Briefly describe what this PR changes and why.

## Component

- [ ] Capture application (`ddd-gui`, `ddd-update`, `ddd-jtag`)
- [ ] FX3 firmware (`fx3/firmware`)
- [ ] FX3 programmer (`fx3/programmer`)
- [ ] FPGA gateware (`fpga/`)
- [ ] Hardware / PCB (`hardware/`)
- [ ] Documentation (`docs/`)
- [ ] Build, packaging or Nix flake
- [ ] Repository tooling (`tools/`)

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] Refactor
- [ ] Documentation
- [ ] Other

## Validation

Describe how you tested this change — the commands you ran and what they reported.

- [ ] `nix flake check`, or the component's own tests (`ctest --test-dir <component>/build`)
- [ ] Built natively as well as with Nix, where the change could affect either route
- [ ] Hardware-in-the-loop check per [TESTING.md](../TESTING.md) — **required** for gateware,
      FX3 firmware, or anything on the capture path, where a green build is not sufficient

## Checklist

- [ ] Scope is focused and minimal
- [ ] Build passes locally
- [ ] New source files carry the SPDX licence header — `./tools/check-licence-headers.sh`
- [ ] Follows the conventions in [AGENTS.md](../AGENTS.md)
- [ ] Related docs under [docs/content/](../docs/content/) were updated if needed
- [ ] Linked issue (if applicable)

## Related issue

Closes #

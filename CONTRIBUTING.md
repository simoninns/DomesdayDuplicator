# Contributing to Domesday Duplicator

Thank you for your interest in improving Domesday Duplicator. Please follow the guidelines below to help us review and merge changes smoothly.

## Project Contributions (Firmware/Hardware/Tools)
- Discuss significant changes via an issue first when possible so we can align on approach and scope.
- Keep changes focused: smaller, self-contained pull requests are easier to review.
- Include clear descriptions of what changed and why; mention related issues or discussions.
- Add or update tests when applicable, and describe how you verified the change (e.g., build, simulation, or manual testing steps).
- Follow existing coding style in the area you touch and avoid unnecessary whitespace-only changes.

## Documentation Contributions
- The project documentation lives in [docs/](docs/) **in this repository**. It was previously
  in a separate `DomesdayDuplicator-docs` repository; that repository is no longer the source
  of truth, and pull requests opened against it will not reach the published site.
- Edit the markdown under [docs/content/](docs/content/). Keep PRs concise and include
  screenshots or before/after comparisons when UI or visual changes are involved.
- If you spot documentation gaps while working on code or hardware, fix them in the same pull
  request where that makes sense — they are now the same repository.

## Before you open a pull request

- Run the tests for anything you touched: `nix flake check`, or `ctest --test-dir <component>/build`.
- Describe how you verified the change. For gateware, FX3 firmware or anything on the capture
  path, a green build is not sufficient — see the hardware-in-the-loop procedure in
  [TESTING.md](TESTING.md).
- [AGENTS.md](AGENTS.md) records the project conventions, and applies to people as much as to
  automated assistants.

## Where things live

See the repository layout table in [README.md](README.md). Engineering-process documentation
for the repository itself — the reorganisation plan, decisions and per-component testing — is
in [docs-tech/](docs-tech/).

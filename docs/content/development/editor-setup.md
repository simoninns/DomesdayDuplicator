# Editor Setup

This project does not require an IDE, and no editor is privileged over any other. Everything
below is optional — the build works from a terminal with nothing but CMake and a compiler.

The reason this page exists is history. The FX3 sources arrived as an Eclipse CDT project,
the GUI as a Qt Creator qmake project, and the gateware as a Quartus GUI project. Each
brought its own build definition, each drifted from the real one, and each made contributing
conditional on installing a particular large application. All three are gone. What replaces
them is one mechanism that every modern editor already speaks: the Language Server Protocol.

## How it works, in one paragraph

Every CMake component sets `CMAKE_EXPORT_COMPILE_COMMANDS`, so configuring a build writes
`build/compile_commands.json` — the exact compiler invocation for every source file. Each
component also has a `.clangd` file pointing at that build directory. Start `clangd` in any
editor and you get completion, go-to-definition, find-references, inline diagnostics and
rename across the C and C++ in that component. Verilog gets the same treatment from
`verible-verilog-ls`. Both language servers come with the Nix dev shells, so there is nothing
to install per developer.

## Once, before anything else

Get the toolchain, then configure each component you intend to work on.

**Run all of this from the repository root.** The `nix develop` line would work from any
subdirectory — there is one `flake.nix`, at the root, and Nix walks up to find it — but the
`cmake` lines below use paths relative to the root, so the root is the one place every
command here is correct.

```bash
nix develop                      # everything free, across all components
cmake -B gui/build -S gui
cmake -B fx3/programmer/build -S fx3/programmer
cmake -B fx3/firmware/build -S fx3/firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
```

The configure step is what produces `compile_commands.json`.

!!! warning "Configure first"

    Until a component has been configured at least once, clangd has nothing to read and will
    report errors on every include. That is the single most common cause of "the language
    server does not work here".

Re-run configure after adding or removing a source file.

### With direnv

If you use [direnv](https://direnv.net/), `.envrc` in the repository root activates the
default shell automatically:

```bash
direnv allow
```

Editors launched from inside the directory then inherit the toolchain, which matters for
GUI editors that do not read your shell profile. To use a component shell instead, put
`use flake .#gui` in `.envrc.local`.

## Per-component notes

| Component | Language server | Notes |
| --- | --- | --- |
| `gui/` | clangd | Qt's moc and uic output lands in the build tree; the compile database already includes those paths |
| `fx3/programmer/` | clangd | Nothing special — ordinary host C |
| `fx3/firmware/` | clangd | `.clangd` sets `Compiler: arm-none-eabi-gcc`. Without it clangd probes the *host* compiler for system headers and reports hundreds of false errors, because the firmware is freestanding and never sees the host libc |
| `fpga/` | verible-verilog-ls | `nix develop .#fpga` — free tools only, no Quartus needed to edit, lint or simulate |
| `docs/` | — | Markdown; any editor |
| `hardware/` | — | KiCad's own GUI. `nix develop .#hardware` |

## VS Code

Install two extensions:

- **clangd** (`llvm-vs-code-extensions.vscode-clangd`)
- **CMake Tools** (`ms-vscode.cmake-tools`)

Disable the C/C++ extension's IntelliSense if you have it installed — it and clangd fight
over the same files and produce contradictory diagnostics:

```json
{
  "C_Cpp.intelliSenseEngine": "disabled",
  "clangd.arguments": ["--background-index", "--clang-tidy"]
}
```

Open the *component* directory (`gui/`, `fx3/firmware/`) rather than the repository root, so
clangd finds the right `.clangd`. If you prefer a single window, use a multi-root workspace
with one folder per component.

Do not commit a `.vscode/` directory — it is gitignored at the root deliberately.

## Neovim

With the built-in LSP client and `nvim-lspconfig`:

```lua
require('lspconfig').clangd.setup {
  cmd = { 'clangd', '--background-index', '--clang-tidy' },
}

require('lspconfig').verible.setup {
  cmd = { 'verible-verilog-ls', '--rules_config_search' },
}
```

`clangd` locates the right configuration by walking up from the file being edited, so opening
a file anywhere in the tree works — there is no per-project setup beyond the above.

For Verilog, `verible-verilog-ls` is in the `fpga` shell. Formatting is
`verible-verilog-format --inplace`.

## Emacs

With `eglot` (built in since Emacs 29):

```elisp
(add-hook 'c-mode-hook #'eglot-ensure)
(add-hook 'c++-mode-hook #'eglot-ensure)
(add-hook 'verilog-mode-hook #'eglot-ensure)

(with-eval-after-load 'eglot
  (add-to-list 'eglot-server-programs
               '(verilog-mode . ("verible-verilog-ls"))))
```

`lsp-mode` works equally well; nothing in the repository depends on which you choose.

## Helix

Helix has an LSP client built in and needs no configuration for C and C++ — it looks for
`clangd` on `PATH`, which the dev shell provides. For Verilog, add to `languages.toml`:

```toml
[[language]]
name = "verilog"
language-servers = ["verible"]

[language-server.verible]
command = "verible-verilog-ls"
```

## Qt Creator

Qt Creator opens CMake projects natively. Use *File → Open File or Project* and select
`gui/CMakeLists.txt`.

Do **not** look for `.pro` files — the qmake project files were removed. They were a second
build definition maintained by hand alongside the CMake one, and they drifted. Qt Creator
loses nothing: it reads the CMake project directly, including the Qt-specific targets.

Its generated `*.user` files are gitignored.

## CLion and KDevelop

Both open `CMakeLists.txt` directly and manage their own build directory. Point them at the
component directory. Neither needs anything from this repository beyond the CMake project.

If CLion cannot find Qt or libusb, it is being launched outside the Nix shell — start it from
a terminal inside `nix develop`, or use direnv.

## Formatting

There is no automatic formatter in CI, and no repository-wide reformat has been done —
that would bury the history of every file. What exists:

- **`.editorconfig`** at the repository root, honoured by every editor listed above (VS Code
  and Emacs need a plugin; Neovim, Helix, CLion, KDevelop and Qt Creator support it natively).
  It sets indentation, line endings and trailing-whitespace behaviour per file type.
- **`clang-format`** ships in the dev shells but there is no `.clang-format` file yet, so
  running it would reformat against its default style. Do not run it across existing files.
- **`verible-verilog-format`** for Verilog, likewise available but not enforced.

The rule that matters: **do not reformat code you are not otherwise changing.**
Whitespace-only diffs bury the actual change and break `git blame`.

Paths that must never be reformatted at all — vendored or generated — are listed in
[AGENTS.md](https://github.com/simoninns/DomesdayDuplicator/blob/main/AGENTS.md) §3 and
marked `unset` in `.editorconfig` so a format-on-save cannot touch them.

## When the language server misbehaves

| Symptom | Cause |
| --- | --- |
| Every `#include` is red | The component has not been configured — no `compile_commands.json` yet |
| Only *some* files are broken | Those files were added after the last configure; re-run `cmake -B build` |
| Firmware sources show hundreds of libc errors | clangd is not using `fx3/firmware/.clangd`; you probably opened the repository root rather than the component |
| Qt headers not found | The editor was launched outside the Nix shell |
| Stale diagnostics after a rebuild | Clear clangd's index: `rm -rf .cache/clangd` |

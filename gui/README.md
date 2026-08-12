# Domesday Duplicator GUI Application

Qt 6 desktop applications for driving the Domesday Duplicator hardware and working with the
data it captures.

## Components

| Path | Component | Role |
| --- | --- | --- |
| [src/DomesdayDuplicator/](src/DomesdayDuplicator/) | `DomesdayDuplicator` | Main capture application: controls the hardware, monitors amplitude, drives the LaserDisc player |
| [src/dddutil/](src/dddutil/) | `dddutil` | GUI utility for analysing and converting captured data |
| [src/dddconv/](src/dddconv/) | `dddconv` | Command-line data conversion tool |

Supporting directories:

| Path | Contents |
| --- | --- |
| [cmake/](cmake/) | `FindLibUSB.cmake` and any other CMake modules |
| [CMakeLists.txt](CMakeLists.txt) | The single build definition for all three programs |

## Building

See [BUILD.md](BUILD.md) for requirements and instructions.

There is one build system — CMake. The qmake `.pro` files that used to sit alongside it were
a second, independently maintained definition of the same build and have been removed; Qt
Creator opens CMake projects natively.

## Editor support

`CMAKE_EXPORT_COMPILE_COMMANDS` is on, so configuring the build writes
`build/compile_commands.json`. [.clangd](.clangd) points clangd at it, which gives working
completion, navigation and diagnostics in any editor with a language-server client — no IDE
required.

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/domesdayduplicator).

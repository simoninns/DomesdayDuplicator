# Domesday Duplicator GUI Application

Qt 6 desktop applications for driving the Domesday Duplicator hardware and working with the
data it captures.

## Components

| Path | Component | Role |
| --- | --- | --- |
| [src/DomesdayDuplicator/](src/DomesdayDuplicator/) | `DomesdayDuplicator` | Main capture application: controls the hardware, monitors amplitude, drives the LaserDisc player, analyses test-pattern captures |
| [src/common/](src/common/) | `ddd-common` | The Qt-free core — sample codec, FLAC writer, capture reader, test-data analyser — split out so it can be tested without a GUI |

`dddconv` and `dddutil` were removed in Phase 7. The capture application writes FLAC (`.ldf`)
directly, so the conversion step both tools existed for is gone; `dddutil`'s test-data
analysis moved into the capture application.

Supporting directories:

| Path | Contents |
| --- | --- |
| [cmake/](cmake/) | `FindLibUSB.cmake`, `FindFLAC.cmake` and any other CMake modules |
| [packaging/](packaging/) | Flatpak manifest, WiX installer definition, and the shared desktop/AppStream/icon assets |
| [CMakeLists.txt](CMakeLists.txt) | The single build definition |

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

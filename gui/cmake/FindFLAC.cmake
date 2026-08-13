# Find libFLAC, the Ogg FLAC encoder and decoder the capture application writes .ldf with
# (P7-21, P7-24).
#
# pkg-config first, because that is the path that works unchanged on Nix, Homebrew and
# MSYS2 — the three toolchains the packaging jobs use. flac 1.5 also installs a CMake
# package config, so that is the fallback rather than hand-rolled find_path/find_library
# calls, which get the Ogg dependency wrong on static builds.
#
# Defines:
#  FLAC_FOUND
#  FLAC_INCLUDE_DIRS
#  FLAC_LIBRARIES

if(NOT PKG_CONFIG_FOUND)
  find_package(PkgConfig)
endif()

if(PKG_CONFIG_FOUND)
  if(FLAC_FIND_REQUIRED)
    set(_flac_required REQUIRED)
  endif()

  # Ogg encapsulation is what makes the output an .ldf rather than a native FLAC file, and
  # it is a compile-time option in libFLAC. 1.5.0 is the first release with multithreaded
  # encoding; older versions still build (the writer falls back to one thread) so this is a
  # floor, not the version the packaging jobs should install.
  pkg_check_modules(FLAC ${_flac_required} flac>=1.4.0)

  if(FLAC_FOUND)
    # Resolve to an absolute path, matching what FindLibUSB.cmake does: pkg-config hands
    # back bare library names, which the Windows link step cannot use directly.
    find_library(FLAC_LIBRARY
      NAMES ${FLAC_LIBRARIES} FLAC
      HINTS ${FLAC_LIBRARY_DIRS}
    )
    if(FLAC_LIBRARY)
      set(FLAC_LIBRARIES ${FLAC_LIBRARY})
    endif()
    return()
  endif()
endif()

# Fall back to flac's own CMake package config
find_package(FLAC CONFIG QUIET)
if(TARGET FLAC::FLAC)
  set(FLAC_FOUND TRUE)
  set(FLAC_LIBRARIES FLAC::FLAC)
  set(FLAC_INCLUDE_DIRS "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FLAC FOUND_VAR FLAC_FOUND REQUIRED_VARS FLAC_LIBRARIES)

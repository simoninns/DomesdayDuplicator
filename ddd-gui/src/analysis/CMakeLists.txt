# The display mathematics — Qt-free, like the engine, and for the same reason.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Everything the signal panels compute lives here rather than in the widgets that draw it:
# sample-to-pixel mapping, the amplitude history ring, the FFT and the spectrum scaling,
# and the front-end gain declaration that turns codes into volts.
#
# It is a separate library from ddd_capture rather than part of it because none of it is
# needed to make a capture — the engine must be buildable and testable without any of this
# — and separate from ddd_gui_lib because a QPainter cannot be unit tested and this can.
# ddd_analysis_tests links no Qt, which is what keeps the rule enforceable rather than
# aspirational: if a QColor is ever reached for in here, that binary stops linking.

add_library(ddd_analysis STATIC
    amplitude_history.cpp
    fourier_transform.cpp
    frequency_axis.cpp
    front_end_gain.cpp
    sinc_interpolation.cpp
    spectrogram_history.cpp
    spectrum_analyser.cpp
    waveform_mapping.cpp
    waveform_trigger.cpp
)

target_include_directories(ddd_analysis PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(ddd_analysis PRIVATE -Wall -Wextra)

# The engine's value types — SampleMetricsSnapshot and the wire-format constants — are
# what this consumes. The dependency runs this way and only this way: nothing in
# ddd_capture may include anything from here.
target_link_libraries(ddd_analysis PUBLIC ddd_capture)

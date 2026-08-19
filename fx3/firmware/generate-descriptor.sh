#!/bin/bash
# Generate USB product descriptor with embedded git commit hash
# Outputs a C header file defining USB_DESC_PRODUCT_BYTES macro with all descriptor data
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2025-2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

OUTPUT_DIR="$1"
COMMIT="${2:-unknown}"

# The release this firmware belongs to, where it belongs to one.
#
# Only a build from an fw-v* tag has a release version; every other build is a commit and
# nothing more, and the string then has exactly the shape it always had. This is why the
# argument is optional rather than defaulted to something: there is no sensible stand-in
# for "which release is this", and inventing one would put a number in a descriptor that
# no release ever carried.
RELEASE="${3:-}"

# Create Python script to generate UTF-16LE bytes
python3 -c "
commit = '$COMMIT'
release = '$RELEASE'

# 'Domesday Duplicator 1.5.0 (a1b2c3d4)', or 'Domesday Duplicator (a1b2c3d4)' where there
# is no release to name. The commit stays in brackets at the end in both forms, which is
# what lets a host that only knows the older shape go on reading it — see
# ParseFirmwareCommit in ddd-gui/src/capture/firmware_version.cpp.
base = 'Domesday Duplicator '
if release:
    base = base + release + ' '
full_string = base + '(' + commit + ')'

# Generate UTF-16LE bytes
bytes_list = []
for char in full_string:
    bytes_list.append('0x{:02x}'.format(ord(char) & 0xFF))
    bytes_list.append('0x{:02x}'.format((ord(char) >> 8) & 0xFF))

# Descriptor size = len(bytes) + 2 (for size and type bytes)
desc_size = len(bytes_list) + 2

# Create full descriptor bytes: [size][type][data...]
full_bytes = ['0x{:02x}'.format(desc_size), '0x03']
full_bytes.extend(bytes_list)

# Generate C code
print('// Auto-generated USB product descriptor')
print('// Commit: ' + commit)
print('// Format: [size(1)][type(1)][string_data]')
print('#ifndef __GENERATED_DESCRIPTOR_DATA_H__')
print('#define __GENERATED_DESCRIPTOR_DATA_H__')
print('')
print('// USB descriptor bytes: size, type, and UTF-16LE encoded string')
print('#define USB_DESC_PRODUCT_BYTES \\\\')

# Format bytes into 16-per-line for readability
for i in range(0, len(full_bytes), 16):
    chunk = full_bytes[i:min(i+16, len(full_bytes))]
    is_last = (i + 16 >= len(full_bytes))
    suffix = ',' if is_last else ', \\\\'
    print('    ' + ', '.join(chunk) + suffix)

print('')
print('#endif // __GENERATED_DESCRIPTOR_DATA_H__')
"


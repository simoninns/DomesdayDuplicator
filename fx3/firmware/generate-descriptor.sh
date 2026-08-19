#!/bin/bash
# Generate USB product descriptor with embedded git commit hash
# Outputs a C header file defining USB_DESC_PRODUCT_BYTES macro with all descriptor data
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2025-2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

OUTPUT_DIR="$1"
COMMIT="${2:-unknown}"

# Create Python script to generate UTF-16LE bytes
python3 -c "
commit = '$COMMIT'

# 'Domesday Duplicator (a1b2c3d4)'. The commit is the whole of what the device says about
# which build it is running, and no release version is stamped into it: the firmware and
# the gateware are installed together from one signed bundle built from one commit, so the
# two of them reporting the same hash is what 'good' means. Which release that commit
# belongs to is in the bundle's signed manifest, which is verified before it is read.
#
# The commit sits in brackets at the end — see ParseFirmwareCommit in
# ddd-gui/src/capture/firmware_version.cpp, which finds the last bracketed group, so a
# host built today keeps working if anything is ever named before it.
full_string = 'Domesday Duplicator (' + commit + ')'

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


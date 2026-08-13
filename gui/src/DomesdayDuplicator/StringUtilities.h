/************************************************************************

    StringUtilities.h

    UTF-8 and wide-string conversion helpers
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2025-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once
#include <string>

inline std::string WStringToUtf8String(const std::wstring& wideString);
inline std::wstring Utf8StringToWString(const std::string& stringUtf8);

#include "StringUtilities.inl"

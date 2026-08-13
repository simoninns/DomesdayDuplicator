/************************************************************************

    test_stringutilities.cpp

    Domesday Duplicator - GUI tests

    T1 (unit) coverage for StringUtilities.h/.inl — UTF-8 <-> wide-string conversion.

    These functions exist because the Windows USB backend deals in wide strings while the
    rest of the application uses std::string. They are header-only and pure, which makes
    them the natural first test: if this file does not build and pass, the test harness
    itself is broken and no other result in the suite means anything.

    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "StringUtilities.h"

#include <string>

namespace
{

// --- ASCII, the common case -------------------------------------------------------------

TEST(StringUtilities, EmptyStringsRoundTrip)
{
    EXPECT_EQ(WStringToUtf8String(L""), "");
    EXPECT_EQ(Utf8StringToWString(""), L"");
}

TEST(StringUtilities, AsciiRoundTrips)
{
    const std::string ascii = "Domesday Duplicator";
    EXPECT_EQ(WStringToUtf8String(Utf8StringToWString(ascii)), ascii);
}

TEST(StringUtilities, AsciiIsOneBytePerCharacter)
{
    // ASCII must not gain a BOM or any other prefix — the USB descriptor code depends on
    // the byte count matching the character count for the ASCII case.
    EXPECT_EQ(WStringToUtf8String(L"abc").size(), 3u);
}

// --- Two-, three- and four-byte UTF-8 sequences ------------------------------------------

TEST(StringUtilities, TwoByteSequenceRoundTrips)
{
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE
    const std::wstring wide = L"café";
    const std::string utf8 = WStringToUtf8String(wide);

    ASSERT_EQ(utf8.size(), 5u); // 3 ASCII + 2 bytes
    EXPECT_EQ(static_cast<unsigned char>(utf8[3]), 0xC3u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[4]), 0xA9u);
    EXPECT_EQ(Utf8StringToWString(utf8), wide);
}

TEST(StringUtilities, ThreeByteSequenceRoundTrips)
{
    // U+20AC EURO SIGN
    const std::wstring wide = L"€";
    const std::string utf8 = WStringToUtf8String(wide);

    ASSERT_EQ(utf8.size(), 3u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[0]), 0xE2u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[1]), 0x82u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[2]), 0xACu);
    EXPECT_EQ(Utf8StringToWString(utf8), wide);
}

TEST(StringUtilities, FourByteSequenceRoundTrips)
{
    // U+1F600 GRINNING FACE — outside the BMP, so this is the surrogate-pair path on
    // platforms with 2-byte wchar_t and the direct path on those with 4-byte wchar_t.
    const std::wstring wide = L"\U0001F600";
    const std::string utf8 = WStringToUtf8String(wide);

    ASSERT_EQ(utf8.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[0]), 0xF0u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[1]), 0x9Fu);
    EXPECT_EQ(static_cast<unsigned char>(utf8[2]), 0x98u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[3]), 0x80u);
    EXPECT_EQ(Utf8StringToWString(utf8), wide);
}

TEST(StringUtilities, MixedWidthsRoundTrip)
{
    const std::wstring wide = L"aé€\U0001F600z";
    EXPECT_EQ(Utf8StringToWString(WStringToUtf8String(wide)), wide);
}

// --- Truncated input --------------------------------------------------------------------
//
// Both functions stop at a sequence they cannot complete rather than reading past the end
// of the buffer. These tests pin that behaviour: they document what the code does today,
// which matters because the alternative — reading past the end — is a memory-safety bug.

TEST(StringUtilities, TruncatedUtf8SequenceIsDropped)
{
    // First two bytes of the three-byte U+20AC sequence
    const std::string truncated = "ab\xE2\x82";
    const std::wstring result = Utf8StringToWString(truncated);

    EXPECT_EQ(result, L"ab");
}

TEST(StringUtilities, LoneHighSurrogateIsDropped)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        // A high surrogate with no low surrogate following it cannot be decoded
        const std::wstring lone(1, static_cast<wchar_t>(0xD83D));
        EXPECT_EQ(WStringToUtf8String(lone), "");
    }
    else
    {
        GTEST_SKIP() << "4-byte wchar_t: surrogate pairs are not used";
    }
}

// --- Embedded NUL -----------------------------------------------------------------------

TEST(StringUtilities, EmbeddedNulIsPreserved)
{
    // std::string is not NUL-terminated in the C sense, so a NUL in the middle must survive
    const std::wstring wide = std::wstring(L"a\0b", 3);
    const std::string utf8 = WStringToUtf8String(wide);

    ASSERT_EQ(utf8.size(), 3u);
    EXPECT_EQ(utf8[1], '\0');
    EXPECT_EQ(Utf8StringToWString(utf8), wide);
}

} // namespace

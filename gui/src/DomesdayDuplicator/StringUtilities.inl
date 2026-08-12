//----------------------------------------------------------------------------------------------------------------------
inline std::string WStringToUtf8String(const std::wstring& wideString)
{
    // Convert the encoding of the supplied string
    std::string stringUTF8;
    size_t sourceStringPos = 0;
    size_t sourceStringSize = wideString.size();
    stringUTF8.reserve(sourceStringSize * 2);
    while (sourceStringPos < sourceStringSize)
    {
        // Check if a surrogate pair is used for this character
        bool usesSurrogatePair = (((unsigned int)wideString[sourceStringPos] & 0xF800) == 0xD800);

        // Ensure that the requested number of code units are left in the source string
        if (usesSurrogatePair && ((sourceStringPos + 2) > sourceStringSize))
        {
            break;
        }

        // Decode the character from UTF-16 encoding
        unsigned int unicodeCodePoint;
        if (usesSurrogatePair)
        {
            unicodeCodePoint = 0x10000 + ((((unsigned int)wideString[sourceStringPos] & 0x03FF) << 10) | ((unsigned int)wideString[sourceStringPos + 1] & 0x03FF));
        }
        else
        {
            unicodeCodePoint = (unsigned int)wideString[sourceStringPos];
        }

        // Encode the character into UTF-8 encoding
        if (unicodeCodePoint <= 0x7F)
        {
            stringUTF8.push_back((char)unicodeCodePoint);
        }
        else if (unicodeCodePoint <= 0x07FF)
        {
            char convertedCodeUnit1 = (char)(0xC0 | (unicodeCodePoint >> 6));
            char convertedCodeUnit2 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
        }
        else if (unicodeCodePoint <= 0xFFFF)
        {
            char convertedCodeUnit1 = (char)(0xE0 | (unicodeCodePoint >> 12));
            char convertedCodeUnit2 = (char)(0x80 | ((unicodeCodePoint >> 6) & 0x3F));
            char convertedCodeUnit3 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
            stringUTF8.push_back(convertedCodeUnit3);
        }
        else
        {
            char convertedCodeUnit1 = (char)(0xF0 | (unicodeCodePoint >> 18));
            char convertedCodeUnit2 = (char)(0x80 | ((unicodeCodePoint >> 12) & 0x3F));
            char convertedCodeUnit3 = (char)(0x80 | ((unicodeCodePoint >> 6) & 0x3F));
            char convertedCodeUnit4 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
            stringUTF8.push_back(convertedCodeUnit3);
            stringUTF8.push_back(convertedCodeUnit4);
        }

        // Advance past the converted code units
        sourceStringPos += (usesSurrogatePair) ? 2 : 1;
    }

    // Return the converted string to the caller
    return stringUTF8;
}

//----------------------------------------------------------------------------------------------------------------------
inline std::wstring Utf8StringToWString(const std::string& stringUtf8)
{
    // Ensure the wchar_t type is a supported size (2-byte = UTF-16, 4-byte = UTF-32)
    static_assert((sizeof(wchar_t) == 2) || (sizeof(wchar_t) == 4), "Expected wchar_t to be 2 or 4 bytes");

    // Convert the encoding of the supplied string. We assume UTF8 encoding here, which will also work for basic ASCII
    // without codepage usage. Any codepage conversion is on the caller, as we can't know or understand the myriad of
    // encodings that may be used in this case.
    std::wstring stringAsWideString;
    size_t sourceStringPos = 0;
    size_t sourceStringSize = stringUtf8.size();
    stringAsWideString.reserve(sourceStringSize);
    while (sourceStringPos < sourceStringSize)
    {
        // Determine the number of code units required for the next character
        static const unsigned int codeUnitCountLookup[] = { 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 4 };
        unsigned int codeUnitCount = codeUnitCountLookup[(uint8_t)stringUtf8[sourceStringPos] >> 4];

        // Ensure that the requested number of code units are left in the source string
        if ((sourceStringPos + codeUnitCount) > sourceStringSize)
        {
            break;
        }

        // Convert the encoding of this character
        switch (codeUnitCount)
        {
        case 1:
        {
            stringAsWideString.push_back((wchar_t)stringUtf8[sourceStringPos]);
            break;
        }
        case 2:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x1F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F);
            stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            break;
        }
        case 3:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x0F) << 12) | (((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 2] & 0x3F);
            stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            break;
        }
        case 4:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x07) << 18) | (((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F) << 12) | (((unsigned int)stringUtf8[sourceStringPos + 2] & 0x3F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 3] & 0x3F);
            if constexpr (sizeof(wchar_t) == 2)
            {
                wchar_t convertedCodeUnit1 = 0xD800 | (((unicodeCodePoint - 0x10000) >> 10) & 0x03FF);
                wchar_t convertedCodeUnit2 = 0xDC00 | ((unicodeCodePoint - 0x10000) & 0x03FF);
                stringAsWideString.push_back(convertedCodeUnit1);
                stringAsWideString.push_back(convertedCodeUnit2);
            }
            else
            {
                stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            }
            break;
        }
        }

        // Advance past the converted code units
        sourceStringPos += codeUnitCount;
    }

    // Return the converted string to the caller
    return stringAsWideString;
}

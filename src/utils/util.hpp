// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#ifndef __UTIL_H_
#define __UTIL_H_

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <locale>
#include <set>
#include <string>
#include <string_view>
// from this project https://git.sr.ht/~leite/cso-pak

template <typename CHAR_TYPE>
inline size_t GetSumOfChars(std::basic_string_view<CHAR_TYPE> str)
{
    size_t res = 0;

    for (const auto character : str)
    {
        res += character;
    }

    return res;
}

inline std::string String_UTF16toUTF8(std::u16string_view str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    return conv.to_bytes(str.data(), str.data() + str.size());
}

inline std::string String_ToLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

// base cut to maxLength, with _1, _2 ... until no name in taken matches it without case; the result is added to taken
inline std::string String_UniqueName(std::string base, size_t maxLength, std::set<std::string>& taken)
{
    if (base.size() > maxLength)
    {
        base.resize(maxLength);
    }
    std::string candidate = base;
    int counter = 1;
    while (taken.count(String_ToLower(candidate)))
    {
        const std::string suffix = "_" + std::to_string(counter++);
        std::string stem = base;
        if (stem.size() + suffix.size() > maxLength)
        {
            stem.resize(maxLength - suffix.size());
        }
        candidate = stem + suffix;
    }
    taken.insert(String_ToLower(candidate));
    return candidate;
}

#endif  // __UTIL_H_

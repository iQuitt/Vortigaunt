#ifndef __UTIL_H_
#define __UTIL_H_

#include <codecvt>
#include <locale>
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

#endif  // __UTIL_H_

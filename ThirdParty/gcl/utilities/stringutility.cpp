#include "gcl/utilities/stringutility.h"

#include <locale>

namespace GCL::Utilities {

void toUpper(string& input)
{
    locale locale;

    for (auto& chr : input) {
        chr = toupper(chr, locale);
    }
}

void toLower(string& input)
{
    locale locale;

    for (auto& chr : input) {
        chr = tolower(chr, locale);
    }
}

void toUpperFirst(string& input)
{
    locale locale;
    input[0] = toupper(input[0], locale);
}

void toLowerFirst(string& input)
{
    locale locale;
    input[0] = tolower(input[0], locale);
}

} // namespace GCL::Utilities

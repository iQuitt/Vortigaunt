#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <streambuf>
#include <algorithm>
#include <cstring>

namespace Utils {


struct membuf : std::streambuf {
    membuf(const char* base, std::size_t size) {
        char* p = const_cast<char*>(base);
        this->setg(p, p, p + size);
    }
    
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override {
        char* new_gptr = nullptr;
        if (dir == std::ios_base::beg) {
            new_gptr = this->eback() + off;
        } else if (dir == std::ios_base::cur) {
            new_gptr = this->gptr() + off;
        } else if (dir == std::ios_base::end) {
            new_gptr = this->egptr() + off;
        }
        if (new_gptr < this->eback() || new_gptr > this->egptr()) {
            return pos_type(off_type(-1));
        }
        this->setg(this->eback(), new_gptr, this->egptr());
        return this->gptr() - this->eback();
    }

    pos_type seekpos(pos_type sp, std::ios_base::openmode which = std::ios_base::in) override {
        return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
    }
};

inline std::uint16_t Swap16(std::uint16_t v) {
    return ((v & 0xFF) << 8) | (v >> 8);
}

inline std::uint32_t Swap32(std::uint32_t v) {
    return ((v & 0xFF) << 24) |
           ((v & 0xFF00) << 8) |
           ((v & 0xFF0000) >> 8) |
           ((v & 0xFF000000) >> 24);
}

inline std::uint64_t Swap64(std::uint64_t v) {
    return ((v & 0xFFULL) << 56) |
           ((v & 0xFF00ULL) << 40) |
           ((v & 0xFF0000ULL) << 24) |
           ((v & 0xFF000000ULL) << 8) |
           ((v & 0xFF00000000ULL) >> 8) |
           ((v & 0xFF0000000000ULL) >> 24) |
           ((v & 0xFF000000000000ULL) >> 40) |
           ((v & 0xFF00000000000000ULL) >> 56);
}

inline std::uint16_t Read16(const std::uint8_t* p, bool bigEndian) {
    std::uint16_t v;
    std::memcpy(&v, p, 2);
    return bigEndian ? Swap16(v) : v;
}

inline std::uint32_t Read32(const std::uint8_t* p, bool bigEndian) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return bigEndian ? Swap32(v) : v;
}

inline std::uint16_t ReadU16(std::istream& in, bool bigEndian) {
    std::uint16_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 2);
    return bigEndian ? Swap16(v) : v;
}

inline std::uint32_t ReadU32(std::istream& in, bool bigEndian) {
    std::uint32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 4);
    return bigEndian ? Swap32(v) : v;
}

inline std::uint64_t ReadU64(std::istream& in, bool bigEndian) {
    std::uint64_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 8);
    return bigEndian ? Swap64(v) : v;
}

inline std::int16_t ReadS16(std::istream& in, bool bigEndian) {
    return static_cast<std::int16_t>(ReadU16(in, bigEndian));
}

inline std::int32_t ReadS32(std::istream& in, bool bigEndian) {
    return static_cast<std::int32_t>(ReadU32(in, bigEndian));
}

inline std::int64_t ReadS64(std::istream& in, bool bigEndian) {
    return static_cast<std::int64_t>(ReadU64(in, bigEndian));
}




inline std::uint16_t ReadBE16(const std::uint8_t* p) {
    return (static_cast<std::uint16_t>(p[0]) << 8) | p[1];
}

inline std::uint32_t ReadBE32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8)  |
           p[3];
}

inline std::uint64_t ReadBE64(const std::uint8_t* p) {
    return (static_cast<std::uint64_t>(p[0]) << 56) |
           (static_cast<std::uint64_t>(p[1]) << 48) |
           (static_cast<std::uint64_t>(p[2]) << 40) |
           (static_cast<std::uint64_t>(p[3]) << 32) |
           (static_cast<std::uint64_t>(p[4]) << 24) |
           (static_cast<std::uint64_t>(p[5]) << 16) |
           (static_cast<std::uint64_t>(p[6]) << 8)  |
           p[7];
}

// Converts Utilities
inline float Float16ToFloat32(std::uint16_t h) {
    std::uint32_t sign = (h & 0x8000) << 16;
    std::uint32_t exp = (h & 0x7C00) >> 10;
    std::uint32_t mant = h & 0x03FF;
    
    if (exp == 0) {
        if (mant != 0) {
            exp = 1;
            while ((mant & 0x0400) == 0) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x03FF;
            exp = (exp - 15 + 127) & 0xFF;
        }
    } else if (exp == 0x1F) {
        exp = 0xFF;
    } else {
        exp = (exp - 15 + 127) & 0xFF;
    }
    
    std::uint32_t val = sign | (exp << 23) | (mant << 13);
    float f = 0.0f;
    std::memcpy(&f, &val, 4);
    return f;
}

// String / Path Utilities

inline std::string ReadNullTerminatedString(std::istream& in, size_t limit = 10000) {
    std::string s;
    char c = 0;
    while (in.get(c) && c != '\0') {
        s.push_back(c);
        if (s.size() > limit) {
            break;
        }
    }
    return s;
}

inline std::string Normalise(const std::string& path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    return p;
}

inline std::string SanitizeFilename(const std::string& str) {
    std::string clean = str;
    for (char& c : clean) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return clean;
}

} // namespace Utils

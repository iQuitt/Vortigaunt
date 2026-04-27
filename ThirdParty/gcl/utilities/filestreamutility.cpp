#include "gcl/utilities/filestreamutility.h"

namespace GCL::Utilities::FileStreamUtility {

template <typename T>
T binaryread(fstream& file, unsigned offset)
{
    T bytes;
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&bytes), sizeof(T));
    return bytes;
}

template <>
unsigned binaryread(fstream& file, unsigned offset)
{
    unsigned bytes;
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&bytes), sizeof(unsigned));
    return bytes;
}

void binarywrite(fstream& file, unsigned offset, const char* bytes, unsigned size)
{
    file.seekp(offset);
    file.write(bytes, size > 0 ? size : strlen(bytes));
}

unsigned filesize(fstream& file)
{
    file.seekg(0, std::ios::end);
    unsigned size = static_cast<unsigned>(file.tellg());
    file.seekg(0, std::ios::beg);
    return size;
}

vector<char> binarydata(fstream& file)
{
    unsigned size = filesize(file);

    vector<char> bytes;
    bytes.resize(static_cast<unsigned>(size), 0x00);
    file.read(bytes.data(), size);

    return bytes;
}

} // namespace GCL::Utilities::FileStreamUtility

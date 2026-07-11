#include "Png.h"
#include "FileIO.h"

#include <cstdio>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

namespace
{
    struct WriteContext
    {
        FILE* file = nullptr;
        bool  ok = true;
    };

    void writeToFile(void* context, void* data, int size)
    {
        auto* ctx = static_cast<WriteContext*>(context);
        ctx->ok &= (std::fwrite(data, 1, static_cast<size_t>(size), ctx->file) == static_cast<size_t>(size));
    }
}

namespace PNG
{

bool saveArgb(const std::string& filename, int width, int height, const uint32_t* argbPixels)
{
    if (width <= 0 || height <= 0 || !argbPixels)
        return false;

    const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> rgba(total * 4);
    for (size_t i = 0; i < total; ++i)
    {
        uint32_t c = argbPixels[i];
        rgba[i * 4 + 0] = (c >> 16) & 0xFF; // R
        rgba[i * 4 + 1] = (c >> 8)  & 0xFF; // G
        rgba[i * 4 + 2] = (c)       & 0xFF; // B
        rgba[i * 4 + 3] = (c >> 24) & 0xFF; // A
    }

    WriteContext ctx;
    ctx.file = FileIO::openFile(filename, "wb");
    if (!ctx.file)
        return false;

    int encoded = stbi_write_png_to_func(&writeToFile, &ctx, width, height, 4, rgba.data(), width * 4);
    bool closed = (std::fclose(ctx.file) == 0); // fclose reports buffered-flush failures
    return encoded != 0 && ctx.ok && closed;
}

} // namespace PNG

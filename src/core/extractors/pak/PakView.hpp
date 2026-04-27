#ifndef __PAKVIEW_H_
#define __PAKVIEW_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>

template <size_t ALIGNMENT>
inline size_t GetAlignedLength(size_t len) noexcept
{
    return ALIGNMENT * ((len + ALIGNMENT - 1) / ALIGNMENT);
}

template <class CIPHER, size_t ALIGNMENT>
class PakView
{
public:
    PakView(std::span<const uint8_t> buffer, std::span<const uint8_t> cipherKey)
        : m_Buffer(buffer), m_CurOffset(0), m_AvailableBytesCount(0)
    {
        this->m_Cipher.SetKey(cipherKey.data());
    }

    template <typename DATA_TYPE>
    [[nodiscard]] DATA_TYPE Read() requires std::is_arithmetic<DATA_TYPE>::value
    {
        DATA_TYPE res;

        const auto byteLen = sizeof(DATA_TYPE);
        const auto alignedLen = GetAlignedLength<ALIGNMENT>(byteLen);

        if (this->m_AvailableBytesCount > 0)
        {
            this->ReadRemainingData(
                { reinterpret_cast<uint8_t*>(&res), byteLen });
            return res;
        }

        auto buf = this->ReadBytes(alignedLen);
        std::memcpy(&res, buf.get(), byteLen);

        this->SaveAnyRemainingBytes(reinterpret_cast<uint8_t*>(&res), byteLen,
            alignedLen);

        return res;
    }

    template <typename STRING_TYPE>
    [[nodiscard]] STRING_TYPE ReadString(size_t length)
    {
        const auto byteLen = length * sizeof(typename STRING_TYPE::value_type);
        const auto alignedLen = GetAlignedLength<ALIGNMENT>(byteLen);

        STRING_TYPE res;

        if (this->m_AvailableBytesCount > 0)
        {
            auto strBuf = std::unique_ptr<typename STRING_TYPE::value_type>(
                new typename STRING_TYPE::value_type[length]);
            auto strPtr = strBuf.get();

            this->ReadRemainingData(
                { reinterpret_cast<uint8_t*>(strPtr), byteLen });

            res = { strPtr, length };
        }
        else
        {
            if (this->CanReadBytes(alignedLen) == false)
            {
                throw std::out_of_range("The data buffer is too small");
            }

            auto buf = this->ReadBytes(alignedLen);
            auto bufPtr = buf.get();

            res = { reinterpret_cast<typename STRING_TYPE::value_type*>(bufPtr),
                    length };

            this->SaveAnyRemainingBytes(reinterpret_cast<uint8_t*>(bufPtr),
                byteLen, alignedLen);
        }

        return res;
    }

    template <typename T, size_t ARRAY_SIZE>
    [[nodiscard]] std::array<T, ARRAY_SIZE> ReadArray()
    {
        const size_t byteLen = sizeof(T) * ARRAY_SIZE;
        const auto alignedLen = GetAlignedLength<ALIGNMENT>(byteLen);

        std::array<T, ARRAY_SIZE> result;

        if (this->m_AvailableBytesCount > 0)
        {
            this->ReadRemainingData(
                { reinterpret_cast<uint8_t*>(result.data()), byteLen });
        }
        else
        {
            if (this->CanReadBytes(alignedLen) == false)
            {
                throw std::length_error(
                    "The array's length is larger than the available data");
            }

            auto buf = this->ReadBytes(alignedLen);
            std::memcpy(result.data(), buf.get(), byteLen);

            this->SaveAnyRemainingBytes(
                reinterpret_cast<uint8_t*>(result.data()), byteLen, alignedLen);
        }

        return result;
    }

    [[nodiscard]] std::unique_ptr<uint8_t> ReadBytes(size_t length,
        bool aligned = false)
    {
        if (aligned == true)
        {
            length = GetAlignedLength<ALIGNMENT>(length);
        }

        if (this->CanReadBytes(length) == false)
        {
            throw std::out_of_range("The data buffer is too small");
        }

        std::unique_ptr<uint8_t> res(new uint8_t[length]);

        this->m_Cipher.DecryptBuffer(
            res.get(), this->m_Buffer.data() + this->m_CurOffset, length);
        this->m_CurOffset += length;

        return res;
    }

    [[nodiscard]] inline size_t GetCurOffset() const
    {
        return this->m_CurOffset;
    }

protected:
    inline void SaveAnyRemainingBytes(const uint8_t* buf, size_t length,
        size_t alignedLen)
    {
        const auto leftoverBytes = alignedLen - length;

        if (leftoverBytes > 0)
        {
            std::memcpy(
                this->m_AvailableBytes.data() + this->m_AvailableBytesCount,
                buf + length, leftoverBytes);
            this->m_AvailableBytesCount += leftoverBytes;
        }
    }

    inline void ReadRemainingData(std::span<uint8_t> data)
    {
        std::memcpy(data.data(), this->m_AvailableBytes.data(),
            this->m_AvailableBytesCount);

        if (this->m_AvailableBytesCount >= data.size_bytes())
        {
            this->m_AvailableBytesCount -= data.size_bytes();
        }
        else
        {
            const auto remainingBytes =
                data.size_bytes() - this->m_AvailableBytesCount;
            const auto alignedRemBytes =
                GetAlignedLength<ALIGNMENT>(remainingBytes);
            const auto offset = this->m_AvailableBytesCount;

            this->m_AvailableBytesCount = 0;

            auto remainingBuf = this->ReadBytes(alignedRemBytes);

            this->SaveAnyRemainingBytes(remainingBuf.get(), remainingBytes,
                alignedRemBytes);

            std::memcpy(data.data() + offset, remainingBuf.get(),
                remainingBytes);
        }
    }

    [[nodiscard]] inline bool CanReadBytes(size_t bytes) const noexcept
    {
        return this->m_Buffer.size_bytes() >= this->m_CurOffset + bytes;
    }

    [[nodiscard]] inline size_t GetRemainingBytes() const noexcept
    {
        return this->m_Buffer.size_bytes() - this->m_CurOffset;
    }

private:
    CIPHER m_Cipher;
    std::span<const uint8_t> m_Buffer;
    size_t m_CurOffset;

    std::array<uint8_t, ALIGNMENT> m_AvailableBytes;
    size_t m_AvailableBytesCount;
};

#endif  // __PAKVIEW_H_

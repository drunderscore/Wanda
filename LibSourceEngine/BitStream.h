#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Vector.h>

namespace SourceEngine
{
class BitStream
{
public:
    // Instead of having these in a class like SeekableBitStream, and then needing types that are both writable and
    // seekable or readable and seekable, we just have inheritors implement them always. If they can't support them,
    // then error. Not great but better than having many classes to represent the different variations of streams, and
    // for methods that sometimes want a stream that can seek.
    ALWAYS_INLINE virtual ErrorOr<size_t> position() const = 0;
    ALWAYS_INLINE virtual ErrorOr<void> set_position(size_t) = 0;
};

class ReadableBitStream : public BitStream
{
public:
    // Reads a single bit
    ALWAYS_INLINE virtual ErrorOr<bool> read() = 0;

    template<typename T>
    ALWAYS_INLINE ErrorOr<T> read_typed(u8 number_of_bits = sizeof(T) << 3) requires(IsIntegral<T>)
    {
        T value = 0;

        for (auto i = 0; i < number_of_bits; ++i)
        {
            if (TRY(read()))
                value |= (1 << i);
        }

        return value;
    }

    // FIXME: This should be IsFloatintPoint<T> but double had some problems with always being nan. Because we
    //        read/write doubles yet we'll fix it when we need it.
    template<typename T>
    ALWAYS_INLINE ErrorOr<T> read_typed(u8 number_of_bits = sizeof(T) << 3) requires(IsSame<T, float>)
    {
        // FIXME: Is this okay to do?
        //        Conditional<sizeof(T) == 4, u32, u64> value = 0;
        u32 value = 0;

        for (auto i = 0; i < number_of_bits; ++i)
        {
            if (TRY(read()))
                value |= (1 << i);
        }

        // FIXME: This kinda sucks! Must we reinterpret cast and then deref in order to get the floating point value
        //        without conversion?
        return *reinterpret_cast<T*>(&value);
    }

    ALWAYS_INLINE ErrorOr<ByteBuffer> read_bytes(size_t number_of_bytes)
    {
        auto buffer = TRY(ByteBuffer::create_uninitialized(number_of_bytes));
        for (auto i = 0; i < number_of_bytes; i++)
            buffer.data()[i] = TRY(read_typed<u8>());

        return buffer;
    }

    ALWAYS_INLINE ErrorOr<void> read_bytes(Bytes bytes)
    {
        for (auto i = 0; i < bytes.size(); i++)
            bytes.data()[i] = TRY(read_typed<u8>());

        return {};
    }

    // FIXME: Implement this better? This one is straight from the Engine and might suck
    // TODO: Might be nice to template this, don't need it yet though
    ALWAYS_INLINE ErrorOr<u32> read_varint()
    {
        u32 result = 0;
        int count = 0;
        u32 byte;

        do
        {
            if (count == 5)
                return result;

            byte = TRY(read_typed<u8>());
            result |= (byte & 0x7F) << (7 * count);
            ++count;
        } while (byte & 0x80);

        return result;
    }
};

class WritableBitStream : public BitStream
{
public:
    // Writes a single bit
    ALWAYS_INLINE virtual ErrorOr<void> write(bool value) = 0;

    template<typename T>
    ALWAYS_INLINE ErrorOr<void> write_typed(T value, u8 number_of_bits = sizeof(T) << 3) requires(IsIntegral<T>)
    {
        for (auto i = 0; i < number_of_bits; ++i)
            TRY(write((value & (1 << i)) != 0));

        return {};
    }

    // FIXME: This should be IsFloatintPoint<T> but double had some problems with always being nan. Because we
    //        read/write doubles yet we'll fix it when we need it.
    template<typename T>
    ALWAYS_INLINE ErrorOr<void> write_typed(T value, u8 number_of_bits = sizeof(T) << 3) requires(IsSame<T, float>)
    {
        // FIXME: Is this okay to do?
        //        auto value_integral = *reinterpret_cast<Conditional<sizeof(T) == 4, u32*, u64*>>(&value);
        auto value_integral = *reinterpret_cast<u32*>(&value);

        for (auto i = 0; i < number_of_bits; ++i)
            TRY(write((value_integral & (1 << i)) != 0));

        return {};
    }

    ALWAYS_INLINE ErrorOr<void> write_bytes(ReadonlyBytes bytes)
    {
        for (auto b : bytes)
            TRY(write_typed(b));

        return {};
    }

    ALWAYS_INLINE ErrorOr<void> write_varint(u32 value)
    {
        while (value > 0x7F)
        {
            TRY(write_typed<u8>((value & 0x7F) | 0x80));
            value >>= 7;
        }

        TRY(write_typed(value & 0x7F));

        return {};
    }
};

class MemoryBitStream final : public ReadableBitStream, public WritableBitStream
{
public:
    explicit MemoryBitStream(Bytes bytes) : m_bytes(bytes) {}
    explicit MemoryBitStream(ReadonlyBytes bytes) : m_bytes(bytes) {}

    ALWAYS_INLINE ErrorOr<bool> read() override
    {
        auto bytes = readonly_bytes();

        auto index = m_current_bit >> 3;
        if (index >= bytes.size())
            return Error::from_string_literal("Cannot read out of bounds");

        // We index the pointer directly instead of using the Bytes (Span<u8>) indexer as we don't want to incur a
        // second bounds check
        auto value = bytes.data()[index] >> (m_current_bit & 7);

        ++m_current_bit;
        return value & 1;
    }

    ALWAYS_INLINE ErrorOr<void> write(bool value) override
    {
        if (m_bytes.has<ReadonlyBytes>())
            return Error::from_string_literal("Cannot write to a ReadonlyBytes-backed MemoryBitStream");

        auto bytes = m_bytes.get<Bytes>();

        auto index = m_current_bit >> 3;
        if (index >= bytes.size())
            return Error::from_string_literal("Cannot write out of bounds");

        if (value)
            bytes.data()[index] |= 1 << (m_current_bit & 7);
        else
            bytes.data()[index] &= ~(1 << (m_current_bit & 7));

        ++m_current_bit;

        return {};
    }

    ALWAYS_INLINE ErrorOr<size_t> position() const override { return m_current_bit; }

    ALWAYS_INLINE ErrorOr<void> set_position(size_t position) override
    {
        auto bytes = readonly_bytes();

        if ((position >> 3) >= bytes.size())
            return Error::from_string_literal("Cannot set position out of bounds");

        m_current_bit = position;
        return {};
    }

    // We should always copy Span<T> or Bytes types, they are already a view (a kind of reference!)
    ALWAYS_INLINE Variant<ReadonlyBytes, Bytes> bytes() const { return m_bytes; }

    ALWAYS_INLINE ReadonlyBytes readonly_bytes() const
    {
        return m_bytes.visit([](ReadonlyBytes value) -> ReadonlyBytes { return value; },
                             [](Bytes value) -> ReadonlyBytes { return value; });
    }

    ALWAYS_INLINE ErrorOr<void> skip(size_t number_of_bits)
    {
        auto bytes = readonly_bytes();

        auto target_current_bit = m_current_bit + number_of_bits;
        if ((target_current_bit >> 3) > bytes.size())
            return Error::from_string_literal("Cannot skip out of bounds");

        m_current_bit = target_current_bit;

        return {};
    }

private:
    Variant<ReadonlyBytes, Bytes> m_bytes;
    size_t m_current_bit{};
};

class ExpandingBitStream final : public WritableBitStream
{
public:
    ExpandingBitStream() {}
    explicit ExpandingBitStream(ByteBuffer&& bytes) : m_bytes(move(bytes)) {}

    ALWAYS_INLINE ErrorOr<void> write(bool value) override
    {
        auto index = m_current_bit >> 3;
        auto* value_at_index = TRY(at(index));

        if (value)
            *value_at_index |= 1 << (m_current_bit & 7);
        else
            *value_at_index &= ~(1 << (m_current_bit & 7));

        ++m_current_bit;

        return {};
    }

    ReadonlyBytes bytes() const
    {
        // FIXME: This calculation kinda sucks, any way we can do better?
        //        If we are on a byte boundary, we don't want an additional byte, but otherwise we need to include that
        //        last partial byte
        auto length_bytes = (m_current_bit >> 3) + (m_current_bit % 8 == 0 ? 0 : 1);

        return m_bytes.span().slice(0, length_bytes);
    }

    Bytes bytes()
    {
        // FIXME: This calculation kinda sucks, any way we can do better?
        //        If we are on a byte boundary, we don't want an additional byte, but otherwise we need to include that
        //        last partial byte
        auto length_bytes = (m_current_bit >> 3) + (m_current_bit % 8 == 0 ? 0 : 1);

        return m_bytes.span().slice(0, length_bytes);
    }

    ErrorOr<ByteBuffer> release_bytes()
    {
        // FIXME: This calculation kinda sucks, any way we can do better?
        //        If we are on a byte boundary, we don't want an additional byte, but otherwise we need to include that
        //        last partial byte
        auto length_bytes = (m_current_bit >> 3) + (m_current_bit % 8 == 0 ? 0 : 1);

        TRY(m_bytes.try_resize(length_bytes));
        return move(m_bytes);
    }

    ALWAYS_INLINE ErrorOr<size_t> position() const override { return m_current_bit; }

    ALWAYS_INLINE ErrorOr<void> set_position(size_t position) override
    {
        // FIXME: Should we just resize m_bytes to fit within this new position?
        if ((position >> 3) >= m_bytes.size())
            return Error::from_string_literal("Cannot set position out of bounds");

        m_current_bit = position;
        return {};
    }

private:
    // Can't do a ErrorOr<T&>, have to use a pointer :^(
    ALWAYS_INLINE ErrorOr<u8*> at(size_t index)
    {
        if (index >= m_bytes.size())
            TRY(m_bytes.try_resize(m_bytes.size() + amount_to_resize_each_allocation));

        return &m_bytes[index];
    }

    static constexpr size_t amount_to_resize_each_allocation = 256;
    ByteBuffer m_bytes;
    size_t m_current_bit{};
};
}

template<typename T>
ErrorOr<void> operator>>(SourceEngine::ReadableBitStream& stream, T& value) requires(IsIntegral<T>)
{
    value = TRY(stream.template read_typed<T>());
    return {};
}

template<typename T>
ErrorOr<void> operator>>(SourceEngine::ReadableBitStream& stream, T& value) requires(IsSame<T, float>)
{
    value = TRY(stream.template read_typed<T>());
    return {};
}

template<typename T>
ErrorOr<void> operator<<(SourceEngine::WritableBitStream& stream, T value) requires(IsIntegral<T>)
{
    return stream.template write_typed(value);
}

template<typename T>
ErrorOr<void> operator<<(SourceEngine::WritableBitStream& stream, T value) requires(IsSame<T, float>)
{
    return stream.template write_typed(value);
}

ErrorOr<void> operator>>(SourceEngine::ReadableBitStream& stream, String& value);
ErrorOr<void> operator<<(SourceEngine::WritableBitStream& stream, StringView value);
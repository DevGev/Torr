#pragma once

#include <memory>
#include <optional>
#include <cstdint>
#include <random>
#include <print>

inline bool bitset_u8_bit_get(uint8_t b, uint8_t pos)
{
    return ((b & (1 << pos) ) != 0);
}

inline void bitset_u8_bit_toggle(uint8_t* b, uint8_t pos, bool toggle)
{
    if (toggle) {
        b[pos / 8] |= (0x1 << (pos % 8));
    } else {
        b[pos / 8] &= (~(0x1 << (pos % 8)));
    }
}


class dynamic_bitset {
private:
    std::unique_ptr<std::byte> m_bytes = nullptr;
    uint8_t* m_alternate_bytes = 0;
    size_t m_bytes_size = 0;
    size_t m_bits_size = 0;

public:
    dynamic_bitset() {}
    ~dynamic_bitset() {}

    static void copy(dynamic_bitset& target, const dynamic_bitset& source)
    {
        target.m_alternate_bytes = source.m_alternate_bytes;
        target.m_bytes_size = source.m_bytes_size;
        target.m_bits_size = source.m_bits_size;
        target.m_bytes = std::unique_ptr<std::byte>(new std::byte[target.m_bytes_size]);
    }

    dynamic_bitset(const dynamic_bitset& source)
    {
        copy(*this, source);
    }

    dynamic_bitset& operator=(const dynamic_bitset& source) {
        if (this != &source) {
            copy(*this, source);
        }
        return *this;
    }

    size_t bits_size() const { return m_bits_size; }
    size_t bytes_size() const { return m_bytes_size; }

    uint8_t* data()
    {
        if (m_alternate_bytes != 0)
            return m_alternate_bytes;
        return (uint8_t*)m_bytes.get();
    }

    const uint8_t* const_data() const
    {
        if (m_alternate_bytes != 0)
            return (const uint8_t*)m_alternate_bytes;
        return (const uint8_t*)m_bytes.get();
    }

    bool boundary(size_t bit) const
    {
        if (bit >= bits_size())
            return false;
        return true;
    }

    bool bit_set(size_t bit)
    {
        if (!boundary(bit))
            return false;
        data()[bit / 8] |= (0x1 << (bit % 8));
        return true;
    }

    bool bit_clear(size_t bit)
    {
        if (!boundary(bit))
            return false;
        data()[bit / 8] &= (~(0x1 << (bit % 8)));
        return true;
    }

    bool bit_get(size_t bit) const
    {
        if (!boundary(bit))
            return false;
        return ((const_data()[bit / 8] & (0x1 << (bit % 0x8))) != 0x0);
    }

    void resize(size_t bytes)
    {
        m_bytes_size = bytes;
        m_bits_size = bytes * 8;
        m_bytes = std::unique_ptr<std::byte>(new std::byte[bytes]);
    }

    void from_existing_buffer(uint8_t* bytes, size_t size_in_bytes)
    {
        m_alternate_bytes = bytes;
        m_bytes_size = size_in_bytes;
        m_bits_size = size_in_bytes * 8;
    }

    /* Find the occurence where
     * this.bit_get(index) = false and
     * bitset_friend.bit_get(index) = true
     * */
    std::optional<size_t> find_positive_bit_compliment
        (const dynamic_bitset& bitset_friend, bool randomize = false) const
    {
        std::optional<size_t> found = {};
        size_t bitfield_size = std::min(bytes_size(), bitset_friend.bytes_size());
        std::println("bitfield_size {}", bitfield_size);
        int64_t mismatched_bit_position = -1;
        int64_t mismatched_byte_position = -1;
        uint32_t skips = 0;

        if (randomize) {
            std::random_device os_seed;
            const uint_least32_t seed = os_seed();
            std::mt19937 generator(seed);
            std::uniform_int_distribution<uint_least32_t> distribute(0, bitfield_size - 1);
            skips = distribute(generator);
        }

        for (int64_t i = 0; i < bitfield_size; ++i) {
            if (bitset_friend.const_data()[i] != this->const_data()[i]) {
                mismatched_bit_position = i * 8;
                mismatched_byte_position = i;

                if (skips <= 0) break;
                skips--;
            }
        }

        if (mismatched_byte_position == -1)
            return found;

        for (int64_t i = 0; i < 8; ++i) {
            if (bitset_friend.bit_get(mismatched_bit_position + i)
                && !this->bit_get(mismatched_bit_position + i)) {
                found = mismatched_bit_position + i;
                break;
            }
        }

        return found;
    }

    std::optional<size_t> find_first_positive_bit_compliment
        (const dynamic_bitset& bitset_friend) const
    {
        return find_positive_bit_compliment(bitset_friend);
    }
};

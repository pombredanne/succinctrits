#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include "trit_vector.hpp"

namespace succinctrits {

template <uint8_t Trit>
class rs_support {
  private:
    static_assert(Trit < 3, "");

    static constexpr uint64_t TRITS_PER_LB = 65550;
    static constexpr uint64_t TRITS_PER_SB = 50;

    static constexpr uint64_t TRITS_PER_BYTE = trit_vector::TRITS_PER_BYTE;
    static constexpr uint64_t TRYTES_PER_LB = TRITS_PER_LB / TRITS_PER_BYTE;  // 13110 trytes
    static constexpr uint64_t TRYTES_PER_SB = TRITS_PER_SB / TRITS_PER_BYTE;  // 10 trytes
    static constexpr uint64_t LB_PER_SB = TRITS_PER_LB / TRITS_PER_SB;  // 1311

  public:
    rs_support() = default;

    explicit rs_support(const trit_vector* vec) {
        build(vec);
    }

    void build(const trit_vector* vec) {
        m_vec = vec;
        m_large_blocks.clear();
        m_small_blocks.clear();
        m_large_blocks.reserve(m_vec->m_trytes.size() / TRYTES_PER_LB + 1);
        m_small_blocks.reserve(m_vec->m_trytes.size() / TRYTES_PER_SB + 1);

        uint64_t rank = 0;
        for (uint64_t i = 0; i < m_vec->m_trytes.size(); ++i) {
            if (i % TRYTES_PER_LB == 0) {
                m_large_blocks.push_back(rank);
            }
            if (i % TRYTES_PER_SB == 0) {
                assert(rank - m_large_blocks.back() <= UINT16_MAX);
                m_small_blocks.push_back(uint16_t(rank - m_large_blocks.back()));
            }
            rank += LUT[4][m_vec->m_trytes[i]];
        }
        m_num_target_trits = rank;
    }

    void set_vector(const trit_vector* vec) {
        m_vec = vec;
    }

    uint8_t get(uint64_t i) const {
        assert(m_vec != nullptr);
        return m_vec->get(i);
    }
    uint8_t operator[](uint64_t i) const {
        return get(i);
    }

    // Returns the number of occurrences of the target trits in m_vec between positions 0 and i-1.
    uint64_t rank(const uint64_t i) const {
        assert(m_vec != nullptr);
        assert(i < m_vec->get_num_trits());

        const uint64_t lb_pos = i / TRITS_PER_LB;
        const uint64_t sb_pos = i / TRITS_PER_SB;
        uint64_t rank = m_large_blocks[lb_pos] + m_small_blocks[sb_pos];

        const uint64_t tryte_pos = i / TRITS_PER_BYTE;
        const uint64_t tryte_beg = tryte_pos / TRYTES_PER_SB * TRYTES_PER_SB;
        for (uint64_t j = tryte_beg; j < tryte_pos; ++j) {
            rank += LUT[4][m_vec->m_trytes[j]];
        }

        const uint8_t tryte = m_vec->m_trytes[tryte_pos];
        const uint64_t k = i % TRITS_PER_BYTE;

        if (k != 0) {
            rank += LUT[k - 1][tryte];
        }
        return rank;
    }

    // Returns the position of the (n+1)-th occurrence of the target trit in m_vec.
    uint64_t select(uint64_t n) const {
        assert(m_vec != nullptr);
        assert(n < m_num_target_trits);

        // (1) Search on Large Blocks
        uint64_t left = 0;
        uint64_t right = m_large_blocks.size();

        while (left + 1 < right) {
            const uint64_t center = (left + right) / 2;
            if (n < m_large_blocks[center]) {
                right = center;
            } else {
                left = center;
            }
        }
        assert(m_large_blocks[left] <= n);

        // (2) Search on Small Blocks
        n = n - m_large_blocks[left];

        left = left * LB_PER_SB;  // position of SB
        right = std::min<uint64_t>(left + LB_PER_SB, m_small_blocks.size());

        while (left + 1 < right) {
            const uint64_t center = (left + right) / 2;
            if (n < m_small_blocks[center]) {
                right = center;
            } else {
                left = center;
            }
        }
        uint64_t i = left;
        assert(m_small_blocks[i] <= n);

        // (3) Search on the remaining trytes
        n = n - m_small_blocks[i];
        i = i * TRYTES_PER_SB;  // position of trytes

        ++n;

        for (;; ++i) {
            const uint8_t cnt = LUT[4][m_vec->m_trytes[i]];
            if (n <= cnt) {
                break;
            }
            n = n - cnt;
        }

        const uint8_t tryte = m_vec->m_trytes[i];
        assert(n <= LUT[4][tryte]);

        if (n == LUT[0][tryte]) {
            return i * TRITS_PER_BYTE;
        } else if (n == LUT[1][tryte]) {
            return i * TRITS_PER_BYTE + 1;
        } else if (n == LUT[2][tryte]) {
            return i * TRITS_PER_BYTE + 2;
        } else if (n == LUT[3][tryte]) {
            return i * TRITS_PER_BYTE + 3;
        } else {
            assert(n == LUT[4][tryte]);
            return i * TRITS_PER_BYTE + 4;
        }
    }

    uint64_t get_num_trits() const {
        return m_vec->get_num_trits();
    }
    uint64_t get_num_target_trits() const {
        return m_num_target_trits;
    }
    uint64_t size_in_bytes() const {
        return m_large_blocks.size() * sizeof(uint64_t) +  //
               m_small_blocks.size() * sizeof(uint16_t) +  //
               sizeof(m_num_target_trits);
    }

    void save(std::ostream& os) const {
        size_t n_L = m_large_blocks.size();
        os.write(reinterpret_cast<const char*>(&n_L), sizeof(size_t));
        os.write(reinterpret_cast<const char*>(m_large_blocks.data()), sizeof(uint64_t) * n_L);
        size_t n_S = m_small_blocks.size();
        os.write(reinterpret_cast<const char*>(&n_S), sizeof(size_t));
        os.write(reinterpret_cast<const char*>(m_small_blocks.data()), sizeof(uint16_t) * n_S);
        os.write(reinterpret_cast<const char*>(&m_num_target_trits), sizeof(m_num_target_trits));
    }
    void load(std::istream& is) {
        size_t n_L = 0;
        is.read(reinterpret_cast<char*>(&n_L), sizeof(size_t));
        m_large_blocks.resize(n_L);
        is.read(reinterpret_cast<char*>(m_large_blocks.data()), sizeof(uint64_t) * n_L);
        size_t n_S = 0;
        is.read(reinterpret_cast<char*>(&n_S), sizeof(size_t));
        m_small_blocks.resize(n_S);
        is.read(reinterpret_cast<char*>(m_small_blocks.data()), sizeof(uint16_t) * n_S);
        is.read(reinterpret_cast<char*>(&m_num_target_trits), sizeof(m_num_target_trits));
    }

  private:
    static const uint8_t LUT[5][243];  // 243 = 3**5

    const trit_vector* m_vec = nullptr;
    std::vector<uint64_t> m_large_blocks;
    std::vector<uint16_t> m_small_blocks;
    uint64_t m_num_target_trits = 0;
};

template <>
const uint8_t rs_support<0>::LUT[5][243] = {
    {
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    },
    {
        2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0,
        0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1,
        0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0,
        1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0,
        0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1,
        0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1,
        1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0,
    },
    {
        3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1,
        1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1,
        0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0,
        1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1,
        1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1,
        0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1,
        1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0,
    },
    {
        4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1,
        1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1,
        0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1,
        2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1,
        1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2,
        1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1,
        1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0,
    },
    {
        5, 4, 4, 4, 3, 3, 4, 3, 3, 4, 3, 3, 3, 2, 2, 3, 2, 2, 4, 3, 3, 3, 2, 2, 3, 2, 2, 4, 3, 3, 3, 2, 2, 3, 2,
        2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2,
        1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1,
        2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1,
        1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2,
        1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1,
        1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0,
    },
};

template <>
const uint8_t rs_support<1>::LUT[5][243] = {
    {
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    },
    {
        0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1,
        0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0,
        1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1,
        0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2,
        1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1,
        2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0,
        1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0,
    },
    {
        0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1,
        0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1,
        2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1,
        0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2,
        1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2,
        3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0,
        1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0,
    },
    {
        0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2,
        1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1,
        2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1,
        0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2,
        1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2,
        3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1,
        2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0,
    },
    {
        0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2,
        1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1,
        2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2,
        1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 3, 4, 3, 4, 5, 4, 3, 4, 3, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3,
        2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2,
        3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1,
        2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0,
    },
};

template <>
const uint8_t rs_support<2>::LUT[5][243] = {
    {
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
    },
    {
        0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1,
        2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1,
        1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1,
        1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0,
        1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0,
        0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1,
        0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2,
    },
    {
        0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1,
        2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1,
        1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2,
        2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0,
        1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0,
        0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2,
        1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3,
    },
    {
        0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1,
        2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2,
        2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2,
        2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1,
        2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0,
        0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2,
        1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4,
    },
    {
        0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1,
        2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2,
        2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2,
        2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1,
        2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1,
        1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3,
        2, 2, 3, 3, 3, 4, 2, 2, 3, 2, 2, 3, 3, 3, 4, 2, 2, 3, 2, 2, 3, 3, 3, 4, 3, 3, 4, 3, 3, 4, 4, 4, 5,
    },
};

}  // namespace succinctrits
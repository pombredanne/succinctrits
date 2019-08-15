#pragma once

#include <cassert>
#include <iostream>
#include <vector>

#include "trit_vector.hpp"

namespace succinctrits {

template <uint8_t Trit>
class rank_support {
  private:
    static_assert(Trit < 3, "");

    static constexpr uint32_t TRITS_PER_LB = 65550U;
    static constexpr uint32_t TRITS_PER_SB = 50U;

    static constexpr uint32_t TRYTES_PER_LB = TRITS_PER_LB / trit_vector::TRITS_PER_BYTE;  // 13110 trytes
    static constexpr uint32_t TRYTES_PER_SB = TRITS_PER_SB / trit_vector::TRITS_PER_BYTE;  // 10 trytes

  public:
    rank_support() = default;

    explicit rank_support(const trit_vector* vec) {
        build(vec);
    }

    void build(const trit_vector* vec) {
        m_vec = vec;
        m_LBs.clear();
        m_SBs.clear();

        m_LBs.reserve(m_vec->m_trytes.size() / TRYTES_PER_LB + 1U);
        m_SBs.reserve(m_vec->m_trytes.size() / TRYTES_PER_SB + 1U);

        uint32_t rank = 0;
        for (uint32_t i = 0; i < m_vec->m_trytes.size(); ++i) {
            if (i % TRYTES_PER_LB == 0) {
                m_LBs.push_back(rank);
            }
            if (i % TRYTES_PER_SB == 0) {
                assert(rank - m_LBs.back() <= UINT16_MAX);
                m_SBs.push_back(uint16_t(rank - m_LBs.back()));
            }
            rank += LUT[m_vec->m_trytes[i]];
        }
    }

    uint32_t rank(const uint32_t i) const {
        assert(m_vec != nullptr);
        assert(i < m_vec->get_num_trits());

        const uint32_t lb_pos = i / TRITS_PER_LB;
        const uint32_t sb_pos = i / TRITS_PER_SB;
        uint32_t rank = m_LBs[lb_pos] + m_SBs[sb_pos];

        const uint32_t tryte_pos = i / trit_vector::TRITS_PER_BYTE;
        const uint32_t tryte_beg = tryte_pos / TRYTES_PER_SB * TRYTES_PER_SB;
        for (uint32_t j = tryte_beg; j < tryte_pos; ++j) {
            rank += LUT[m_vec->m_trytes[j]];
        }
        const uint8_t tryte = m_vec->m_trytes[tryte_pos];
        const uint32_t k = i % trit_vector::TRITS_PER_BYTE;

        if (k > 0)
            if (tryte % 3 == Trit) ++rank;
        if (k > 1)
            if (tryte / 3 % 3 == Trit) ++rank;
        if (k > 2)
            if (tryte / 9 % 3 == Trit) ++rank;
        if (k > 3)
            if (tryte / 27 % 3 == Trit) ++rank;
        if (k > 4)
            if (tryte / 81 % 3 == Trit) ++rank;

        return rank;
    }

  private:
    static const uint8_t LUT[243];  // 243 = 3**5

    const trit_vector* m_vec = nullptr;
    std::vector<uint32_t> m_LBs;
    std::vector<uint16_t> m_SBs;
};

template <>
const uint8_t rank_support<0>::LUT[243] = {
    5, 4, 4, 4, 3, 3, 4, 3, 3, 4, 3, 3, 3, 2, 2, 3, 2, 2, 4, 3, 3, 3, 2, 2, 3, 2, 2, 4, 3, 3, 3, 2, 2, 3, 2,
    2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2,
    1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1,
    2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1,
    1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0, 4, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 2,
    1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1,
    1, 0, 0, 1, 0, 0, 3, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0, 1, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0,
};

template <>
const uint8_t rank_support<1>::LUT[243] = {
    0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2,
    1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1,
    2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2,
    1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 3, 4, 3, 4, 5, 4, 3, 4, 3, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3,
    2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2,
    3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 2, 3, 2, 3, 4, 3, 2, 3, 2, 1, 2, 1,
    2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0, 1, 2, 1, 2, 3, 2, 1, 2, 1, 0, 1, 0, 1, 2, 1, 0, 1, 0,
};

template <>
const uint8_t rank_support<2>::LUT[243] = {
    0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1,
    2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2,
    2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2,
    2, 2, 3, 0, 0, 1, 0, 0, 1, 1, 1, 2, 0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1,
    2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1,
    1, 2, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 4, 1, 1, 2, 1, 1, 2, 2, 2, 3, 1, 1, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3,
    2, 2, 3, 3, 3, 4, 2, 2, 3, 2, 2, 3, 3, 3, 4, 2, 2, 3, 2, 2, 3, 3, 3, 4, 3, 3, 4, 3, 3, 4, 4, 4, 5,
};

}  // namespace succinctrits
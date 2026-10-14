#pragma once
#include "types.h"
#include <string>

#include "position.h"

namespace Bitboard {
    extern std::string current_fen;

    inline void set_bit(U64& bb, int sq) { bb |= (1ULL << sq); }
    inline bool get_bit(U64 bb, int sq) { return (bb & (1ULL << sq)) != 0; }
    inline void pop_bit(U64& bb, int sq) { bb &= ~(1ULL << sq); }

    inline int count_bits(U64 bb) {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_popcountll(bb);
#elif defined(_MSC_VER)
        return (int)__popcnt64(bb);
#else
        int count = 0;
        while (bb) {
            count++;
            bb &= bb - 1;
        }
        return count;
#endif
    }

    inline int lsb(U64 bb) {
        if (bb) {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_ctzll(bb);
#elif defined(_MSC_VER)
            unsigned long index;
            _BitScanForward64(&index, bb);
            return index;
#else
            return count_bits((bb & -bb) - 1);
#endif
        }
        return -1;
    }

    void init(BoardState& pos);
    void parse_fen(BoardState& pos, const std::string& fen);
    void print_board(const BoardState& pos);
}

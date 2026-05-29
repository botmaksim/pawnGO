#pragma once
#include "types.h"
#include <string>

#include "position.h"

namespace Bitboard {
    extern std::string current_fen;

    inline void set_bit(U64& bb, int sq) { bb |= (1ULL << sq); }
    inline bool get_bit(U64 bb, int sq) { return (bb & (1ULL << sq)) != 0; }
    inline void pop_bit(U64& bb, int sq) { bb &= ~(1ULL << sq); }

    int count_bits(U64 bb);
    int lsb(U64 bb);

    void init(BoardState& pos);
    void parse_fen(BoardState& pos, const std::string& fen);
    void print_board(const BoardState& pos);
}

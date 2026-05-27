#pragma once
#include "types.h"
#include <string>

namespace Bitboard {
    extern thread_local U64 pieceBB[12];
    extern thread_local U64 occupancies[3];
    extern thread_local int side;
    extern thread_local int enpassant;
    extern thread_local int castle;
    extern thread_local U64 hash_key;
    extern std::string current_fen;

    inline void set_bit(U64& bb, int sq) { bb |= (1ULL << sq); }
    inline bool get_bit(U64 bb, int sq) { return (bb & (1ULL << sq)) != 0; }
    inline void pop_bit(U64& bb, int sq) { bb &= ~(1ULL << sq); }

    int count_bits(U64 bb);
    int lsb(U64 bb);

    void init();
    void parse_fen(const std::string& fen);
    void print_board();
}

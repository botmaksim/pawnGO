#pragma once
#include "bitboard.h"

#include "magics.h"

namespace MoveGen {
    extern U64 pawn_attacks[2][64];
    extern U64 knight_attacks[64];
    extern U64 king_attacks[64];

    extern U64 bishop_masks[64];
    extern U64 rook_masks[64];
    extern U64 bishop_attacks[64][512];
    extern U64 rook_attacks[64][4096];
    extern const int bishop_relevant_bits[64];
    extern const int rook_relevant_bits[64];

    void init_leapers();
    void init_sliders();
    void init_all();

    inline U64 get_bishop_attacks(int square, U64 occupancy) {
        occupancy &= bishop_masks[square];
        occupancy *= bishop_magic_numbers[square];
        occupancy >>= 64 - bishop_relevant_bits[square];
        return bishop_attacks[square][occupancy];
    }

    inline U64 get_rook_attacks(int square, U64 occupancy) {
        occupancy &= rook_masks[square];
        occupancy *= rook_magic_numbers[square];
        occupancy >>= 64 - rook_relevant_bits[square];
        return rook_attacks[square][occupancy];
    }

    inline U64 get_queen_attacks(int square, U64 occupancy) {
        return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
    }
}

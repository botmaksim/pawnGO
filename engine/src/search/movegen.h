#pragma once
#include "bitboard.h"

namespace MoveGen {
    extern U64 pawn_attacks[2][64];
    extern U64 knight_attacks[64];
    extern U64 king_attacks[64];

    extern U64 bishop_masks[64];
    extern U64 rook_masks[64];
    extern U64 bishop_attacks[64][512];
    extern U64 rook_attacks[64][4096];

    void init_leapers();
    void init_sliders();
    void init_all();

    U64 get_bishop_attacks(int square, U64 occupancy);
    U64 get_rook_attacks(int square, U64 occupancy);
    U64 get_queen_attacks(int square, U64 occupancy);
}

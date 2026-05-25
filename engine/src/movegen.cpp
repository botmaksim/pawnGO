#include "movegen.h"
#include "magics.h"

namespace MoveGen {
    U64 pawn_attacks[2][64];
    U64 knight_attacks[64];
    U64 king_attacks[64];

    const U64 not_a_file = 0xFEFEFEFEFEFEFEFEULL;
    const U64 not_h_file = 0x7F7F7F7F7F7F7F7FULL;
    const U64 not_hg_file = 0x3F3F3F3F3F3F3F3FULL;
    const U64 not_ab_file = 0xFCFCFCFCFCFCFCFCULL;

    U64 mask_pawn_attacks(int side, int square) {
        U64 attacks = 0ULL;
        U64 bitboard = 0ULL;
        Bitboard::set_bit(bitboard, square);

        if (!side) { // WHITE
            attacks |= (bitboard << 7) & not_h_file;
            attacks |= (bitboard << 9) & not_a_file;
        } else { // BLACK
            attacks |= (bitboard >> 9) & not_h_file;
            attacks |= (bitboard >> 7) & not_a_file;
        }
        return attacks;
    }

    U64 mask_knight_attacks(int square) {
        U64 attacks = 0ULL;
        U64 bitboard = 0ULL;
        Bitboard::set_bit(bitboard, square);

        attacks |= (bitboard & not_h_file) << 17;
        attacks |= (bitboard & not_a_file) << 15;
        attacks |= (bitboard & not_hg_file) << 10;
        attacks |= (bitboard & not_ab_file) << 6;
        attacks |= (bitboard & not_a_file) >> 17;
        attacks |= (bitboard & not_h_file) >> 15;
        attacks |= (bitboard & not_ab_file) >> 10;
        attacks |= (bitboard & not_hg_file) >> 6;

        return attacks;
    }

    U64 mask_king_attacks(int square) {
        U64 attacks = 0ULL;
        U64 bitboard = 0ULL;
        Bitboard::set_bit(bitboard, square);

        attacks |= bitboard << 8;
        attacks |= (bitboard & not_h_file) << 9;
        attacks |= (bitboard & not_a_file) << 7;
        attacks |= (bitboard & not_h_file) << 1;
        attacks |= bitboard >> 8;
        attacks |= (bitboard & not_a_file) >> 9;
        attacks |= (bitboard & not_h_file) >> 7;
        attacks |= (bitboard & not_a_file) >> 1;

        return attacks;
    }

    void init_leapers() {
        for (int sq = 0; sq < 64; sq++) {
            pawn_attacks[WHITE][sq] = mask_pawn_attacks(WHITE, sq);
            pawn_attacks[BLACK][sq] = mask_pawn_attacks(BLACK, sq);
            knight_attacks[sq] = mask_knight_attacks(sq);
            king_attacks[sq] = mask_king_attacks(sq);
        }
    }

    const int bishop_relevant_bits[64] = {
      6, 5, 5, 5, 5, 5, 5, 6, 
      5, 5, 5, 5, 5, 5, 5, 5, 
      5, 5, 7, 7, 7, 7, 5, 5, 
      5, 5, 7, 9, 9, 7, 5, 5, 
      5, 5, 7, 9, 9, 7, 5, 5, 
      5, 5, 7, 7, 7, 7, 5, 5, 
      5, 5, 5, 5, 5, 5, 5, 5, 
      6, 5, 5, 5, 5, 5, 5, 6
    };

    const int rook_relevant_bits[64] = {
      12, 11, 11, 11, 11, 11, 11, 12, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      11, 10, 10, 10, 10, 10, 10, 11, 
      12, 11, 11, 11, 11, 11, 11, 12
    };

    U64 bishop_masks[64];
    U64 rook_masks[64];
    U64 bishop_attacks[64][512];
    U64 rook_attacks[64][4096];

    U64 mask_bishop_attacks(int square) {
        U64 attacks = 0ULL;
        int tr = square / 8, tf = square % 8;
        for (int r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) Bitboard::set_bit(attacks, r * 8 + f);
        for (int r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) Bitboard::set_bit(attacks, r * 8 + f);
        for (int r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) Bitboard::set_bit(attacks, r * 8 + f);
        for (int r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) Bitboard::set_bit(attacks, r * 8 + f);
        return attacks;
    }

    U64 mask_rook_attacks(int square) {
        U64 attacks = 0ULL;
        int tr = square / 8, tf = square % 8;
        for (int r = tr + 1; r <= 6; r++) Bitboard::set_bit(attacks, r * 8 + tf);
        for (int r = tr - 1; r >= 1; r--) Bitboard::set_bit(attacks, r * 8 + tf);
        for (int f = tf + 1; f <= 6; f++) Bitboard::set_bit(attacks, tr * 8 + f);
        for (int f = tf - 1; f >= 1; f--) Bitboard::set_bit(attacks, tr * 8 + f);
        return attacks;
    }

    U64 bishop_attacks_on_the_fly(int square, U64 block) {
        U64 attacks = 0ULL;
        int r, f;
        int tr = square / 8, tf = square % 8;
        for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) { Bitboard::set_bit(attacks, r * 8 + f); if (Bitboard::get_bit(block, r * 8 + f)) break; }
        for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) { Bitboard::set_bit(attacks, r * 8 + f); if (Bitboard::get_bit(block, r * 8 + f)) break; }
        for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) { Bitboard::set_bit(attacks, r * 8 + f); if (Bitboard::get_bit(block, r * 8 + f)) break; }
        for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) { Bitboard::set_bit(attacks, r * 8 + f); if (Bitboard::get_bit(block, r * 8 + f)) break; }
        return attacks;
    }

    U64 rook_attacks_on_the_fly(int square, U64 block) {
        U64 attacks = 0ULL;
        int r, f;
        int tr = square / 8, tf = square % 8;
        for (r = tr + 1; r <= 7; r++) { Bitboard::set_bit(attacks, r * 8 + tf); if (Bitboard::get_bit(block, r * 8 + tf)) break; }
        for (r = tr - 1; r >= 0; r--) { Bitboard::set_bit(attacks, r * 8 + tf); if (Bitboard::get_bit(block, r * 8 + tf)) break; }
        for (f = tf + 1; f <= 7; f++) { Bitboard::set_bit(attacks, tr * 8 + f); if (Bitboard::get_bit(block, tr * 8 + f)) break; }
        for (f = tf - 1; f >= 0; f--) { Bitboard::set_bit(attacks, tr * 8 + f); if (Bitboard::get_bit(block, tr * 8 + f)) break; }
        return attacks;
    }

    U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
        U64 occupancy = 0ULL;
        for (int count = 0; count < bits_in_mask; count++) {
            int square = Bitboard::lsb(attack_mask);
            Bitboard::pop_bit(attack_mask, square);
            if (index & (1 << count)) occupancy |= (1ULL << square);
        }
        return occupancy;
    }

    void init_sliders() {
        for (int sq = 0; sq < 64; sq++) {
            bishop_masks[sq] = mask_bishop_attacks(sq);
            rook_masks[sq] = mask_rook_attacks(sq);
            
            int b_indices = 1 << bishop_relevant_bits[sq];
            for (int index = 0; index < b_indices; index++) {
                U64 occupancy = set_occupancy(index, bishop_relevant_bits[sq], bishop_masks[sq]);
                int magic_index = (occupancy * bishop_magic_numbers[sq]) >> (64 - bishop_relevant_bits[sq]);
                bishop_attacks[sq][magic_index] = bishop_attacks_on_the_fly(sq, occupancy);
            }
            
            int r_indices = 1 << rook_relevant_bits[sq];
            for (int index = 0; index < r_indices; index++) {
                U64 occupancy = set_occupancy(index, rook_relevant_bits[sq], rook_masks[sq]);
                int magic_index = (occupancy * rook_magic_numbers[sq]) >> (64 - rook_relevant_bits[sq]);
                rook_attacks[sq][magic_index] = rook_attacks_on_the_fly(sq, occupancy);
            }
        }
    }

    void init_all() {
        init_leapers();
        init_sliders();
    }

    U64 get_bishop_attacks(int square, U64 occupancy) {
        occupancy &= bishop_masks[square];
        occupancy *= bishop_magic_numbers[square];
        occupancy >>= 64 - bishop_relevant_bits[square];
        return bishop_attacks[square][occupancy];
    }

    U64 get_rook_attacks(int square, U64 occupancy) {
        occupancy &= rook_masks[square];
        occupancy *= rook_magic_numbers[square];
        occupancy >>= 64 - rook_relevant_bits[square];
        return rook_attacks[square][occupancy];
    }

    U64 get_queen_attacks(int square, U64 occupancy) {
        return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
    }
}

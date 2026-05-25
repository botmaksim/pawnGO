#include "zobrist.h"
#include "bitboard.h"

namespace Zobrist {
    U64 piece_keys[12][64];
    U64 enpassant_keys[64];
    U64 castle_keys[16];
    U64 side_key;

    // Pseudo-random number generator
    unsigned int random_state = 1804289383;
    
    unsigned int get_random_U32_number() {
        unsigned int number = random_state;
        // XOR shift algorithm
        number ^= number << 13;
        number ^= number >> 17;
        number ^= number << 5;
        random_state = number;
        return number;
    }

    U64 get_random_U64_number() {
        U64 n1, n2, n3, n4;
        n1 = (U64)(get_random_U32_number()) & 0xFFFF;
        n2 = (U64)(get_random_U32_number()) & 0xFFFF;
        n3 = (U64)(get_random_U32_number()) & 0xFFFF;
        n4 = (U64)(get_random_U32_number()) & 0xFFFF;
        return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
    }

    void init() {
        // Init piece keys
        for (int piece = P; piece <= k; piece++) {
            for (int sq = 0; sq < 64; sq++) {
                piece_keys[piece][sq] = get_random_U64_number();
            }
        }
        
        // Init enpassant keys
        for (int sq = 0; sq < 64; sq++) {
            enpassant_keys[sq] = get_random_U64_number();
        }

        // Init castling keys
        for (int i = 0; i < 16; i++) {
            castle_keys[i] = get_random_U64_number();
        }

        // Init side key
        side_key = get_random_U64_number();
    }

    U64 generate_hash_key() {
        U64 final_key = 0ULL;
        U64 bitboard;

        // Hash pieces
        for (int piece = P; piece <= k; piece++) {
            bitboard = Bitboard::pieceBB[piece];
            while (bitboard) {
                int sq = Bitboard::lsb(bitboard);
                final_key ^= piece_keys[piece][sq];
                Bitboard::pop_bit(bitboard, sq);
            }
        }

        // Hash enpassant
        if (Bitboard::enpassant != no_sq) {
            final_key ^= enpassant_keys[Bitboard::enpassant];
        }

        // Hash castling
        final_key ^= castle_keys[Bitboard::castle];

        // Hash side
        if (Bitboard::side == BLACK) {
            final_key ^= side_key;
        }

        return final_key;
    }
}

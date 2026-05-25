#pragma once
#include "types.h"
#include "bitboard.h"
#include "move_generator.h"
#include "nnue/src/nnue.h"

namespace Board {
    // Macro to copy board state
    #define COPY_BOARD \
        U64 pieceBB_copy[12]; \
        U64 occupancies_copy[3]; \
        for (int i=0; i<12; i++) pieceBB_copy[i] = Bitboard::pieceBB[i]; \
        for (int i=0; i<3; i++) occupancies_copy[i] = Bitboard::occupancies[i]; \
        int side_copy = Bitboard::side; \
        int enpassant_copy = Bitboard::enpassant; \
        int castle_copy = Bitboard::castle; \
        U64 hash_key_copy = Bitboard::hash_key;

    // Macro to restore board state
    #define RESTORE_BOARD \
        for (int i=0; i<12; i++) Bitboard::pieceBB[i] = pieceBB_copy[i]; \
        for (int i=0; i<3; i++) Bitboard::occupancies[i] = occupancies_copy[i]; \
        Bitboard::side = side_copy; \
        Bitboard::enpassant = enpassant_copy; \
        Bitboard::castle = castle_copy; \
        Bitboard::hash_key = hash_key_copy;

    int make_move(Move move, int move_flag, DirtyPiece* dp = nullptr);
    
    extern long long nodes;
    void perft_driver(int depth);
    void perft_test(int depth);
}

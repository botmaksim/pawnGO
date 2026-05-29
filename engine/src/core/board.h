#pragma once
#include "types.h"
#include "bitboard.h"
#include "move_generator.h"
#include "nnue/src/nnue.h"

namespace Board {
    struct UndoInfo {
        int captured_piece;
        int enpassant;
        int castle;
        U64 hash_key;
    };

    int make_move(BoardState& pos, Move move, int move_flag, UndoInfo* undo = nullptr, DirtyPiece* dp = nullptr);
    void unmake_move(BoardState& pos, Move move, const UndoInfo& undo, DirtyPiece* dp = nullptr);
    
    extern long long nodes;
    void perft_driver(BoardState& pos, int depth);
    void perft_test(BoardState& pos, int depth);
}

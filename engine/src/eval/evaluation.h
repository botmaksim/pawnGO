#ifndef EVALUATION_H
#define EVALUATION_H

#include "types.h"
#include "board.h"
#include "nnue/src/nnue.h"

namespace Evaluation {
    void init_nnue(const char* file_path);
    int evaluate(const BoardState& pos);

    extern const int nnue_piece_map[16];
    extern bool use_nnue;
    
    const int MAX_PLY = 512;
    extern thread_local NNUEdata* nnue_stack;
    void allocate_nnue_stack();
    int evaluate_incremental(const BoardState& pos, int ply);

    extern const int pawn_score[64];
    extern const int knight_score[64];
    extern const int bishop_score[64];
    extern const int rook_score[64];
    extern const int king_score[64];
    
    // Piece weights: P, N, B, R, Q, K
    // White: 0-5, Black: 6-11
    extern const int piece_weights[12];

}

#endif // EVALUATION_H

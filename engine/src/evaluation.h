#ifndef EVALUATION_H
#define EVALUATION_H

#include "types.h"
#include "board.h"

namespace Evaluation {
    void init_nnue(const char* file_path);
    int evaluate();

    extern const int pawn_score[64];
    extern const int knight_score[64];
    extern const int bishop_score[64];
    extern const int rook_score[64];
    extern const int king_score[64];
    
    // Piece weights: P, N, B, R, Q, K
    // White: 0-5, Black: 6-11
    extern const int piece_weights[12];

    int evaluate();
    std::string shadow_evaluate();
}

#endif // EVALUATION_H

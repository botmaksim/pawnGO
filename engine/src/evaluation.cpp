#include <fstream>
#include "evaluation.h"
#include "bitboard.h"
#include "nnue.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>

namespace Evaluation {
    // NNUE Piece Map (from pawnGO to nnue-probe format)
    const int nnue_piece_map[12] = {
        6, 5, 4, 3, 2, 1,   // P N B R Q K -> wpawn...wking
        12, 11, 10, 9, 8, 7 // p n b r q k -> bpawn...bking
    };

    bool use_nnue = false;

    void init_nnue(const char* file_path) {
        std::ifstream f(file_path);
        if (f.good()) {
            nnue_init(file_path);
            use_nnue = true;
        } else {
            std::cout << "Engine OUT: Failed to load NNUE from " << file_path << ", falling back to PeSTO evaluation." << std::endl;
            use_nnue = false;
        }
    }
    // PeSTO's Evaluation Weights
    const int mg_value[6] = { 82, 337, 365, 477, 1025, 0 };
    const int eg_value[6] = { 94, 281, 297, 512,  936, 0 };

    const int mg_pawn_pst[64] = {
          0,   0,   0,   0,   0,   0,  0,   0,
         98, 134,  61,  95,  68, 126, 34, -11,
         -6,   7,  26,  31,  65,  56, 25, -20,
        -14,  13,   6,  21,  23,  12, 17, -23,
        -27,  -2,  -5,  12,  17,   6, 10, -25,
        -26,  -4,  -4, -10,   3,   3, 33, -12,
        -35,  -1, -20, -23, -15,  24, 38, -22,
          0,   0,   0,   0,   0,   0,  0,   0
    };

    const int eg_pawn_pst[64] = {
          0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
         94, 100,  85,  67,  56,  53,  82,  84,
         32,  24,  13,   5,  -2,   4,  17,  17,
         13,   9,  -3,  -7,  -7,  -8,   3,  -1,
          4,   7,  -6,   1,   0,  -5,  -1,  -8,
         13,   8,   8,  10,  13,   0,   2,  -7,
          0,   0,   0,   0,   0,   0,   0,   0
    };

    const int mg_knight_pst[64] = {
        -167, -89, -34, -49,  61, -97, -15, -107,
         -73, -41,  72,  36,  23,  62,   7,  -17,
         -47,  60,  37,  65,  84, 129,  73,   44,
          -9,  17,  19,  53,  37,  69,  18,   22,
         -13,   4,  16,  13,  28,  19,  21,   -8,
         -23,  -9,  12,  10,  19,  17,  25,  -16,
         -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23
    };

    const int eg_knight_pst[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -38, -22, -27, -43, -74
    };

    const int mg_bishop_pst[64] = {
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21
    };

    const int eg_bishop_pst[64] = {
        -14, -21, -11,  -8, -7,  -9, -17, -24,
         -8,  -4,   7, -12, -3, -13,  -4, -14,
          2,  -8,   0,  -1, -2,   6,   0,   4,
         -3,   9,  12,   9, 14,  10,   3,   2,
         -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   8,  10, 13,   3,  -7, -15,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16,  -5, -17
    };

    const int mg_rook_pst[64] = {
         32,  42,  32,  51, 63,  9,  31,  43,
         27,  32,  58,  62, 80, 67,  26,  44,
         -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  17, 16,  7, -37, -26
    };

    const int eg_rook_pst[64] = {
         13, 10, 18, 15, 12,  12,   8,   5,
         11, 13, 13, 11, -3,   3,   8,   3,
          7,  7,  7,  5,  4,  -3,  -5,  -3,
          4,  3, 13,  1,  2,   1,  -1,   2,
          3,  5,  8,  4, -5,  -6,  -8, -11,
         -4,  0, -5, -1, -7, -12,  -8, -16,
         -6, -6,  0,  2, -9,  -9, -11,  -3,
         -9,  2,  3, -1, -5, -13,   4, -20
    };

    const int mg_queen_pst[64] = {
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50
    };

    const int eg_queen_pst[64] = {
         -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  12,  11,
         -16, -27,  15,   6,   9,  17,  10,   5,
         -22, -23, -30, -16, -16, -23, -36, -32,
         -33, -28, -22, -43,  -5, -32, -20, -41
    };

    const int mg_king_pst[64] = {
        -65,  23,  16, -15, -56, -34,   2,  13,
         29,  -1, -20,  -7,  -8,  -4, -38, -29,
         -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
          1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14
    };

    const int eg_king_pst[64] = {
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
         10,  17,  23,  15,  20,  45,  44,  13,
         -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
    };

    int get_game_phase() {
        int phase = 24;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[N]) * 1;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[n]) * 1;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[B]) * 1;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[b]) * 1;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[R]) * 2;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[r]) * 2;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[Q]) * 4;
        phase -= Bitboard::count_bits(Bitboard::pieceBB[q]) * 4;
        return std::max(0, phase);
    }

    int evaluate() {
        int mg_score[2] = {0, 0};
        int eg_score[2] = {0, 0};

        int white_mat = 0;
        int black_mat = 0;

        for (int pc = P; pc <= Q; pc++) {
            white_mat += Bitboard::count_bits(Bitboard::pieceBB[pc]) * mg_value[pc - P];
        }
        for (int pc = p; pc <= q; pc++) {
            black_mat += Bitboard::count_bits(Bitboard::pieceBB[pc]) * mg_value[pc - p];
        }

        // Lazy Evaluation: If material imbalance is huge, skip complex eval
        int mat_diff = white_mat - black_mat;
        if (std::abs(mat_diff) >= 1500) {
            return (Bitboard::side == WHITE) ? mat_diff : -mat_diff;
        }

        if (use_nnue) {
            int pieces[33];
            int squares[33];
            int index = 2; // 0 and 1 are reserved for kings

            // Kings must be at index 0 and 1
            pieces[0] = 1; // wking
            squares[0] = Bitboard::lsb(Bitboard::pieceBB[K]);
            pieces[1] = 7; // bking
            squares[1] = Bitboard::lsb(Bitboard::pieceBB[k]);

            for (int p = P; p <= k; p++) {
                if (p == K || p == k) continue;
                
                U64 bb = Bitboard::pieceBB[p];
                while (bb) {
                    int sq = Bitboard::lsb(bb);
                    pieces[index] = nnue_piece_map[p];
                    squares[index] = sq;
                    index++;
                    Bitboard::pop_bit(bb, sq);
                }
            }
            pieces[index] = 0; // End of array marker

            int nnue_score = nnue_evaluate(Bitboard::side, pieces, squares);
            return nnue_score;
        }

        // Arrays of pointers to PSTs for easy iteration
        const int* mg_psts[6] = {mg_pawn_pst, mg_knight_pst, mg_bishop_pst, mg_rook_pst, mg_queen_pst, mg_king_pst};
        const int* eg_psts[6] = {eg_pawn_pst, eg_knight_pst, eg_bishop_pst, eg_rook_pst, eg_queen_pst, eg_king_pst};

        // Evaluate White
        for (int pc = P; pc <= K; pc++) {
            U64 bb = Bitboard::pieceBB[pc];
            int pType = pc - P;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                mg_score[WHITE] += mg_value[pType] + mg_psts[pType][sq];
                eg_score[WHITE] += eg_value[pType] + eg_psts[pType][sq];
                Bitboard::pop_bit(bb, sq);
            }
        }

        // Evaluate Black
        for (int pc = p; pc <= k; pc++) {
            U64 bb = Bitboard::pieceBB[pc];
            int pType = pc - p;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                int mirrored_sq = sq ^ 56; // Mirror for black
                mg_score[BLACK] += mg_value[pType] + mg_psts[pType][mirrored_sq];
                eg_score[BLACK] += eg_value[pType] + eg_psts[pType][mirrored_sq];
                Bitboard::pop_bit(bb, sq);
            }
        }

        int phase = get_game_phase();
        
        int mg_total = mg_score[WHITE] - mg_score[BLACK];
        int eg_total = eg_score[WHITE] - eg_score[BLACK];

        // phase is 24 at start (100% MG), 0 at endgame (100% EG)
        int score = (mg_total * phase + eg_total * (24 - phase)) / 24;

        return (Bitboard::side == WHITE) ? score : -score;
    }

    std::string shadow_evaluate() {
        int white_mat = 0, black_mat = 0;
        int white_act = 0, black_act = 0;
        int white_king = 0, black_king = 0;
        int white_center = 0, black_center = 0;

        const int* mg_psts[6] = {mg_pawn_pst, mg_knight_pst, mg_bishop_pst, mg_rook_pst, mg_queen_pst, mg_king_pst};

        // Center squares: d4(27), e4(28), d5(35), e5(36)
        U64 center_mask = (1ULL << 27) | (1ULL << 28) | (1ULL << 35) | (1ULL << 36);

        // Evaluate White
        for (int pc = P; pc <= K; pc++) {
            U64 bb = Bitboard::pieceBB[pc];
            int pType = pc - P;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                white_mat += mg_value[pType];
                if (pType == 5) {
                    white_king += mg_psts[pType][sq];
                } else {
                    white_act += mg_psts[pType][sq];
                }
                if ((1ULL << sq) & center_mask) {
                    if (pType == 0 || pType == 1) white_center += 50; // Pawn/Knight in center
                }
                Bitboard::pop_bit(bb, sq);
            }
        }

        // Evaluate Black
        for (int pc = p; pc <= k; pc++) {
            U64 bb = Bitboard::pieceBB[pc];
            int pType = pc - p;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                int mirrored_sq = sq ^ 56;
                black_mat += mg_value[pType];
                if (pType == 5) {
                    black_king += mg_psts[pType][mirrored_sq];
                } else {
                    black_act += mg_psts[pType][mirrored_sq];
                }
                if ((1ULL << sq) & center_mask) {
                    if (pType == 0 || pType == 1) black_center += 50;
                }
                Bitboard::pop_bit(bb, sq);
            }
        }

        double mat_delta = (white_mat - black_mat) / 100.0;
        double act_delta = (white_act - black_act) / 100.0;
        double king_delta = (white_king - black_king) / 100.0;
        double center_delta = (white_center - black_center) / 100.0;

        // Multiply by -1 if it's black's turn so positive means good for side to move?
        // Actually it's better to keep it absolute (positive = white advantage).
        
        std::stringstream ss;
        ss << "{";
        ss << "\"material\": " << mat_delta << ", ";
        ss << "\"activity\": " << act_delta << ", ";
        ss << "\"king_safety\": " << king_delta << ", ";
        ss << "\"center_control\": " << center_delta;
        ss << "}";
        return ss.str();
    }
}

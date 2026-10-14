#include <fstream>
#include "evaluation.h"
#include "bitboard.h"
#include "nnue.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h>

namespace Evaluation {
    // NNUE Piece Map (from pawnGO to nnue-probe format)
    const int nnue_piece_map[16] = {
        6, 5, 4, 3, 2, 1,   // P N B R Q K -> wpawn...wking
        12, 11, 10, 9, 8, 7, // p n b r q k -> bpawn...bking
        0, 0, 0, 0
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

    int get_game_phase(const BoardState& pos) {
        int phase = 0;
        phase += Bitboard::count_bits(pos.pieceBB[N] | pos.pieceBB[n]) * 1;
        phase += Bitboard::count_bits(pos.pieceBB[B] | pos.pieceBB[b]) * 1;
        phase += Bitboard::count_bits(pos.pieceBB[R] | pos.pieceBB[r]) * 2;
        phase += Bitboard::count_bits(pos.pieceBB[Q] | pos.pieceBB[q]) * 4;
        return std::min(24, phase);
    }

    U64 passed_pawn_masks[2][64];
    bool tables_initialized = false;

    void init_evaluation_tables() {
        if (tables_initialized) return;
        for (int sq = 0; sq < 64; sq++) {
            int file = sq % 8;
            int rank = sq / 8;

            U64 white_mask = 0ULL;
            U64 black_mask = 0ULL;

            for (int r = rank + 1; r < 8; r++) {
                if (file > 0) white_mask |= (1ULL << (r * 8 + file - 1));
                white_mask |= (1ULL << (r * 8 + file));
                if (file < 7) white_mask |= (1ULL << (r * 8 + file + 1));
            }

            for (int r = rank - 1; r >= 0; r--) {
                if (file > 0) black_mask |= (1ULL << (r * 8 + file - 1));
                black_mask |= (1ULL << (r * 8 + file));
                if (file < 7) black_mask |= (1ULL << (r * 8 + file + 1));
            }

            passed_pawn_masks[WHITE][sq] = white_mask;
            passed_pawn_masks[BLACK][sq] = black_mask;
        }
        tables_initialized = true;
    }

    inline int chebyshev_distance(int sq1, int sq2) {
        int r1 = sq1 / 8, f1 = sq1 % 8;
        int r2 = sq2 / 8, f2 = sq2 % 8;
        return std::max(std::abs(r1 - r2), std::abs(f1 - f2));
    }

    int adjust_eval(int score, const BoardState& pos) {
        init_evaluation_tables();

        int side = pos.side;

        // 50-move rule progress scaling to avoid shuffling into draws
        if (pos.rule50 >= 40 && std::abs(score) < 25000) {
            score = score * (100 - pos.rule50) / 60;
        }

        // Material-based draw recognition
        int w_pawns = Bitboard::count_bits(pos.pieceBB[P]);
        int b_pawns = Bitboard::count_bits(pos.pieceBB[p]);
        int w_queens = Bitboard::count_bits(pos.pieceBB[Q]);
        int b_queens = Bitboard::count_bits(pos.pieceBB[q]);
        int w_rooks = Bitboard::count_bits(pos.pieceBB[R]);
        int b_rooks = Bitboard::count_bits(pos.pieceBB[r]);
        int w_bishops = Bitboard::count_bits(pos.pieceBB[B]);
        int b_bishops = Bitboard::count_bits(pos.pieceBB[b]);
        int w_knights = Bitboard::count_bits(pos.pieceBB[N]);
        int b_knights = Bitboard::count_bits(pos.pieceBB[n]);

        int w_minors = w_bishops + w_knights;
        int b_minors = b_bishops + b_knights;
        int w_majors = w_queens + w_rooks;
        int b_majors = b_queens + b_rooks;

        if (w_pawns == 0 && b_pawns == 0) {
            if (w_majors == 0 && b_majors == 0) {
                if (w_minors <= 1 && b_minors <= 1) return 0;
                if ((w_minors == 2 && w_knights == 2 && b_minors == 0) ||
                    (b_minors == 2 && b_knights == 2 && w_minors == 0)) {
                    return 0;
                }
            }
            if (w_majors == 1 && b_majors == 1 && w_rooks == 1 && b_rooks == 1 && w_minors == 0 && b_minors == 0) {
                score /= 4;
            }
        }

        // Pure opposite-colored bishops fortress scaling (no rooks, queens, or knights)
        if (w_queens == 0 && b_queens == 0 && w_rooks == 0 && b_rooks == 0 && w_knights == 0 && b_knights == 0 && w_bishops == 1 && b_bishops == 1) {
            int w_bsq = Bitboard::lsb(pos.pieceBB[B]);
            int b_bsq = Bitboard::lsb(pos.pieceBB[b]);
            bool w_light = ((w_bsq / 8 + w_bsq % 8) % 2 == 0);
            bool b_light = ((b_bsq / 8 + b_bsq % 8) % 2 == 0);
            if (w_light != b_light) {
                int pawn_diff = std::abs(w_pawns - b_pawns);
                if (pawn_diff <= 1) {
                    score /= 3;
                } else if (pawn_diff == 2) {
                    score /= 2;
                }
            }
        }

        // Positional endgame evaluation (passed pawns and rook placement)
        int positional_bonus = 0;
        int w_king_sq = Bitboard::lsb(pos.pieceBB[K]);
        int b_king_sq = Bitboard::lsb(pos.pieceBB[k]);

        // Passed pawns: advancement, Tarrasch rule, and king escort
        U64 wp = pos.pieceBB[P];
        while (wp) {
            int sq = Bitboard::lsb(wp);
            if (!(pos.pieceBB[p] & passed_pawn_masks[WHITE][sq])) {
                int rank = sq / 8;
                int file = sq % 8;
                int rank_bonus = 0;
                if (rank == 4) rank_bonus = 15;
                else if (rank == 5) rank_bonus = 35;
                else if (rank == 6) rank_bonus = 75;

                // Tarrasch rule: rook behind passed pawn
                U64 file_mask = 0x0101010101010101ULL << file;
                U64 behind_mask = file_mask & ((1ULL << sq) - 1);
                if (pos.pieceBB[R] & behind_mask) rank_bonus += 25;
                if (pos.pieceBB[r] & behind_mask) rank_bonus -= 20;

                // King escort in endgame
                if (w_majors + b_majors <= 2 && w_king_sq != -1 && b_king_sq != -1) {
                    int w_dist = chebyshev_distance(w_king_sq, sq);
                    int b_dist = chebyshev_distance(b_king_sq, sq);
                    rank_bonus += (b_dist - w_dist) * 6;
                }
                positional_bonus += rank_bonus;
            }
            Bitboard::pop_bit(wp, sq);
        }

        U64 bp = pos.pieceBB[p];
        while (bp) {
            int sq = Bitboard::lsb(bp);
            if (!(pos.pieceBB[P] & passed_pawn_masks[BLACK][sq])) {
                int rank = 7 - (sq / 8);
                int file = sq % 8;
                int rank_bonus = 0;
                if (rank == 4) rank_bonus = 15;
                else if (rank == 5) rank_bonus = 35;
                else if (rank == 6) rank_bonus = 75;

                // Tarrasch rule: rook behind passed pawn
                U64 file_mask = 0x0101010101010101ULL << file;
                U64 behind_mask = file_mask & ~((1ULL << (sq + 1)) - 1);
                if (pos.pieceBB[r] & behind_mask) rank_bonus += 25;
                if (pos.pieceBB[R] & behind_mask) rank_bonus -= 20;

                // King escort in endgame
                if (w_majors + b_majors <= 2 && w_king_sq != -1 && b_king_sq != -1) {
                    int w_dist = chebyshev_distance(w_king_sq, sq);
                    int b_dist = chebyshev_distance(b_king_sq, sq);
                    rank_bonus += (w_dist - b_dist) * 6;
                }
                positional_bonus -= rank_bonus;
            }
            Bitboard::pop_bit(bp, sq);
        }

        // Rooks on open and semi-open files
        U64 wr = pos.pieceBB[R];
        while (wr) {
            int sq = Bitboard::lsb(wr);
            int file = sq % 8;
            U64 f_mask = 0x0101010101010101ULL << file;
            if (!(pos.pieceBB[P] & f_mask)) {
                if (!(pos.pieceBB[p] & f_mask)) positional_bonus += 20;
                else positional_bonus += 10;
            }
            Bitboard::pop_bit(wr, sq);
        }

        U64 br = pos.pieceBB[r];
        while (br) {
            int sq = Bitboard::lsb(br);
            int file = sq % 8;
            U64 f_mask = 0x0101010101010101ULL << file;
            if (!(pos.pieceBB[p] & f_mask)) {
                if (!(pos.pieceBB[P] & f_mask)) positional_bonus -= 20;
                else positional_bonus -= 10;
            }
            Bitboard::pop_bit(br, sq);
        }

        // Rook on 7th rank (attacking pawns or cutting off king)
        U64 wr7 = pos.pieceBB[R] & 0x00FF000000000000ULL; // rank 7
        if (wr7 && (b_king_sq >= 48 || (pos.pieceBB[p] & 0x00FF000000000000ULL))) {
            positional_bonus += 30 * Bitboard::count_bits(wr7);
        }
        U64 br2 = pos.pieceBB[r] & 0x000000000000FF00ULL; // rank 2
        if (br2 && (w_king_sq <= 15 || (pos.pieceBB[P] & 0x000000000000FF00ULL))) {
            positional_bonus -= 30 * Bitboard::count_bits(br2);
        }

        int perspective_bonus = (side == WHITE) ? positional_bonus : -positional_bonus;
        return score + perspective_bonus;
    }

    thread_local NNUEdata* nnue_stack = nullptr;

    struct NNUEStackManager {
        ~NNUEStackManager() {
            if (nnue_stack) {
                free(nnue_stack);
                nnue_stack = nullptr;
            }
        }
    };
    thread_local NNUEStackManager nnue_stack_manager;

    void allocate_nnue_stack() {
        if (!nnue_stack) {
            if (posix_memalign((void**)&nnue_stack, 64, sizeof(NNUEdata) * MAX_PLY) != 0) {
                std::cerr << "Failed to allocate 64-byte aligned memory for NNUE stack" << std::endl;
                exit(1);
            }
            for (int i = 0; i < MAX_PLY; i++) {
                nnue_stack[i].dirtyPiece.dirtyNum = 0;
                nnue_stack[i].accumulator.computedAccumulation = 0;
            }
        }
    }

    int evaluate_incremental(const BoardState& pos, int ply) {
        if (!use_nnue || ply >= MAX_PLY) return evaluate(pos);

        int pieces[65];
        int squares[65];
        int index = 2;

        pieces[0] = 1;
        squares[0] = Bitboard::lsb(pos.pieceBB[K]);
        pieces[1] = 7;
        squares[1] = Bitboard::lsb(pos.pieceBB[k]);

        if (squares[0] == -1 || squares[1] == -1) return evaluate(pos);

        for (int p = P; p <= k; p++) {
            if (p == K || p == k) continue;
            
            U64 bb = pos.pieceBB[p];
            while (bb && index < 64) {
                int sq = Bitboard::lsb(bb);
                pieces[index] = nnue_piece_map[p];
                squares[index] = sq;
                index++;
                Bitboard::pop_bit(bb, sq);
            }
        }
        pieces[index] = 0;

        NNUEdata* nnue_data[3];
        nnue_data[0] = &nnue_stack[ply];
        nnue_data[1] = (ply >= 1) ? &nnue_stack[ply - 1] : nullptr;
        nnue_data[2] = (ply >= 2) ? &nnue_stack[ply - 2] : nullptr;

        int raw_score = nnue_evaluate_incremental(pos.side, pieces, squares, nnue_data);
        return adjust_eval(raw_score, pos);
    }

    int evaluate(const BoardState& pos) {
        int mg_score[2] = {0, 0};
        int eg_score[2] = {0, 0};

        int white_mat = 0;
        int black_mat = 0;

        for (int pc = P; pc <= Q; pc++) {
            white_mat += Bitboard::count_bits(pos.pieceBB[pc]) * mg_value[pc - P];
        }
        for (int pc = p; pc <= q; pc++) {
            black_mat += Bitboard::count_bits(pos.pieceBB[pc]) * mg_value[pc - p];
        }

        // Lazy evaluation: skip expensive calculation on decisive material imbalance
        int mat_diff = white_mat - black_mat;
        if (std::abs(mat_diff) >= 1500) {
            return (pos.side == WHITE) ? mat_diff : -mat_diff;
        }

        if (use_nnue) {
            int pieces[65];
            int squares[65];
            int index = 2;

            pieces[0] = 1;
            squares[0] = Bitboard::lsb(pos.pieceBB[K]);
            pieces[1] = 7;
            squares[1] = Bitboard::lsb(pos.pieceBB[k]);

            if (squares[0] == -1 || squares[1] == -1) return (pos.side == WHITE) ? mat_diff : -mat_diff;

            for (int p = P; p <= k; p++) {
                if (p == K || p == k) continue;
                
                U64 bb = pos.pieceBB[p];
                while (bb && index < 64) {
                    int sq = Bitboard::lsb(bb);
                    pieces[index] = nnue_piece_map[p];
                    squares[index] = sq;
                    index++;
                    Bitboard::pop_bit(bb, sq);
                }
            }
            pieces[index] = 0;

            int nnue_score = nnue_evaluate(pos.side, pieces, squares);
            return adjust_eval(nnue_score, pos);
        }

        const int* mg_psts[6] = {mg_pawn_pst, mg_knight_pst, mg_bishop_pst, mg_rook_pst, mg_queen_pst, mg_king_pst};
        const int* eg_psts[6] = {eg_pawn_pst, eg_knight_pst, eg_bishop_pst, eg_rook_pst, eg_queen_pst, eg_king_pst};

        for (int pc = P; pc <= K; pc++) {
            U64 bb = pos.pieceBB[pc];
            int pType = pc - P;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                int mirrored_sq = sq ^ 56;
                mg_score[WHITE] += mg_value[pType] + mg_psts[pType][mirrored_sq];
                eg_score[WHITE] += eg_value[pType] + eg_psts[pType][mirrored_sq];
                Bitboard::pop_bit(bb, sq);
            }
        }

        for (int pc = p; pc <= k; pc++) {
            U64 bb = pos.pieceBB[pc];
            int pType = pc - p;
            while (bb) {
                int sq = Bitboard::lsb(bb);
                mg_score[BLACK] += mg_value[pType] + mg_psts[pType][sq];
                eg_score[BLACK] += eg_value[pType] + eg_psts[pType][sq];
                Bitboard::pop_bit(bb, sq);
            }
        }

        int phase = get_game_phase(pos);
        
        int mg_total = mg_score[WHITE] - mg_score[BLACK];
        int eg_total = eg_score[WHITE] - eg_score[BLACK];

        // Tapered evaluation linearly interpolated by game phase (24=MG, 0=EG)
        int score = (mg_total * phase + eg_total * (24 - phase)) / 24;

        return adjust_eval((pos.side == WHITE) ? score : -score, pos);
    }

} // namespace Evaluation

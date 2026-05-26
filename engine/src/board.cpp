#include "board.h"
#include "movegen.h"
#include "zobrist.h"
#include "nnue/src/nnue.h"
#include "evaluation.h"
#include <iostream>
#include <chrono>

namespace Board {
    
    long long nodes = 0;

    const int castling_rights[64] = {
         13, 15, 15, 15, 12, 15, 15, 14,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
         7, 15, 15, 15,  3, 15, 15, 11
    };

    int make_move(Move move, int move_flag, DirtyPiece* dp) {
        if (move_flag == 1) { // captures only
            if (!GET_MOVE_CAPTURE(move)) return 0;
        }

        int source = GET_MOVE_SOURCE(move);
        int target = GET_MOVE_TARGET(move);
        int piece = GET_MOVE_PIECE(move);
        int promoted = GET_MOVE_PROMOTED(move);
        int capture = GET_MOVE_CAPTURE(move);
        int double_push = GET_MOVE_DOUBLE_PUSH(move);
        int enpassant = GET_MOVE_ENPASSANT(move);
        int castling = GET_MOVE_CASTLING(move);

        // 1. Remove old enpassant and castle keys from hash
        if (Bitboard::enpassant != no_sq) Bitboard::hash_key ^= Zobrist::enpassant_keys[Bitboard::enpassant];
        Bitboard::hash_key ^= Zobrist::castle_keys[Bitboard::castle];

        if (dp) {
            dp->dirtyNum = 0;
            dp->pc[0] = Evaluation::nnue_piece_map[piece];
            dp->from[0] = source;
            dp->to[0] = target;
            dp->dirtyNum++;
        }

        Bitboard::pop_bit(Bitboard::pieceBB[piece], source);
        Bitboard::set_bit(Bitboard::pieceBB[piece], target);
        
        // Hash piece move
        Bitboard::hash_key ^= Zobrist::piece_keys[piece][source];
        Bitboard::hash_key ^= Zobrist::piece_keys[piece][target];

        if (capture) {
            int start_piece, end_piece;
            if (Bitboard::side == WHITE) { start_piece = p; end_piece = k; }
            else { start_piece = P; end_piece = K; }

            int captured_piece = -1;
            for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
                if (Bitboard::get_bit(Bitboard::pieceBB[bb_piece], target)) {
                    captured_piece = bb_piece;
                    Bitboard::pop_bit(Bitboard::pieceBB[bb_piece], target);
                    break;
                }
            }
            if (captured_piece != -1) {
                Bitboard::hash_key ^= Zobrist::piece_keys[captured_piece][target];
                if (dp) {
                    dp->pc[dp->dirtyNum] = Evaluation::nnue_piece_map[captured_piece];
                    dp->from[dp->dirtyNum] = target;
                    dp->to[dp->dirtyNum] = 64; // removed
                    dp->dirtyNum++;
                }
            }
        }

        if (promoted) {
            Bitboard::pop_bit(Bitboard::pieceBB[piece], target);
            Bitboard::set_bit(Bitboard::pieceBB[promoted], target);
            
            Bitboard::hash_key ^= Zobrist::piece_keys[piece][target];
            Bitboard::hash_key ^= Zobrist::piece_keys[promoted][target];
            
            if (dp) {
                dp->to[0] = 64; // Pawn disappears
                dp->pc[dp->dirtyNum] = Evaluation::nnue_piece_map[promoted];
                dp->from[dp->dirtyNum] = 64; // Promoted piece appears
                dp->to[dp->dirtyNum] = target;
                dp->dirtyNum++;
            }
        }

        if (enpassant) {
            int ep_pawn_sq = (Bitboard::side == WHITE) ? (target - 8) : (target + 8);
            int ep_pawn = (Bitboard::side == WHITE) ? p : P;
            Bitboard::pop_bit(Bitboard::pieceBB[ep_pawn], ep_pawn_sq);
            
            Bitboard::hash_key ^= Zobrist::piece_keys[ep_pawn][ep_pawn_sq];
            
            if (dp) {
                dp->pc[dp->dirtyNum] = Evaluation::nnue_piece_map[ep_pawn];
                dp->from[dp->dirtyNum] = ep_pawn_sq;
                dp->to[dp->dirtyNum] = 64; // removed
                dp->dirtyNum++;
            }
        }

        Bitboard::enpassant = no_sq;

        if (double_push) {
            (Bitboard::side == WHITE) ? (Bitboard::enpassant = target - 8) : (Bitboard::enpassant = target + 8);
        }

        if (castling) {
            int rook_sq = -1, new_rook_sq = -1, r_piece = (Bitboard::side == WHITE) ? R : r;
            switch (target) {
                case g1: rook_sq = h1; new_rook_sq = f1; break;
                case c1: rook_sq = a1; new_rook_sq = d1; break;
                case g8: rook_sq = h8; new_rook_sq = f8; break;
                case c8: rook_sq = a8; new_rook_sq = d8; break;
            }
            if (rook_sq != -1) {
                Bitboard::pop_bit(Bitboard::pieceBB[r_piece], rook_sq); 
                Bitboard::set_bit(Bitboard::pieceBB[r_piece], new_rook_sq);
                
                Bitboard::hash_key ^= Zobrist::piece_keys[r_piece][rook_sq];
                Bitboard::hash_key ^= Zobrist::piece_keys[r_piece][new_rook_sq];
                
                if (dp) {
                    dp->pc[dp->dirtyNum] = Evaluation::nnue_piece_map[r_piece];
                    dp->from[dp->dirtyNum] = rook_sq;
                    dp->to[dp->dirtyNum] = new_rook_sq;
                    dp->dirtyNum++;
                }
            }
        }

        Bitboard::castle &= castling_rights[source];
        Bitboard::castle &= castling_rights[target];

        // 2. Add new enpassant, castle, and side keys to hash
        if (Bitboard::enpassant != no_sq) Bitboard::hash_key ^= Zobrist::enpassant_keys[Bitboard::enpassant];
        Bitboard::hash_key ^= Zobrist::castle_keys[Bitboard::castle];
        Bitboard::hash_key ^= Zobrist::side_key;

        Bitboard::occupancies[WHITE] = 0ULL;
        Bitboard::occupancies[BLACK] = 0ULL;
        Bitboard::occupancies[BOTH] = 0ULL;

        for (int piece_idx = P; piece_idx <= K; piece_idx++) Bitboard::occupancies[WHITE] |= Bitboard::pieceBB[piece_idx];
        for (int piece_idx = p; piece_idx <= k; piece_idx++) Bitboard::occupancies[BLACK] |= Bitboard::pieceBB[piece_idx];
        Bitboard::occupancies[BOTH] |= Bitboard::occupancies[WHITE];
        Bitboard::occupancies[BOTH] |= Bitboard::occupancies[BLACK];

        Bitboard::side ^= 1; // Change side

        // Check if king is in check after our move
        // Since we already changed side, if side is BLACK, White just moved, so check White king (K)
        int king_sq = (Bitboard::side == BLACK) ? Bitboard::lsb(Bitboard::pieceBB[K]) : Bitboard::lsb(Bitboard::pieceBB[k]);

        if (MoveGen::is_square_attacked(king_sq, Bitboard::side)) {
            return 0; // Illegal move
        }

        return 1;
    }

    void perft_driver(int depth) {
        if (depth == 0) {
            nodes++;
            return;
        }

        MoveList move_list;
        MoveGen::generate_moves(move_list);

        for (int i = 0; i < move_list.count; i++) {
            COPY_BOARD;
            if (!make_move(move_list.moves[i], 0)) {
                RESTORE_BOARD;
                continue;
            }
            perft_driver(depth - 1);
            RESTORE_BOARD;
        }
    }

    void perft_test(int depth) {
        std::cout << "\nPerformance test\n";
        nodes = 0;
        MoveList move_list;
        MoveGen::generate_moves(move_list);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < move_list.count; i++) {
            COPY_BOARD;
            if (!make_move(move_list.moves[i], 0)) {
                RESTORE_BOARD;
                continue;
            }
            long long cummulative_nodes = nodes;
            perft_driver(depth - 1);
            long long old_nodes = nodes - cummulative_nodes;
            RESTORE_BOARD;

            int source = GET_MOVE_SOURCE(move_list.moves[i]);
            int target = GET_MOVE_TARGET(move_list.moves[i]);
            
            // Print square coordinates (e.g. e2e4)
            std::cout << "move: " << char((source % 8) + 'a') << (source / 8) + 1
                      << char((target % 8) + 'a') << (target / 8) + 1;
            
            if (GET_MOVE_PROMOTED(move_list.moves[i])) {
                char p_char = '?';
                int promoted = GET_MOVE_PROMOTED(move_list.moves[i]);
                if (promoted == Q || promoted == q) p_char = 'q';
                if (promoted == R || promoted == r) p_char = 'r';
                if (promoted == B || promoted == b) p_char = 'b';
                if (promoted == N || promoted == n) p_char = 'n';
                std::cout << p_char;
            }
            std::cout << " nodes: " << old_nodes << std::endl;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        std::cout << "\nDepth: " << depth << std::endl;
        std::cout << "Nodes: " << nodes << std::endl;
        std::cout << "Time:  " << duration.count() << " seconds\n";
        std::cout << "NPS:   " << (long long)(nodes / duration.count()) << " nodes/sec\n";
    }
}

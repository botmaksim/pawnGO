#include "board.h"
#include "movegen.h"
#include "zobrist.h"
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

    int make_move(Move move, int move_flag) {
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

        Bitboard::pop_bit(Bitboard::pieceBB[piece], source);
        Bitboard::set_bit(Bitboard::pieceBB[piece], target);

        if (capture) {
            int start_piece, end_piece;
            if (Bitboard::side == WHITE) { start_piece = p; end_piece = k; }
            else { start_piece = P; end_piece = K; }

            for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
                if (Bitboard::get_bit(Bitboard::pieceBB[bb_piece], target)) {
                    Bitboard::pop_bit(Bitboard::pieceBB[bb_piece], target);
                    break;
                }
            }
        }

        if (promoted) {
            Bitboard::pop_bit(Bitboard::pieceBB[piece], target);
            Bitboard::set_bit(Bitboard::pieceBB[promoted], target);
        }

        if (enpassant) {
            (Bitboard::side == WHITE) ? Bitboard::pop_bit(Bitboard::pieceBB[p], target - 8) : Bitboard::pop_bit(Bitboard::pieceBB[P], target + 8);
        }

        Bitboard::enpassant = no_sq;

        if (double_push) {
            (Bitboard::side == WHITE) ? (Bitboard::enpassant = target - 8) : (Bitboard::enpassant = target + 8);
        }

        if (castling) {
            switch (target) {
                case g1: Bitboard::pop_bit(Bitboard::pieceBB[R], h1); Bitboard::set_bit(Bitboard::pieceBB[R], f1); break;
                case c1: Bitboard::pop_bit(Bitboard::pieceBB[R], a1); Bitboard::set_bit(Bitboard::pieceBB[R], d1); break;
                case g8: Bitboard::pop_bit(Bitboard::pieceBB[r], h8); Bitboard::set_bit(Bitboard::pieceBB[r], f8); break;
                case c8: Bitboard::pop_bit(Bitboard::pieceBB[r], a8); Bitboard::set_bit(Bitboard::pieceBB[r], d8); break;
            }
        }

        Bitboard::castle &= castling_rights[source];
        Bitboard::castle &= castling_rights[target];

        Bitboard::occupancies[WHITE] = 0ULL;
        Bitboard::occupancies[BLACK] = 0ULL;
        Bitboard::occupancies[BOTH] = 0ULL;

        for (int piece_idx = P; piece_idx <= K; piece_idx++) Bitboard::occupancies[WHITE] |= Bitboard::pieceBB[piece_idx];
        for (int piece_idx = p; piece_idx <= k; piece_idx++) Bitboard::occupancies[BLACK] |= Bitboard::pieceBB[piece_idx];
        Bitboard::occupancies[BOTH] |= Bitboard::occupancies[WHITE];
        Bitboard::occupancies[BOTH] |= Bitboard::occupancies[BLACK];

        // Update Hash Key (full recalculation for simplicity, can be optimized later)
        Bitboard::hash_key = Zobrist::generate_hash_key();

        // Check if king is in check after our move
        int king_sq = (Bitboard::side == WHITE) ? Bitboard::lsb(Bitboard::pieceBB[K]) : Bitboard::lsb(Bitboard::pieceBB[k]);
        
        Bitboard::side ^= 1; // Change side

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

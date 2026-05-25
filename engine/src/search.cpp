#include "search.h"
#include "evaluation.h"
#include "movegen.h"
#include "tt.h"
#include "zobrist.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace Search {
    int max_depth = 6;
    int multi_pv = 1;
    std::atomic<bool> stopped(false);
    
    std::atomic<long long> nodes(0);

    thread_local int history_table[2][64][64];
    thread_local Move killer_moves[2][100];

    void clear_heuristics() {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 100; j++) killer_moves[i][j] = 0;
            for (int j = 0; j < 64; j++) {
                for (int k = 0; k < 64; k++) history_table[i][j][k] = 0;
            }
        }
    }

    int get_piece_value(int piece) {
        if (piece == P || piece == p) return 100;
        if (piece == N || piece == n) return 300;
        if (piece == B || piece == b) return 320;
        if (piece == R || piece == r) return 500;
        if (piece == Q || piece == q) return 900;
        return 0;
    }

    int score_move(Move move, Move hash_move, int ply) {
        if (move == hash_move) return 30000;
        if (GET_MOVE_CAPTURE(move)) return 20000 + get_piece_value(GET_MOVE_PIECE(move)) - get_piece_value(GET_MOVE_PIECE(move))/10;
        if (GET_MOVE_PROMOTED(move)) return 19000;
        
        if (ply < 100) {
            if (killer_moves[0][ply] == move) return 18000;
            if (killer_moves[1][ply] == move) return 17000;
        }

        int src = GET_MOVE_SOURCE(move);
        int tgt = GET_MOVE_TARGET(move);
        return history_table[Bitboard::side][src][tgt];
    }

    void sort_moves(MoveList& move_list, Move hash_move, int ply) {
        int scores[256];
        for (int i = 0; i < move_list.count; i++) {
            scores[i] = score_move(move_list.moves[i], hash_move, ply);
        }
        for (int i = 1; i < move_list.count; i++) {
            int j = i;
            while (j > 0 && scores[j - 1] < scores[j]) {
                std::swap(scores[j - 1], scores[j]);
                std::swap(move_list.moves[j - 1], move_list.moves[j]);
                j--;
            }
        }
    }

    int quiescence(int alpha, int beta, int ply) {
        if (stopped) return 0;
        nodes++;

        int eval = Evaluation::evaluate_incremental(ply);
        if (eval >= beta) return beta;
        if (eval > alpha) alpha = eval;

        MoveList move_list;
        MoveGen::generate_moves(move_list, true);
        sort_moves(move_list, 0, 0);

        for (int i = 0; i < move_list.count; i++) {
            if (!GET_MOVE_CAPTURE(move_list.moves[i])) continue;

            // Delta Pruning
            int margin = 200 + 900; // Safe upper bound for captured piece
            if (eval + margin < alpha && !GET_MOVE_PROMOTED(move_list.moves[i])) {
                continue;
            }

            COPY_BOARD;
            struct DirtyPiece dp;
            if (!Board::make_move(move_list.moves[i], 0, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                RESTORE_BOARD;
                continue;
            }
            int score = -quiescence(-beta, -alpha, ply + 1);
            RESTORE_BOARD;

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
        return alpha;
    }

    int alpha_beta(int depth, int alpha, int beta, int ply, bool do_null) {
        if (stopped) return 0;
        
        if (depth <= 0) {
            return quiescence(alpha, beta, ply);
        }
        nodes++;
        
        Move hash_move = 0;
        int tt_score = TT::read_hash_entry(Bitboard::hash_key, alpha, beta, depth, hash_move);
        if (tt_score != -50000) {
            return tt_score;
        }

        int king_sq = Bitboard::side == WHITE ? Bitboard::lsb(Bitboard::pieceBB[K]) : Bitboard::lsb(Bitboard::pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(king_sq, Bitboard::side ^ 1);

        if (in_check) depth++;

        // Null Move Pruning
        if (do_null && depth >= 3 && !in_check && ply > 0) {
            // Only if we have non-pawn pieces
            bool has_pieces = (Bitboard::side == WHITE) ? 
                (Bitboard::pieceBB[N] | Bitboard::pieceBB[B] | Bitboard::pieceBB[R] | Bitboard::pieceBB[Q]) :
                (Bitboard::pieceBB[n] | Bitboard::pieceBB[b] | Bitboard::pieceBB[r] | Bitboard::pieceBB[q]);
                
            if (has_pieces) {
                COPY_BOARD;
                Bitboard::side ^= 1;
                Bitboard::enpassant = no_sq;
                Bitboard::hash_key = Zobrist::generate_hash_key();
                
                int score = -alpha_beta(depth - 1 - 2, -beta, -beta + 1, ply + 1, false);
                
                RESTORE_BOARD;
                if (stopped) return 0;
                
                if (score >= beta) return beta;
            }
        }

        MoveList move_list;
        MoveGen::generate_moves(move_list);
        sort_moves(move_list, hash_move, ply);

        int legal_moves = 0;
        int old_alpha = alpha;
        Move best_move = 0;
        int hash_flag = hash_flag_alpha;

        for (int i = 0; i < move_list.count; i++) {
            COPY_BOARD;
            if (!Board::make_move(move_list.moves[i], 0, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                RESTORE_BOARD;
                continue;
            }
            legal_moves++;
            
            int score;
            bool is_quiet = !GET_MOVE_CAPTURE(move_list.moves[i]) && !GET_MOVE_PROMOTED(move_list.moves[i]);
            
            // Late Move Reductions (LMR) & PVS
            if (legal_moves >= 4 && depth >= 3 && !in_check && is_quiet) {
                // Reduced depth
                score = -alpha_beta(depth - 2, -alpha - 1, -alpha, ply + 1, true);
                if (score > alpha && score < beta) {
                    // Re-search full depth
                    score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1, true);
                }
            } else {
                score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1, true);
            }
            
            RESTORE_BOARD;

            if (stopped) return 0;

            if (score >= beta) {
                TT::write_hash_entry(Bitboard::hash_key, beta, depth, hash_flag_beta, move_list.moves[i]);
                if (is_quiet) {
                    if (ply < 100) {
                        killer_moves[1][ply] = killer_moves[0][ply];
                        killer_moves[0][ply] = move_list.moves[i];
                    }
                    int src = GET_MOVE_SOURCE(move_list.moves[i]);
                    int tgt = GET_MOVE_TARGET(move_list.moves[i]);
                    history_table[Bitboard::side][src][tgt] += depth * depth;
                }
                return beta;
            }
            if (score > alpha) {
                hash_flag = hash_flag_exact;
                alpha = score;
                best_move = move_list.moves[i];
            }
        }

        if (legal_moves == 0) {
            if (in_check) return -49000 + ply;
            return 0; // Stalemate
        }

        TT::write_hash_entry(Bitboard::hash_key, alpha, depth, hash_flag, best_move);
        return alpha;
    }

    std::vector<std::thread> thread_pool;
    std::mutex search_mtx;
    std::condition_variable search_cv;
    bool search_running = false;
    bool exit_threads = false;

    // Snapshot for helpers
    U64 root_pBB[12];
    U64 root_occ[3];
    int root_s, root_e, root_c;
    int root_max_depth;

    void helper_thread_loop(int thread_id) {
        while (true) {
            std::unique_lock<std::mutex> lock(search_mtx);
            search_cv.wait(lock, []{ return search_running || exit_threads; });
            if (exit_threads) break;
            
            for(int i=0; i<12; i++) Bitboard::pieceBB[i] = root_pBB[i];
            for(int i=0; i<3; i++) Bitboard::occupancies[i] = root_occ[i];
            Bitboard::side = root_s;
            Bitboard::enpassant = root_e;
            Bitboard::castle = root_c;
            Bitboard::hash_key = Zobrist::generate_hash_key();
            int current_target_depth = root_max_depth;
            lock.unlock();

            clear_heuristics();
            
            // Helper does its own iterative deepening, using random jitter to avoid duplicate trees
            for (int current_depth = 1; current_depth <= current_target_depth + (thread_id % 2); current_depth++) {
                if (stopped || !search_running) break;
                // Just use generic PVS for root
                MoveList move_list;
                MoveGen::generate_moves(move_list);
                sort_moves(move_list, 0, 0); // initial sort
                for (int i=0; i < move_list.count; i++) {
                    COPY_BOARD;
                    if (!Board::make_move(move_list.moves[i], 0, &Evaluation::nnue_stack[1].dirtyPiece)) {
                        RESTORE_BOARD;
                        continue;
                    }
                    // Aspiration-like or full window
                    -alpha_beta(current_depth - 1, -50000, 50000, 1, true);
                    RESTORE_BOARD;
                    if (stopped || !search_running) break;
                }
            }
            
            // Sleep until next launch
            lock.lock();
            // wait for search_running to be false to avoid immediate re-trigger if main is still running
        }
    }

    void init_threads(int num_threads) {
        stop_threads();
        exit_threads = false;
        search_running = false;
        for (int i = 0; i < num_threads - 1; i++) {
            thread_pool.emplace_back(helper_thread_loop, i + 1);
        }
    }

    void stop_threads() {
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            exit_threads = true;
            search_running = true;
        }
        search_cv.notify_all();
        for (auto& t : thread_pool) {
            if (t.joinable()) t.join();
        }
        thread_pool.clear();
    }

    std::string extract_pv(int max_depth_val) {
        std::string pv_str = "";
        U64 current_hash = Bitboard::hash_key;
        int depth = 0;
        
        U64 pBB[12];
        U64 occ[3];
        for(int i=0; i<12; i++) pBB[i] = Bitboard::pieceBB[i];
        for(int i=0; i<3; i++) occ[i] = Bitboard::occupancies[i];
        int s = Bitboard::side;
        int e = Bitboard::enpassant;
        int c = Bitboard::castle;
        
        while (depth < max_depth_val) {
            Move best_move = 0;
            TT::read_hash_entry(current_hash, -50000, 50000, 0, best_move);
            if (best_move == 0) break;
            
            int src = GET_MOVE_SOURCE(best_move);
            int tgt = GET_MOVE_TARGET(best_move);
            int promoted = GET_MOVE_PROMOTED(best_move);

            if (depth > 0) pv_str += " ";
            pv_str += char((src % 8) + 'a');
            pv_str += char((src / 8) + '1');
            pv_str += char((tgt % 8) + 'a');
            pv_str += char((tgt / 8) + '1');

            if (promoted) {
                int p_type = promoted % 6;
                if (p_type == 1) pv_str += "n";
                if (p_type == 2) pv_str += "b";
                if (p_type == 3) pv_str += "r";
                if (p_type == 4) pv_str += "q";
            }

            COPY_BOARD;
            if (!Board::make_move(best_move, 0)) {
                RESTORE_BOARD;
                break;
            }
            
            current_hash = Bitboard::hash_key;
            depth++;
        }

        for(int i=0; i<12; i++) Bitboard::pieceBB[i] = pBB[i];
        for(int i=0; i<3; i++) Bitboard::occupancies[i] = occ[i];
        Bitboard::side = s;
        Bitboard::enpassant = e;
        Bitboard::castle = c;
        Bitboard::hash_key = Zobrist::generate_hash_key();
        
        return pv_str;
    }

    struct RootMove {
        Move move;
        int score;
    };

    void search_position(int depth) {
        if (Bitboard::current_fen != "") {
            Bitboard::parse_fen(Bitboard::current_fen);
        }
        clear_heuristics();
        
        max_depth = depth;
        stopped = false;
        nodes = 0;

        MoveList move_list;
        MoveGen::generate_moves(move_list);
        sort_moves(move_list, 0, 0);

        std::vector<RootMove> legal_moves;
        for (int i = 0; i < move_list.count; i++) {
            COPY_BOARD;
            if (Board::make_move(move_list.moves[i], 0)) {
                legal_moves.push_back({move_list.moves[i], 0});
            }
            RESTORE_BOARD;
        }

        if (legal_moves.empty()) {
            std::cout << "bestmove 0000" << std::endl;
            return;
        }

        for(int i=0; i<12; i++) root_pBB[i] = Bitboard::pieceBB[i];
        for(int i=0; i<3; i++) root_occ[i] = Bitboard::occupancies[i];
        root_s = Bitboard::side;
        root_e = Bitboard::enpassant;
        root_c = Bitboard::castle;
        
        // Wake up threads
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            root_max_depth = depth;
            search_running = true;
        }
        search_cv.notify_all();

        int actual_multi_pv = std::min(multi_pv, (int)legal_moves.size());
        std::vector<Move> global_best_moves(actual_multi_pv, 0);

        for (int current_depth = 1; current_depth <= depth; current_depth++) {
            std::vector<Move> excluded_moves;
            
            // Re-sort legal moves based on previous depth's scores
            std::sort(legal_moves.begin(), legal_moves.end(), [](const RootMove& a, const RootMove& b) {
                return a.score > b.score;
            });

            for (int pv_idx = 0; pv_idx < actual_multi_pv; pv_idx++) {
                int best_score = -50000;
                Move best_move_this_iteration = 0;
                
                int alpha = -50000;
                int beta = 50000;
                
                // For main line, use aspiration windows
                if (current_depth >= 4 && pv_idx == 0) {
                    alpha = legal_moves[0].score - 50;
                    beta = legal_moves[0].score + 50;
                }

                while (true) {
                    best_score = -50000;
                    best_move_this_iteration = 0;
                    int current_alpha = alpha;

                    for (auto& rm : legal_moves) {
                        Move move = rm.move;
                        if (std::find(excluded_moves.begin(), excluded_moves.end(), move) != excluded_moves.end()) continue;
                        
                        COPY_BOARD;
                        if (!Board::make_move(move, 0, &Evaluation::nnue_stack[1].dirtyPiece)) {
                            RESTORE_BOARD;
                            continue;
                        }
                        
                        int score = -alpha_beta(current_depth - 1, -beta, -current_alpha, 1, true);
                        RESTORE_BOARD;
                        
                        if (stopped) break;
                        
                        if (pv_idx == 0) rm.score = score;
                        
                        if (score > best_score) {
                            best_score = score;
                            best_move_this_iteration = move;
                        }
                        if (score > current_alpha) {
                            current_alpha = score;
                        }
                    }
                    
                    if (stopped) break;
                    
                    if (best_score <= alpha && alpha != -50000) {
                        alpha = -50000;
                        continue;
                    }
                    if (best_score >= beta && beta != 50000) {
                        beta = 50000;
                        continue;
                    }
                    break;
                }
                
                if (stopped) break;
                
                if (best_move_this_iteration != 0) {
                    excluded_moves.push_back(best_move_this_iteration);
                    global_best_moves[pv_idx] = best_move_this_iteration;
                    
                    TT::write_hash_entry(Bitboard::hash_key, best_score, current_depth, hash_flag_exact, best_move_this_iteration);
                    
                    std::string pv_line = extract_pv(current_depth);
                    
                    std::cout << "info depth " << current_depth 
                              << " multipv " << (pv_idx + 1)
                              << " score cp " << best_score 
                              << " nodes " << nodes 
                              << " pv " << pv_line << std::endl;
                }
            }
            if (stopped) break;
        }

        // Sleep threads
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            search_running = false;
        }

        Move best_final_move = global_best_moves[0];
        if (best_final_move == 0) best_final_move = legal_moves[0].move;

        int src = GET_MOVE_SOURCE(best_final_move);
        int tgt = GET_MOVE_TARGET(best_final_move);
        int promoted = GET_MOVE_PROMOTED(best_final_move);

        std::string move_str = "";
        move_str += char((src % 8) + 'a');
        move_str += char((src / 8) + '1');
        move_str += char((tgt % 8) + 'a');
        move_str += char((tgt / 8) + '1');

        if (promoted) {
            int p_type = promoted % 6;
            if (p_type == 1) move_str += "n";
            if (p_type == 2) move_str += "b";
            if (p_type == 3) move_str += "r";
            if (p_type == 4) move_str += "q";
        }
        
        std::cout << "bestmove " << move_str << std::endl;
    }
}

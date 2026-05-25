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
    std::atomic<bool> stopped(false);
    
    // Engine statistics
    std::atomic<long long> nodes(0);

    // Heuristics tables
    thread_local Move killer_moves[64][2];
    thread_local int history_moves[12][64];

    void clear_heuristics() {
        for (int i = 0; i < 64; i++) {
            killer_moves[i][0] = 0;
            killer_moves[i][1] = 0;
        }
        for (int i = 0; i < 12; i++) {
            for (int j = 0; j < 64; j++) {
                history_moves[i][j] = 0;
            }
        }
    }

    int score_move(Move move, Move hash_move, int ply) {
        if (move == hash_move) return 20000;
        if (GET_MOVE_CAPTURE(move)) return 10000;
        if (GET_MOVE_PROMOTED(move)) return 9000;
        
        // Killer moves
        if (ply < 64) {
            if (killer_moves[ply][0] == move) return 8000;
            if (killer_moves[ply][1] == move) return 7000;
        }

        // History heuristic
        int piece = GET_MOVE_PIECE(move);
        int target = GET_MOVE_TARGET(move);
        return history_moves[piece][target];
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

    int quiescence(int alpha, int beta) {
        if (stopped) return 0;
        nodes++;

        int eval = Evaluation::evaluate();
        if (eval >= beta) return beta;
        if (eval > alpha) alpha = eval;

        MoveList move_list;
        MoveGen::generate_moves(move_list);
        sort_moves(move_list, 0, 0);

        for (int i = 0; i < move_list.count; i++) {
            if (!GET_MOVE_CAPTURE(move_list.moves[i])) continue; // Only captures

            COPY_BOARD;
            if (!Board::make_move(move_list.moves[i], 0)) {
                RESTORE_BOARD;
                continue;
            }
            int score = -quiescence(-beta, -alpha);
            RESTORE_BOARD;

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
        return alpha;
    }

    int alpha_beta(int depth, int alpha, int beta, int ply) {
        if (stopped) return 0;
        
        if (depth == 0) {
            return quiescence(alpha, beta);
        }
        nodes++;
        
        Move hash_move = 0;
        int tt_score = TT::read_hash_entry(Bitboard::hash_key, alpha, beta, depth, hash_move);
        if (tt_score != -50000) {
            return tt_score;
        }

        int king_sq = Bitboard::side == WHITE ? Bitboard::lsb(Bitboard::pieceBB[K]) : Bitboard::lsb(Bitboard::pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(king_sq, Bitboard::side ^ 1);

        // Null Move Pruning
        if (depth >= 3 && !in_check && ply > 0) {
            COPY_BOARD;
            Bitboard::side ^= 1;
            Bitboard::enpassant = no_sq;
            Bitboard::hash_key = Zobrist::generate_hash_key();
            
            int score = -alpha_beta(depth - 1 - 2, -beta, -beta + 1, ply + 1);
            
            RESTORE_BOARD;
            if (stopped) return 0;
            
            if (score >= beta) return beta;
        }

        // Reverse Futility Pruning (Static Null Move Pruning)
        int static_eval = Evaluation::evaluate();
        if (depth <= 3 && !in_check && ply > 0) {
            if (static_eval - 120 * depth >= beta) {
                return beta;
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
            // Futility Pruning
            if (depth == 1 && !in_check && legal_moves > 0) {
                if (!GET_MOVE_CAPTURE(move_list.moves[i]) && !GET_MOVE_PROMOTED(move_list.moves[i])) {
                    if (static_eval + 200 <= alpha) {
                        continue; // Skip move
                    }
                }
            }

            COPY_BOARD;
            if (!Board::make_move(move_list.moves[i], 0)) {
                RESTORE_BOARD;
                continue;
            }
            legal_moves++;
            
            int score;
            
            // Late Move Reductions (LMR)
            if (legal_moves >= 4 && depth >= 3 && !in_check && 
                !GET_MOVE_CAPTURE(move_list.moves[i]) && !GET_MOVE_PROMOTED(move_list.moves[i])) {
                
                // Search with reduced depth
                score = -alpha_beta(depth - 2, -alpha - 1, -alpha, ply + 1);
                
                // If it beats alpha, re-search with full depth
                if (score > alpha) {
                    score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1);
                }
            } else {
                // Normal search
                score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1);
            }
            
            RESTORE_BOARD;

            if (stopped) return 0;

            if (score >= beta) {
                TT::write_hash_entry(Bitboard::hash_key, beta, depth, hash_flag_beta, move_list.moves[i]);
                
                // Update killer moves and history for quiet moves
                if (!GET_MOVE_CAPTURE(move_list.moves[i])) {
                    if (ply < 64) {
                        killer_moves[ply][1] = killer_moves[ply][0];
                        killer_moves[ply][0] = move_list.moves[i];
                    }
                    int piece = GET_MOVE_PIECE(move_list.moves[i]);
                    int target = GET_MOVE_TARGET(move_list.moves[i]);
                    history_moves[piece][target] += depth * depth;
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
            if (in_check) {
                return -49000 + (max_depth - depth); // Mate score
            } else {
                return 0; // Stalemate
            }
        }

        TT::write_hash_entry(Bitboard::hash_key, alpha, depth, hash_flag, best_move);
        return alpha;
    }

    int multi_pv = 1;

    std::vector<std::thread> thread_pool;
    std::mutex search_mtx;
    std::condition_variable search_cv;
    bool search_running = false;
    bool exit_threads = false;

    // Snapshot for helpers
    U64 root_pBB[12];
    U64 root_occ[3];
    int root_s, root_e, root_c;
    
    // Shared state for synchronization
    int root_max_depth;
    int root_pv_idx;
    std::vector<Move> root_excluded_moves;

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
            
            int current_target_depth = root_max_depth + thread_id; 
            int my_pv_idx = root_pv_idx;
            std::vector<Move> my_excluded_moves = root_excluded_moves;

            lock.unlock();

            MoveList move_list;
            MoveGen::generate_moves(move_list);
            sort_moves(move_list, 0, 0);
            
            std::vector<Move> legal_moves;
            for (int i = 0; i < move_list.count; i++) {
                COPY_BOARD;
                if (Board::make_move(move_list.moves[i], 0)) legal_moves.push_back(move_list.moves[i]);
                RESTORE_BOARD;
            }

            // Helpers only search once per notification, they don't loop indefinitely
            int alpha = -50000;
            int beta = 50000;
            for (Move move : legal_moves) {
                if (stopped || !search_running || root_pv_idx != my_pv_idx || root_max_depth != (current_target_depth - thread_id)) break;
                
                if (std::find(my_excluded_moves.begin(), my_excluded_moves.end(), move) != my_excluded_moves.end()) {
                    continue;
                }
                
                COPY_BOARD;
                if (!Board::make_move(move, 0)) {
                    RESTORE_BOARD;
                    continue;
                }
                int score = -alpha_beta(current_target_depth - 1, -beta, -alpha, 1);
                RESTORE_BOARD;
            }
        }
    }

    void init_threads(int num_threads) {
        for (int i = 0; i < num_threads - 1; i++) {
            thread_pool.emplace_back(helper_thread_loop, i + 1);
        }
    }

    void stop_threads() {
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            exit_threads = true;
        }
        search_cv.notify_all();
        for (auto& t : thread_pool) {
            if (t.joinable()) t.join();
        }
    }

    std::string extract_pv(int max_depth) {
        std::string pv_str = "";
        U64 current_hash = Bitboard::hash_key;
        int depth = 0;
        
        // Snapshot
        U64 pBB[12];
        U64 occ[3];
        for(int i=0; i<12; i++) pBB[i] = Bitboard::pieceBB[i];
        for(int i=0; i<3; i++) occ[i] = Bitboard::occupancies[i];
        int s = Bitboard::side;
        int e = Bitboard::enpassant;
        int c = Bitboard::castle;
        
        while (depth < max_depth) {
            Move best_move = 0;
            // Read TT blindly (we just want the best_move stored)
            TT::read_hash_entry(current_hash, -50000, 50000, 0, best_move);
            if (best_move == 0) break;
            
            // Format move
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

            // Pseudo-legal check to prevent infinite loops from bad TT data
            COPY_BOARD;
            if (!Board::make_move(best_move, 0)) {
                RESTORE_BOARD;
                break;
            }
            
            current_hash = Bitboard::hash_key;
            depth++;
        }

        // Restore snapshot
        for(int i=0; i<12; i++) Bitboard::pieceBB[i] = pBB[i];
        for(int i=0; i<3; i++) Bitboard::occupancies[i] = occ[i];
        Bitboard::side = s;
        Bitboard::enpassant = e;
        Bitboard::castle = c;
        Bitboard::hash_key = Zobrist::generate_hash_key();
        
        return pv_str;
    }

    void search_position(int depth) {
        max_depth = depth;
        stopped = false;
        nodes = 0;

        MoveList move_list;
        MoveGen::generate_moves(move_list);
        sort_moves(move_list, 0, 0);

        // Filter legal moves
        std::vector<Move> legal_moves;
        for (int i = 0; i < move_list.count; i++) {
            COPY_BOARD;
            if (Board::make_move(move_list.moves[i], 0)) {
                legal_moves.push_back(move_list.moves[i]);
            }
            RESTORE_BOARD;
        }

        if (legal_moves.empty()) {
            std::cout << "bestmove 0000" << std::endl;
            return;
        }

        int actual_multi_pv = std::min(multi_pv, (int)legal_moves.size());
        std::vector<Move> global_best_moves(actual_multi_pv, 0);

        // Wake up helper threads
        for(int i=0; i<12; i++) root_pBB[i] = Bitboard::pieceBB[i];
        for(int i=0; i<3; i++) root_occ[i] = Bitboard::occupancies[i];
        root_s = Bitboard::side;
        root_e = Bitboard::enpassant;
        root_c = Bitboard::castle;
        // Iterative deepening
        for (int current_depth = 1; current_depth <= depth; current_depth++) {
            std::vector<Move> excluded_moves;
            
            for (int pv_idx = 0; pv_idx < actual_multi_pv; pv_idx++) {
                
                // Wake up helper threads for this specific depth and pv_idx
                {
                    std::lock_guard<std::mutex> lock(search_mtx);
                    root_max_depth = current_depth;
                    root_pv_idx = pv_idx;
                    root_excluded_moves = excluded_moves;
                    search_running = true;
                }
                search_cv.notify_all();

                int best_score = -50000;
                Move best_move_this_iteration = 0;
                int alpha = -50000;
                int beta = 50000;
                
                // Root search for this PV line
                for (Move move : legal_moves) {
                    // Skip if move is already found in better PV lines
                    if (std::find(excluded_moves.begin(), excluded_moves.end(), move) != excluded_moves.end()) {
                        continue;
                    }
                    
                    COPY_BOARD;
                    if (!Board::make_move(move, 0)) {
                        RESTORE_BOARD;
                        continue;
                    }
                    
                    int score = -alpha_beta(current_depth - 1, -beta, -alpha, 1);
                    RESTORE_BOARD;
                    
                    if (stopped) break;
                    
                    if (score > best_score) {
                        best_score = score;
                        best_move_this_iteration = move;
                    }
                    if (score > alpha) alpha = score;
                }
                
                // Sleep helper threads while we process results and extract PV
                {
                    std::lock_guard<std::mutex> lock(search_mtx);
                    search_running = false;
                }
                
                if (stopped) break;
                
                if (best_move_this_iteration != 0) {
                    excluded_moves.push_back(best_move_this_iteration);
                    global_best_moves[pv_idx] = best_move_this_iteration;
                    
                    // To extract PV correctly, we need the root hash move to be best_move_this_iteration
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

        Move best_final_move = global_best_moves[0];
        if (best_final_move == 0) best_final_move = legal_moves[0];

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

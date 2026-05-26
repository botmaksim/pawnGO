#include "search.h"
#include "evaluation.h"
#include "syzygy/tbprobe.h"
#include "movegen.h"
#include "tt.h"
#include "zobrist.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>

namespace Search {
    int max_depth = 6;
    int multi_pv = 1;
    std::atomic<bool> stopped(false);
    
    std::atomic<long long> nodes(0);

    thread_local int history_table[2][64][64];
    thread_local Move killer_moves[2][100];

    thread_local Move counter_move[64][64];

    int LMR_table[64][64];

    void init_lmr_table() {
        for (int depth = 0; depth < 64; depth++) {
            for (int moves = 0; moves < 64; moves++) {
                if (depth >= 3 && moves >= 4) {
                    LMR_table[depth][moves] = 1 + std::log(depth) * std::log(moves) / 2.0;
                } else {
                    LMR_table[depth][moves] = 0;
                }
            }
        }
    }

    void clear_heuristics() {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 100; j++) killer_moves[i][j] = 0;
            for (int j = 0; j < 64; j++) {
                for (int k = 0; k < 64; k++) history_table[i][j][k] = 0;
            }
        }
        for (int j = 0; j < 64; j++) {
            for (int k = 0; k < 64; k++) counter_move[j][k] = 0;
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

    int score_move(Move move, Move hash_move, int ply, Move prev_move = 0) {
        if (move == hash_move) return 30000;
        if (GET_MOVE_CAPTURE(move)) return 20000 + get_piece_value(GET_MOVE_PIECE(move)) - get_piece_value(GET_MOVE_PIECE(move))/10;
        if (GET_MOVE_PROMOTED(move)) return 19000;
        
        if (ply < 100) {
            if (killer_moves[0][ply] == move) return 18000;
            if (killer_moves[1][ply] == move) return 17000;
        }

        if (prev_move != 0) {
            if (counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] == move) {
                return 16000;
            }
        }

        int src = GET_MOVE_SOURCE(move);
        int tgt = GET_MOVE_TARGET(move);
        return history_table[Bitboard::side][src][tgt];
    }

    void sort_moves(MoveList& move_list, Move hash_move, int ply, Move prev_move) {
        int scores[256];
        for (int i = 0; i < move_list.count; i++) {
            scores[i] = score_move(move_list.moves[i], hash_move, ply, prev_move);
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

    inline int get_piece_on_square(int sq) {
        for (int p = 0; p < 12; p++) {
            if (Bitboard::pieceBB[p] & (1ULL << sq)) return p;
        }
        return -1;
    }

    int quiescence(int alpha, int beta, int ply) {
        if (stopped) return 0;
        nodes++;

        int eval = Evaluation::evaluate_incremental(ply);
        if (eval >= beta) return beta;
        if (eval > alpha) alpha = eval;

        MoveList move_list;
        MoveGen::generate_moves(move_list, true);
        sort_moves(move_list, 0, 0, 0); // Need to pass prev_move

        for (int i = 0; i < move_list.count; i++) {
            Move move = move_list.moves[i];
            if (!GET_MOVE_CAPTURE(move)) continue;

            int tgt = GET_MOVE_TARGET(move);
            int attacker = GET_MOVE_PIECE(move);
            int victim = get_piece_on_square(tgt);

            // Basic SEE-like Pruning
            if (victim != -1 && get_piece_value(attacker) > get_piece_value(victim) && !GET_MOVE_PROMOTED(move)) {
                if (MoveGen::is_square_attacked(tgt, Bitboard::side ^ 1)) {
                    // It's a bad capture (e.g. Queen taking Pawn and Pawn is defended)
                    continue;
                }
            }

            // Delta Pruning
            int margin = 200 + (victim != -1 ? get_piece_value(victim) : 900); 
            if (eval + margin < alpha && !GET_MOVE_PROMOTED(move)) {
                continue;
            }

            COPY_BOARD;
            if (!Board::make_move(move, 0, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
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

    int alpha_beta(int depth, int alpha, int beta, int ply, bool do_null, Move prev_move) {
        if (stopped) return 0;
        
        U64 gen_hash = Zobrist::generate_hash_key();
        if (Bitboard::hash_key != gen_hash) {
            std::cout << "info string HASH MISMATCH IN ALPHA_BETA! depth=" << depth << " ply=" << ply << std::endl;
        }
        
        int king_sq = Bitboard::side == WHITE ? Bitboard::lsb(Bitboard::pieceBB[K]) : Bitboard::lsb(Bitboard::pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(king_sq, Bitboard::side ^ 1);
        
        // Check transposition table
        Move hash_move = 0;
        int tt_score = TT::read_hash_entry(Bitboard::hash_key, alpha, beta, depth, hash_move);
        if (tt_score != -32000) {
            return tt_score;
        }

        // Syzygy Tablebase Probe
        uint64_t white_pieces = Bitboard::pieceBB[P] | Bitboard::pieceBB[N] | Bitboard::pieceBB[B] | Bitboard::pieceBB[R] | Bitboard::pieceBB[Q] | Bitboard::pieceBB[K];
        uint64_t black_pieces = Bitboard::pieceBB[p] | Bitboard::pieceBB[n] | Bitboard::pieceBB[b] | Bitboard::pieceBB[r] | Bitboard::pieceBB[q] | Bitboard::pieceBB[k];
        int num_pieces = __builtin_popcountll(white_pieces | black_pieces);

        if (TB_LARGEST > 0 && num_pieces <= TB_LARGEST && !in_check) {
            uint64_t kings = Bitboard::pieceBB[K] | Bitboard::pieceBB[k];
            uint64_t queens = Bitboard::pieceBB[Q] | Bitboard::pieceBB[q];
            uint64_t rooks = Bitboard::pieceBB[R] | Bitboard::pieceBB[r];
            uint64_t bishops = Bitboard::pieceBB[B] | Bitboard::pieceBB[b];
            uint64_t knights = Bitboard::pieceBB[N] | Bitboard::pieceBB[n];
            uint64_t pawns = Bitboard::pieceBB[P] | Bitboard::pieceBB[p];
            unsigned ep = Bitboard::enpassant != no_sq ? Bitboard::enpassant : 0;
            
            unsigned wdl = tb_probe_wdl(white_pieces, black_pieces, kings, queens, rooks, bishops, knights, pawns, 0, 0, ep, Bitboard::side == 0);
            
            if (wdl != TB_RESULT_FAILED) {
                int tb_score = 0;
                if (wdl == TB_WIN) tb_score = 20000 - ply;
                else if (wdl == TB_LOSS) tb_score = -20000 + ply;
                else tb_score = 0;

                // Adjust for mate scores to avoid bounds issues
                if (tb_score > beta) return beta;
                if (tb_score < alpha) return alpha;
                return tb_score;
            }
        }

        if (depth <= 0) {
            return quiescence(alpha, beta, ply);
        }
        nodes++;
        
        if (in_check && ply < max_depth + 10) depth++;

        int static_eval = 0;
        if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY) {
            static_eval = Evaluation::evaluate_incremental(ply);
        } else {
            static_eval = Evaluation::evaluate();
        }

        bool is_pv = (beta - alpha) > 1;

        // Razoring
        if (!is_pv && !in_check && depth <= 3) {
            int razor_margin = 300 * depth;
            if (static_eval + razor_margin <= alpha) {
                int score = quiescence(alpha - razor_margin, beta, ply);
                if (score <= alpha) return score;
            }
        }

        // Reverse Futility Pruning (Static Null Move Pruning)
        if (!in_check && !is_pv && depth < 5 && ply > 0) {
            int rfp_margin = 120 * depth;
            if (static_eval - rfp_margin >= beta) {
                return static_eval;
            }
        }

        // Null Move Pruning
        if (do_null && !in_check && !is_pv && depth >= 3 && ply > 0 && static_eval >= beta) {
            bool has_pieces = (Bitboard::side == WHITE) ? 
                (Bitboard::pieceBB[N] | Bitboard::pieceBB[B] | Bitboard::pieceBB[R] | Bitboard::pieceBB[Q]) :
                (Bitboard::pieceBB[n] | Bitboard::pieceBB[b] | Bitboard::pieceBB[r] | Bitboard::pieceBB[q]);
                
            if (has_pieces) {
                COPY_BOARD;
                Bitboard::side ^= 1;
                Bitboard::hash_key ^= Zobrist::side_key;
                if (Bitboard::enpassant != no_sq) {
                    Bitboard::hash_key ^= Zobrist::enpassant_keys[Bitboard::enpassant];
                }
                Bitboard::enpassant = no_sq;
                
                if (ply + 1 < Evaluation::MAX_PLY) {
                    Evaluation::nnue_stack[ply + 1].dirtyPiece.dirtyNum = 0;
                }
                
                int R = 3 + depth / 4;
                int score = -alpha_beta(depth - 1 - R, -beta, -beta + 1, ply + 1, false, 0);
                
                RESTORE_BOARD;
                if (stopped) return 0;
                
                if (score >= beta) return beta;
            }
        }

        if (is_pv && hash_move == 0 && depth >= 4) {
            // Do a shallow search to find a hash move
            int d = depth - 2;
            alpha_beta(d, alpha, beta, ply, do_null, prev_move);
            TT::read_hash_entry(Bitboard::hash_key, alpha, beta, depth, hash_move);
        }

        MoveList move_list;
        MoveGen::generate_moves(move_list);
        sort_moves(move_list, hash_move, ply, prev_move);

        int legal_moves = 0;
        int old_alpha = alpha;
        Move best_move = 0;
        int hash_flag = hash_flag_alpha;

        bool futility_pruning = !in_check && !is_pv && depth <= 4 && (static_eval + 150 * depth <= alpha);

        for (int i = 0; i < move_list.count; i++) {
            bool is_quiet = !GET_MOVE_CAPTURE(move_list.moves[i]) && !GET_MOVE_PROMOTED(move_list.moves[i]);
            
            // Late Move Pruning
            if (!in_check && !is_pv && is_quiet && depth < 4 && legal_moves > (3 + 2 * depth * depth)) {
                continue;
            }

            // Futility Pruning
            if (futility_pruning && is_quiet && legal_moves > 0 && 
                move_list.moves[i] != killer_moves[0][ply] && 
                move_list.moves[i] != killer_moves[1][ply]) {
                continue;
            }

            COPY_BOARD;
            if (!Board::make_move(move_list.moves[i], 0, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                RESTORE_BOARD;
                continue;
            }
            legal_moves++;
            
            int score;
            
            // PVS / LMR
            if (legal_moves == 1) {
                score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1, true, move_list.moves[i]);
            } else {
                int R = 0;
                if (depth >= 3 && legal_moves >= 4 && !in_check && is_quiet) {
                    int d = std::min(depth, 63);
                    int m = std::min(legal_moves, 63);
                    R = LMR_table[d][m];
                    
                    int src = GET_MOVE_SOURCE(move_list.moves[i]);
                    int tgt = GET_MOVE_TARGET(move_list.moves[i]);
                    int history_score = history_table[Bitboard::side][src][tgt];
                    
                    // History-based LMR adjustment
                    if (history_score > 4000) R -= 2;
                    else if (history_score > 1000) R -= 1;
                    
                    if (R < 0) R = 0;
                    if (R > depth - 1) R = depth - 1;
                }
                
                score = -alpha_beta(depth - 1 - R, -alpha - 1, -alpha, ply + 1, true, move_list.moves[i]);
                
                if (score > alpha && R > 0) {
                    score = -alpha_beta(depth - 1, -alpha - 1, -alpha, ply + 1, true, move_list.moves[i]);
                }
                if (score > alpha && score < beta) {
                    score = -alpha_beta(depth - 1, -beta, -alpha, ply + 1, true, move_list.moves[i]);
                }
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
                    if (prev_move != 0) {
                        counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] = move_list.moves[i];
                    }
                    int src = GET_MOVE_SOURCE(move_list.moves[i]);
                    int tgt = GET_MOVE_TARGET(move_list.moves[i]);
                    int bonus = depth * depth;
                    history_table[Bitboard::side][src][tgt] += bonus;
                    if (history_table[Bitboard::side][src][tgt] > 10000) {
                        // Normalize history table
                        for (int s = 0; s < 64; s++) {
                            for (int t = 0; t < 64; t++) {
                                history_table[0][s][t] /= 2;
                                history_table[1][s][t] /= 2;
                            }
                        }
                    }
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
            if (in_check) return -31000 + ply;
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
            lock.unlock();

            clear_heuristics();
            
            // Helper does its own iterative deepening infinitely until stopped
            for (int current_depth = 1 + (thread_id % 4); current_depth < 100; current_depth++) {
                if (stopped || !search_running) break;
                
                MoveList move_list;
                MoveGen::generate_moves(move_list);
                sort_moves(move_list, 0, 0); 
                
                for (int i=0; i < move_list.count; i++) {
                    COPY_BOARD;
                    Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;
                    if (!Board::make_move(move_list.moves[i], 0, &Evaluation::nnue_stack[1].dirtyPiece)) {
                        RESTORE_BOARD;
                        continue;
                    }
                    
                    int score;
                    if (i == 0) {
                        score = -alpha_beta(current_depth - 1, -32000, 32000, 1, true, move_list.moves[i]);
                    } else {
                        score = -alpha_beta(current_depth - 1, -32000, 32000, 1, true, move_list.moves[i]);
                    }
                    
                    RESTORE_BOARD;
                    if (stopped || !search_running) break;
                }
            }
            
            // Sleep until next launch
            lock.lock();
            while (search_running && !exit_threads) {
                // If main thread hasn't set search_running = false yet, wait for it
                search_cv.wait_for(lock, std::chrono::milliseconds(1));
            }
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
            TT::read_hash_entry(current_hash, -32000, 32000, 0, best_move);
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
        // We do NOT parse current_fen here, because main.cpp already sets up the board and applies moves!
        clear_heuristics();
        
        max_depth = depth;
        stopped = false;
        nodes = 0;
        
        Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;

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

        uint64_t white_pieces = Bitboard::pieceBB[P] | Bitboard::pieceBB[N] | Bitboard::pieceBB[B] | Bitboard::pieceBB[R] | Bitboard::pieceBB[Q] | Bitboard::pieceBB[K];
        uint64_t black_pieces = Bitboard::pieceBB[p] | Bitboard::pieceBB[n] | Bitboard::pieceBB[b] | Bitboard::pieceBB[r] | Bitboard::pieceBB[q] | Bitboard::pieceBB[k];
        int num_pieces = __builtin_popcountll(white_pieces | black_pieces);

        if (TB_LARGEST > 0 && num_pieces <= TB_LARGEST) {
            uint64_t kings = Bitboard::pieceBB[K] | Bitboard::pieceBB[k];
            uint64_t queens = Bitboard::pieceBB[Q] | Bitboard::pieceBB[q];
            uint64_t rooks = Bitboard::pieceBB[R] | Bitboard::pieceBB[r];
            uint64_t bishops = Bitboard::pieceBB[B] | Bitboard::pieceBB[b];
            uint64_t knights = Bitboard::pieceBB[N] | Bitboard::pieceBB[n];
            uint64_t pawns = Bitboard::pieceBB[P] | Bitboard::pieceBB[p];
            unsigned ep = Bitboard::enpassant != no_sq ? Bitboard::enpassant : 0;
            
            unsigned root_res = tb_probe_root(white_pieces, black_pieces, kings, queens, rooks, bishops, knights, pawns, 0, 0, ep, Bitboard::side == WHITE, NULL);
            if (root_res != TB_RESULT_FAILED) {
                unsigned from = TB_GET_FROM(root_res);
                unsigned to = TB_GET_TO(root_res);
                unsigned promotes = TB_GET_PROMOTES(root_res);
                
                std::string m_str = "";
                m_str += char((from % 8) + 'a');
                m_str += char((from / 8) + '1');
                m_str += char((to % 8) + 'a');
                m_str += char((to / 8) + '1');
                if (promotes) {
                    if (promotes == TB_PROMOTES_KNIGHT) m_str += "n";
                    else if (promotes == TB_PROMOTES_BISHOP) m_str += "b";
                    else if (promotes == TB_PROMOTES_ROOK) m_str += "r";
                    else if (promotes == TB_PROMOTES_QUEEN) m_str += "q";
                }
                
                std::cout << "bestmove " << m_str << std::endl;
                return;
            }
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
                int best_score = -32000;
                Move best_move_this_iteration = 0;
                
                int alpha = -32000;
                int beta = 32000;
                
                // For main line, use aspiration windows
                if (current_depth >= 4 && pv_idx == 0) {
                    alpha = legal_moves[0].score - 50;
                    beta = legal_moves[0].score + 50;
                }

                while (true) {
                    best_score = -32000;
                    best_move_this_iteration = 0;
                    int current_alpha = alpha;

                    for (auto& rm : legal_moves) {
                        Move move = rm.move;
                        if (std::find(excluded_moves.begin(), excluded_moves.end(), move) != excluded_moves.end()) continue;
                        
                        COPY_BOARD;
                        Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;
                        if (!Board::make_move(move, 0, &Evaluation::nnue_stack[1].dirtyPiece)) {
                            RESTORE_BOARD;
                            continue;
                        }
                        
                        int score = -alpha_beta(current_depth - 1, -beta, -current_alpha, 1, true, move);
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
                    
                    if (best_score <= alpha && alpha != -32000) {
                        alpha = -32000;
                        continue;
                    }
                    if (best_score >= beta && beta != 32000) {
                        beta = 32000;
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
                              << " multipv " << (pv_idx + 1) << " ";
                              
                    if (best_score > 30000) {
                        int plies = 31000 - best_score;
                        int moves = (plies + 1) / 2;
                        std::cout << "score mate " << moves;
                    } else if (best_score < -30000) {
                        int plies = 31000 + best_score;
                        int moves = (plies + 1) / 2;
                        std::cout << "score mate " << -moves;
                    } else {
                        std::cout << "score cp " << best_score;
                    }
                    
                    std::cout << " nodes " << nodes 
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

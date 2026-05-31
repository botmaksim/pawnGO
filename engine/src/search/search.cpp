#include "polyglot.h"
#include "search.h"
#include "evaluation.h"
#include "syzygy/tbprobe.h"
#include "movegen.h"
#include "tt.h"
#include "zobrist.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>

extern bool UseOwnBook;
extern std::string BookFile;
extern int game_ply;

namespace Search {
    int max_depth = 6;
    int multi_pv = 1;
    std::atomic<bool> stopped(false);
    std::atomic<int> search_id(0);
    thread_local int thread_search_id = 0;
    
    std::chrono::time_point<std::chrono::steady_clock> search_end_time;
    bool time_managed = false;
    
    std::atomic<long long> nodes(0);
    
    inline bool is_stopped() {
        if (stopped || (thread_search_id != 0 && thread_search_id != search_id.load(std::memory_order_relaxed))) return true;
        if (time_managed && (nodes.load(std::memory_order_relaxed) & 2047) == 0) {
            if (std::chrono::steady_clock::now() >= search_end_time) {
                stopped = true;
                return true;
            }
        }
        return false;
    }


    std::atomic<int> history_table[2][64][64];
    std::atomic<Move> killer_moves[2][200]; // MAX_PLY
    std::atomic<Move> counter_move[64][64];

    int LMR_table[64][64];

    void init_lmr_table() {
        for (int depth = 0; depth < 64; depth++) {
            for (int moves = 0; moves < 64; moves++) {
                if (depth >= 3 && moves >= 4) {
                    LMR_table[depth][moves] = 1.5 + std::log(depth) * std::log(moves) / 1.5;
                } else {
                    LMR_table[depth][moves] = 0;
                }
            }
        }
    }

    void clear_heuristics() {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 200; j++) killer_moves[i][j] = 0;
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

    inline int get_piece_on_square(const BoardState& pos, int sq) {
        if (!(pos.occupancies[2] & (1ULL << sq))) return -1;
        if (pos.side == WHITE) {
            if (pos.pieceBB[p] & (1ULL << sq)) return p;
            if (pos.pieceBB[n] & (1ULL << sq)) return n;
            if (pos.pieceBB[b] & (1ULL << sq)) return b;
            if (pos.pieceBB[r] & (1ULL << sq)) return r;
            if (pos.pieceBB[q] & (1ULL << sq)) return q;
            return p;
        } else {
            if (pos.pieceBB[P] & (1ULL << sq)) return P;
            if (pos.pieceBB[N] & (1ULL << sq)) return N;
            if (pos.pieceBB[B] & (1ULL << sq)) return B;
            if (pos.pieceBB[R] & (1ULL << sq)) return R;
            if (pos.pieceBB[Q] & (1ULL << sq)) return Q;
            return P;
        }
    }

    int score_move(const BoardState& pos, Move move, Move hash_move, int ply, Move prev_move) {
        if (move == hash_move) return 30000;
        if (GET_MOVE_CAPTURE(move)) {
            int victim = get_piece_on_square(pos, GET_MOVE_TARGET(move));
            int victim_val = (victim != -1) ? get_piece_value(victim) : 100; // En passant victim is a pawn
            return 20000 + victim_val * 10 - get_piece_value(GET_MOVE_PIECE(move));
        }
        if (GET_MOVE_PROMOTED(move)) return 19000;
        
        if (ply < 200) {
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
        return history_table[pos.side][src][tgt].load(std::memory_order_relaxed);
    }

    struct MovePicker {
        const BoardState* pos;
        Move hash_move;
        int ply;
        Move prev_move;
        bool only_captures;
        bool in_check;
        
        MoveList move_list;
        int scores[512];
        int index;
        int stage; // 0: captures, 1: quiets, 2: done

        MovePicker(const BoardState& pos, Move hash_move, int ply, Move prev_move, bool only_captures, bool in_check = false) 
            : pos(&pos), hash_move(hash_move), ply(ply), prev_move(prev_move), only_captures(only_captures), in_check(in_check), index(0), stage(0) {
            
            if (in_check) {
                // Generate all evasions immediately
                MoveGen::generate_moves(*this->pos, move_list, false);
                stage = 2; // All moves generated
            } else {
                // Generate captures first
                MoveGen::generate_moves(*this->pos, move_list, true);
                if (only_captures) stage = 2; // No quiets needed
            }
            
            for (int i = 0; i < move_list.count; i++) {
                scores[i] = score_move(*this->pos, move_list.moves[i], hash_move, ply, prev_move);
            }
        }

        Move next_move() {
            if (index >= move_list.count) {
                if (stage == 0) {
                    // Transition to quiets
                    stage = 1;
                    index = 0;
                    MoveList all_moves;
                    MoveGen::generate_moves(*pos, all_moves, false);
                    move_list.count = 0;
                    for (int i = 0; i < all_moves.count; i++) {
                        Move m = all_moves.moves[i];
                        if (!GET_MOVE_CAPTURE(m)) {
                            move_list.add(m);
                            scores[move_list.count - 1] = score_move(*pos, m, hash_move, ply, prev_move);
                        }
                    }
                    if (index >= move_list.count) {
                        stage = 2;
                        return 0;
                    }
                } else {
                    return 0;
                }
            }
            
            int best_score = -100000;
            int best_idx = index;
            
            for (int i = index; i < move_list.count; i++) {
                if (scores[i] > best_score) {
                    best_score = scores[i];
                    best_idx = i;
                }
            }
            
            std::swap(scores[index], scores[best_idx]);
            std::swap(move_list.moves[index], move_list.moves[best_idx]);
            
            return move_list.moves[index++];
        }
    };

    int quiescence(BoardState& pos, int alpha, int beta, int ply) {
        if (is_stopped()) return 0;
        nodes++;

        if (ply >= Evaluation::MAX_PLY - 1) {
            return Evaluation::evaluate_incremental(pos, ply);
        }

        int king_sq = pos.side == WHITE ? Bitboard::lsb(pos.pieceBB[K]) : Bitboard::lsb(pos.pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(pos, king_sq, pos.side ^ 1);

        Move hash_move = 0;
        int tt_score = TT::read_hash_entry(pos.hash_key, alpha, beta, 0, ply, hash_move);
        if (tt_score != -32000) {
            return tt_score;
        }

        int old_alpha = alpha;
        Move best_move = 0;

        if (!in_check) {
            int eval = Evaluation::evaluate_incremental(pos, ply);
            if (eval >= beta) return beta;
            if (eval > alpha) alpha = eval;
            
            // Delta Pruning
            if (eval + 1000 < alpha) return alpha;
        }

        MovePicker move_picker(pos, hash_move, ply, 0, !in_check, in_check);
        int legal_moves = 0;

        while (Move move = move_picker.next_move()) {
            if (!in_check && !GET_MOVE_CAPTURE(move)) continue;

            int tgt = GET_MOVE_TARGET(move);
            int attacker = GET_MOVE_PIECE(move);
            int victim = get_piece_on_square(pos, tgt);

            Board::UndoInfo undo;
            if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY - 1) {
                Evaluation::nnue_stack[ply + 1].accumulator.computedAccumulation = 0;
            }
            if (!Board::make_move(pos, move, 1, &undo, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                continue;
            }
            legal_moves++;
            nodes++;
            int score = -quiescence(pos, -beta, -alpha, ply + 1);
            Board::unmake_move(pos, move, undo);

            if (score >= beta) {
                TT::write_hash_entry(pos.hash_key, beta, 0, ply, hash_flag_beta, move);
                return beta;
            }
            if (score > alpha) {
                alpha = score;
                best_move = move;
            }
        }

        if (in_check && legal_moves == 0) {
            return -31000 + ply;
        }

        int hash_flag = (alpha > old_alpha) ? hash_flag_exact : hash_flag_alpha;
        TT::write_hash_entry(pos.hash_key, alpha, 0, ply, hash_flag, best_move);

        return alpha;
    }

    int alpha_beta(BoardState& pos, int depth, int alpha, int beta, int ply, bool do_null, Move prev_move) {
        if (is_stopped()) return 0;
        
        if (ply >= Evaluation::MAX_PLY - 1) {
            return Evaluation::evaluate_incremental(pos, ply);
        }

        int king_sq = pos.side == WHITE ? Bitboard::lsb(pos.pieceBB[K]) : Bitboard::lsb(pos.pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(pos, king_sq, pos.side ^ 1);
        
        // Check transposition table
        Move hash_move = 0;
        int tt_score = TT::read_hash_entry(pos.hash_key, alpha, beta, depth, ply, hash_move);
        if (tt_score != -32000) {
            return tt_score;
        }

        // Syzygy Tablebase Probe
        uint64_t white_pieces = pos.pieceBB[P] | pos.pieceBB[N] | pos.pieceBB[B] | pos.pieceBB[R] | pos.pieceBB[Q] | pos.pieceBB[K];
        uint64_t black_pieces = pos.pieceBB[p] | pos.pieceBB[n] | pos.pieceBB[b] | pos.pieceBB[r] | pos.pieceBB[q] | pos.pieceBB[k];
        int num_pieces = __builtin_popcountll(white_pieces | black_pieces);

        if (TB_LARGEST > 0 && num_pieces <= TB_LARGEST && !in_check) {
            uint64_t kings = pos.pieceBB[K] | pos.pieceBB[k];
            uint64_t queens = pos.pieceBB[Q] | pos.pieceBB[q];
            uint64_t rooks = pos.pieceBB[R] | pos.pieceBB[r];
            uint64_t bishops = pos.pieceBB[B] | pos.pieceBB[b];
            uint64_t knights = pos.pieceBB[N] | pos.pieceBB[n];
            uint64_t pawns = pos.pieceBB[P] | pos.pieceBB[p];
            unsigned ep = pos.enpassant != no_sq ? pos.enpassant : 0;
            
            unsigned wdl = tb_probe_wdl(white_pieces, black_pieces, kings, queens, rooks, bishops, knights, pawns, 0, 0, ep, pos.side == 0);
            
            if (wdl != TB_RESULT_FAILED) {
                int tb_score = 0;
                if (wdl == TB_WIN) tb_score = 31000 - ply;
                else if (wdl == TB_LOSS) tb_score = -31000 + ply;
                else tb_score = 0;

                // Adjust for mate scores to avoid bounds issues
                if (tb_score > beta) return beta;
                if (tb_score < alpha) return alpha;
                return tb_score;
            }
        }

        if (in_check && ply < max_depth + 10) depth++;

        if (depth <= 0) {
            return quiescence(pos, alpha, beta, ply);
        }
        nodes++;

        int static_eval = 0;
        if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY) {
            static_eval = Evaluation::evaluate_incremental(pos, ply);
        } else {
            static_eval = Evaluation::evaluate(pos);
        }

        bool is_pv = (beta - alpha) > 1;

        // Razoring
        if (!is_pv && !in_check && depth <= 3) {
            int razor_margin = 250 * depth;
            if (static_eval + razor_margin <= alpha) {
                int score = quiescence(pos, alpha - razor_margin, beta, ply);
                if (score <= alpha) return score;
            }
        }

        // Reverse Futility Pruning (Static Null Move Pruning)
        if (!in_check && !is_pv && depth < 5 && ply > 0) {
            int rfp_margin = 80 * depth;
            if (static_eval - rfp_margin >= beta) {
                return static_eval;
            }
        }

        // Null Move Pruning
        if (do_null && !in_check && !is_pv && depth >= 3 && ply > 0 && static_eval >= beta) {
            bool has_pieces = (pos.side == WHITE) ? 
                (pos.pieceBB[N] | pos.pieceBB[B] | pos.pieceBB[R] | pos.pieceBB[Q]) :
                (pos.pieceBB[n] | pos.pieceBB[b] | pos.pieceBB[r] | pos.pieceBB[q]);
                
            if (has_pieces) {
                int old_enpassant = pos.enpassant;
                U64 old_hash = pos.hash_key;
                
                pos.side ^= 1;
                pos.hash_key ^= Zobrist::side_key;
                if (pos.enpassant != no_sq) {
                    pos.hash_key ^= Zobrist::enpassant_keys[pos.enpassant];
                }
                pos.enpassant = no_sq;
                
                if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY - 1) {
                    Evaluation::nnue_stack[ply + 1].accumulator.computedAccumulation = 0;
                    Evaluation::nnue_stack[ply + 1].dirtyPiece.dirtyNum = 0;
                }
                
                int R = 4 + depth / 4;
                int score = -alpha_beta(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, false, 0);
                
                pos.side ^= 1;
                pos.enpassant = old_enpassant;
                pos.hash_key = old_hash;
                
                if (is_stopped()) return 0;
                
                if (score >= beta) return beta;
            }
        }

        if (is_pv && hash_move == 0 && depth >= 4) {
            // Do a shallow search to find a hash move
            int d = depth - 2;
            alpha_beta(pos, d, alpha, beta, ply, do_null, prev_move);
            TT::read_hash_entry(pos.hash_key, alpha, beta, depth, ply, hash_move);
        }

        MovePicker move_picker(pos, hash_move, ply, prev_move, false);

        int legal_moves = 0;
        int old_alpha = alpha;
        Move best_move = 0;
        int hash_flag = hash_flag_alpha;

        bool futility_pruning = !in_check && !is_pv && depth <= 4 && (static_eval + 150 * depth <= alpha);

        while (Move move = move_picker.next_move()) {
            bool is_quiet = !GET_MOVE_CAPTURE(move) && !GET_MOVE_PROMOTED(move);
            
            // Late Move Pruning
            if (!in_check && !is_pv && is_quiet && depth < 4 && legal_moves > (3 + 2 * depth * depth)) {
                continue;
            }

            // Futility Pruning
            if (futility_pruning && is_quiet && legal_moves > 0 && 
                move != killer_moves[0][ply] && 
                move != killer_moves[1][ply]) {
                continue;
            }

            Board::UndoInfo undo;
            if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY - 1) {
                Evaluation::nnue_stack[ply + 1].accumulator.computedAccumulation = 0;
            }
            if (!Board::make_move(pos, move, 0, &undo, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                continue;
            }
            legal_moves++;
            
            int score;
            
            // PVS / LMR
            if (legal_moves == 1) {
                score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, true, move);
            } else {
                int R = 0;
                if (depth >= 3 && legal_moves >= 4 && !in_check && is_quiet) {
                    int d = std::min(depth, 63);
                    int m = std::min(legal_moves, 63);
                    R = LMR_table[d][m];
                    
                    int src = GET_MOVE_SOURCE(move);
                    int tgt = GET_MOVE_TARGET(move);
                    int history_score = history_table[pos.side][src][tgt];
                    
                    // History-based LMR adjustment
                    if (history_score > 4000) R -= 2;
                    else if (history_score > 1000) R -= 1;
                    
                    if (R < 0) R = 0;
                    if (R > depth - 1) R = depth - 1;
                }
                
                score = -alpha_beta(pos, depth - 1 - R, -alpha - 1, -alpha, ply + 1, true, move);
                
                if (score > alpha && R > 0) {
                    score = -alpha_beta(pos, depth - 1, -alpha - 1, -alpha, ply + 1, true, move);
                }
                if (score > alpha && score < beta) {
                    score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, true, move);
                }
            }
            
            Board::unmake_move(pos, move, undo);

            if (is_stopped()) return 0;

            if (score >= beta) {
                TT::write_hash_entry(pos.hash_key, beta, depth, ply, hash_flag_beta, move);
                if (is_quiet) {
                    if (ply < 200) {
                        killer_moves[1][ply].store(killer_moves[0][ply].load(std::memory_order_relaxed), std::memory_order_relaxed);
                        killer_moves[0][ply] = move;
                    }
                    if (prev_move != 0) {
                        counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] = move;
                    }
                    int src = GET_MOVE_SOURCE(move);
                    int tgt = GET_MOVE_TARGET(move);
                    int bonus = depth * depth;
                    history_table[pos.side][src][tgt] += bonus;
                    if (history_table[pos.side][src][tgt] > 10000) {
                        // Normalize history table
                        for (int s = 0; s < 64; s++) {
                            for (int t = 0; t < 64; t++) {
                                history_table[0][s][t].store(history_table[0][s][t].load(std::memory_order_relaxed) / 2, std::memory_order_relaxed);
                                history_table[1][s][t].store(history_table[1][s][t].load(std::memory_order_relaxed) / 2, std::memory_order_relaxed);
                            }
                        }
                    }
                }
                return beta;
            }
            if (score > alpha) {
                hash_flag = hash_flag_exact;
                alpha = score;
                best_move = move;
            }
        }

        if (legal_moves == 0) {
            if (in_check) return -31000 + ply;
            return 0; // Stalemate
        }

        TT::write_hash_entry(pos.hash_key, alpha, depth, ply, hash_flag, best_move);
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
    int root_search_id;

    std::mutex main_search_mtx;

    void helper_thread_loop(int thread_id) {
        while (true) {
            std::unique_lock<std::mutex> lock(search_mtx);
            search_cv.wait(lock, []{ return search_running || exit_threads; });
            if (exit_threads) break;
            
            BoardState pos;
            for(int i=0; i<12; i++) pos.pieceBB[i] = root_pBB[i];
            for(int i=0; i<3; i++) pos.occupancies[i] = root_occ[i];
            pos.side = root_s;
            pos.enpassant = root_e;
            pos.castle = root_c;
            pos.hash_key = Zobrist::generate_hash_key(pos);
            Search::thread_search_id = root_search_id;
            lock.unlock();
            
            Evaluation::allocate_nnue_stack();
            Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;
            if (Evaluation::use_nnue) {
                Evaluation::evaluate_incremental(pos, 0);
            }
            
            // Helper does its own iterative deepening until stopped or max_depth
            for (int current_depth = 1 + (thread_id % 4); current_depth <= root_max_depth; current_depth++) {
                if (is_stopped() || !search_running) break;
                
                MovePicker move_picker(pos, 0, 0, 0, false);
                
                int first_move = true;
                while (Move move = move_picker.next_move()) {
                    Board::UndoInfo undo;
                    Evaluation::nnue_stack[1].accumulator.computedAccumulation = 0;
                    if (!Board::make_move(pos, move, 0, &undo, &Evaluation::nnue_stack[1].dirtyPiece)) {
                        continue;
                    }
                    
                    int score;
                    if (first_move) {
                        score = -alpha_beta(pos, current_depth - 1, -32000, 32000, 1, true, move);
                        first_move = false;
                    } else {
                        score = -alpha_beta(pos, current_depth - 1, -32000, 32000, 1, true, move);
                    }
                    
                    Board::unmake_move(pos, move, undo);
                    if (is_stopped() || !search_running) break;
                }
            }
            
            // Sleep until next launch
            lock.lock();
            while (search_running && !exit_threads) {
                // Wait for the main thread to set search_running = false and notify
                search_cv.wait(lock);
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

    std::string extract_pv(BoardState& pos, int max_depth_val) {
        std::string pv_str = "";
        U64 current_hash = pos.hash_key;
        int depth = 0;
        
        U64 pBB[12];
        U64 occ[3];
        for(int i=0; i<12; i++) pBB[i] = pos.pieceBB[i];
        for(int i=0; i<3; i++) occ[i] = pos.occupancies[i];
        int s = pos.side;
        int e = pos.enpassant;
        int c = pos.castle;
        
        std::vector<Board::UndoInfo> undos;
        std::vector<Move> moves;
        std::vector<U64> hashes_seen;
        hashes_seen.push_back(current_hash);
        
        while (depth < max_depth_val) {
            Move best_move = 0;
            TT::read_hash_entry(current_hash, -32000, 32000, 0, 0, best_move);
            if (best_move == 0) break;
            
            MoveList move_list;
            MoveGen::generate_moves(pos, move_list, false);
            bool is_pseudo_legal = false;
            for (int i = 0; i < move_list.count; i++) {
                if (move_list.moves[i] == best_move) {
                    is_pseudo_legal = true;
                    break;
                }
            }
            if (!is_pseudo_legal) break;
            
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

            Board::UndoInfo undo;
            Board::make_move(pos, best_move, 0, &undo, nullptr);
            undos.push_back(undo);
            moves.push_back(best_move);
            
            current_hash = pos.hash_key;
            if (std::find(hashes_seen.begin(), hashes_seen.end(), current_hash) != hashes_seen.end()) break;
            hashes_seen.push_back(current_hash);
            
            depth++;
        }

        for (int i = (int)moves.size() - 1; i >= 0; i--) {
            Board::unmake_move(pos, moves[i], undos[i]);
        }

        for(int i=0; i<12; i++) pos.pieceBB[i] = pBB[i];
        for(int i=0; i<3; i++) pos.occupancies[i] = occ[i];
        pos.side = s;
        pos.enpassant = e;
        pos.castle = c;
        pos.hash_key = Zobrist::generate_hash_key(pos);
        
        return pv_str;
    }

    struct RootMove {
        Move move;
        int score;
    };

    void search_position(BoardState& pos, int depth, int current_search_id, long long time_for_move_ms) {
        std::lock_guard<std::mutex> main_lock(main_search_mtx);

        if (current_search_id != -1 && current_search_id != search_id.load(std::memory_order_relaxed)) {
            return; // Superseded before it even started!
        }

        if (time_for_move_ms != -1) {
            time_managed = true;
            search_end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_for_move_ms);
        } else {
            time_managed = false;
        }

        // We do NOT parse current_fen here, because main.cpp already sets up the board and applies moves!
        clear_heuristics();
        
        auto move_to_string = [](Move m) -> std::string {
            int s = GET_MOVE_SOURCE(m);
            int t = GET_MOVE_TARGET(m);
            int p = GET_MOVE_PROMOTED(m);
            std::string res = "";
            res += char((s % 8) + 'a');
            res += char((s / 8) + '1');
            res += char((t % 8) + 'a');
            res += char((t / 8) + '1');
            if (p) {
                int p_type = p % 6;
                if (p_type == 1) res += "n";
                if (p_type == 2) res += "b";
                if (p_type == 3) res += "r";
                if (p_type == 4) res += "q";
            }
            return res;
        };
        
        // Book moves are now handled in the main search loop to support MultiPV
        
        max_depth = depth;
        stopped = false;
        if (current_search_id != -1) {
            search_id.store(current_search_id, std::memory_order_relaxed);
        }
        nodes = 0;
        
        Evaluation::allocate_nnue_stack();
        Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;
        if (Evaluation::use_nnue) {
            Evaluation::evaluate_incremental(pos, 0); // Initialize root accumulator properly!
        }

        MovePicker move_picker(pos, 0, 0, 0, false);

        std::vector<RootMove> legal_moves;
        while (Move move = move_picker.next_move()) {
            Board::UndoInfo undo;
            if (Board::make_move(pos, move, 0, &undo, nullptr)) {
                legal_moves.push_back({move, 0});
                Board::unmake_move(pos, move, undo);
            }
        }

        if (legal_moves.empty()) {
            if (current_search_id == -1 || current_search_id == search_id.load(std::memory_order_relaxed)) {
                printf("bestmove 0000\n");
                fflush(stdout);
            }
            return;
        }

        std::vector<Move> current_book_moves;
        if (UseOwnBook && !BookFile.empty()) {
            auto b_moves = Polyglot::get_all_book_moves(pos, BookFile);
            for (auto& bm : b_moves) current_book_moves.push_back(bm.first);
        }

        uint64_t white_pieces = pos.pieceBB[P] | pos.pieceBB[N] | pos.pieceBB[B] | pos.pieceBB[R] | pos.pieceBB[Q] | pos.pieceBB[K];
        uint64_t black_pieces = pos.pieceBB[p] | pos.pieceBB[n] | pos.pieceBB[b] | pos.pieceBB[r] | pos.pieceBB[q] | pos.pieceBB[k];
        int num_pieces = __builtin_popcountll(white_pieces | black_pieces);

        if (TB_LARGEST > 0 && num_pieces <= TB_LARGEST) {
            uint64_t kings = pos.pieceBB[K] | pos.pieceBB[k];
            uint64_t queens = pos.pieceBB[Q] | pos.pieceBB[q];
            uint64_t rooks = pos.pieceBB[R] | pos.pieceBB[r];
            uint64_t bishops = pos.pieceBB[B] | pos.pieceBB[b];
            uint64_t knights = pos.pieceBB[N] | pos.pieceBB[n];
            uint64_t pawns = pos.pieceBB[P] | pos.pieceBB[p];
            unsigned ep = pos.enpassant != no_sq ? pos.enpassant : 0;
            
            unsigned root_res = tb_probe_root(white_pieces, black_pieces, kings, queens, rooks, bishops, knights, pawns, 0, 0, ep, pos.side == WHITE, NULL);
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
                
                printf("bestmove %s\n", m_str.c_str());
                fflush(stdout);
                return;
            }
        }

        for(int i=0; i<12; i++) root_pBB[i] = pos.pieceBB[i];
        for(int i=0; i<3; i++) root_occ[i] = pos.occupancies[i];
        root_s = pos.side;
        root_e = pos.enpassant;
        root_c = pos.castle;
        
        // Wake up threads
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            root_max_depth = depth;
            root_search_id = current_search_id;
            search_running = true;
        }
        search_cv.notify_all();

        int actual_multi_pv = std::min(multi_pv, (int)legal_moves.size());
        std::vector<Move> global_best_moves(actual_multi_pv, 0);

        for (int current_depth = 1; current_depth <= depth; current_depth++) {
            std::vector<Move> excluded_moves;
            
            // Re-sort legal moves: book moves first, then by score
            std::sort(legal_moves.begin(), legal_moves.end(), [&](const RootMove& a, const RootMove& b) {
                bool a_is_book = false, b_is_book = false;
                for (auto& bm : current_book_moves) {
                    if (bm == a.move) a_is_book = true;
                    if (bm == b.move) b_is_book = true;
                }
                if (a_is_book && !b_is_book) return true;
                if (!a_is_book && b_is_book) return false;
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

                // Fast path for book moves
                bool is_book_slot = false;
                Move book_move = 0;
                if (pv_idx < current_book_moves.size()) {
                    is_book_slot = true;
                }

                while (true) {
                    best_score = -32000;
                    best_move_this_iteration = 0;
                    int current_alpha = alpha;

                    for (auto& rm : legal_moves) {
                        Move move = rm.move;
                        if (std::find(excluded_moves.begin(), excluded_moves.end(), move) != excluded_moves.end()) continue;
                        
                        Board::UndoInfo undo;
                        if (Evaluation::use_nnue) {
                            Evaluation::nnue_stack[1].accumulator.computedAccumulation = 0;
                        }
                        if (!Board::make_move(pos, move, 0, &undo, Evaluation::use_nnue ? &Evaluation::nnue_stack[1].dirtyPiece : nullptr)) {
                            continue;
                        }

                        nodes++;
                        int score = -alpha_beta(pos, current_depth - 1, -beta, -current_alpha, 1, false, move);
                        Board::unmake_move(pos, move, undo);
                        
                        if (is_stopped()) break;
                        
                        if (pv_idx == 0) rm.score = score;
                        
                        if (score > best_score) {
                            best_score = score;
                            best_move_this_iteration = move;
                        }
                        if (score > current_alpha) {
                            current_alpha = score;
                        }
                    }
                    
                    if (is_stopped()) break;
                    
                    if (current_depth >= 4 && pv_idx == 0) {
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
                    break;
                }
                
                if (is_stopped()) break;
                
                if (best_move_this_iteration != 0) {
                    excluded_moves.push_back(best_move_this_iteration);
                    global_best_moves[pv_idx] = best_move_this_iteration;
                    
                    TT::write_hash_entry(pos.hash_key, best_score, current_depth, 0, hash_flag_exact, best_move_this_iteration);
                    
                    std::string pv_line = extract_pv(pos, current_depth);
                    
                    if (best_score > 30000) {
                        int plies = 31000 - best_score;
                        int moves = (plies + 1) / 2;
                        printf("info depth %d multipv %d score mate %d%s nodes %lld pv %s\n", 
                               current_depth, (pv_idx + 1), moves, (is_book_slot ? " book 1" : ""), (long long)nodes.load(std::memory_order_relaxed), pv_line.c_str());
                    } else if (best_score < -30000) {
                        int plies = 31000 + best_score;
                        int moves = (plies + 1) / 2;
                        printf("info depth %d multipv %d score mate %d%s nodes %lld pv %s\n", 
                               current_depth, (pv_idx + 1), -moves, (is_book_slot ? " book 1" : ""), (long long)nodes.load(std::memory_order_relaxed), pv_line.c_str());
                    } else {
                        printf("info depth %d multipv %d score cp %d%s nodes %lld pv %s\n", 
                               current_depth, (pv_idx + 1), best_score, (is_book_slot ? " book 1" : ""), (long long)nodes.load(std::memory_order_relaxed), pv_line.c_str());
                    }
                }
            }
            if (is_stopped()) break;
        }

        // Sleep threads
        {
            std::lock_guard<std::mutex> lock(search_mtx);
            search_running = false;
        }
        search_cv.notify_all();

        Move best_final_move = global_best_moves[0];
        if (best_final_move == 0 && legal_moves.size() > 0) best_final_move = legal_moves[0].move;

        if (best_final_move != 0) {
            if (current_search_id != -1 && current_search_id != search_id.load(std::memory_order_relaxed)) {
                return; // Superseded by a newer search, do not print bestmove
            }
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
        
        printf("bestmove %s\n", move_str.c_str());
        fflush(stdout);
    }
}
}

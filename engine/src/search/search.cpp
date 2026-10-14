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
#include <cstring>

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
        if (time_managed) {
            static thread_local int time_check_counter = 0;
            time_check_counter++;
            if (time_check_counter > 1024) {
                time_check_counter = 0;
                if (std::chrono::steady_clock::now() >= search_end_time) {
                    stopped = true;
                    return true;
                }
            }
        }
        return false;
    }


    U64 game_history[MAX_GAME_MOVES];
    int game_history_len = 0;
    thread_local U64 search_history[Evaluation::MAX_PLY];
    thread_local int static_eval_history[Evaluation::MAX_PLY];
    thread_local Move move_history[Evaluation::MAX_PLY];

    bool is_repetition(const BoardState& pos, int ply) {
        int L = game_history_len;
        int curr_idx = L + ply;
        int count = 0;

        for (int step = 2; step <= pos.rule50 && curr_idx - step >= 0; step += 2) {
            int prev_idx = curr_idx - step;
            U64 past_hash = (prev_idx >= L) ? search_history[prev_idx - L] : game_history[prev_idx];
            if (past_hash == pos.hash_key) {
                if (prev_idx >= L) return true;
                count++;
                if (count >= (ply > 0 ? 1 : 2)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool is_draw(const BoardState& pos, int ply) {
        if (pos.rule50 >= 100) return true;
        return is_repetition(pos, ply);
    }

    int history_table[2][64][64];
    Move killer_moves[2][512]; // MAX_PLY
    Move counter_move[64][64];
    int16_t conthist_1ply[12][64][64];
    int16_t conthist_2ply[12][64][64];
    int16_t capture_history[12][64][12];

    int LMR_table[64][64];

    void init_lmr_table() {
        for (int depth = 0; depth < 64; depth++) {
            for (int moves = 0; moves < 64; moves++) {
                if (depth >= 2 && moves >= 2) {
                    LMR_table[depth][moves] = (int)(0.75 + std::log(depth) * std::log(moves) / 2.25);
                } else {
                    LMR_table[depth][moves] = 0;
                }
            }
        }
    }

    void clear_heuristics() {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 512; j++) killer_moves[i][j] = 0;
            for (int j = 0; j < 64; j++) {
                for (int k = 0; k < 64; k++) history_table[i][j][k] = 0;
            }
        }
        for (int j = 0; j < 64; j++) {
            for (int k = 0; k < 64; k++) counter_move[j][k] = 0;
        }
        std::memset(conthist_1ply, 0, sizeof(conthist_1ply));
        std::memset(conthist_2ply, 0, sizeof(conthist_2ply));
        std::memset(capture_history, 0, sizeof(capture_history));
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
        if (!(pos.occupancies[BOTH] & (1ULL << sq))) return -1;
        for (int piece = P; piece <= k; piece++) {
            if (pos.pieceBB[piece] & (1ULL << sq)) return piece;
        }
        return -1;
    }

    constexpr int see_piece_values[13] = {
        100, 320, 330, 500, 900, 20000, // P, N, B, R, Q, K
        100, 320, 330, 500, 900, 20000, // p, n, b, r, q, k
        0
    };

    inline U64 attacks_to(const BoardState& pos, int sq, U64 occ) {
        return (MoveGen::pawn_attacks[BLACK][sq] & pos.pieceBB[P]) |
               (MoveGen::pawn_attacks[WHITE][sq] & pos.pieceBB[p]) |
               (MoveGen::knight_attacks[sq] & (pos.pieceBB[N] | pos.pieceBB[n])) |
               (MoveGen::get_bishop_attacks(sq, occ) & (pos.pieceBB[B] | pos.pieceBB[b] | pos.pieceBB[Q] | pos.pieceBB[q])) |
               (MoveGen::get_rook_attacks(sq, occ) & (pos.pieceBB[R] | pos.pieceBB[r] | pos.pieceBB[Q] | pos.pieceBB[q])) |
               (MoveGen::king_attacks[sq] & (pos.pieceBB[K] | pos.pieceBB[k]));
    }

    inline U64 consider_xrays(const BoardState& pos, int sq, U64 occ) {
        return (MoveGen::get_bishop_attacks(sq, occ) & (pos.pieceBB[B] | pos.pieceBB[b] | pos.pieceBB[Q] | pos.pieceBB[q])) |
               (MoveGen::get_rook_attacks(sq, occ) & (pos.pieceBB[R] | pos.pieceBB[r] | pos.pieceBB[Q] | pos.pieceBB[q]));
    }

    inline U64 get_least_valuable_piece(const BoardState& pos, U64 attadef, int side, int& piece) {
        int offset = (side == WHITE) ? 0 : 6;
        for (int p_idx = P; p_idx <= K; p_idx++) {
            U64 subset = attadef & pos.pieceBB[p_idx + offset];
            if (subset) {
                piece = p_idx + offset;
                return subset & -subset;
            }
        }
        return 0ULL;
    }

    int see(const BoardState& pos, Move move) {
        int from = GET_MOVE_SOURCE(move);
        int to = GET_MOVE_TARGET(move);
        int piece = GET_MOVE_PIECE(move);
        int promoted = GET_MOVE_PROMOTED(move);
        int is_ep = GET_MOVE_ENPASSANT(move);

        int gain[32];
        int d = 0;

        int target_piece = -1;
        if (is_ep) {
            gain[0] = 100;
        } else {
            target_piece = get_piece_on_square(pos, to);
            gain[0] = (target_piece != -1) ? see_piece_values[target_piece] : 0;
        }

        int aPiece = piece;
        if (promoted) {
            gain[0] += (see_piece_values[promoted] - 100);
            aPiece = promoted;
        }

        U64 fromSet = 1ULL << from;
        U64 occ = pos.occupancies[BOTH];
        if (is_ep) {
            int ep_sq = (pos.side == WHITE) ? to - 8 : to + 8;
            occ ^= (1ULL << ep_sq);
        }

        U64 attadef = attacks_to(pos, to, occ);
        int current_side = pos.side;

        do {
            d++;
            gain[d] = see_piece_values[aPiece] - gain[d - 1];

            attadef ^= fromSet;
            occ ^= fromSet;

            // X-rays
            attadef |= (consider_xrays(pos, to, occ) & occ);

            current_side ^= 1;
            fromSet = get_least_valuable_piece(pos, attadef, current_side, aPiece);

            // King cannot move into check
            if ((aPiece == K || aPiece == k) && fromSet) {
                int opp = current_side ^ 1;
                if (attadef & pos.occupancies[opp]) {
                    fromSet = 0;
                }
            }
        } while (fromSet && d < 31);

        while (--d) {
            gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
        }

        return gain[0];
    }

    bool see_ge(const BoardState& pos, Move move, int threshold) {
        if (threshold == 0 && !GET_MOVE_PROMOTED(move) && !GET_MOVE_ENPASSANT(move)) {
            int tgt = GET_MOVE_TARGET(move);
            int victim = get_piece_on_square(pos, tgt);
            if (victim != -1) {
                int victim_val = see_piece_values[victim];
                int attacker_val = see_piece_values[GET_MOVE_PIECE(move)];
                if (victim_val >= attacker_val) return true;
            }
        }
        return see(pos, move) >= threshold;
    }

    int score_move(const BoardState& pos, Move move, Move hash_move, int ply, Move prev_move) {
        if (move == hash_move) return 30000;
        
        int src = GET_MOVE_SOURCE(move);
        int tgt = GET_MOVE_TARGET(move);
        int piece = GET_MOVE_PIECE(move);

        if (GET_MOVE_CAPTURE(move)) {
            int victim = get_piece_on_square(pos, tgt);
            int victim_val = (victim != -1) ? get_piece_value(victim) : 100; // En passant victim is a pawn
            int attacker_val = get_piece_value(piece);
            int cap_hist = (victim != -1) ? (int)capture_history[piece][tgt][victim] : 0;
            
            if (see_ge(pos, move, 0)) {
                return 22000 + victim_val * 10 - attacker_val + (cap_hist / 32);
            } else {
                return 5000 + see(pos, move) + (cap_hist / 64);
            }
        }
        if (GET_MOVE_PROMOTED(move)) return 20000;
        
        if (ply < 512) {
            if (killer_moves[0][ply] == move) return 18000;
            if (killer_moves[1][ply] == move) return 17000;
        }

        if (prev_move != 0) {
            if (counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] == move) {
                return 16000;
            }
        }

        int score = history_table[pos.side][src][tgt];

        if (prev_move != 0) {
            int prev_piece = GET_MOVE_PIECE(prev_move);
            int prev_tgt = GET_MOVE_TARGET(prev_move);
            score += conthist_1ply[prev_piece][prev_tgt][tgt];
        }

        if (ply >= 2 && move_history[ply - 2] != 0) {
            Move prev2 = move_history[ply - 2];
            int prev2_piece = GET_MOVE_PIECE(prev2);
            int prev2_tgt = GET_MOVE_TARGET(prev2);
            score += conthist_2ply[prev2_piece][prev2_tgt][tgt] / 2;
        }

        return score;
    }

    bool is_pseudo_legal(const BoardState& pos, Move move) {
        if (move == 0) return false;
        int from = GET_MOVE_SOURCE(move);
        int to = GET_MOVE_TARGET(move);
        int piece = GET_MOVE_PIECE(move);
        int side = pos.side;
        int opp = side ^ 1;

        if (side == WHITE && (piece < P || piece > K)) return false;
        if (side == BLACK && (piece < p || piece > k)) return false;

        if (!(pos.pieceBB[piece] & (1ULL << from))) return false;

        // Destination square must not contain a friendly piece
        if (pos.occupancies[side] & (1ULL << to)) return false;

        bool is_cap = GET_MOVE_CAPTURE(move) != 0;
        bool is_ep = GET_MOVE_ENPASSANT(move) != 0;

        if (is_cap) {
            if (is_ep) {
                if (to != pos.enpassant) return false;
                if (pos.occupancies[BOTH] & (1ULL << to)) return false;
            } else {
                if (!(pos.occupancies[opp] & (1ULL << to))) return false;
            }
        } else {
            // Quiet move: destination MUST be empty
            if (pos.occupancies[BOTH] & (1ULL << to)) return false;
        }

        int piece_type = (side == WHITE) ? piece : (piece - 6);
        if (piece_type == P) {
            int diff = to - from;
            int promoted = GET_MOVE_PROMOTED(move);
            if (side == WHITE) {
                if (to >= a8 && to <= h8) {
                    if (promoted != Q && promoted != R && promoted != B && promoted != N) return false;
                } else {
                    if (promoted != 0) return false;
                }

                if (is_cap) {
                    return (MoveGen::pawn_attacks[WHITE][from] & (1ULL << to)) != 0;
                } else {
                    if (diff == 8) return true;
                    if (diff == 16 && (from >= a2 && from <= h2) &&
                        !(pos.occupancies[BOTH] & (1ULL << (from + 8)))) return true;
                    return false;
                }
            } else {
                if (to >= a1 && to <= h1) {
                    if (promoted != q && promoted != r && promoted != b && promoted != n) return false;
                } else {
                    if (promoted != 0) return false;
                }

                if (is_cap) {
                    return (MoveGen::pawn_attacks[BLACK][from] & (1ULL << to)) != 0;
                } else {
                    if (diff == -8) return true;
                    if (diff == -16 && (from >= a7 && from <= h7) &&
                        !(pos.occupancies[BOTH] & (1ULL << (from - 8)))) return true;
                    return false;
                }
            }
        } else if (piece_type == N) {
            return (MoveGen::knight_attacks[from] & (1ULL << to)) != 0;
        } else if (piece_type == B) {
            return (MoveGen::get_bishop_attacks(from, pos.occupancies[BOTH]) & (1ULL << to)) != 0;
        } else if (piece_type == R) {
            return (MoveGen::get_rook_attacks(from, pos.occupancies[BOTH]) & (1ULL << to)) != 0;
        } else if (piece_type == Q) {
            return (MoveGen::get_queen_attacks(from, pos.occupancies[BOTH]) & (1ULL << to)) != 0;
        } else if (piece_type == K) {
            if (GET_MOVE_CASTLING(move)) {
                MoveList ml;
                MoveGen::generate_moves(pos, ml, false);
                for (int i = 0; i < ml.count; i++) {
                    if (ml.moves[i] == move) return true;
                }
                return false;
            }
            return (MoveGen::king_attacks[from] & (1ULL << to)) != 0;
        }
        return false;
    }

    struct MovePicker {
        const BoardState* pos;
        Move hash_move;
        int ply;
        Move prev_move;
        bool only_captures;
        bool in_check;

        enum Stage {
            STAGE_HASH,
            STAGE_GEN_CAPTURES,
            STAGE_GOOD_CAPTURES,
            STAGE_KILLER_1,
            STAGE_KILLER_2,
            STAGE_COUNTER_MOVE,
            STAGE_GEN_QUIETS,
            STAGE_QUIET_MOVES,
            STAGE_BAD_CAPTURES,
            STAGE_DONE
        } stage;

        MoveList move_list;
        int scores[256];
        int index;

        Move bad_captures[64];
        int bad_scores[64];
        int bad_count;
        int bad_index;

        Move killer1;
        Move killer2;
        Move counter;

        MovePicker(const BoardState& pos, Move hash_move, int ply, Move prev_move, bool only_captures, bool in_check = false) 
            : pos(&pos), hash_move(hash_move), ply(ply), prev_move(prev_move), only_captures(only_captures), in_check(in_check),
              index(0), bad_count(0), bad_index(0) {
            
            if (in_check) {
                MoveGen::generate_moves(*this->pos, move_list, false);
                for (int i = 0; i < move_list.count; i++) {
                    scores[i] = score_move(*this->pos, move_list.moves[i], hash_move, ply, prev_move);
                }
                stage = STAGE_DONE;
            } else {
                killer1 = (ply < 512) ? killer_moves[0][ply] : 0;
                killer2 = (ply < 512) ? killer_moves[1][ply] : 0;
                counter = (prev_move != 0) ? counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] : 0;

                bool hash_valid = (hash_move != 0 && is_pseudo_legal(pos, hash_move) && (!only_captures || GET_MOVE_CAPTURE(hash_move)));
                stage = hash_valid ? STAGE_HASH : STAGE_GEN_CAPTURES;
            }
        }

        Move next_move() {
            if (in_check) {
                if (index >= move_list.count) return 0;
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

            switch (stage) {
                case STAGE_HASH: {
                    stage = STAGE_GEN_CAPTURES;
                    return hash_move;
                }

                case STAGE_GEN_CAPTURES: {
                    MoveList caps;
                    MoveGen::generate_moves(*pos, caps, true);
                    move_list.count = 0;
                    bad_count = 0;
                    bad_index = 0;
                    index = 0;

                    for (int i = 0; i < caps.count; i++) {
                        Move m = caps.moves[i];
                        if (m == hash_move) continue;

                        int score = score_move(*pos, m, hash_move, ply, prev_move);
                        if (see_ge(*pos, m, 0)) {
                            move_list.add(m);
                            scores[move_list.count - 1] = score;
                        } else {
                            if (bad_count < 64) {
                                bad_captures[bad_count] = m;
                                bad_scores[bad_count] = score;
                                bad_count++;
                            }
                        }
                    }
                    stage = STAGE_GOOD_CAPTURES;
                    // Fall through
                }

                case STAGE_GOOD_CAPTURES: {
                    if (index < move_list.count) {
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

                    if (only_captures) {
                        stage = STAGE_DONE;
                        return 0;
                    }

                    stage = STAGE_KILLER_1;
                    // Fall through
                }

                case STAGE_KILLER_1: {
                    stage = STAGE_KILLER_2;
                    if (killer1 != 0 && killer1 != hash_move && !GET_MOVE_CAPTURE(killer1) && is_pseudo_legal(*pos, killer1)) {
                        return killer1;
                    }
                    // Fall through
                }

                case STAGE_KILLER_2: {
                    stage = STAGE_COUNTER_MOVE;
                    if (killer2 != 0 && killer2 != hash_move && killer2 != killer1 && !GET_MOVE_CAPTURE(killer2) && is_pseudo_legal(*pos, killer2)) {
                        return killer2;
                    }
                    // Fall through
                }

                case STAGE_COUNTER_MOVE: {
                    stage = STAGE_GEN_QUIETS;
                    if (counter != 0 && counter != hash_move && counter != killer1 && counter != killer2 && !GET_MOVE_CAPTURE(counter) && is_pseudo_legal(*pos, counter)) {
                        return counter;
                    }
                    // Fall through
                }

                case STAGE_GEN_QUIETS: {
                    MoveList all_moves;
                    MoveGen::generate_moves(*pos, all_moves, false);
                    move_list.count = 0;
                    index = 0;

                    for (int i = 0; i < all_moves.count; i++) {
                        Move m = all_moves.moves[i];
                        if (!GET_MOVE_CAPTURE(m) && m != hash_move && m != killer1 && m != killer2 && m != counter) {
                            move_list.add(m);
                            scores[move_list.count - 1] = score_move(*pos, m, hash_move, ply, prev_move);
                        }
                    }
                    stage = STAGE_QUIET_MOVES;
                    // Fall through
                }

                case STAGE_QUIET_MOVES: {
                    if (index < move_list.count) {
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

                    stage = STAGE_BAD_CAPTURES;
                    // Fall through
                }

                case STAGE_BAD_CAPTURES: {
                    if (bad_index < bad_count) {
                        int best_score = -100000;
                        int best_idx = bad_index;
                        for (int i = bad_index; i < bad_count; i++) {
                            if (bad_scores[i] > best_score) {
                                best_score = bad_scores[i];
                                best_idx = i;
                            }
                        }
                        std::swap(bad_scores[bad_index], bad_scores[best_idx]);
                        std::swap(bad_captures[bad_index], bad_captures[best_idx]);
                        return bad_captures[bad_index++];
                    }

                    stage = STAGE_DONE;
                    return 0;
                }

                case STAGE_DONE:
                default:
                    return 0;
            }
        }
    };

    int quiescence(BoardState& pos, int alpha, int beta, int ply) {
        if (is_stopped()) return 0;
        nodes++;

        if (ply >= Evaluation::MAX_PLY - 1) {
            return Evaluation::evaluate_incremental(pos, ply);
        }

        if (pos.rule50 >= 100) {
            return 0;
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
            if (!in_check && !GET_MOVE_CAPTURE(move) && !GET_MOVE_PROMOTED(move)) continue;

            // SEE Pruning: prune losing captures in quiescence (never prune promotions)
            if (!in_check && !GET_MOVE_PROMOTED(move) && !see_ge(pos, move, 0)) continue;

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

        if (ply > 0 && is_draw(pos, ply)) {
            return 0;
        }

        if (ply < Evaluation::MAX_PLY) {
            search_history[ply] = pos.hash_key;
        }

        int king_sq = pos.side == WHITE ? Bitboard::lsb(pos.pieceBB[K]) : Bitboard::lsb(pos.pieceBB[k]);
        bool in_check = MoveGen::is_square_attacked(pos, king_sq, pos.side ^ 1);
        
        if (in_check && ply < max_depth + 10) depth++;

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

        if (ply < Evaluation::MAX_PLY) {
            static_eval_history[ply] = static_eval;
        }

        bool improving = !in_check && ply >= 2 && static_eval >= static_eval_history[ply - 2];
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
            int rfp_margin = (improving ? 80 : 120) * depth;
            if (static_eval - rfp_margin >= beta) {
                return beta;
            }
        }

        // Null Move Pruning
        if (do_null && !in_check && !is_pv && depth >= 3 && ply > 0 && static_eval >= beta + (improving ? 0 : 50)) {
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
                
                int R = 4 + depth / 4 + (improving ? 1 : 0);
                int score = -alpha_beta(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, false, 0);
                
                pos.side ^= 1;
                pos.enpassant = old_enpassant;
                pos.hash_key = old_hash;
                
                if (is_stopped()) return 0;
                
                if (score >= beta) return beta;
            }
        }

        // Internal Iterative Reduction (IIR)
        if (hash_move == 0 && depth >= 4 && !is_pv) {
            depth--;
        }

        // Singular Extensions
        int singular_extension = 0;
        if (depth >= 6 && hash_move != 0 && !is_pv && ply > 0 && ply < max_depth + 10) {
            int tt_score_val, tt_depth_val, tt_flag_val;
            Move tt_probe_move;
            if (TT::probe_entry(pos.hash_key, tt_score_val, tt_depth_val, tt_flag_val, tt_probe_move, ply)) {
                if (tt_probe_move == hash_move && tt_depth_val >= depth - 3 &&
                    (tt_flag_val == hash_flag_exact || tt_flag_val == hash_flag_beta) &&
                    std::abs(tt_score_val) < 19000) {
                    
                    int s_beta = tt_score_val - 2 * depth;
                    int s_depth = (depth - 1) / 2;
                    
                    MovePicker s_picker(pos, 0, ply, prev_move, false);
                    int s_moves = 0;
                    int s_best = -32000;
                    
                    while (Move m = s_picker.next_move()) {
                        if (m == hash_move) continue;
                        
                        Board::UndoInfo u;
                        if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY - 1) {
                            Evaluation::nnue_stack[ply + 1].accumulator.computedAccumulation = 0;
                        }
                        if (!Board::make_move(pos, m, 0, &u, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) continue;
                        s_moves++;
                        
                        int s_score = -alpha_beta(pos, s_depth, -s_beta - 1, -s_beta, ply + 1, false, m);
                        Board::unmake_move(pos, m, u);
                        
                        if (s_score > s_best) s_best = s_score;
                        if (s_best >= s_beta) break;
                    }
                    
                    if (s_moves > 0 && s_best < s_beta) {
                        singular_extension = (s_best < s_beta - 25) ? 2 : 1;
                    } else if (s_best >= s_beta && tt_score_val >= beta) {
                        return beta;
                    }
                }
            }
        }

        MovePicker move_picker(pos, hash_move, ply, prev_move, false);

        Move quiets_tried[64];
        int quiets_count = 0;
        Move captures_tried[32];
        int captures_count = 0;

        int legal_moves = 0;
        int old_alpha = alpha;
        Move best_move = 0;
        int hash_flag = hash_flag_alpha;

        bool futility_pruning = !in_check && !is_pv && depth <= 4 && (static_eval + 150 * depth <= alpha);

        while (Move move = move_picker.next_move()) {
            bool is_quiet = !GET_MOVE_CAPTURE(move) && !GET_MOVE_PROMOTED(move);
            
            bool is_advanced_pawn = false;
            int move_piece = GET_MOVE_PIECE(move);
            int move_tgt = GET_MOVE_TARGET(move);
            if (move_piece == P && move_tgt >= a6 && move_tgt <= h7) is_advanced_pawn = true;
            else if (move_piece == p && move_tgt >= a2 && move_tgt <= h3) is_advanced_pawn = true;

            // Late Move Pruning (exempt advanced pawns)
            if (!in_check && !is_pv && is_quiet && !is_advanced_pawn && depth < 4 && legal_moves > (3 + 2 * depth * depth)) {
                continue;
            }

            // Futility Pruning (exempt advanced pawns)
            if (futility_pruning && is_quiet && !is_advanced_pawn && legal_moves > 0 && 
                move != killer_moves[0][ply] && 
                move != killer_moves[1][ply]) {
                continue;
            }

            // Quiet SEE Pruning (exempt advanced pawns)
            if (!in_check && !is_pv && depth <= 3 && is_quiet && !is_advanced_pawn && legal_moves > 0) {
                if (move != killer_moves[0][ply] && move != killer_moves[1][ply]) {
                    if (!see_ge(pos, move, -50 * depth)) {
                        continue;
                    }
                }
            }

            // Capture SEE Pruning: prune losing captures at shallow depths
            if (!in_check && !is_pv && depth <= 5 && !is_quiet && !GET_MOVE_PROMOTED(move) && legal_moves > 0) {
                if (!see_ge(pos, move, -90 * depth)) {
                    continue;
                }
            }

            // History Pruning
            if (!in_check && !is_pv && depth <= 3 && is_quiet && legal_moves > 3) {
                int s_src = GET_MOVE_SOURCE(move);
                int s_tgt = GET_MOVE_TARGET(move);
                if (history_table[pos.side][s_src][s_tgt] < -2000 * depth) {
                    continue;
                }
            }

            int R = 0;
            if (depth >= 3 && legal_moves >= 2 && !in_check) {
                if (is_quiet) {
                    int d = std::min(depth, 63);
                    int m = std::min(legal_moves, 63);
                    R = LMR_table[d][m];

                    if (!is_pv) R++;

                    int src = GET_MOVE_SOURCE(move);
                    int tgt = GET_MOVE_TARGET(move);
                    int history_score = history_table[pos.side][src][tgt];
                    
                    // History-based LMR adjustment
                    if (history_score > 3000) R -= 2;
                    else if (history_score > 1000) R -= 1;
                    else if (history_score < -2000) R += 1;

                    // Improving flag LMR adjustment
                    if (!improving) R = std::max(0, R - 1);

                    // Don't reduce advanced pawn pushes
                    if (is_advanced_pawn) {
                        if ((pos.side == WHITE && move_tgt >= a7) || (pos.side == BLACK && move_tgt <= h2)) {
                            R = 0;
                        } else {
                            R = std::max(0, R - 1);
                        }
                    }

                    // Don't reduce killers or countermoves
                    if (ply < 512 && (move == killer_moves[0][ply] || move == killer_moves[1][ply])) R = 0;
                    if (prev_move != 0 && counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] == move) R = 0;
                } else if (!see_ge(pos, move, 0)) {
                    // Reduce losing captures (negative SEE)
                    R = 1 + (!is_pv ? 1 : 0);
                }
                
                if (R < 0) R = 0;
                if (R > depth - 1) R = depth - 1;
            }

            Board::UndoInfo undo;
            if (Evaluation::use_nnue && ply < Evaluation::MAX_PLY - 1) {
                Evaluation::nnue_stack[ply + 1].accumulator.computedAccumulation = 0;
            }
            if (!Board::make_move(pos, move, 0, &undo, (ply < Evaluation::MAX_PLY - 1) ? &Evaluation::nnue_stack[ply + 1].dirtyPiece : nullptr)) {
                continue;
            }
            legal_moves++;
            if (is_quiet && quiets_count < 64) {
                quiets_tried[quiets_count++] = move;
            } else if (!is_quiet && captures_count < 32) {
                captures_tried[captures_count++] = move;
            }
            if (ply < Evaluation::MAX_PLY) {
                move_history[ply] = move;
            }

            int opp_king = (pos.side == WHITE) ? Bitboard::lsb(pos.pieceBB[K]) : Bitboard::lsb(pos.pieceBB[k]);
            bool gives_check = MoveGen::is_square_attacked(pos, opp_king, pos.side ^ 1);
            if (gives_check && R > 0) R--;
            
            int score;
            
            // PVS / LMR
            if (legal_moves == 1) {
                int ext = singular_extension;
                score = -alpha_beta(pos, depth - 1 + ext, -beta, -alpha, ply + 1, true, move);
            } else {
                score = -alpha_beta(pos, depth - 1 - R, -alpha - 1, -alpha, ply + 1, true, move);
                
                if (score > alpha && R > 0) {
                    score = -alpha_beta(pos, depth - 1, -alpha - 1, -alpha, ply + 1, true, move);
                }
                if (score > alpha && score < beta) {
                    score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, true, move);
                }
            }
            
            Board::unmake_move(pos, move, undo);
            if (ply < Evaluation::MAX_PLY) {
                move_history[ply] = 0;
            }

            if (is_stopped()) return 0;

            if (score >= beta) {
                TT::write_hash_entry(pos.hash_key, beta, depth, ply, hash_flag_beta, move);
                int bonus = std::min(depth * depth, 400);

                if (is_quiet) {
                    if (ply < 512) {
                        killer_moves[1][ply] = killer_moves[0][ply];
                        killer_moves[0][ply] = move;
                    }
                    if (prev_move != 0) {
                        counter_move[GET_MOVE_SOURCE(prev_move)][GET_MOVE_TARGET(prev_move)] = move;
                    }
                    int src = GET_MOVE_SOURCE(move);
                    int tgt = GET_MOVE_TARGET(move);
                    int piece = GET_MOVE_PIECE(move);

                    int h = history_table[pos.side][src][tgt];
                    history_table[pos.side][src][tgt] += bonus - (h * bonus) / 16384;

                    if (prev_move != 0) {
                        int prev_piece = GET_MOVE_PIECE(prev_move);
                        int prev_tgt = GET_MOVE_TARGET(prev_move);
                        int new_val = conthist_1ply[prev_piece][prev_tgt][tgt] + bonus;
                        conthist_1ply[prev_piece][prev_tgt][tgt] = (int16_t)std::clamp(new_val, -8000, 8000);
                    }

                    if (ply >= 2 && move_history[ply - 2] != 0) {
                        Move prev2 = move_history[ply - 2];
                        int prev2_piece = GET_MOVE_PIECE(prev2);
                        int prev2_tgt = GET_MOVE_TARGET(prev2);
                        int new_val = conthist_2ply[prev2_piece][prev2_tgt][tgt] + bonus;
                        conthist_2ply[prev2_piece][prev2_tgt][tgt] = (int16_t)std::clamp(new_val, -8000, 8000);
                    }

                    // Quiet History Malus
                    for (int q = 0; q < quiets_count - 1; q++) {
                        Move q_move = quiets_tried[q];
                        int q_src = GET_MOVE_SOURCE(q_move);
                        int q_tgt = GET_MOVE_TARGET(q_move);
                        int q_h = history_table[pos.side][q_src][q_tgt];
                        history_table[pos.side][q_src][q_tgt] -= bonus + (q_h * bonus) / 16384;

                        if (prev_move != 0) {
                            int prev_piece = GET_MOVE_PIECE(prev_move);
                            int prev_tgt = GET_MOVE_TARGET(prev_move);
                            int new_val = conthist_1ply[prev_piece][prev_tgt][q_tgt] - bonus;
                            conthist_1ply[prev_piece][prev_tgt][q_tgt] = (int16_t)std::clamp(new_val, -8000, 8000);
                        }
                    }

                    // Capture Malus: captures that failed before quiet cutoff
                    for (int c = 0; c < captures_count; c++) {
                        Move c_move = captures_tried[c];
                        int c_tgt = GET_MOVE_TARGET(c_move);
                        int c_piece = GET_MOVE_PIECE(c_move);
                        int c_victim = get_piece_on_square(pos, c_tgt);
                        if (c_victim != -1) {
                            int new_val = capture_history[c_piece][c_tgt][c_victim] - bonus;
                            capture_history[c_piece][c_tgt][c_victim] = (int16_t)std::clamp(new_val, -8000, 8000);
                        }
                    }
                } else if (GET_MOVE_CAPTURE(move)) {
                    int tgt = GET_MOVE_TARGET(move);
                    int piece = GET_MOVE_PIECE(move);
                    int victim = get_piece_on_square(pos, tgt);
                    if (victim != -1) {
                        int new_val = capture_history[piece][tgt][victim] + bonus;
                        capture_history[piece][tgt][victim] = (int16_t)std::clamp(new_val, -8000, 8000);
                    }

                    // Capture Malus: earlier captures that failed to cutoff
                    for (int c = 0; c < captures_count - 1; c++) {
                        Move c_move = captures_tried[c];
                        int c_tgt = GET_MOVE_TARGET(c_move);
                        int c_piece = GET_MOVE_PIECE(c_move);
                        int c_victim = get_piece_on_square(pos, c_tgt);
                        if (c_victim != -1) {
                            int new_val = capture_history[c_piece][c_tgt][c_victim] - bonus;
                            capture_history[c_piece][c_tgt][c_victim] = (int16_t)std::clamp(new_val, -8000, 8000);
                        }
                    }

                    // Quiet Malus: quiets that failed before capture cutoff
                    for (int q = 0; q < quiets_count; q++) {
                        Move q_move = quiets_tried[q];
                        int q_src = GET_MOVE_SOURCE(q_move);
                        int q_tgt = GET_MOVE_TARGET(q_move);
                        int q_h = history_table[pos.side][q_src][q_tgt];
                        history_table[pos.side][q_src][q_tgt] -= bonus + (q_h * bonus) / 16384;
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
    int root_s, root_e, root_c, root_r50;
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
            pos.rule50 = root_r50;
            pos.hash_key = Zobrist::generate_hash_key(pos);
            Search::thread_search_id = root_search_id;
            search_history[0] = pos.hash_key;
            lock.unlock();
            
            Evaluation::allocate_nnue_stack();
            Evaluation::nnue_stack[0].accumulator.computedAccumulation = 0;
            if (Evaluation::use_nnue) {
                Evaluation::evaluate_incremental(pos, 0);
            }
            
            // Helper does its own iterative deepening until stopped or max_depth
            for (int current_depth = 1 + (thread_id % 2); current_depth <= root_max_depth; current_depth++) {
                if (is_stopped() || !search_running) break;

                Move hash_move = 0;
                int tt_s, tt_d, tt_f;
                Move tt_m;
                if (TT::probe_entry(pos.hash_key, tt_s, tt_d, tt_f, tt_m, 0)) {
                    hash_move = tt_m;
                }
                
                MovePicker move_picker(pos, hash_move, 0, 0, false);
                int best_score = -32000;
                Move best_move = 0;
                
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

                    if (score > best_score) {
                        best_score = score;
                        best_move = move;
                    }
                }
                if (!is_stopped() && search_running && best_move != 0) {
                    TT::write_hash_entry(pos.hash_key, best_score, current_depth, 0, hash_flag_exact, best_move);
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
            
            Board::UndoInfo undo;
            if (!Board::make_move(pos, best_move, 0, &undo, nullptr)) {
                break;
            }
            undos.push_back(undo);
            moves.push_back(best_move);

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

    void search_position(BoardState& pos, int depth, int current_search_id, long long time_for_move_ms, bool is_fixed_movetime) {
        std::lock_guard<std::mutex> main_lock(main_search_mtx);

        if (current_search_id != -1 && current_search_id != search_id.load(std::memory_order_relaxed)) {
            return;
        }

        auto search_start_time = std::chrono::steady_clock::now();
        if (time_for_move_ms != -1) {
            time_managed = true;
            if (is_fixed_movetime) {
                // Fixed movetime: hard stop with a 25ms communication buffer
                long long hard_stop = std::max(10LL, time_for_move_ms - 25);
                search_end_time = search_start_time + std::chrono::milliseconds(hard_stop);
            } else {
                // Clock-managed time: allow up to 2x nominal allocation in tactical positions
                search_end_time = search_start_time + std::chrono::milliseconds(time_for_move_ms * 2);
            }
        } else {
            time_managed = false;
        }

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
            Evaluation::evaluate_incremental(pos, 0); // Initialize root NNUE accumulator
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

        if (legal_moves.size() == 1) {
            Move m = legal_moves[0].move;
            if (current_search_id == -1 || current_search_id == search_id.load(std::memory_order_relaxed)) {
                std::string move_str = move_to_string(m);
                printf("info depth 1 nodes 1 pv %s\n", move_str.c_str());
                printf("bestmove %s\n", move_str.c_str());
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
        root_r50 = pos.rule50;
        
        search_history[0] = pos.hash_key;

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

        Move last_iteration_best_move = 0;

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
                
                int delta = 25;
                int alpha = -32000;
                int beta = 32000;
                
                // For main line, use aspiration windows
                if (current_depth >= 4 && pv_idx == 0) {
                    alpha = std::max(-32000, legal_moves[0].score - delta);
                    beta = std::min(32000, legal_moves[0].score + delta);
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
                    int moves_searched = 0;

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
                        int score;
                        if (moves_searched == 0) {
                            score = -alpha_beta(pos, current_depth - 1, -beta, -current_alpha, 1, false, move);
                        } else {
                            score = -alpha_beta(pos, current_depth - 1, -current_alpha - 1, -current_alpha, 1, false, move);
                            if (score > current_alpha && score < beta) {
                                score = -alpha_beta(pos, current_depth - 1, -beta, -current_alpha, 1, false, move);
                            }
                        }
                        moves_searched++;

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
                        if (best_score <= alpha) {
                            alpha = std::max(-32000, alpha - delta);
                            delta += delta / 2;
                            continue;
                        }
                        if (best_score >= beta) {
                            beta = std::min(32000, beta + delta);
                            delta += delta / 2;
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
            Move current_best_move = global_best_moves[0];
            bool move_stable = (current_depth >= 4 && current_best_move == last_iteration_best_move);
            last_iteration_best_move = current_best_move;

            if (is_stopped() || (time_managed && std::chrono::steady_clock::now() >= search_end_time)) {
                stopped = true;
                break;
            }
            if (time_managed) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - search_start_time).count();
                if (is_fixed_movetime) {
                    // Fixed movetime: allow searching up to 85% of allocated time before cutting off
                    long long cutoff_time = (time_for_move_ms * 85) / 100;
                    if (elapsed >= cutoff_time) {
                        stopped = true;
                        break;
                    }
                } else {
                    // Clock-managed time: early cutoff if best move is stable to preserve clock
                    long long cutoff_time = move_stable ? (time_for_move_ms * 6 / 10) : (time_for_move_ms * 12 / 10);
                    if (elapsed >= cutoff_time) {
                        stopped = true;
                        break;
                    }
                }
            }
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
                printf("bestmove (none)\n");
                fflush(stdout);
                return;
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

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include "position.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace Search {
    extern int max_depth;
    extern int multi_pv;
    extern std::atomic<bool> stopped;
    extern std::atomic<int> search_id;
    extern thread_local int thread_search_id;

    // Game history for repetition detection
    constexpr int MAX_GAME_MOVES = 2048;
    extern U64 game_history[MAX_GAME_MOVES];
    extern int game_history_len;

    bool is_repetition(const BoardState& pos, int ply);
    bool is_draw(const BoardState& pos, int ply);

    // History and Killer Heuristics (shared for lazy SMP)
    extern int history_table[2][64][64];
    extern Move killer_moves[2][512]; 
    extern Move counter_move[64][64];
    extern int LMR_table[64][64];
    void init_lmr_table();
    
    // Sort moves to improve alpha-beta pruning
    void sort_moves(BoardState& pos, MoveList& move_list, Move hash_move, int ply, Move prev_move = 0);

    // Static Exchange Evaluation
    int see(const BoardState& pos, Move move);
    bool see_ge(const BoardState& pos, Move move, int threshold = 0);

    // Alpha-Beta with Quiescence search
    int quiescence(BoardState& pos, int alpha, int beta, int ply);
    int alpha_beta(BoardState& pos, int depth, int alpha, int beta, int ply, bool do_null, Move prev_move = 0);
    
    // Lazy SMP Thread Pool
    void init_threads(int num_threads = 4);
    void stop_threads();
    
    // Multithreaded root search
    void search_position(BoardState& pos, int depth, int current_search_id = -1, long long time_for_move_ms = -1, bool is_fixed_movetime = false);
}

#endif // SEARCH_H

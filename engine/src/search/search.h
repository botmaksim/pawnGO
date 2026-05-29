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

    // History and Killer Heuristics (shared for lazy SMP)
    extern std::atomic<int> history_table[2][64][64];
    extern std::atomic<Move> killer_moves[2][100]; 
    extern std::atomic<Move> counter_move[64][64];
    extern int LMR_table[64][64];
    void init_lmr_table();
    
    // Sort moves to improve alpha-beta pruning
    void sort_moves(BoardState& pos, MoveList& move_list, Move hash_move, int ply, Move prev_move = 0);

    // Alpha-Beta with Quiescence search
    int quiescence(BoardState& pos, int alpha, int beta, int ply);
    int alpha_beta(BoardState& pos, int depth, int alpha, int beta, int ply, bool do_null, Move prev_move = 0);
    
    // Lazy SMP Thread Pool
    void init_threads(int num_threads = 4);
    void stop_threads();
    
    // Multithreaded root search
    void search_position(BoardState& pos, int depth);
}

#endif // SEARCH_H

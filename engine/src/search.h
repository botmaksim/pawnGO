#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace Search {
    extern int max_depth;
    extern int multi_pv;
    extern std::atomic<bool> stopped;

    // History and Killer Heuristics (thread_local for lazy SMP)
    extern thread_local int history_table[2][64][64];
    extern thread_local Move killer_moves[2][100]; 
    
    // Sort moves to improve alpha-beta pruning
    void sort_moves(MoveList& move_list, Move hash_move, int ply);

    // Alpha-Beta with Quiescence search
    int quiescence(int alpha, int beta);
    int alpha_beta(int depth, int alpha, int beta);
    
    // Lazy SMP Thread Pool
    void init_threads(int num_threads = 4);
    void stop_threads();
    
    // Multithreaded root search
    void search_position(int depth);
}

#endif // SEARCH_H

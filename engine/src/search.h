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
    
    // Sort moves to improve alpha-beta pruning
    void sort_moves(MoveList& move_list);

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

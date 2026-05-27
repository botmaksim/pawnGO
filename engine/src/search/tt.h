#ifndef TT_H
#define TT_H

#include "types.h"
#include <atomic>

// TT Flags
const int hash_flag_exact = 0;
const int hash_flag_alpha = 1;
const int hash_flag_beta = 2;

struct TTEntry {
    U64 hash_key;
    std::atomic<U64> data;
};

struct alignas(64) TTBucket {
    TTEntry entries[4];
};

namespace TT {
    extern TTBucket* table;
    extern int num_buckets;
    
    void init(int mb_size);
    void free_table();
    void clear();
    
    int read_hash_entry(U64 hash_key, int alpha, int beta, int depth, int ply, Move& best_move);
    void write_hash_entry(U64 hash_key, int score, int depth, int ply, int hash_flag, Move best_move);
}

#endif // TT_H

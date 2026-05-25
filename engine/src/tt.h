#ifndef TT_H
#define TT_H

#include "types.h"

// TT Flags
const int hash_flag_exact = 0;
const int hash_flag_alpha = 1;
const int hash_flag_beta = 2;

struct TTEntry {
    U64 hash_key;
    int depth;
    int flag;
    int score;
    Move best_move;
};

namespace TT {
    extern TTEntry* table;
    extern int num_entries;
    
    void init(int mb_size);
    void free_table();
    void clear();
    
    int read_hash_entry(U64 hash_key, int alpha, int beta, int depth, Move& best_move);
    void write_hash_entry(U64 hash_key, int score, int depth, int hash_flag, Move best_move);
}

#endif // TT_H

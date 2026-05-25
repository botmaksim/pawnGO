#include "tt.h"
#include <iostream>

namespace TT {
    TTEntry* table = nullptr;
    int num_entries = 0;

    void init(int mb_size) {
        int hash_size = 0x100000 * mb_size;
        num_entries = hash_size / sizeof(TTEntry);
        
        free_table();
        table = new TTEntry[num_entries];
        clear();
        
        std::cout << "info string Hash table initialized with " << num_entries << " entries (" << mb_size << " MB)" << std::endl;
    }

    void free_table() {
        if (table != nullptr) {
            delete[] table;
            table = nullptr;
        }
    }

    void clear() {
        for (int i = 0; i < num_entries; i++) {
            table[i].hash_key = 0;
            table[i].depth = 0;
            table[i].flag = 0;
            table[i].score = 0;
            table[i].best_move = 0;
        }
    }

    // Returns a valid score if there's a cutoff/exact match, else returns -50000
    int read_hash_entry(U64 hash_key, int alpha, int beta, int depth, Move& best_move) {
        if (table == nullptr) return -50000;
        
        TTEntry* entry = &table[hash_key % num_entries];
        
        if (entry->hash_key == hash_key) {
            best_move = entry->best_move; // Always useful for move ordering
            
            if (entry->depth >= depth) {
                int score = entry->score;
                
                if (entry->flag == hash_flag_exact) {
                    return score;
                }
                if ((entry->flag == hash_flag_alpha) && (score <= alpha)) {
                    return alpha;
                }
                if ((entry->flag == hash_flag_beta) && (score >= beta)) {
                    return beta;
                }
            }
        }
        return -50000;
    }

    void write_hash_entry(U64 hash_key, int score, int depth, int hash_flag, Move best_move) {
        if (table == nullptr) return;
        
        TTEntry* entry = &table[hash_key % num_entries];
        
        // Basic replacement scheme: always replace
        entry->hash_key = hash_key;
        entry->score = score;
        entry->flag = hash_flag;
        entry->depth = depth;
        entry->best_move = best_move;
    }
}

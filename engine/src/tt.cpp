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
            table[i].data.store(0, std::memory_order_relaxed);
        }
    }

    inline U64 pack_data(int score, int depth, int flag, Move move, U64 hash_key) {
        U64 m = move & 0xFFFFFFULL;
        U64 s = (uint16_t)score;
        U64 d = depth & 0xFFULL;
        U64 f = flag & 0x3ULL;
        U64 checksum = (hash_key >> 50) & 0x3FFFULL; // 14 bits
        return m | (s << 24) | (d << 40) | (f << 48) | (checksum << 50);
    }

    inline void unpack_data(U64 data, int& score, int& depth, int& flag, Move& move) {
        move = data & 0xFFFFFFULL;
        score = (int16_t)((data >> 24) & 0xFFFFULL);
        depth = (data >> 40) & 0xFFULL;
        flag = (data >> 48) & 0x3ULL;
    }

    int read_hash_entry(U64 hash_key, int alpha, int beta, int depth, Move& best_move) {
        if (table == nullptr) return -32000;

        int index = hash_key % num_entries;
        TTEntry* entry = &table[index];

        U64 key = entry->hash_key;
        U64 data = entry->data.load(std::memory_order_relaxed);

        if (key == hash_key) {
            U64 checksum = (hash_key >> 50) & 0x3FFFULL;
            if (((data >> 50) & 0x3FFFULL) != checksum) {
                return -32000; // Torn read detected
            }

            int score, entry_depth, flag;
            unpack_data(data, score, entry_depth, flag, best_move);

            if (entry_depth >= depth) {
                if (flag == hash_flag_exact) {
                    return score;
                }
                if ((flag == hash_flag_alpha) && (score <= alpha)) {
                    return alpha;
                }
                if ((flag == hash_flag_beta) && (score >= beta)) {
                    return beta;
                }
            }
        }
        return -32000;
    }

    void write_hash_entry(U64 hash_key, int score, int depth, int hash_flag, Move best_move) {
        if (table == nullptr) return;

        int index = hash_key % num_entries;
        TTEntry* entry = &table[index];

        U64 data = pack_data(score, depth, hash_flag, best_move, hash_key);

        entry->data.store(data, std::memory_order_relaxed);
        entry->hash_key = hash_key;
    }
}

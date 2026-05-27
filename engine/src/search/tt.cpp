#include "tt.h"
#include <iostream>

namespace TT {
    TTBucket* table = nullptr;
    int num_buckets = 0;

    void init(int mb_size) {
        int hash_size = 0x100000 * mb_size;
        num_buckets = hash_size / sizeof(TTBucket);

        free_table();
        table = new TTBucket[num_buckets];
        clear();

        std::cout << "info string Hash table initialized with " << (num_buckets * 4) << " entries (" << mb_size << " MB)" << std::endl;
    }

    void free_table() {
        if (table != nullptr) {
            delete[] table;
            table = nullptr;
        }
    }

    void clear() {
        for (int i = 0; i < num_buckets; i++) {
            for (int j = 0; j < 4; j++) {
                table[i].entries[j].hash_key = 0;
                table[i].entries[j].data.store(0, std::memory_order_relaxed);
            }
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

    int read_hash_entry(U64 hash_key, int alpha, int beta, int depth, int ply, Move& best_move) {
        if (table == nullptr) return -32000;

        int index = hash_key % num_buckets;
        TTBucket* bucket = &table[index];

        for (int i = 0; i < 4; i++) {
            TTEntry* entry = &bucket->entries[i];
            U64 key = entry->hash_key;
            U64 data = entry->data.load(std::memory_order_relaxed);

            if (key == hash_key) {
                U64 checksum = (hash_key >> 50) & 0x3FFFULL;
                if (((data >> 50) & 0x3FFFULL) != checksum) {
                    return -32000; // Torn read detected
                }

                int score, entry_depth, flag;
                unpack_data(data, score, entry_depth, flag, best_move);
                if (score > 19000) score -= ply;
                if (score < -19000) score += ply;

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
                // Return best move even if depth/bounds don't match
                return -32000; 
            }
        }
        return -32000;
    }

    void write_hash_entry(U64 hash_key, int score, int depth, int ply, int hash_flag, Move best_move) {
        if (table == nullptr) return;
        if (score > 19000) score += ply;
        if (score < -19000) score -= ply;

        int index = hash_key % num_buckets;
        TTBucket* bucket = &table[index];

        int replace_idx = 0;
        int min_depth = 999;

        for (int i = 0; i < 4; i++) {
            TTEntry* entry = &bucket->entries[i];
            if (entry->hash_key == hash_key || entry->hash_key == 0) {
                replace_idx = i;
                break;
            }
            // Always replace scheme based on lowest depth
            U64 data = entry->data.load(std::memory_order_relaxed);
            int entry_depth = (data >> 40) & 0xFFULL;
            if (entry_depth < min_depth) {
                min_depth = entry_depth;
                replace_idx = i;
            }
        }

        TTEntry* entry = &bucket->entries[replace_idx];
        U64 data = pack_data(score, depth, hash_flag, best_move, hash_key);

        entry->data.store(data, std::memory_order_relaxed);
        entry->hash_key = hash_key;
    }
}

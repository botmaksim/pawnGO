#include "polyglot.h"
#include "polyglot_keys.h"
#include "bitboard.h"
#include "board.h"
#include "movegen.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

namespace Polyglot {

    // Endianness swap for 16, 32, 64 bit values
    uint16_t swap16(uint16_t v) { return (v >> 8) | (v << 8); }
    uint32_t swap32(uint32_t v) { return ((v >> 24) & 0xff) | ((v << 8) & 0xff0000) | ((v >> 8) & 0xff00) | ((v << 24) & 0xff000000); }
    uint64_t swap64(uint64_t v) {
        return ((v & 0xff00000000000000ULL) >> 56) |
               ((v & 0x00ff000000000000ULL) >> 40) |
               ((v & 0x0000ff0000000000ULL) >> 24) |
               ((v & 0x000000ff00000000ULL) >>  8) |
               ((v & 0x00000000ff000000ULL) <<  8) |
               ((v & 0x0000000000ff0000ULL) << 24) |
               ((v & 0x000000000000ff00ULL) << 40) |
               ((v & 0x00000000000000ffULL) << 56);
    }

    int get_polyglot_piece(int piece_type) {
        switch (piece_type) {
            case P: return 1;
            case p: return 0;
            case N: return 3;
            case n: return 2;
            case B: return 5;
            case b: return 4;
            case R: return 7;
            case r: return 6;
            case Q: return 9;
            case q: return 8;
            case K: return 11;
            case k: return 10;
            default: return -1;
        }
    }

    U64 compute_polyglot_key() {
        U64 hash = 0ULL;
        
        // Pieces
        for (int piece = P; piece <= k; piece++) {
            U64 bb = Bitboard::pieceBB[piece];
            int poly_piece = get_polyglot_piece(piece);
            while (bb) {
                int sq = Bitboard::lsb(bb);
                Bitboard::pop_bit(bb, sq);
                // Convert pawnGO sq (a1=0) to polyglot sq (a1=0). No change!
                // Wait! Polyglot board: a1 is 0, b1 is 1... h8 is 63.
                // pawnGO board: a1 is 0, b1 is 1... h8 is 63.
                int poly_sq = sq;
                // Exception: Polyglot piece mapping maps from 0..63 where a1=0.
                hash ^= Random64[64 * poly_piece + poly_sq];
            }
        }
        
        // Castling
        // pawnGO: wk=1, wq=2, bk=4, bq=8.
        if (Bitboard::castle & 1) hash ^= Random64[768]; // White KS
        if (Bitboard::castle & 2) hash ^= Random64[769]; // White QS
        if (Bitboard::castle & 4) hash ^= Random64[770]; // Black KS
        if (Bitboard::castle & 8) hash ^= Random64[771]; // Black QS
        
        // En-passant
        if (Bitboard::enpassant != no_sq) {
            int ep_file = Bitboard::enpassant % 8;
            int ep_rank = Bitboard::enpassant / 8;
            bool can_capture = false;
            
            // Check if there's actually an enemy pawn adjacent that could capture
            if (Bitboard::side == WHITE) {
                // White to move, so Black pawn moved double. The ep square is on rank 5 (index 40..47)
                // White pawns could be on ep_sq - 8 - 1 or ep_sq - 8 + 1
                U64 white_pawns = Bitboard::pieceBB[P];
                U64 ep_bb = (1ULL << Bitboard::enpassant);
                // Shift down to rank 5 (which is the rank of the white pawns)
                U64 target = ep_bb >> 8;
                if ((target & ~0x8080808080808080ULL) >> 1 & white_pawns) can_capture = true;
                if ((target & ~0x0101010101010101ULL) << 1 & white_pawns) can_capture = true;
            } else {
                U64 black_pawns = Bitboard::pieceBB[p];
                U64 ep_bb = (1ULL << Bitboard::enpassant);
                U64 target = ep_bb << 8;
                if ((target & ~0x8080808080808080ULL) >> 1 & black_pawns) can_capture = true;
                if ((target & ~0x0101010101010101ULL) << 1 & black_pawns) can_capture = true;
            }
            
            if (can_capture) {
                hash ^= Random64[772 + ep_file];
            }
        }
        
        // Side to move
        if (Bitboard::side == WHITE) {
            hash ^= Random64[780];
        }
        
        return hash;
    }

    Move polyglot_to_pawngo_move(uint16_t poly_move) {
        int to_file = poly_move & 7;
        int to_rank = (poly_move >> 3) & 7;
        int from_file = (poly_move >> 6) & 7;
        int from_rank = (poly_move >> 9) & 7;
        int promo = (poly_move >> 12) & 7;
        
        int source = from_rank * 8 + from_file;
        int target = to_rank * 8 + to_file;
        
        // Generate pseudo legal moves and find the matching one to get the full move encoded
        MoveList move_list;
        MoveGen::generate_moves(move_list, false);
        
        for (int i = 0; i < move_list.count; i++) {
            Move m = move_list.moves[i];
            int s = GET_MOVE_SOURCE(m);
            int t = GET_MOVE_TARGET(m);
            if (s == source && t == target) {
                int p = GET_MOVE_PROMOTED(m);
                if (promo == 0 && p == 0) return m;
                // Polyglot promo: 1=knight, 2=bishop, 3=rook, 4=queen
                if (promo > 0) {
                    if (Bitboard::side == WHITE) {
                        if (promo == 1 && p == N) return m;
                        if (promo == 2 && p == B) return m;
                        if (promo == 3 && p == R) return m;
                        if (promo == 4 && p == Q) return m;
                    } else {
                        if (promo == 1 && p == n) return m;
                        if (promo == 2 && p == b) return m;
                        if (promo == 3 && p == r) return m;
                        if (promo == 4 && p == q) return m;
                    }
                }
            }
        }
        
        return 0; // Invalid move
    }

    Move get_book_move(const std::string& book_path) {
        std::ifstream file(book_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return 0;
        
        std::streamsize size = file.tellg();
        if (size % 16 != 0) return 0;
        
        int num_entries = size / 16;
        if (num_entries == 0) return 0;
        
        U64 target_key = compute_polyglot_key();
        
        int low = 0;
        int high = num_entries - 1;
        int first_match = -1;
        
        while (low <= high) {
            int mid = low + (high - low) / 2;
            file.seekg(mid * 16, std::ios::beg);
            U64 key;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));
            key = swap64(key);
            
            if (key < target_key) {
                low = mid + 1;
            } else if (key > target_key) {
                high = mid - 1;
            } else {
                first_match = mid;
                high = mid - 1; // Find the FIRST match
            }
        }
        
        if (first_match == -1) return 0;
        
        std::vector<PolyglotEntry> entries;
        file.seekg(first_match * 16, std::ios::beg);
        
        int total_weight = 0;
        while (first_match < num_entries) {
            PolyglotEntry entry;
            file.read(reinterpret_cast<char*>(&entry.key), 8);
            file.read(reinterpret_cast<char*>(&entry.move), 2);
            file.read(reinterpret_cast<char*>(&entry.weight), 2);
            file.read(reinterpret_cast<char*>(&entry.learn), 4);
            
            entry.key = swap64(entry.key);
            if (entry.key != target_key) break;
            
            entry.move = swap16(entry.move);
            entry.weight = swap16(entry.weight);
            
            entries.push_back(entry);
            total_weight += entry.weight;
            first_match++;
        }
        
        if (entries.empty()) return 0;
        if (total_weight == 0) {
            // Pick uniformly if all weights are 0
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, entries.size() - 1);
            return polyglot_to_pawngo_move(entries[dis(gen)].move);
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, total_weight - 1);
        int r = dis(gen);
        
        int sum = 0;
        for (const auto& entry : entries) {
            sum += entry.weight;
            if (r < sum) {
                return polyglot_to_pawngo_move(entry.move);
            }
        }
        
        return polyglot_to_pawngo_move(entries.back().move);
    }
}

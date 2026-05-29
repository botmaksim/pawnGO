#include "../core/position.h"
#ifndef POLYGLOT_H
#define POLYGLOT_H

#include "types.h"
#include <string>
#include <vector>

struct PolyglotEntry {
    U64 key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};

namespace Polyglot {
    U64 compute_polyglot_key(const BoardState& pos);
    Move get_book_move(const BoardState& pos, const std::string& book_path);
    std::vector<std::pair<Move, int>> get_all_book_moves(const BoardState& pos, const std::string& book_path);
    int pawngo_move_to_polyglot(Move move);
    Move polyglot_to_pawngo_move(const BoardState& pos, uint16_t poly_move);
}

#endif // POLYGLOT_H

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
    U64 compute_polyglot_key();
    Move get_book_move(const std::string& book_path);
    int pawngo_move_to_polyglot(Move move);
    Move polyglot_to_pawngo_move(uint16_t poly_move);
}

#endif // POLYGLOT_H

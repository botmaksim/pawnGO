#pragma once
#include "types.h"
#include "bitboard.h"
#include "movegen.h"

#include "position.h"

namespace MoveGen {
    bool is_square_attacked(const BoardState& pos, int square, int side);
    void generate_moves(const BoardState& pos, MoveList& move_list, bool only_captures = false);
}

#pragma once
#include "types.h"
#include "bitboard.h"
#include "movegen.h"

namespace MoveGen {
    bool is_square_attacked(int square, int side);
    void generate_moves(MoveList& move_list, bool only_captures = false);
}

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace Zobrist {
    extern U64 piece_keys[12][64];
    extern U64 enpassant_keys[64];
    extern U64 castle_keys[16];
    extern U64 side_key;

    void init();
    U64 generate_hash_key();
}

#endif // ZOBRIST_H

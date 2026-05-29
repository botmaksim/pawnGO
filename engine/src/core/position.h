#pragma once
#include "types.h"

struct BoardState {
    U64 pieceBB[12];
    U64 occupancies[3];
    int side;
    int enpassant;
    int castle;
    U64 hash_key;

    BoardState() {
        for (int i = 0; i < 12; i++) pieceBB[i] = 0;
        for (int i = 0; i < 3; i++) occupancies[i] = 0;
        side = 0;
        enpassant = no_sq;
        castle = 0;
        hash_key = 0;
    }
};

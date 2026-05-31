#pragma once
#include <cstdint>

typedef uint64_t U64;

enum Color { WHITE, BLACK, BOTH };

enum Piece {
    P, N, B, R, Q, K,
    p, n, b, r, q, k
};

enum Square {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8, no_sq
};

enum CastlingRights {
    wk = 1, wq = 2, bk = 4, bq = 8
};

// 24-bit move encoding
// 0-5: source square (6 bits)
// 6-11: target square (6 bits)
// 12-15: piece (4 bits)
// 16-19: promoted piece (4 bits)
// 20: capture flag (1 bit)
// 21: double pawn push flag (1 bit)
// 22: enpassant flag (1 bit)
// 23: castling flag (1 bit)

typedef uint32_t Move;

#define ENCODE_MOVE(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | ((capture) << 20) | ((double_push) << 21) | ((enpassant) << 22) | ((castling) << 23))

#define GET_MOVE_SOURCE(move) ((move) & 0x3f)
#define GET_MOVE_TARGET(move) (((move) >> 6) & 0x3f)
#define GET_MOVE_PIECE(move) (((move) >> 12) & 0xf)
#define GET_MOVE_PROMOTED(move) (((move) >> 16) & 0xf)
#define GET_MOVE_CAPTURE(move) ((move) & 0x100000)
#define GET_MOVE_DOUBLE_PUSH(move) ((move) & 0x200000)
#define GET_MOVE_ENPASSANT(move) ((move) & 0x400000)
#define GET_MOVE_CASTLING(move) ((move) & 0x800000)

struct MoveList {
    Move moves[256];
    int count;

    inline void add(Move move) {
        if (count < 256) moves[count++] = move;
    }
};

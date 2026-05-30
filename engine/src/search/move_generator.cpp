#include "move_generator.h"

namespace MoveGen {
    bool is_square_attacked(const BoardState& pos, int square, int side) {
        if (side == WHITE) {
            if (pawn_attacks[BLACK][square] & pos.pieceBB[P]) return true;
            if (knight_attacks[square] & pos.pieceBB[N]) return true;
            if (get_bishop_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[B]) return true;
            if (get_rook_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[R]) return true;
            if (get_queen_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[Q]) return true;
            if (king_attacks[square] & pos.pieceBB[K]) return true;
        } else {
            if (pawn_attacks[WHITE][square] & pos.pieceBB[p]) return true;
            if (knight_attacks[square] & pos.pieceBB[n]) return true;
            if (get_bishop_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[b]) return true;
            if (get_rook_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[r]) return true;
            if (get_queen_attacks(square, pos.occupancies[BOTH]) & pos.pieceBB[q]) return true;
            if (king_attacks[square] & pos.pieceBB[k]) return true;
        }
        return false;
    }

    void generate_moves(const BoardState& pos, MoveList& move_list, bool only_captures) {
        move_list.count = 0;
        int source, target;
        U64 bitboard, attacks;

        int piece_offset = pos.side == WHITE ? 0 : 6;
        int opp_side = pos.side == WHITE ? BLACK : WHITE;

        // PAWNS
        bitboard = pos.pieceBB[P + piece_offset];
        while (bitboard) {
            source = Bitboard::lsb(bitboard);
            Bitboard::pop_bit(bitboard, source);

            if (pos.side == WHITE) {
                target = source + 8;
                if (!(pos.occupancies[BOTH] & (1ULL << target))) {
                    if (source >= a7 && source <= h7) {
                        move_list.add(ENCODE_MOVE(source, target, P, Q, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, R, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, B, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, N, 0, 0, 0, 0));
                    } else if (!only_captures) {
                        move_list.add(ENCODE_MOVE(source, target, P, 0, 0, 0, 0, 0));
                        if ((source >= a2 && source <= h2) && !(pos.occupancies[BOTH] & (1ULL << (target + 8)))) {
                            move_list.add(ENCODE_MOVE(source, target + 8, P, 0, 0, 1, 0, 0));
                        }
                    }
                }
                attacks = pawn_attacks[WHITE][source] & pos.occupancies[BLACK];
                while (attacks) {
                    target = Bitboard::lsb(attacks);
                    Bitboard::pop_bit(attacks, target);
                    if (Bitboard::get_bit(pos.pieceBB[k], target)) continue; // Don't capture the king
                    if (source >= a7 && source <= h7) {
                        move_list.add(ENCODE_MOVE(source, target, P, Q, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, R, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, B, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, P, N, 1, 0, 0, 0));
                    } else {
                        move_list.add(ENCODE_MOVE(source, target, P, 0, 1, 0, 0, 0));
                    }
                }
                if (pos.enpassant != no_sq) {
                    U64 ep_attacks = pawn_attacks[WHITE][source] & (1ULL << pos.enpassant);
                    if (ep_attacks) {
                        target = pos.enpassant;
                        move_list.add(ENCODE_MOVE(source, target, P, 0, 1, 0, 1, 0));
                    }
                }
            } else {
                target = source - 8;
                if (!(pos.occupancies[BOTH] & (1ULL << target))) {
                    if (source >= a2 && source <= h2) {
                        move_list.add(ENCODE_MOVE(source, target, p, q, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, r, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, b, 0, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, n, 0, 0, 0, 0));
                    } else if (!only_captures) {
                        move_list.add(ENCODE_MOVE(source, target, p, 0, 0, 0, 0, 0));
                        if ((source >= a7 && source <= h7) && !(pos.occupancies[BOTH] & (1ULL << (target - 8)))) {
                            move_list.add(ENCODE_MOVE(source, target - 8, p, 0, 0, 1, 0, 0));
                        }
                    }
                }
                attacks = pawn_attacks[BLACK][source] & pos.occupancies[WHITE];
                while (attacks) {
                    target = Bitboard::lsb(attacks);
                    Bitboard::pop_bit(attacks, target);
                    if (Bitboard::get_bit(pos.pieceBB[K], target)) continue; // Don't capture the king
                    if (source >= a2 && source <= h2) {
                        move_list.add(ENCODE_MOVE(source, target, p, q, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, r, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, b, 1, 0, 0, 0));
                        move_list.add(ENCODE_MOVE(source, target, p, n, 1, 0, 0, 0));
                    } else {
                        move_list.add(ENCODE_MOVE(source, target, p, 0, 1, 0, 0, 0));
                    }
                }
                if (pos.enpassant != no_sq) {
                    U64 ep_attacks = pawn_attacks[BLACK][source] & (1ULL << pos.enpassant);
                    if (ep_attacks) {
                        target = pos.enpassant;
                        move_list.add(ENCODE_MOVE(source, target, p, 0, 1, 0, 1, 0));
                    }
                }
            }
        }

        // CASTLING
        if (!only_captures) {
            if (pos.side == WHITE) {
                if (pos.castle & wk) {
                    if (!Bitboard::get_bit(pos.occupancies[BOTH], f1) && !Bitboard::get_bit(pos.occupancies[BOTH], g1)) {
                        if (!is_square_attacked(pos, e1, BLACK) && !is_square_attacked(pos, f1, BLACK)) {
                            move_list.add(ENCODE_MOVE(e1, g1, K, 0, 0, 0, 0, 1));
                        }
                    }
                }
                if (pos.castle & wq) {
                    if (!Bitboard::get_bit(pos.occupancies[BOTH], d1) && !Bitboard::get_bit(pos.occupancies[BOTH], c1) && !Bitboard::get_bit(pos.occupancies[BOTH], b1)) {
                        if (!is_square_attacked(pos, e1, BLACK) && !is_square_attacked(pos, d1, BLACK)) {
                            move_list.add(ENCODE_MOVE(e1, c1, K, 0, 0, 0, 0, 1));
                        }
                    }
                }
            } else {
                if (pos.castle & bk) {
                    if (!Bitboard::get_bit(pos.occupancies[BOTH], f8) && !Bitboard::get_bit(pos.occupancies[BOTH], g8)) {
                        if (!is_square_attacked(pos, e8, WHITE) && !is_square_attacked(pos, f8, WHITE)) {
                            move_list.add(ENCODE_MOVE(e8, g8, k, 0, 0, 0, 0, 1));
                        }
                    }
                }
                if (pos.castle & bq) {
                    if (!Bitboard::get_bit(pos.occupancies[BOTH], d8) && !Bitboard::get_bit(pos.occupancies[BOTH], c8) && !Bitboard::get_bit(pos.occupancies[BOTH], b8)) {
                        if (!is_square_attacked(pos, e8, WHITE) && !is_square_attacked(pos, d8, WHITE)) {
                            move_list.add(ENCODE_MOVE(e8, c8, k, 0, 0, 0, 0, 1));
                        }
                    }
                }
            }
        }

        // PIECES
        int pieces_to_gen[] = { N, B, R, Q, K };
        for (int p_idx = 0; p_idx < 5; p_idx++) {
            int piece = pieces_to_gen[p_idx] + piece_offset;
            bitboard = pos.pieceBB[piece];

            while (bitboard) {
                source = Bitboard::lsb(bitboard);
                Bitboard::pop_bit(bitboard, source);

                if (piece == N || piece == n) attacks = knight_attacks[source];
                else if (piece == B || piece == b) attacks = get_bishop_attacks(source, pos.occupancies[BOTH]);
                else if (piece == R || piece == r) attacks = get_rook_attacks(source, pos.occupancies[BOTH]);
                else if (piece == Q || piece == q) attacks = get_queen_attacks(source, pos.occupancies[BOTH]);
                else if (piece == K || piece == k) attacks = king_attacks[source];

                attacks &= ~pos.occupancies[pos.side];

                while (attacks) {
                    target = Bitboard::lsb(attacks);
                    Bitboard::pop_bit(attacks, target);

                    int is_capture = Bitboard::get_bit(pos.occupancies[opp_side], target) ? 1 : 0;
                    if (only_captures && !is_capture) continue;
                    if (is_capture && Bitboard::get_bit(pos.pieceBB[opp_side == WHITE ? K : k], target)) continue; // Don't capture the king
                    move_list.add(ENCODE_MOVE(source, target, piece, 0, is_capture, 0, 0, 0));
                }
            }
        }
    }
}

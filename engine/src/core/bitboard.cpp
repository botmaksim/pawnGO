#include "bitboard.h"
#include "zobrist.h"
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Bitboard {
    std::string current_fen = "";

    int count_bits(U64 bb) {
        int count = 0;
        while (bb) {
            count++;
            bb &= bb - 1; // reset LS1B
        }
        return count;
    }

    int lsb(U64 bb) {
        if (bb) {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_ctzll(bb);
#elif defined(_MSC_VER)
            unsigned long index;
            _BitScanForward64(&index, bb);
            return index;
#else
            return count_bits((bb & -bb) - 1);
#endif
        }
        return -1;
    }

    void init(BoardState& pos) {
        for (int i = 0; i < 12; ++i) pos.pieceBB[i] = 0ULL;
        for (int i = 0; i < 3; ++i) pos.occupancies[i] = 0ULL;
        pos.side = WHITE;
        pos.enpassant = no_sq;
        pos.castle = 0;
    }

    void parse_fen(BoardState& pos, const std::string& fen) {
        init(pos);
        current_fen = fen;
        int rank = 7;
        int file = 0;
        size_t i = 0;

        // Parse Board
        while (i < fen.length() && fen[i] != ' ') {
            char c = fen[i];
            if (c == '/') {
                rank--;
                file = 0;
            } else if (c >= '1' && c <= '8') {
                file += (c - '0');
            } else {
                int piece = -1;
                switch (c) {
                    case 'P': piece = P; break;
                    case 'N': piece = N; break;
                    case 'B': piece = B; break;
                    case 'R': piece = R; break;
                    case 'Q': piece = Q; break;
                    case 'K': piece = K; break;
                    case 'p': piece = p; break;
                    case 'n': piece = n; break;
                    case 'b': piece = b; break;
                    case 'r': piece = r; break;
                    case 'q': piece = q; break;
                    case 'k': piece = k; break;
                }
                if (piece != -1) {
                    int sq = rank * 8 + file;
                    set_bit(pos.pieceBB[piece], sq);
                    file++;
                }
            }
            i++;
        }
        i++; // skip space

        // Parse Side to move
        if (i < fen.length()) {
            pos.side = (fen[i] == 'w') ? WHITE : BLACK;
            i += 2;
        }

        // Parse Castling
        pos.castle = 0;
        while (i < fen.length() && fen[i] != ' ') {
            switch (fen[i]) {
                case 'K': pos.castle |= wk; break;
                case 'Q': pos.castle |= wq; break;
                case 'k': pos.castle |= bk; break;
                case 'q': pos.castle |= bq; break;
                case '-': break;
            }
            i++;
        }
        i++; // skip space

        // Validate castling rights
        if (!(pos.pieceBB[K] & (1ULL << 4))) pos.castle &= ~(wk | wq);
        if (!(pos.pieceBB[R] & (1ULL << 7))) pos.castle &= ~wk;
        if (!(pos.pieceBB[R] & (1ULL << 0))) pos.castle &= ~wq;
        if (!(pos.pieceBB[k] & (1ULL << 60))) pos.castle &= ~(bk | bq);
        if (!(pos.pieceBB[r] & (1ULL << 63))) pos.castle &= ~bk;
        if (!(pos.pieceBB[r] & (1ULL << 56))) pos.castle &= ~bq;

        // Parse En Passant
        if (i < fen.length() && fen[i] != '-') {
            int f = fen[i] - 'a';
            int r = fen[i+1] - '1';
            pos.enpassant = r * 8 + f;
        } else {
            pos.enpassant = no_sq;
        }

        // Update occupancies
        for (int piece = P; piece <= K; ++piece) pos.occupancies[WHITE] |= pos.pieceBB[piece];
        for (int piece = p; piece <= k; ++piece) pos.occupancies[BLACK] |= pos.pieceBB[piece];
        pos.occupancies[BOTH] = pos.occupancies[WHITE] | pos.occupancies[BLACK];
        
        // Generate initial hash key
        pos.hash_key = Zobrist::generate_hash_key(pos);
    }

    void print_board(const BoardState& pos) {
        std::cout << "\n";
        for (int rank = 7; rank >= 0; rank--) {
            std::cout << rank + 1 << "  ";
            for (int file = 0; file < 8; file++) {
                int sq = rank * 8 + file;
                int piece = -1;

                // Find which piece is on this square
                for (int p = P; p <= k; p++) {
                    if (get_bit(pos.pieceBB[p], sq)) {
                        piece = p;
                        break;
                    }
                }

                char p_char = '.';
                if (piece != -1) {
                    const char* ascii_pieces = "PNBRQKpnbrqk";
                    p_char = ascii_pieces[piece];
                }
                std::cout << p_char << " ";
            }
            std::cout << "\n";
        }
        std::cout << "\n   a b c d e f g h\n\n";
        std::cout << "Side: " << ((pos.side == WHITE) ? "White" : "Black") << std::endl;
        std::cout << "Enpassant: " << ((pos.enpassant != no_sq) ? (std::string(1, 'a' + (pos.enpassant % 8)) + std::to_string((pos.enpassant / 8) + 1)) : "no") << std::endl;
        std::cout << "Castle: " << ((pos.castle & wk) ? "K" : "") << ((pos.castle & wq) ? "Q" : "")
                  << ((pos.castle & bk) ? "k" : "") << ((pos.castle & bq) ? "q" : "") << std::endl;
        std::cout << "Hash: " << std::hex << pos.hash_key << std::dec << std::endl;
    }
}

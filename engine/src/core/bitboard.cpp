#include "bitboard.h"
#include "zobrist.h"
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Bitboard {
    thread_local U64 pieceBB[12];
    thread_local U64 occupancies[3];
    thread_local int side;
    thread_local int enpassant = no_sq;
    thread_local int castle;
    thread_local U64 hash_key;
    
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

    void init() {
        for (int i = 0; i < 12; ++i) pieceBB[i] = 0ULL;
        for (int i = 0; i < 3; ++i) occupancies[i] = 0ULL;
        side = WHITE;
        enpassant = no_sq;
        castle = 0;
    }

    void parse_fen(const std::string& fen) {
        init();
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
                    set_bit(pieceBB[piece], sq);
                    file++;
                }
            }
            i++;
        }
        i++; // skip space

        // Parse Side to move
        if (i < fen.length()) {
            side = (fen[i] == 'w') ? WHITE : BLACK;
            i += 2;
        }

        // Parse Castling
        castle = 0;
        while (i < fen.length() && fen[i] != ' ') {
            switch (fen[i]) {
                case 'K': castle |= wk; break;
                case 'Q': castle |= wq; break;
                case 'k': castle |= bk; break;
                case 'q': castle |= bq; break;
                case '-': break;
            }
            i++;
        }
        i++; // skip space

        // Validate castling rights
        if (!(pieceBB[K] & (1ULL << 4))) castle &= ~(wk | wq);
        if (!(pieceBB[R] & (1ULL << 7))) castle &= ~wk;
        if (!(pieceBB[R] & (1ULL << 0))) castle &= ~wq;
        if (!(pieceBB[k] & (1ULL << 60))) castle &= ~(bk | bq);
        if (!(pieceBB[r] & (1ULL << 63))) castle &= ~bk;
        if (!(pieceBB[r] & (1ULL << 56))) castle &= ~bq;

        // Parse En Passant
        if (i < fen.length() && fen[i] != '-') {
            int f = fen[i] - 'a';
            int r = fen[i+1] - '1';
            enpassant = r * 8 + f;
        } else {
            enpassant = no_sq;
        }

        // Update occupancies
        for (int piece = P; piece <= K; piece++) occupancies[WHITE] |= pieceBB[piece];
        for (int piece = p; piece <= k; piece++) occupancies[BLACK] |= pieceBB[piece];
        // Update occupancies for BOTH sides
        occupancies[BOTH] |= occupancies[WHITE];
        occupancies[BOTH] |= occupancies[BLACK];
        
        // Generate initial hash key
        hash_key = Zobrist::generate_hash_key();
    }

    void print_board() {
        std::cout << "\n";
        for (int rank = 7; rank >= 0; rank--) {
            std::cout << rank + 1 << "  ";
            for (int file = 0; file < 8; file++) {
                int sq = rank * 8 + file;
                int piece = -1;

                // Find which piece is on this square
                for (int p = P; p <= k; p++) {
                    if (get_bit(pieceBB[p], sq)) {
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
        std::cout << "Side:     " << (side == WHITE ? "white" : "black") << "\n";
        std::cout << "Enpassant: " << (enpassant != no_sq ? enpassant : -1) << "\n";
        std::cout << "Castling:  " 
                  << ((castle & wk) ? 'K' : '-')
                  << ((castle & wq) ? 'Q' : '-')
                  << ((castle & bk) ? 'k' : '-')
                  << ((castle & bq) ? 'q' : '-') << "\n\n";
    }
}

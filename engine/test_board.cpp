#include "board.h"
#include <iostream>

int main() {
    Bitboard::init();
    MoveGen::init_all();
    Bitboard::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    Board::make_move(ENCODE_MOVE(a2, a3, P, 0, 0, 0, 0, 0), 0);
    Board::make_move(ENCODE_MOVE(a7, a6, p, 0, 0, 0, 0, 0), 0);
    Board::make_move(ENCODE_MOVE(b2, b4, P, 0, 0, 1, 0, 0), 0);
    
    MoveList move_list;
    MoveGen::generate_moves(move_list);
    std::cout << "Black moves: " << move_list.count << std::endl;
    for (int i=0; i<move_list.count; i++) {
        int src = GET_MOVE_SOURCE(move_list.moves[i]);
        int tgt = GET_MOVE_TARGET(move_list.moves[i]);
        int piece = GET_MOVE_PIECE(move_list.moves[i]);
        std::cout << char((src % 8) + 'a') << (src / 8) + 1 << char((tgt % 8) + 'a') << (tgt / 8) + 1 << " piece: " << piece << "\n";
    }
    
    return 0;
}

import chess
import chess.pgn
import random

board = chess.Board()
moves_list = []

for _ in range(50):
    b = chess.Board()
    game = chess.pgn.Game()
    node = game
    for i in range(4): # 4 half-moves = 2 full moves
        moves = list(b.legal_moves)
        m = random.choice(moves)
        b.push(m)
        node = node.add_variation(m)
    moves_list.append(game)

with open("scripts/openings.pgn", "w") as f:
    for g in moves_list:
        f.write(str(g) + "\n\n")

print("Generated 50 random 2-move PGN openings.")

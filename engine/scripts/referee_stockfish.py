#!/usr/bin/env python3
"""
Full Stockfish 17.1 Match Referee and Game Reviewer.
Analyzes games played by pawnGO, verifies adjudications, and computes accuracy & ACPL.
"""

import sys
import os
import re
import subprocess
import chess
import chess.pgn

STOCKFISH_CMD = "stockfish"

def run_referee(pgn_path, depth=12):
    if not os.path.exists(pgn_path):
        print(f"Error: PGN file not found: {pgn_path}")
        return

    sf = subprocess.Popen(
        [STOCKFISH_CMD],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    def sf_cmd(c):
        sf.stdin.write(c + '\n')
        sf.stdin.flush()

    sf_cmd("uci")
    sf_cmd("setoption name Threads value 2")
    sf_cmd("setoption name Hash value 128")
    sf_cmd("isready")

    while True:
        line = sf.stdout.readline()
        if "readyok" in line:
            break

    def evaluate_pos(board):
        fen = board.fen()
        sf_cmd(f"position fen {fen}")
        sf_cmd(f"go depth {depth}")
        score_cp = 0
        is_mate = False
        mate_moves = 0
        best_move = ""

        while True:
            line = sf.stdout.readline()
            if not line:
                break
            if "score cp" in line:
                m = re.search(r"score cp (-?\d+)", line)
                if m:
                    score_cp = int(m.group(1))
                    is_mate = False
            elif "score mate" in line:
                m = re.search(r"score mate (-?\d+)", line)
                if m:
                    mate_moves = int(m.group(1))
                    is_mate = True
                    score_cp = (10000 - abs(mate_moves)) if mate_moves > 0 else (-10000 + abs(mate_moves))
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) > 1:
                    best_move = parts[1]
                break

        white_score = score_cp if board.turn == chess.WHITE else -score_cp
        return white_score, is_mate, mate_moves, best_move

    print("=================================================================")
    print(f"   FULL STOCKFISH 17.1 MATCH REFEREE (Depth {depth})")
    print(f"   Analyzing: {pgn_path}")
    print("=================================================================\n")

    with open(pgn_path) as f:
        game_num = 0
        pawngo_total_cpl = 0
        pawngo_total_moves = 0
        pawngo_blunders = 0

        while True:
            game = chess.pgn.read_game(f)
            if game is None:
                break
            game_num += 1

            white_name = game.headers.get("White", "White")
            black_name = game.headers.get("Black", "Black")
            result = game.headers.get("Result", "*")
            termination = game.headers.get("Termination", "")
            opening = game.headers.get("Opening", "Unknown")

            is_pawngo_white = "pawngo" in white_name.lower()
            is_pawngo_black = "pawngo" in black_name.lower()

            print(f"--- Game #{game_num}: {white_name} vs {black_name} [{result}] ---")
            print(f"Opening: {opening} | Termination: {termination}")

            board = game.board()
            moves = list(game.mainline_moves())

            pawngo_game_cpl = 0
            pawngo_game_moves = 0
            game_blunders = []

            for ply, move in enumerate(moves):
                turn = board.turn
                is_pawngo_move = (turn == chess.WHITE and is_pawngo_white) or (turn == chess.BLACK and is_pawngo_black)

                score_before, _, _, best_move = evaluate_pos(board)
                board.push(move)
                score_after, _, _, _ = evaluate_pos(board)

                mover_loss = (score_before - score_after) if turn == chess.WHITE else (score_after - score_before)
                mover_loss = max(0, min(1000, mover_loss))

                if is_pawngo_move:
                    pawngo_game_cpl += mover_loss
                    pawngo_game_moves += 1
                    if mover_loss >= 150:
                        game_blunders.append((ply // 2 + 1, "White" if turn == chess.WHITE else "Black", move.uci(), best_move, mover_loss / 100))

            final_eval, final_mate, mate_val, _ = evaluate_pos(board)
            final_eval_pawns = final_eval / 100.0
            
            adjudication_valid = True
            if "adjudication" in termination.lower():
                if result == "1-0" and final_eval < 250:
                    adjudication_valid = False
                elif result == "0-1" and final_eval > -250:
                    adjudication_valid = False
                elif result == "1/2-1/2" and abs(final_eval) > 300:
                    adjudication_valid = False

            adjudication_str = "CONFIRMED DECISIVE" if adjudication_valid else "SUSPICIOUS / PREMATURE"
            if "adjudication" in termination.lower():
                print(f"Final Position Eval by SF 17.1: {final_eval_pawns:+.2f} ({adjudication_str})")

            avg_cpl = (pawngo_game_cpl / max(1, pawngo_game_moves))
            acc = max(0, min(100, 100 - (avg_cpl * 0.4)))
            print(f"pawnGO Accuracy: {acc:.1f}% | ACPL: {avg_cpl:.1f} cp | Blunders: {len(game_blunders)}")
            for b in game_blunders:
                print(f"  Move {b[0]} ({b[1]}): played {b[2]} instead of SF best {b[3]} (eval drop: -{b[4]:.2f})")

            pawngo_total_cpl += pawngo_game_cpl
            pawngo_total_moves += pawngo_game_moves
            pawngo_blunders += len(game_blunders)
            print()

        sf_cmd("quit")
        sf.terminate()

        if pawngo_total_moves > 0:
            overall_acpl = pawngo_total_cpl / pawngo_total_moves
            overall_acc = max(0, min(100, 100 - (overall_acpl * 0.4)))
            print("=================================================================")
            print("                      OVERALL REFEREE VERDICT                    ")
            print(f"Total Games Reviewed: {game_num}")
            print(f"pawnGO Overall Accuracy: {overall_acc:.1f}%")
            print(f"pawnGO Average Centipawn Loss (ACPL): {overall_acpl:.1f} cp")
            print(f"Total pawnGO Blunders: {pawngo_blunders}")
            print("=================================================================")

if __name__ == "__main__":
    pgn_file = sys.argv[1] if len(sys.argv) > 1 else "engine/scripts/games/match.pgn"
    depth = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    run_referee(pgn_file, depth)

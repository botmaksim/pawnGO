#!/usr/bin/env python3
"""
High-Performance Parallel Stockfish 17.1 Referee.
Analyzes tournament PGNs using 12 concurrent worker processes.
Evaluates ACPL, accuracy, and move quality distributions for all games.
"""

import os
import sys
import glob
import re
import json
import time
import subprocess
import multiprocessing
import chess
import chess.pgn

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
VERDICT_JSON = os.path.join(BENCH_DIR, "referee_verdict.json")
STOCKFISH_CMD = "stockfish"

def analyze_single_game(game_text, depth=12):
    import io
    game = chess.pgn.read_game(io.StringIO(game_text))
    if not game:
        return None

    white_player = game.headers.get("White", "")
    black_player = game.headers.get("Black", "")
    is_pawngo_white = "pawnGO" in white_player
    is_pawngo_black = "pawnGO" in black_player

    if not (is_pawngo_white or is_pawngo_black):
        return None

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
    sf_cmd("setoption name Threads value 1")
    sf_cmd("setoption name Hash value 32")
    sf_cmd("isready")
    while True:
        line = sf.stdout.readline()
        if "readyok" in line:
            break

    def evaluate_pos(board):
        sf_cmd(f"position fen {board.fen()}")
        sf_cmd(f"go depth {depth}")
        score_cp = 0
        best_move = ""

        while True:
            line = sf.stdout.readline()
            if not line:
                break
            if "score cp" in line:
                m = re.search(r"score cp (-?\d+)", line)
                if m:
                    score_cp = int(m.group(1))
            elif "score mate" in line:
                m = re.search(r"score mate (-?\d+)", line)
                if m:
                    mate_moves = int(m.group(1))
                    score_cp = (10000 - abs(mate_moves)) if mate_moves > 0 else (-10000 + abs(mate_moves))
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) > 1:
                    best_move = parts[1]
                break

        white_score = score_cp if board.turn == chess.WHITE else -score_cp
        return white_score, best_move

    board = game.board()
    stats = {
        "total_moves": 0,
        "total_cpl": 0,
        "best": 0,
        "good": 0,
        "inacc": 0,
        "mistake": 0,
        "blunder": 0,
        "white_moves": 0,
        "white_cpl": 0,
        "black_moves": 0,
        "black_cpl": 0
    }

    for move in game.mainline_moves():
        turn = board.turn
        is_pawngo_move = (turn == chess.WHITE and is_pawngo_white) or (turn == chess.BLACK and is_pawngo_black)

        if is_pawngo_move:
            prev_eval, sf_best = evaluate_pos(board)
            board.push(move)
            curr_eval, _ = evaluate_pos(board)

            loss = (prev_eval - curr_eval) if turn == chess.WHITE else (curr_eval - prev_eval)
            loss = max(0, min(1000, loss))

            stats["total_moves"] += 1
            stats["total_cpl"] += loss

            if turn == chess.WHITE:
                stats["white_moves"] += 1
                stats["white_cpl"] += loss
            else:
                stats["black_moves"] += 1
                stats["black_cpl"] += loss

            if loss <= 15:
                stats["best"] += 1
            elif loss <= 40:
                stats["good"] += 1
            elif loss <= 90:
                stats["inacc"] += 1
            elif loss < 180:
                stats["mistake"] += 1
            else:
                stats["blunder"] += 1
        else:
            board.push(move)

    sf_cmd("quit")
    sf.terminate()
    return stats

def worker_init():
    pass

def run_parallel_referee(depth=10, max_games_per_file=None):
    pgn_files = sorted(glob.glob(os.path.join(BENCH_DIR, "*.pgn")))
    if not pgn_files:
        print("No PGN files found to analyze.")
        return

    print(f"Collecting games from {len(pgn_files)} PGN files...")
    all_games_text = []

    for fpath in pgn_files:
        with open(fpath, "r", encoding="utf-8", errors="ignore") as fp:
            content = fp.read()
        parts = content.split("[Event ")
        count = 0
        for p in parts:
            if not p.strip():
                continue
            all_games_text.append("[Event " + p)
            count += 1
            if max_games_per_file and count >= max_games_per_file:
                break

    total_games = len(all_games_text)
    print(f"Total games to analyze: {total_games}. Starting 4 parallel workers (depth={depth})...")
    start_time = time.time()

    pool = multiprocessing.Pool(processes=4)
    
    # Process with imap_unordered for progress reporting
    results = []
    completed = 0
    step = max(1, total_games // 20)

    for res in pool.imap_unordered(analyze_single_game, all_games_text, chunksize=2):
        if res:
            results.append(res)
        completed += 1
        if completed % step == 0 or completed == total_games:
            elapsed = time.time() - start_time
            rate = completed / elapsed if elapsed > 0 else 0
            pct = (completed / total_games) * 100
            print(f"[{pct:5.1f}%] Analyzed {completed}/{total_games} games ({rate:.1f} games/sec, elapsed {elapsed:.0f}s)")

    pool.close()
    pool.join()

    # Aggregate stats
    agg = {
        "games_analyzed": len(results),
        "total_moves": sum(r["total_moves"] for r in results),
        "total_cpl": sum(r["total_cpl"] for r in results),
        "best": sum(r["best"] for r in results),
        "good": sum(r["good"] for r in results),
        "inacc": sum(r["inacc"] for r in results),
        "mistake": sum(r["mistake"] for r in results),
        "blunder": sum(r["blunder"] for r in results),
        "white_moves": sum(r["white_moves"] for r in results),
        "white_cpl": sum(r["white_cpl"] for r in results),
        "black_moves": sum(r["black_moves"] for r in results),
        "black_cpl": sum(r["black_cpl"] for r in results),
        "duration_sec": round(time.time() - start_time, 1)
    }

    if agg["total_moves"] > 0:
        agg["overall_acpl"] = round(agg["total_cpl"] / agg["total_moves"], 1)
        agg["overall_accuracy"] = round(max(0, min(100, 100 - (agg["overall_acpl"] * 0.4))), 1)

        agg["white_acpl"] = round(agg["white_cpl"] / max(1, agg["white_moves"]), 1)
        agg["white_accuracy"] = round(max(0, min(100, 100 - (agg["white_acpl"] * 0.4))), 1)

        agg["black_acpl"] = round(agg["black_cpl"] / max(1, agg["black_moves"]), 1)
        agg["black_accuracy"] = round(max(0, min(100, 100 - (agg["black_acpl"] * 0.4))), 1)

        agg["best_pct"] = round((agg["best"] / agg["total_moves"]) * 100, 1)
        agg["good_pct"] = round((agg["good"] / agg["total_moves"]) * 100, 1)
        agg["inacc_pct"] = round((agg["inacc"] / agg["total_moves"]) * 100, 1)
        agg["mistake_pct"] = round((agg["mistake"] / agg["total_moves"]) * 100, 1)
        agg["blunder_pct"] = round((agg["blunder"] / agg["total_moves"]) * 100, 1)

    with open(VERDICT_JSON, "w") as fp:
        json.dump(agg, fp, indent=2)

    print("\n=======================================================")
    print("       FULL STOCKFISH 17.1 PARALLEL REFEREE VERDICT    ")
    print("=======================================================")
    print(f"Total Games Analyzed: {agg['games_analyzed']}")
    print(f"Total pawnGO Moves  : {agg['total_moves']}")
    print(f"Overall Accuracy    : {agg['overall_accuracy']}%")
    print(f"Average ACPL        : {agg['overall_acpl']} cp")
    print(f"White Performance   : {agg['white_accuracy']}% accuracy ({agg['white_acpl']} ACPL)")
    print(f"Black Performance   : {agg['black_accuracy']}% accuracy ({agg['black_acpl']} ACPL)")
    print("-------------------------------------------------------")
    print("Move Quality Distribution:")
    print(f"  ★ Best Moves   : {agg['best']} ({agg['best_pct']}%)")
    print(f"  ✓ Good Moves   : {agg['good']} ({agg['good_pct']}%)")
    print(f"  ?! Inaccuracies: {agg['inacc']} ({agg['inacc_pct']}%)")
    print(f"  ? Mistakes     : {agg['mistake']} ({agg['mistake_pct']}%)")
    print(f"  ?? Blunders    : {agg['blunder']} ({agg['blunder_pct']}%)")
    print("=======================================================")
    print(f"Referee verdict saved to: {VERDICT_JSON}")

if __name__ == "__main__":
    depth_arg = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    limit_arg = int(sys.argv[2]) if len(sys.argv) > 2 else None
    run_parallel_referee(depth=depth_arg, max_games_per_file=limit_arg)

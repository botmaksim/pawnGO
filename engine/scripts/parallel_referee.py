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
import hashlib
import chess
import chess.pgn

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
VERDICT_JSON = os.path.join(BENCH_DIR, "referee_verdict.json")
CACHE_JSON = os.path.join(BENCH_DIR, "referee_cache.json")
STOCKFISH_CMD = "stockfish"

def analyze_single_game(arg, depth=10):
    ghash = None
    if isinstance(arg, tuple):
        ghash, game_text, depth = arg
    else:
        game_text = arg

    import io
    game = chess.pgn.read_game(io.StringIO(game_text))
    if not game:
        return (ghash, None) if ghash else None

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
    return (ghash, stats) if ghash else stats

def worker_init():
    pass

def run_parallel_referee(depth=14, max_games_per_file=None, workers=10, target_run=None):
    if target_run:
        pgn_files = sorted(glob.glob(os.path.join(BENCH_DIR, target_run, "*.pgn")))
        print(f"Targeting run directory: {target_run} ({len(pgn_files)} files)")
    else:
        pgn_files = sorted(glob.glob(os.path.join(BENCH_DIR, "**/*.pgn"), recursive=True))

    if not pgn_files:
        print("No PGN files found to analyze.")
        return

    print(f"Collecting games from {len(pgn_files)} PGN files (depth={depth})...")
    cache = {}
    if os.path.exists(CACHE_JSON):
        try:
            with open(CACHE_JSON, "r") as fp:
                cache = json.load(fp)
            print(f"Loaded {len(cache)} cached referee evaluations from {CACHE_JSON}")
        except Exception as e:
            print(f"Warning: could not read referee cache: {e}")

    all_game_hashes = []
    to_analyze = []

    for fpath in pgn_files:
        with open(fpath, "r", encoding="utf-8", errors="ignore") as fp:
            content = fp.read()
        parts = content.split("[Event ")
        count = 0
        for p in parts:
            if not p.strip():
                continue
            gtxt = "[Event " + p
            ghash = hashlib.sha256(f"{gtxt.strip()}::d{depth}".encode("utf-8")).hexdigest()[:16]
            all_game_hashes.append(ghash)
            if ghash not in cache:
                to_analyze.append((ghash, gtxt, depth))
            count += 1
            if max_games_per_file and count >= max_games_per_file:
                break

    total_games = len(all_game_hashes)
    cached_count = total_games - len(to_analyze)
    print(f"Total tournament games: {total_games}. Cached: {cached_count}, Remaining to analyze: {len(to_analyze)}.")

    start_time = time.time()
    if to_analyze:
        print(f"Starting {workers} parallel workers (depth={depth}) for {len(to_analyze)} games...")
        pool = multiprocessing.Pool(processes=workers)
        completed = 0
        step = 5

        for ghash, res in pool.imap_unordered(analyze_single_game, to_analyze, chunksize=1):
            if res:
                cache[ghash] = res
            completed += 1
            if completed % step == 0 or completed == len(to_analyze):
                elapsed = time.time() - start_time
                rate = completed / elapsed if elapsed > 0 else 0
                pct = (completed / len(to_analyze)) * 100
                print(f"[{pct:5.1f}%] Analyzed {completed}/{len(to_analyze)} games ({rate:.1f} games/sec, elapsed {elapsed:.0f}s)", flush=True)

            if completed % 50 == 0:
                try:
                    with open(CACHE_JSON, "w") as fp:
                        json.dump(cache, fp)
                except Exception:
                    pass

        pool.close()
        pool.join()

        try:
            with open(CACHE_JSON, "w") as fp:
                json.dump(cache, fp)
            print(f"Updated referee cache written ({len(cache)} games).")
        except Exception as e:
            print(f"Warning: could not save cache: {e}")

    # Gather results for all games in the tournament
    results = [cache[ghash] for ghash in all_game_hashes if ghash in cache and cache[ghash]]

    # Aggregate stats
    agg = {
        "run": target_run or "all",
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

    target_verdict = os.path.join(BENCH_DIR, target_run, "referee_verdict.json") if target_run else VERDICT_JSON
    with open(target_verdict, "w") as fp:
        json.dump(agg, fp, indent=2)

    print("\n=======================================================")
    print(f"   STOCKFISH 17.1 REFEREE VERDICT ({target_run.upper() if target_run else 'ALL'})")
    print("=======================================================")
    print(f"Total Games Analyzed: {agg['games_analyzed']}")
    print(f"Total pawnGO Moves  : {agg['total_moves']}")
    print(f"Overall Accuracy    : {agg['overall_accuracy']}%")
    print(f"Average ACPL        : {agg['overall_acpl']} cp")
    print(f"White Performance   : {agg['white_accuracy']}% accuracy ({agg['white_acpl']} ACPL)")
    print(f"Black Performance   : {agg['black_accuracy']}% accuracy ({agg['black_acpl']} ACPL)")
    print("-------------------------------------------------------")
    print("Move Quality Distribution:")
    print(f"  [*] Best Moves   : {agg['best']} ({agg['best_pct']}%)")
    print(f"  [+] Good Moves   : {agg['good']} ({agg['good_pct']}%)")
    print(f"  [?!] Inaccuracies: {agg['inacc']} ({agg['inacc_pct']}%)")
    print(f"  [?] Mistakes     : {agg['mistake']} ({agg['mistake_pct']}%)")
    print(f"  [??] Blunders    : {agg['blunder']} ({agg['blunder_pct']}%)")
    print("=======================================================")
    print(f"Referee verdict saved to: {target_verdict}")

if __name__ == "__main__":
    depth_arg = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] != "None" else 14
    limit_arg = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] != "None" else None
    workers_arg = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] != "None" else 14
    run_arg = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] != "None" else None
    run_parallel_referee(depth=depth_arg, max_games_per_file=limit_arg, workers=workers_arg, target_run=run_arg)

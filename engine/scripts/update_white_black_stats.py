#!/usr/bin/env python3
import os
import re
import json
import glob

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
RESULTS_JSON = os.path.join(BENCH_DIR, "results.json")

def parse_pgn_colors(pgn_path):
    if not os.path.exists(pgn_path):
        return None

    with open(pgn_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    games = content.split("[Event ")
    white_stats = {"wins": 0, "losses": 0, "draws": 0, "points": 0.0, "total": 0, "win_rate": 0.0}
    black_stats = {"wins": 0, "losses": 0, "draws": 0, "points": 0.0, "total": 0, "win_rate": 0.0}

    for g in games:
        if not g.strip():
            continue
        is_w = '[White "pawnGO"]' in g
        is_b = '[Black "pawnGO"]' in g
        if not (is_w or is_b):
            continue

        res_match = re.search(r'\[Result "([^"]+)"\]', g)
        if not res_match:
            continue
        res = res_match.group(1)

        if is_w:
            white_stats["total"] += 1
            if res == "1-0":
                white_stats["wins"] += 1
                white_stats["points"] += 1.0
            elif res == "0-1":
                white_stats["losses"] += 1
            elif res == "1/2-1/2":
                white_stats["draws"] += 1
                white_stats["points"] += 0.5
        elif is_b:
            black_stats["total"] += 1
            if res == "0-1":
                black_stats["wins"] += 1
                black_stats["points"] += 1.0
            elif res == "1-0":
                black_stats["losses"] += 1
            elif res == "1/2-1/2":
                black_stats["draws"] += 1
                black_stats["points"] += 0.5

    if white_stats["total"] > 0:
        white_stats["win_rate"] = round((white_stats["points"] / white_stats["total"]) * 100, 1)
    if black_stats["total"] > 0:
        black_stats["win_rate"] = round((black_stats["points"] / black_stats["total"]) * 100, 1)

    return {"white": white_stats, "black": black_stats}

def update_all():
    with open(RESULTS_JSON, "r") as f:
        data = json.load(f)

    for prefix, tc_key in [("fast", "fast"), ("slow", "slow"), ("ultra", "ultraslow")]:
        for pgn in glob.glob(os.path.join(BENCH_DIR, f"{prefix}_*.pgn")):
            lvl = re.search(rf"{prefix}_(\d+)\.pgn", pgn)
            if not lvl:
                continue
            lvl_str = lvl.group(1)
            color_data = parse_pgn_colors(pgn)
            if color_data and lvl_str in data and tc_key in data[lvl_str]:
                data[lvl_str][tc_key]["white"] = color_data["white"]
                data[lvl_str][tc_key]["black"] = color_data["black"]

    with open(RESULTS_JSON, "w") as f:
        json.dump(data, f, indent=2)

    print("Updated results.json with exact White and Black stats for all existing PGNs.")

if __name__ == "__main__":
    update_all()

#!/usr/bin/env python3
"""
Generate publication-quality advanced analytics charts for pawnGO README:
1. assets/benchmark_game_phases.png - Tactical Middlegame Dominance vs Late Endgame Grind
2. assets/benchmark_openings.png - Performance Across Major Opening Repertoires
"""

import os
import glob
import re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
ASSETS_DIR = os.path.join(PROJECT_DIR, "assets")

os.makedirs(ASSETS_DIR, exist_ok=True)

def set_dark_theme(ax):
    ax.set_facecolor("#161b22")
    ax.grid(color="#30363d", linestyle="--", linewidth=0.7, alpha=0.7)
    for spine in ax.spines.values():
        spine.set_color("#30363d")
def normalize_opening(op):
    low = op.lower().strip()
    if "sicilian" in low:
        return "Sicilian Defense"
    if "french" in low:
        return "French Defense"
    if "caro-kann" in low:
        return "Caro-Kann Defense"
    if "queen's gambit declined" in low or "semi-slav" in low:
        return "Queen's Gambit Declined"
    if "ruy lopez" in low or "spanish" in low:
        return "Ruy Lopez"
    if "english" in low:
        return "English Opening"
    if "catalan" in low:
        return "Catalan Opening"
    if "king's indian" in low:
        return "King's Indian Defense"
    if "nimzo-indian" in low:
        return "Nimzo-Indian Defense"
    if "gruenfeld" in low:
        return "Grünfeld Defense"
    if "reti" in low:
        return "Réti Opening"
    if "scotch" in low:
        return "Scotch Game"
    if "petrov" in low:
        return "Petrov's Defense"
    if "pirc" in low or "robatsch" in low:
        return "Pirc / Modern Defense"
    if "giuoco" in low:
        return "Giuoco Piano"
    if "two knights" in low:
        return "Two Knights Defense"
    if "dutch" in low:
        return "Dutch Defense"
    if "queen's bishop game" in low:
        return "Queen's Bishop Game"
    if "queen's indian" in low:
        return "Queen's Indian Defense"
    return op.title()

def generate_advanced_charts():
    pgn_files = glob.glob(os.path.join(BENCH_DIR, "*.pgn"))
    if not pgn_files:
        print("No PGN files found.")
        return

    win_lengths = []
    loss_lengths = []
    draw_lengths = []

    phase_bins = {
        "<35 moves\n(Opening / Early Mid)": [0, 0, 0],
        "35-50 moves\n(Tactical Middlegame)": [0, 0, 0],
        "50-70 moves\n(Early Endgame)": [0, 0, 0],
        ">70 moves\n(Deep Technical Grind)": [0, 0, 0]
    }

    openings_data = {}

    for f in pgn_files:
        with open(f, "r", encoding="utf-8", errors="ignore") as fp:
            content = fp.read()
        games = content.split("[Event ")
        for g in games:
            if not g.strip():
                continue
            ply_m = re.search(r'\[PlyCount "(\d+)"\]', g)
            res_m = re.search(r'\[Result "([^"]+)"\]', g)
            if not (ply_m and res_m):
                continue
            plies = int(ply_m.group(1))
            moves = plies // 2
            res = res_m.group(1)
            is_w = '[White "pawnGO"]' in g
            is_b = '[Black "pawnGO"]' in g
            if not (is_w or is_b):
                continue

            is_win = (is_w and res == "1-0") or (is_b and res == "0-1")
            is_loss = (is_w and res == "0-1") or (is_b and res == "1-0")
            is_draw = (res == "1/2-1/2")

            if is_win:
                win_lengths.append(moves)
            elif is_loss:
                loss_lengths.append(moves)
            elif is_draw:
                draw_lengths.append(moves)

            if moves < 35:
                b_key = "<35 moves\n(Opening / Early Mid)"
            elif moves <= 50:
                b_key = "35-50 moves\n(Tactical Middlegame)"
            elif moves <= 70:
                b_key = "50-70 moves\n(Early Endgame)"
            else:
                b_key = ">70 moves\n(Deep Technical Grind)"

            if is_win:
                phase_bins[b_key][0] += 1
            elif is_loss:
                phase_bins[b_key][1] += 1
            elif is_draw:
                phase_bins[b_key][2] += 1

            op_m = re.search(r'\[Opening "([^"]+)"\]', g)
            if op_m:
                op_raw = normalize_opening(op_m.group(1).split(",")[0])
                if op_raw not in openings_data:
                    openings_data[op_raw] = [0, 0, 0]
                if is_win:
                    openings_data[op_raw][0] += 1
                elif is_loss:
                    openings_data[op_raw][1] += 1
                elif is_draw:
                    openings_data[op_raw][2] += 1

    plt.style.use("dark_background")

    # =========================================================================
    # 1. GAME PHASE DOMINANCE VS ENDGAME CONVERSION (benchmark_game_phases.png)
    # =========================================================================
    fig1, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6.5), dpi=300, facecolor="#0d1117")
    set_dark_theme(ax1)
    set_dark_theme(ax2)

    bin_labels = list(phase_bins.keys())
    bin_rates = []
    bin_totals = []
    for k in bin_labels:
        w, l, d = phase_bins[k]
        tot = w + l + d
        pct = round(((w + 0.5 * d) / tot) * 100, 1) if tot > 0 else 0
        bin_rates.append(pct)
        bin_totals.append(tot)

    x_bins = np.arange(len(bin_labels))
    colors = ["#2ea043" if r >= 60 else ("#58a6ff" if r >= 50 else "#f0883e") for r in bin_rates]
    bars = ax1.bar(x_bins, bin_rates, 0.55, color=colors, edgecolor="#30363d")
    ax1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

    for i, r in enumerate(bin_rates):
        w, l, d = phase_bins[bin_labels[i]]
        ax1.annotate(f"{r}%\n(+{w} -{l} ={d})", (x_bins[i], r + 3),
                     ha="center", fontsize=9.5, fontweight="bold", color="#ffffff")

    ax1.set_title("pawnGO Win Rate by Game Phase / Duration", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax1.set_xlabel("Game Phase Bracket", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_ylabel("Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_xticks(x_bins)
    ax1.set_xticklabels(bin_labels, fontsize=9.5)
    ax1.set_ylim(0, 105)
    ax1.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    # Panel B: Histogram / Distribution of Wins vs Losses
    max_m = 110
    bins_hist = np.linspace(10, max_m, 25)
    ax2.hist(win_lengths, bins=bins_hist, alpha=0.65, color="#2ea043", label=f"Wins (Median: {np.median(win_lengths):.0f} moves)", edgecolor="#161b22", density=True)
    ax2.hist(loss_lengths, bins=bins_hist, alpha=0.6, color="#da3633", label=f"Losses (Median: {np.median(loss_lengths):.0f} moves)", edgecolor="#161b22", density=True)

    ax2.axvline(np.mean(win_lengths), color="#3fb950", linestyle="--", linewidth=2, label=f"Mean Win: {np.mean(win_lengths):.1f}m")
    ax2.axvline(np.mean(loss_lengths), color="#f85149", linestyle="--", linewidth=2, label=f"Mean Loss: {np.mean(loss_lengths):.1f}m")

    ax2.set_title("Conversion Velocity: Tactical Knockouts vs Endgame Grinds", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax2.set_xlabel("Game Length (Moves)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_ylabel("Normalized Game Frequency Density", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    fig1.suptitle("pawnGO Phase Analysis: Tactical Middlegame Dominance vs Deep Technical Endgames", fontsize=16, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out1 = os.path.join(ASSETS_DIR, "benchmark_game_phases.png")
    fig1.savefig(out1, facecolor=fig1.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig1)

    # =========================================================================
    # 2. PERFORMANCE ACROSS MAJOR OPENINGS (benchmark_openings.png)
    # =========================================================================
    top_ops = sorted([k for k in openings_data.keys() if sum(openings_data[k]) >= 60],
                     key=lambda k: sum(openings_data[k]), reverse=True)[:10]

    if top_ops:
        fig2, ax = plt.subplots(figsize=(14, 7), dpi=300, facecolor="#0d1117")
        set_dark_theme(ax)

        op_rates = []
        op_totals = []
        for k in top_ops:
            w, l, d = openings_data[k]
            tot = w + l + d
            pct = round(((w + 0.5 * d) / tot) * 100, 1)
            op_rates.append(pct)
            op_totals.append(tot)

        y_pos = np.arange(len(top_ops))
        bar_colors = ["#2ea043" if r >= 52 else ("#58a6ff" if r >= 48 else "#f0883e") for r in op_rates]
        bars = ax.barh(y_pos, op_rates, 0.6, color=bar_colors, edgecolor="#30363d")
        ax.axvline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

        for i, (r, tot) in enumerate(zip(op_rates, op_totals)):
            w, l, d = openings_data[top_ops[i]]
            ax.annotate(f" {r}%  (+{w} -{l} ={d}, {tot}G)", (r, y_pos[i]),
                        va="center", fontsize=9.5, fontweight="bold", color="#ffffff")

        ax.set_yticks(y_pos)
        ax.set_yticklabels(top_ops, fontsize=10.5, fontweight="bold")
        ax.invert_yaxis()
        ax.set_xlim(0, 100)
        ax.set_xlabel("Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax.set_title("pawnGO Performance Across Major Opening Repertoires (Min. 60 Games)", fontsize=14, fontweight="bold", color="#f0f6fc", pad=12)
        ax.legend(loc="lower right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

        plt.tight_layout()
        out2 = os.path.join(ASSETS_DIR, "benchmark_openings.png")
        fig2.savefig(out2, facecolor=fig2.get_facecolor(), edgecolor="none", bbox_inches="tight")
        plt.close(fig2)

    print("Advanced analytics charts generated:")
    print(f"  - {out1}")
    if top_ops:
        print(f"  - {out2}")

if __name__ == "__main__":
    generate_advanced_charts()

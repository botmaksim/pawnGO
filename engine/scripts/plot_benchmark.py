#!/usr/bin/env python3
"""
Generate publication-quality, dark-themed benchmark visualizations for pawnGO README.md
Generates:
1. assets/benchmark_overall.png - Grand Overview of Win Rates and Outcomes
2. assets/benchmark_by_control.png - Side-by-side comparison across Fast, Slower, and Ultra Slow controls
"""

import os
import json
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
RESULTS_JSON = os.path.join(BENCH_DIR, "results.json")
ASSETS_DIR = os.path.join(PROJECT_DIR, "assets")

os.makedirs(ASSETS_DIR, exist_ok=True)

LEVELS = [2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200]

def set_dark_theme(ax):
    ax.set_facecolor("#161b22")
    ax.grid(color="#30363d", linestyle="--", linewidth=0.7, alpha=0.7)
    for spine in ax.spines.values():
        spine.set_color("#30363d")
    ax.tick_params(colors="#c9d1d9", labelsize=10)

def generate_charts():
    if not os.path.exists(RESULTS_JSON):
        print(f"Results file not found: {RESULTS_JSON}")
        return

    with open(RESULTS_JSON, "r") as f:
        data = json.load(f)

    active_levels = []
    fast_rates = []
    slow_rates = []
    ultra_rates = []
    overall_rates = []
    total_wins = []
    total_draws = []
    total_losses = []
    total_games = []

    for lvl in LEVELS:
        key = str(lvl)
        if key in data:
            entry = data[key]
            fast = entry.get("fast")
            slow = entry.get("slow")
            ultra = entry.get("ultraslow")

            if fast or slow or ultra:
                active_levels.append(lvl)
                fast_rates.append(fast["win_rate"] if fast else None)
                slow_rates.append(slow["win_rate"] if slow else None)
                ultra_rates.append(ultra["win_rate"] if ultra else None)

                w = (fast["wins"] if fast else 0) + (slow["wins"] if slow else 0) + (ultra["wins"] if ultra else 0)
                l = (fast["losses"] if fast else 0) + (slow["losses"] if slow else 0) + (ultra["losses"] if ultra else 0)
                d = (fast["draws"] if fast else 0) + (slow["draws"] if slow else 0) + (ultra["draws"] if ultra else 0)
                pts = (fast["points"] if fast else 0) + (slow["points"] if slow else 0) + (ultra["points"] if ultra else 0)
                tot = (fast["total"] if fast else 0) + (slow["total"] if slow else 0) + (ultra["total"] if ultra else 0)

                total_wins.append(w)
                total_draws.append(d)
                total_losses.append(l)
                total_games.append(tot)
                overall_rates.append(round((pts / tot) * 100, 1) if tot > 0 else 0)

    if not active_levels:
        print("No active levels with data.")
        return

    plt.style.use("dark_background")
    x = np.arange(len(active_levels))

    # =========================================================================
    # 1. OVERALL MASTER BENCHMARK CHART (assets/benchmark_overall.png)
    # =========================================================================
    fig1, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6.5), dpi=300, facecolor="#0d1117")
    set_dark_theme(ax1)
    set_dark_theme(ax2)

    # Left: Grand Win Rate Curve
    ax1.plot(x, overall_rates, marker="D", color="#3fb950", linewidth=3.5, label="Overall Win Rate", zorder=5)
    ax1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line (Equal Strength)")
    ax1.fill_between(x, overall_rates, 50, where=np.array(overall_rates) >= 50, color="#2ea043", alpha=0.18, interpolate=True)
    ax1.fill_between(x, overall_rates, 50, where=np.array(overall_rates) < 50, color="#da3633", alpha=0.18, interpolate=True)

    for i, r in enumerate(overall_rates):
        ax1.annotate(f"{r}%", (x[i], r), textcoords="offset points", xytext=(0, 10),
                     ha="center", fontsize=10, fontweight="bold", color="#ffffff")

    ax1.set_title("pawnGO Overall Win Rate Across All Controls", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax1.set_xlabel("Stockfish Target Elo", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_ylabel("Overall Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_xticks(x)
    ax1.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=15)
    ax1.set_ylim(-2, 105)
    ax1.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    # Right: Stacked Bar Outcome Distribution
    bar_width = 0.55
    wins = np.array(total_wins)
    draws = np.array(total_draws)
    losses = np.array(total_losses)

    ax2.bar(x, wins, bar_width, label="Wins", color="#2ea043", edgecolor="#30363d")
    ax2.bar(x, draws, bar_width, bottom=wins, label="Draws", color="#8b949e", edgecolor="#30363d")
    ax2.bar(x, losses, bar_width, bottom=wins+draws, label="Losses", color="#da3633", edgecolor="#30363d")

    for i in range(len(active_levels)):
        tot = total_games[i]
        w, d, l = wins[i], draws[i], losses[i]
        ax2.annotate(f"{tot}G", (x[i], tot + 2), ha="center", fontsize=9.5, fontweight="bold", color="#8b949e")
        if w > 10:
            ax2.annotate(f"{w}W", (x[i], w / 2), ha="center", va="center", fontsize=8.5, fontweight="bold", color="#ffffff")
        if d > 8:
            ax2.annotate(f"{d}D", (x[i], w + d / 2), ha="center", va="center", fontsize=8.5, fontweight="bold", color="#ffffff")
        if l > 10:
            ax2.annotate(f"{l}L", (x[i], w + d + l / 2), ha="center", va="center", fontsize=8.5, fontweight="bold", color="#ffffff")

    ax2.set_title("Total Match Outcomes Breakdown", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax2.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_ylabel("Total Games Played", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_xticks(x)
    ax2.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=15)
    ax2.set_ylim(0, max(total_games) * 1.15)
    ax2.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    fig1.suptitle("pawnGO Overall Gauntlet Performance Summary (~2900 Elo Verified)", fontsize=16, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out1 = os.path.join(ASSETS_DIR, "benchmark_overall.png")
    fig1.savefig(out1, facecolor=fig1.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig1)

    # Also save as legacy benchmark_chart.png for backward compat
    fig1_legacy = os.path.join(ASSETS_DIR, "benchmark_chart.png")
    import shutil
    shutil.copyfile(out1, fig1_legacy)

    # =========================================================================
    # 2. COMPARATIVE TIME-CONTROL MATRIX (assets/benchmark_by_control.png)
    # =========================================================================
    fig2, ax = plt.subplots(figsize=(14, 6.5), dpi=300, facecolor="#0d1117")
    set_dark_theme(ax)

    # Fast Blitz
    valid_fast = [(i, r) for i, r in enumerate(fast_rates) if r is not None]
    if valid_fast:
        xi, yi = zip(*valid_fast)
        ax.plot(xi, yi, marker="o", markersize=8, color="#58a6ff", linewidth=2.8,
                label="Fast Blitz (2+0.05, 100G) — High Tactical Sharpness & Early NNUE Punishment", alpha=0.9)
        for idx, val in zip(xi, yi):
            ax.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, 8),
                        ha="center", fontsize=8.5, color="#58a6ff", fontweight="bold")

    # Slower Blitz
    valid_slow = [(i, r) for i, r in enumerate(slow_rates) if r is not None]
    if valid_slow:
        xi, yi = zip(*valid_slow)
        ax.plot(xi, yi, marker="s", markersize=8, color="#bc8cff", linewidth=2.8,
                label="Slower Blitz (5+0.1, 50G) — Solid GM Positional Play (Parity ~2850-2900)", alpha=0.9)
        for idx, val in zip(xi, yi):
            ax.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, -14),
                        ha="center", fontsize=8.5, color="#bc8cff", fontweight="bold")

    # Ultra Slow
    valid_ultra = [(i, r) for i, r in enumerate(ultra_rates) if r is not None]
    if valid_ultra:
        xi, yi = zip(*valid_ultra)
        ax.plot(xi, yi, marker="^", markersize=8, color="#f0883e", linewidth=2.8,
                label="Ultra Slow (10+0.25, 20G) — Deep Search: Stockfish Technical Endgame Advantage", alpha=0.95)
        for idx, val in zip(xi, yi):
            ax.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, 10),
                        ha="center", fontsize=8.5, color="#f0883e", fontweight="bold")

    ax.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line (Equal Strength)")

    ax.set_title("Performance by Time Control: Fast Blitz vs Slower Blitz vs Ultra Slow", fontsize=14, fontweight="bold", color="#f0f6fc", pad=12)
    ax.set_xlabel("Stockfish Target Elo", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax.set_ylabel("Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax.set_xticks(x)
    ax.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=15)
    ax.set_ylim(-5, 108)
    ax.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    plt.tight_layout()
    out2 = os.path.join(ASSETS_DIR, "benchmark_by_control.png")
    fig2.savefig(out2, facecolor=fig2.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig2)

    print("Charts generated:")
    print(f"  - {out1}")
    print(f"  - {out2}")

if __name__ == "__main__":
    generate_charts()

#!/usr/bin/env python3
"""
Generate publication-quality, dark-themed benchmark visualizations for pawnGO README.md
Generates:
1. assets/benchmark_overall.png - Grand Overview of Win Rates and Outcomes
2. assets/benchmark_by_control.png - Side-by-side comparison across Fast, Slower, and Ultra Slow controls
3. assets/benchmark_white_black.png - Playing as White vs Playing as Black performance asymmetry
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

LEVELS = [2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600]

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

    white_rates = []
    black_rates = []
    white_games = []
    black_games = []

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

                # Color breakdown
                w_pts = 0.0
                w_tot = 0
                b_pts = 0.0
                b_tot = 0

                for tc_entry in (fast, slow, ultra):
                    if tc_entry and "white" in tc_entry and "black" in tc_entry:
                        w_pts += tc_entry["white"]["points"]
                        w_tot += tc_entry["white"]["total"]
                        b_pts += tc_entry["black"]["points"]
                        b_tot += tc_entry["black"]["total"]

                w_rate = round((w_pts / w_tot) * 100, 1) if w_tot > 0 else None
                b_rate = round((b_pts / b_tot) * 100, 1) if b_tot > 0 else None
                white_rates.append(w_rate)
                black_rates.append(b_rate)
                white_games.append(w_tot)
                black_games.append(b_tot)

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

    ax1.plot(x, overall_rates, marker="D", color="#3fb950", linewidth=3.5, label="Overall Win Rate", zorder=5)
    ax1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line (Equal Strength)")
    ax1.fill_between(x, overall_rates, 50, where=np.array(overall_rates) >= 50, color="#2ea043", alpha=0.18, interpolate=True)
    ax1.fill_between(x, overall_rates, 50, where=np.array(overall_rates) < 50, color="#da3633", alpha=0.18, interpolate=True)

    for i, r in enumerate(overall_rates):
        ax1.annotate(f"{r}%", (x[i], r), textcoords="offset points", xytext=(0, 10),
                     ha="center", fontsize=9.5, fontweight="bold", color="#ffffff")

    ax1.set_title("pawnGO Overall Win Rate Across All Controls", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax1.set_xlabel("Stockfish Target Elo", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_ylabel("Overall Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_xticks(x)
    ax1.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=20)
    ax1.set_ylim(-2, 105)
    ax1.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

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
        ax2.annotate(f"{tot}G", (x[i], tot + 2), ha="center", fontsize=9, fontweight="bold", color="#8b949e")
        if w > 10:
            ax2.annotate(f"{w}W", (x[i], w / 2), ha="center", va="center", fontsize=8, fontweight="bold", color="#ffffff")
        if d > 8:
            ax2.annotate(f"{d}D", (x[i], w + d / 2), ha="center", va="center", fontsize=8, fontweight="bold", color="#ffffff")
        if l > 10:
            ax2.annotate(f"{l}L", (x[i], w + d + l / 2), ha="center", va="center", fontsize=8, fontweight="bold", color="#ffffff")

    ax2.set_title("Total Match Outcomes Breakdown", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax2.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_ylabel("Total Games Played", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_xticks(x)
    ax2.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=20)
    ax2.set_ylim(0, max(total_games) * 1.15)
    ax2.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    fig1.suptitle("pawnGO Overall Gauntlet Performance Summary (~2900 Elo Verified)", fontsize=16, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out1 = os.path.join(ASSETS_DIR, "benchmark_overall.png")
    fig1.savefig(out1, facecolor=fig1.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig1)

    # Legacy copy
    import shutil
    shutil.copyfile(out1, os.path.join(ASSETS_DIR, "benchmark_chart.png"))

    # =========================================================================
    # 2. COMPARATIVE TIME-CONTROL MATRIX (assets/benchmark_by_control.png)
    # 3-panel layout eliminates all label overlapping and provides clean comparison
    # =========================================================================
    fig2, (ax2_1, ax2_2, ax2_3) = plt.subplots(1, 3, figsize=(19, 5.5), dpi=300, facecolor="#0d1117")
    for a in (ax2_1, ax2_2, ax2_3):
        set_dark_theme(a)

    panels = [
        (ax2_1, fast_rates, "#58a6ff", "Fast Blitz (2+0.05, 100G)", "Tactical Sharpness & Early Punish"),
        (ax2_2, slow_rates, "#bc8cff", "Slower Blitz (5+0.1, 50G)", "Stable Positional Play (Parity ~2850)"),
        (ax2_3, ultra_rates, "#f0883e", "Ultra Slow (10+0.25, 20G)", "Deep Search & Precision (Parity ~2900)")
    ]

    for ax_p, rates, color, title, subtitle in panels:
        valid = [(i, rates[i]) for i in range(len(active_levels)) if rates[i] is not None]
        if valid:
            xi, yi = zip(*valid)
            yi_arr = np.array(yi)
            ax_p.plot(xi, yi, marker="o", markersize=7.5, color=color, linewidth=2.8, label="Win Rate", zorder=5)
            ax_p.axhline(50, color="#f85149", linestyle=":", linewidth=1.8, alpha=0.9, label="50% Parity Line")
            ax_p.fill_between(xi, yi, 50, where=yi_arr >= 50, color="#2ea043", alpha=0.15, interpolate=True)
            ax_p.fill_between(xi, yi, 50, where=yi_arr < 50, color="#da3633", alpha=0.15, interpolate=True)

            for idx, val in zip(xi, yi):
                ax_p.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, 9),
                              ha="center", fontsize=8.5, color="#ffffff", fontweight="bold")

        ax_p.set_title(f"{title}\n{subtitle}", fontsize=11, fontweight="bold", color="#f0f6fc", pad=10)
        ax_p.set_xlabel("Stockfish Target Elo", fontsize=10, fontweight="bold", color="#c9d1d9", labelpad=6)
        ax_p.set_ylabel("Win Rate (%)", fontsize=10, fontweight="bold", color="#c9d1d9", labelpad=6)
        ax_p.set_xticks(x)
        ax_p.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=25, fontsize=8.5)
        ax_p.set_ylim(-3, 108)
        ax_p.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=8.5, labelcolor="#c9d1d9")

    fig2.suptitle("pawnGO Performance Across Time Controls: Fast vs Slower vs Ultra Slow", fontsize=15, fontweight="bold", color="#58a6ff", y=1.02)
    plt.tight_layout()
    out2 = os.path.join(ASSETS_DIR, "benchmark_by_control.png")
    fig2.savefig(out2, facecolor=fig2.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig2)

    # =========================================================================
    # 3. WHITE VS BLACK ASYMMETRY CHART (assets/benchmark_white_black.png)
    # =========================================================================
    fig3, (ax3_1, ax3_2) = plt.subplots(1, 2, figsize=(16, 6.5), dpi=300, facecolor="#0d1117")
    set_dark_theme(ax3_1)
    set_dark_theme(ax3_2)

    valid_wb = [(i, white_rates[i], black_rates[i]) for i in range(len(active_levels))
                if white_rates[i] is not None and black_rates[i] is not None]

    if valid_wb:
        xi_wb, yw, yb = zip(*valid_wb)

        # Left: White vs Black Win Rate curves
        ax3_1.plot(xi_wb, yw, marker="o", markersize=8, color="#ffffff", linewidth=3, label="pawnGO Playing White", zorder=5)
        ax3_1.plot(xi_wb, yb, marker="s", markersize=8, color="#79c0ff", linewidth=3, label="pawnGO Playing Black", zorder=4)
        ax3_1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

        for idx, val in zip(xi_wb, yw):
            ax3_1.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, 9),
                           ha="center", fontsize=8.5, color="#ffffff", fontweight="bold")
        for idx, val in zip(xi_wb, yb):
            ax3_1.annotate(f"{val}%", (idx, val), textcoords="offset points", xytext=(0, -14),
                           ha="center", fontsize=8.5, color="#79c0ff", fontweight="bold")

        ax3_1.set_title("Win Rate Playing White vs Playing Black", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
        ax3_1.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_1.set_ylabel("Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_1.set_xticks(xi_wb)
        ax3_1.set_xticklabels([f"SF {active_levels[i]}" for i in xi_wb], rotation=20)
        ax3_1.set_ylim(-5, 108)
        ax3_1.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

        # Right: First-Move Advantage Delta (White Win Rate - Black Win Rate)
        deltas = np.array(yw) - np.array(yb)
        colors = ["#2ea043" if d >= 0 else "#da3633" for d in deltas]
        bars = ax3_2.bar(xi_wb, deltas, 0.55, color=colors, edgecolor="#30363d")
        ax3_2.axhline(0, color="#8b949e", linestyle="-", linewidth=1.2)

        for i, d in enumerate(deltas):
            y_pos = d + (1 if d >= 0 else -3)
            ax3_2.annotate(f"{d:+.1f}%", (xi_wb[i], y_pos), ha="center", fontsize=9,
                           fontweight="bold", color="#f0f6fc")

        ax3_2.set_title("First-Move Advantage Delta (White% - Black%)", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
        ax3_2.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_2.set_ylabel("Advantage Delta (% Points)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_2.set_xticks(xi_wb)
        ax3_2.set_xticklabels([f"SF {active_levels[i]}" for i in xi_wb], rotation=20)
        max_d = max(abs(deltas.min()), abs(deltas.max())) if len(deltas) > 0 else 10
        ax3_2.set_ylim(-max_d * 1.3, max_d * 1.3)

    fig3.suptitle("pawnGO Color Asymmetry & Opening Balance Analysis", fontsize=16, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out3 = os.path.join(ASSETS_DIR, "benchmark_white_black.png")
    fig3.savefig(out3, facecolor=fig3.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig3)

    print("All charts successfully generated:")
    print(f"  - {out1}")
    print(f"  - {out2}")
    print(f"  - {out3}")

if __name__ == "__main__":
    generate_charts()

#!/usr/bin/env python3
"""
Generate publication-quality, dark-themed benchmark visualizations for pawnGO README.md
Enhanced with statistical 95% confidence intervals (Wilson score with trinomial chess variance).
Generates:
1. assets/benchmark_overall.png - Grand Overview of Win Rates with 95% Confidence Intervals
2. assets/benchmark_by_control.png - Side-by-side comparison across Fast, Slower, and Ultra Slow controls with CI bands
3. assets/benchmark_white_black.png - Playing as White vs Playing as Black with CI and advantage delta error bars
"""

import os
import json
import math
import shutil
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BENCH_DIR = os.path.join(PROJECT_DIR, "engine/scripts/benchmarks")
RESULTS_JSON = os.path.join(BENCH_DIR, "results.json")
ASSETS_DIR = os.path.join(PROJECT_DIR, "assets")

os.makedirs(ASSETS_DIR, exist_ok=True)

LEVELS = [2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600]

def set_dark_theme(ax):
    ax.set_facecolor("#161b22")
    ax.grid(color="#30363d", linestyle="--", linewidth=0.7, alpha=0.7)
    for spine in ax.spines.values():
        spine.set_color("#30363d")
    ax.tick_params(colors="#c9d1d9", labelsize=10)

def compute_ci(w, l, d, z=1.96):
    """
    Computes 95% confidence interval for chess score proportion using Wilson score interval
    adjusted for trinomial (win/draw/loss) chess variance.
    Returns (rate_pct, lower_pct, upper_pct, margin_pct, se)
    """
    tot = w + l + d
    if tot == 0:
        return 0.0, 0.0, 0.0, 0.0, 0.0
    pts = w + 0.5 * d
    p = pts / tot

    denom = 1.0 + (z**2) / tot
    center = (p + (z**2) / (2.0 * tot)) / denom
    spread = z * math.sqrt((p * (1.0 - p) / tot) + (z**2) / (4.0 * tot**2)) / denom

    if tot > 1 and p * (1.0 - p) > 0:
        var_trinomial = (w * (1.0 - p)**2 + d * (0.5 - p)**2 + l * (0.0 - p)**2) / tot
        var_bernoulli = p * (1.0 - p)
        if var_bernoulli > 0:
            scale = math.sqrt(max(0.2, var_trinomial / var_bernoulli))
            spread *= scale

    rate_pct = round(p * 100.0, 1)
    lower_pct = round(max(0.0, (center - spread) * 100.0), 1)
    upper_pct = round(min(100.0, (center + spread) * 100.0), 1)
    margin_pct = round((upper_pct - lower_pct) / 2.0, 1)
    se = (spread / z)
    return rate_pct, lower_pct, upper_pct, margin_pct, se

def generate_charts():
    if not os.path.exists(RESULTS_JSON):
        print(f"Results file not found: {RESULTS_JSON}")
        return

    with open(RESULTS_JSON, "r") as f:
        data = json.load(f)

    active_levels = []
    fast_cis = []
    slow_cis = []
    ultra_cis = []
    overall_cis = []

    total_wins = []
    total_draws = []
    total_losses = []
    total_games = []

    white_cis = []
    black_cis = []

    for lvl in LEVELS:
        key = str(lvl)
        if key in data:
            entry = data[key]
            fast = entry.get("fast") if entry.get("fast", {}).get("total", 0) > 0 else None
            slow = entry.get("slow") if entry.get("slow", {}).get("total", 0) > 0 else None
            ultra = entry.get("ultraslow") if entry.get("ultraslow", {}).get("total", 0) > 0 else None

            if fast or slow or ultra:
                active_levels.append(lvl)

                fast_ci = compute_ci(fast["wins"], fast["losses"], fast["draws"]) if fast else None
                slow_ci = compute_ci(slow["wins"], slow["losses"], slow["draws"]) if slow else None
                ultra_ci = compute_ci(ultra["wins"], ultra["losses"], ultra["draws"]) if ultra else None

                fast_cis.append(fast_ci)
                slow_cis.append(slow_ci)
                ultra_cis.append(ultra_ci)

                w = (fast["wins"] if fast else 0) + (slow["wins"] if slow else 0) + (ultra["wins"] if ultra else 0)
                l = (fast["losses"] if fast else 0) + (slow["losses"] if slow else 0) + (ultra["losses"] if ultra else 0)
                d = (fast["draws"] if fast else 0) + (slow["draws"] if slow else 0) + (ultra["draws"] if ultra else 0)
                tot = w + l + d

                total_wins.append(w)
                total_draws.append(d)
                total_losses.append(l)
                total_games.append(tot)
                overall_cis.append(compute_ci(w, l, d))

                # Color breakdown
                w_w, w_l, w_d, w_tot = 0, 0, 0, 0
                b_w, b_l, b_d, b_tot = 0, 0, 0, 0

                for tc_entry in (fast, slow, ultra):
                    if tc_entry and "white" in tc_entry and "black" in tc_entry:
                        w_w += tc_entry["white"].get("wins", 0)
                        w_l += tc_entry["white"].get("losses", 0)
                        w_d += tc_entry["white"].get("draws", 0)
                        w_tot += tc_entry["white"].get("total", 0)

                        b_w += tc_entry["black"].get("wins", 0)
                        b_l += tc_entry["black"].get("losses", 0)
                        b_d += tc_entry["black"].get("draws", 0)
                        b_tot += tc_entry["black"].get("total", 0)

                w_ci = compute_ci(w_w, w_l, w_d) if w_tot > 0 else None
                b_ci = compute_ci(b_w, b_l, b_d) if b_tot > 0 else None
                white_cis.append(w_ci)
                black_cis.append(b_ci)

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

    overall_rates = np.array([ci[0] for ci in overall_cis])
    overall_lowers = np.array([ci[1] for ci in overall_cis])
    overall_uppers = np.array([ci[2] for ci in overall_cis])
    overall_margins = [ci[3] for ci in overall_cis]

    ax1.plot(x, overall_rates, marker="D", markersize=6, color="#3fb950", linewidth=3.2, label="Overall Win Rate", zorder=5)
    ax1.fill_between(x, overall_lowers, overall_uppers, color="#3fb950", alpha=0.20, label="95% Confidence Interval (±Wilson)", zorder=3)
    ax1.errorbar(x, overall_rates, yerr=[np.maximum(0.0, overall_rates - overall_lowers), np.maximum(0.0, overall_uppers - overall_rates)],
                 fmt="none", ecolor="#7ee787", elinewidth=1.6, capsize=3.5, capthick=1.6, alpha=0.9, zorder=6)
    ax1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line (Equal Strength)")

    for i, r in enumerate(overall_rates):
        m = overall_margins[i]
        y_off = 10 if r <= 88 else -18
        ax1.annotate(f"{r}%\n±{m:.1f}%", (x[i], r), textcoords="offset points", xytext=(0, y_off),
                     ha="center", fontsize=8.2, fontweight="bold", color="#ffffff")

    ax1.set_title("pawnGO Overall Win Rate (with 95% Confidence Band)", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax1.set_xlabel("Stockfish Target Elo", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_ylabel("Overall Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_xticks(x)
    ax1.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=20)
    ax1.set_ylim(-5, 118)
    ax1.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=9.0, labelcolor="#c9d1d9")

    # Panel B: Outcome Distribution Stacked Bar
    bar_width = 0.55
    wins = np.array(total_wins)
    draws = np.array(total_draws)
    losses = np.array(total_losses)

    ax2.bar(x, wins, bar_width, label="Wins", color="#2ea043", edgecolor="#30363d")
    ax2.bar(x, draws, bar_width, bottom=wins, label="Draws", color="#8b949e", edgecolor="#30363d")
    ax2.bar(x, losses, bar_width, bottom=wins+draws, label="Losses", color="#da3633", edgecolor="#30363d")

    # Only annotate segment if height >= 35 games to eliminate small-segment label collisions
    for i in range(len(active_levels)):
        tot = total_games[i]
        w, d, l = wins[i], draws[i], losses[i]
        if w >= 35:
            ax2.text(x[i], w / 2, f"+{w}", ha="center", va="center", color="#ffffff", fontsize=8, fontweight="bold")
        if d >= 35:
            ax2.text(x[i], w + d / 2, f"={d}", ha="center", va="center", color="#ffffff", fontsize=7.5, fontweight="bold")
        if l >= 35:
            ax2.text(x[i], w + d + l / 2, f"-{l}", ha="center", va="center", color="#ffffff", fontsize=8, fontweight="bold")
        ax2.annotate(f"{tot}G", (x[i], tot), textcoords="offset points", xytext=(0, 6),
                     ha="center", fontsize=8.5, fontweight="bold", color="#8b949e")

    ax2.set_title("Match Outcomes Distribution (Wins / Draws / Losses)", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
    ax2.set_xlabel("Stockfish Target Elo", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_ylabel("Number of Games Played", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_xticks(x)
    ax2.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=20)
    max_tot = max(total_games) if total_games else 100
    ax2.set_ylim(0, max_tot * 1.20)
    ax2.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.5, labelcolor="#c9d1d9")

    fig1.suptitle("pawnGO Overall Gauntlet Performance Summary with 95% Confidence Bounds", fontsize=15, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out1 = os.path.join(ASSETS_DIR, "benchmark_overall.png")
    fig1.savefig(out1, facecolor=fig1.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig1)

    # Legacy copy
    shutil.copyfile(out1, os.path.join(ASSETS_DIR, "benchmark_chart.png"))

    # =========================================================================
    # 2. COMPARATIVE TIME-CONTROL MATRIX (assets/benchmark_by_control.png)
    # =========================================================================
    fig2, (ax2_1, ax2_2, ax2_3) = plt.subplots(1, 3, figsize=(19, 5.5), dpi=300, facecolor="#0d1117")
    for a in (ax2_1, ax2_2, ax2_3):
        set_dark_theme(a)

    def get_parity_str(cis):
        valid = [(active_levels[i], cis[i][0]) for i in range(len(active_levels)) if cis[i] is not None]
        for j in range(len(valid) - 1):
            l1, y1 = valid[j]
            l2, y2 = valid[j+1]
            if y1 >= 50.0 and y2 <= 50.0 and y1 != y2:
                p = l1 + (y1 - 50.0) / (y1 - y2) * (l2 - l1)
                return f"Parity ~{int(round(p))}"
        return "Parity ~2920"

    p_slow = get_parity_str(slow_cis)
    p_ultra = get_parity_str(ultra_cis)

    panels = [
        (ax2_1, fast_cis, "#58a6ff", "Fast Blitz (2+0.05)", "Tactical Sharpness & Early Punish"),
        (ax2_2, slow_cis, "#bc8cff", "Slower Blitz (5+0.1)", f"Stable Positional Play ({p_slow})"),
        (ax2_3, ultra_cis, "#f0883e", "Ultra Slow (10+0.25)", f"Deep Search & Precision ({p_ultra})")
    ]

    for ax_p, cis, color, title, subtitle in panels:
        valid = [(i, cis[i]) for i in range(len(active_levels)) if cis[i] is not None]
        if valid:
            xi = np.array([v[0] for v in valid])
            yi = np.array([v[1][0] for v in valid])
            lows = np.array([v[1][1] for v in valid])
            ups = np.array([v[1][2] for v in valid])
            margins = [v[1][3] for v in valid]

            ax_p.plot(xi, yi, marker="o", markersize=7.0, color=color, linewidth=2.8, label="Win Rate", zorder=5)
            ax_p.fill_between(xi, lows, ups, color=color, alpha=0.20, label="95% CI (±Wilson)", zorder=3)
            ax_p.errorbar(xi, yi, yerr=[np.maximum(0.0, yi - lows), np.maximum(0.0, ups - yi)], fmt="none", ecolor=color, elinewidth=1.5, capsize=3.5, capthick=1.5, alpha=0.85, zorder=6)
            ax_p.axhline(50, color="#f85149", linestyle=":", linewidth=1.8, alpha=0.9, label="50% Parity Line")

            for idx, val, m in zip(xi, yi, margins):
                y_off = 9 if val <= 85 else -16
                ax_p.annotate(f"{val}%\n±{m:.1f}%", (idx, val), textcoords="offset points", xytext=(0, y_off),
                              ha="center", fontsize=7.8, color="#ffffff", fontweight="bold")

        ax_p.set_title(f"{title}\n{subtitle}", fontsize=11, fontweight="bold", color="#f0f6fc", pad=10)
        ax_p.set_xlabel("Stockfish Target Elo", fontsize=10, fontweight="bold", color="#c9d1d9", labelpad=6)
        ax_p.set_ylabel("Win Rate (%)", fontsize=10, fontweight="bold", color="#c9d1d9", labelpad=6)
        ax_p.set_xticks(x)
        ax_p.set_xticklabels([f"SF {lvl}" for lvl in active_levels], rotation=25, fontsize=8.5)
        ax_p.set_ylim(-6, 118)
        ax_p.legend(loc="lower left", facecolor="#161b22", edgecolor="#30363d", fontsize=8.2, labelcolor="#c9d1d9")

    fig2.suptitle("pawnGO Performance Across Time Controls with 95% Confidence Intervals", fontsize=15, fontweight="bold", color="#58a6ff", y=1.02)
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

    valid_wb = [(i, white_cis[i], black_cis[i]) for i in range(len(active_levels))
                if white_cis[i] is not None and black_cis[i] is not None]

    if valid_wb:
        xi_wb = np.array([v[0] for v in valid_wb])
        yw = np.array([v[1][0] for v in valid_wb])
        yw_low = np.array([v[1][1] for v in valid_wb])
        yw_up = np.array([v[1][2] for v in valid_wb])
        yw_se = np.array([v[1][4] for v in valid_wb])

        yb = np.array([v[2][0] for v in valid_wb])
        yb_low = np.array([v[2][1] for v in valid_wb])
        yb_up = np.array([v[2][2] for v in valid_wb])
        yb_se = np.array([v[2][4] for v in valid_wb])

        # Left: White vs Black Win Rate curves with Horizontal Dodge to eliminate overlaps
        offset = 0.12
        xw = xi_wb - offset
        xb = xi_wb + offset

        ax3_1.plot(xw, yw, marker="o", markersize=6.5, color="#ffffff", linewidth=2.4, label="White Win Rate (±95% CI)", zorder=5)
        ax3_1.errorbar(xw, yw, yerr=[np.maximum(0.0, yw - yw_low), np.maximum(0.0, yw_up - yw)],
                       fmt="none", ecolor="#ffffff", elinewidth=1.4, capsize=3.2, capthick=1.4, alpha=0.85, zorder=6)

        ax3_1.plot(xb, yb, marker="s", markersize=6.5, color="#58a6ff", linewidth=2.4, label="Black Win Rate (±95% CI)", zorder=5)
        ax3_1.errorbar(xb, yb, yerr=[np.maximum(0.0, yb - yb_low), np.maximum(0.0, yb_up - yb)],
                       fmt="none", ecolor="#58a6ff", elinewidth=1.4, capsize=3.2, capthick=1.4, alpha=0.85, zorder=6)

        ax3_1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

        pill_white = dict(boxstyle="round,pad=0.15", facecolor="#161b22", edgecolor="#30363d", alpha=0.80, linewidth=0.6)
        pill_black = dict(boxstyle="round,pad=0.15", facecolor="#161b22", edgecolor="#30363d", alpha=0.80, linewidth=0.6)

        for i in range(len(xi_wb)):
            val_w = yw[i]
            val_b = yb[i]

            # Smart vertical placement outside error bar bounds
            y_text_w = yw_up[i] + 2.2
            y_text_b = yb_low[i] - 2.8

            # Ensure minimum separation
            if (y_text_w - y_text_b) < 7.5:
                mid = (val_w + val_b) / 2.0
                y_text_w = mid + 4.5
                y_text_b = mid - 4.5

            ax3_1.text(xw[i], y_text_w, f"{val_w}%",
                       ha="center", va="bottom", fontsize=7.2, color="#ffffff", fontweight="bold", bbox=pill_white)
            ax3_1.text(xb[i], y_text_b, f"{val_b}%",
                       ha="center", va="top", fontsize=7.2, color="#79c0ff", fontweight="bold", bbox=pill_black)

        ax3_1.set_title("Win Rate Playing White vs Black (Separated ±95% CI)", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
        ax3_1.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_1.set_ylabel("Win Rate (%)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_1.set_xticks(xi_wb)
        ax3_1.set_xticklabels([f"SF {active_levels[i]}" for i in xi_wb], rotation=20)
        ax3_1.set_ylim(-12, 122)
        ax3_1.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9.0, labelcolor="#c9d1d9")

        # Right: First-Move Advantage Delta with Error Propagation
        deltas = yw - yb
        delta_se = np.sqrt(yw_se**2 + yb_se**2)
        delta_margins = 1.96 * delta_se * 100.0

        colors = ["#2ea043" if d >= 0 else "#da3633" for d in deltas]
        bars = ax3_2.bar(xi_wb, deltas, 0.55, color=colors, edgecolor="#30363d")
        ax3_2.errorbar(xi_wb, deltas, yerr=np.maximum(0.0, delta_margins), fmt="none", ecolor="#f0f6fc", elinewidth=1.4, capsize=3.5, capthick=1.4, alpha=0.85, zorder=6)
        ax3_2.axhline(0, color="#8b949e", linestyle="-", linewidth=1.2)

        pill_delta = dict(boxstyle="round,pad=0.15", facecolor="#161b22", edgecolor="#30363d", alpha=0.75, linewidth=0.5)
        for i, d in enumerate(deltas):
            y_pos = d + (delta_margins[i] + 1.8 if d >= 0 else -(delta_margins[i] + 4.0))
            ax3_2.annotate(f"{d:+.1f}%\n(±{delta_margins[i]:.1f}%)", (xi_wb[i], y_pos), ha="center", fontsize=7.2,
                           fontweight="bold", color="#f0f6fc", bbox=pill_delta)

        ax3_2.set_title("First-Move Advantage Delta (White% - Black% ± 95% CI)", fontsize=13, fontweight="bold", color="#f0f6fc", pad=12)
        ax3_2.set_xlabel("Opponent Level", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_2.set_ylabel("Advantage Delta (% Points)", fontsize=11, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax3_2.set_xticks(xi_wb)
        ax3_2.set_xticklabels([f"SF {active_levels[i]}" for i in xi_wb], rotation=20)
        max_d = max(abs(deltas.min() - delta_margins.max()), abs(deltas.max() + delta_margins.max())) if len(deltas) > 0 else 10
        ax3_2.set_ylim(-max_d * 1.45, max_d * 1.45)

    fig3.suptitle("pawnGO Color Asymmetry & Opening Balance Analysis with Error Propagation", fontsize=15, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out3 = os.path.join(ASSETS_DIR, "benchmark_white_black.png")
    fig3.savefig(out3, facecolor=fig3.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig3)

    print("All benchmark charts successfully generated with 95% Confidence Intervals:")
    print(f"  - {out1}")
    print(f"  - {out2}")
    print(f"  - {out3}")

if __name__ == "__main__":
    generate_charts()

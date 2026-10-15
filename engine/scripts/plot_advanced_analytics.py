#!/usr/bin/env python3
"""
Generate publication-quality advanced analytics charts for pawnGO README:
1. assets/benchmark_game_phases.png - Score by Game Length & Distribution
2. assets/benchmark_openings.png - Opening Performance Across Opponent Rating Levels
"""

import os
import glob
import re
import json
import math
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

def compute_ci(w, l, d, z=1.96):
    tot = w + l + d
    if tot == 0:
        return 0.0, 0.0, 0.0, 0.0
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
    return rate_pct, lower_pct, upper_pct, margin_pct

def generate_advanced_charts():
    pgn_files = glob.glob(os.path.join(BENCH_DIR, "**/*.pgn"), recursive=True)
    if not pgn_files:
        print("No PGN files found.")
        return

    win_lengths = []
    loss_lengths = []
    draw_lengths = []

    phase_bins = {
        "< 35 moves": [0, 0, 0],
        "35–50 moves": [0, 0, 0],
        "50–70 moves": [0, 0, 0],
        "> 70 moves": [0, 0, 0]
    }

    # Opening tracking across 3 broad tiers and 5 granular brackets
    tiers = ["2200–2600", "2700–3100", "3200–3600"]
    brackets = ["2200–2400", "2500–2700", "2800–3000", "3100–3300", "3400–3600"]

    def get_tier(lvl):
        if lvl <= 2600:
            return "2200–2600"
        if lvl <= 3100:
            return "2700–3100"
        return "3200–3600"

    def get_bracket(lvl):
        if lvl <= 2400:
            return "2200–2400"
        if lvl <= 2700:
            return "2500–2700"
        if lvl <= 3000:
            return "2800–3000"
        if lvl <= 3300:
            return "3100–3300"
        return "3400–3600"

    openings_data = {}

    for f in pgn_files:
        m = re.search(r"(\d{4})", os.path.basename(f))
        if not m:
            continue
        lvl = int(m.group(1))
        t_key = get_tier(lvl)
        b_key = get_bracket(lvl)

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
                phase_key = "< 35 moves"
            elif moves <= 50:
                phase_key = "35–50 moves"
            elif moves <= 70:
                phase_key = "50–70 moves"
            else:
                phase_key = "> 70 moves"

            if is_win:
                phase_bins[phase_key][0] += 1
            elif is_loss:
                phase_bins[phase_key][1] += 1
            elif is_draw:
                phase_bins[phase_key][2] += 1

            op_m = re.search(r'\[Opening "([^"]+)"\]', g)
            if op_m:
                op_raw = normalize_opening(op_m.group(1).split(",")[0])
                if op_raw not in openings_data:
                    openings_data[op_raw] = {
                        "total": [0, 0, 0],
                        "tiers": {t: [0, 0, 0] for t in tiers},
                        "brackets": {b: [0, 0, 0] for b in brackets}
                    }
                if is_win:
                    openings_data[op_raw]["total"][0] += 1
                    openings_data[op_raw]["tiers"][t_key][0] += 1
                    openings_data[op_raw]["brackets"][b_key][0] += 1
                elif is_loss:
                    openings_data[op_raw]["total"][1] += 1
                    openings_data[op_raw]["tiers"][t_key][1] += 1
                    openings_data[op_raw]["brackets"][b_key][1] += 1
                elif is_draw:
                    openings_data[op_raw]["total"][2] += 1
                    openings_data[op_raw]["tiers"][t_key][2] += 1
                    openings_data[op_raw]["brackets"][b_key][2] += 1

    plt.style.use("dark_background")

    # =========================================================================
    # 1. GAME LENGTH & PHASE ANALYSIS (benchmark_game_phases.png)
    # =========================================================================
    fig1, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6), dpi=300, facecolor="#0d1117")
    set_dark_theme(ax1)
    set_dark_theme(ax2)

    bin_labels = list(phase_bins.keys())
    bin_rates = []
    bin_lowers = []
    bin_uppers = []
    bin_margins = []
    for k in bin_labels:
        w, l, d = phase_bins[k]
        r, low, up, m = compute_ci(w, l, d)
        bin_rates.append(r)
        bin_lowers.append(low)
        bin_uppers.append(up)
        bin_margins.append(m)

    x_bins = np.arange(len(bin_labels))
    colors = ["#2ea043" if r >= 60 else ("#58a6ff" if r >= 50 else "#f0883e") for r in bin_rates]
    ax1.bar(x_bins, bin_rates, 0.52, color=colors, edgecolor="#30363d")
    ax1.errorbar(x_bins, bin_rates, yerr=[np.array(bin_rates) - np.array(bin_lowers), np.array(bin_uppers) - np.array(bin_rates)],
                 fmt="none", ecolor="#ffffff", elinewidth=1.6, capsize=4.5, capthick=1.6, alpha=0.9, zorder=6)
    ax1.axhline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

    for i, r in enumerate(bin_rates):
        w, l, d = phase_bins[bin_labels[i]]
        m = bin_margins[i]
        y_pos = r + max(m, 3.0) + 1.5
        ax1.annotate(f"{r}% (±{m:.1f}%)\n(+{w} -{l} ={d})", (x_bins[i], y_pos),
                     ha="center", fontsize=8.5, fontweight="bold", color="#ffffff")

    ax1.set_title("Score by Game Length Bracket", fontsize=12, fontweight="bold", color="#f0f6fc", pad=10)
    ax1.set_xlabel("Game Length", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_ylabel("Score (%)", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax1.set_xticks(x_bins)
    ax1.set_xticklabels(bin_labels, fontsize=9.5)
    ax1.set_ylim(0, 120)
    ax1.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9, labelcolor="#c9d1d9")

    # Panel B: Histogram of Wins vs Losses
    max_m = 110
    bins_hist = np.linspace(10, max_m, 25)
    med_w = np.median(win_lengths) if win_lengths else 0
    med_l = np.median(loss_lengths) if loss_lengths else 0
    mean_w = np.mean(win_lengths) if win_lengths else 0
    mean_l = np.mean(loss_lengths) if loss_lengths else 0

    ax2.hist(win_lengths, bins=bins_hist, alpha=0.65, color="#2ea043", label=f"Wins (Median: {med_w:.0f}m)", edgecolor="#161b22", density=True)
    ax2.hist(loss_lengths, bins=bins_hist, alpha=0.6, color="#da3633", label=f"Losses (Median: {med_l:.0f}m)", edgecolor="#161b22", density=True)

    ax2.axvline(mean_w, color="#3fb950", linestyle="--", linewidth=2, label=f"Mean Win: {mean_w:.1f}m")
    ax2.axvline(mean_l, color="#f85149", linestyle="--", linewidth=2, label=f"Mean Loss: {mean_l:.1f}m")

    ax2.set_title("Game Length Distribution (Wins vs Losses)", fontsize=12, fontweight="bold", color="#f0f6fc", pad=10)
    ax2.set_xlabel("Game Length (Moves)", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.set_ylabel("Frequency Density", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)
    ax2.legend(loc="upper right", facecolor="#161b22", edgecolor="#30363d", fontsize=9, labelcolor="#c9d1d9")

    fig1.suptitle("pawnGO Game Length and Phase Analysis", fontsize=15, fontweight="bold", color="#58a6ff", y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.88)
    out1 = os.path.join(ASSETS_DIR, "benchmark_game_phases.png")
    fig1.savefig(out1, facecolor=fig1.get_facecolor(), edgecolor="none", bbox_inches="tight")
    plt.close(fig1)

    # =========================================================================
    # 2. OPENING PERFORMANCE DISTRIBUTED BY RATING LEVELS (benchmark_openings.png)
    # =========================================================================
    top_ops = sorted(
        [k for k in openings_data.keys() if sum(openings_data[k]["total"]) >= 70],
        key=lambda k: sum(openings_data[k]["total"]),
        reverse=True
    )[:10]

    if top_ops:
        fig2, (ax_bar, ax_heat) = plt.subplots(1, 2, figsize=(16, 7.5), dpi=300, facecolor="#0d1117", gridspec_kw={"width_ratios": [1.2, 1.0]})
        set_dark_theme(ax_bar)
        set_dark_theme(ax_heat)

        # Left: Grouped horizontal bars across 3 Tiers
        y_pos = np.arange(len(top_ops))
        bar_height = 0.25

        t1_rates, t2_rates, t3_rates = [], [], []
        for op in top_ops:
            d = openings_data[op]["tiers"]
            s1 = sum(d["2200–2600"])
            r1 = (d["2200–2600"][0] + 0.5 * d["2200–2600"][2]) / s1 * 100 if s1 > 0 else 0
            s2 = sum(d["2700–3100"])
            r2 = (d["2700–3100"][0] + 0.5 * d["2700–3100"][2]) / s2 * 100 if s2 > 0 else 0
            s3 = sum(d["3200–3600"])
            r3 = (d["3200–3600"][0] + 0.5 * d["3200–3600"][2]) / s3 * 100 if s3 > 0 else 0
            t1_rates.append(r1)
            t2_rates.append(r2)
            t3_rates.append(r3)

        b1 = ax_bar.barh(y_pos - bar_height, t1_rates, bar_height, label="vs SF 2200–2600", color="#2ea043", edgecolor="#30363d")
        b2 = ax_bar.barh(y_pos, t2_rates, bar_height, label="vs SF 2700–3100", color="#58a6ff", edgecolor="#30363d")
        b3 = ax_bar.barh(y_pos + bar_height, t3_rates, bar_height, label="vs SF 3200–3600", color="#f0883e", edgecolor="#30363d")

        ax_bar.axvline(50, color="#f85149", linestyle=":", linewidth=2, alpha=0.9, label="50% Parity Line")

        for i in range(len(top_ops)):
            ax_bar.annotate(f"{t1_rates[i]:.0f}%", (t1_rates[i] + 1.5, y_pos[i] - bar_height), va="center", fontsize=8.5, color="#7ee787")
            ax_bar.annotate(f"{t2_rates[i]:.0f}%", (t2_rates[i] + 1.5, y_pos[i]), va="center", fontsize=8.5, color="#79c0ff")
            ax_bar.annotate(f"{t3_rates[i]:.0f}%", (t3_rates[i] + 1.5, y_pos[i] + bar_height), va="center", fontsize=8.5, color="#ffa657")

        ax_bar.set_yticks(y_pos)
        ax_bar.set_yticklabels(top_ops, fontsize=10, fontweight="bold")
        ax_bar.invert_yaxis()
        ax_bar.set_xlim(0, 120)
        ax_bar.set_xlabel("Score (%)", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)
        ax_bar.set_title("Performance by Opponent Rating Tier", fontsize=12, fontweight="bold", color="#f0f6fc", pad=32)
        ax_bar.legend(bbox_to_anchor=(0.5, 1.03), loc="lower center", ncol=4,
                      facecolor="#161b22", edgecolor="#30363d", fontsize=8.5, labelcolor="#c9d1d9")

        # Right: Granular Heatmap matrix across 5 rating brackets
        heat_matrix = np.zeros((len(top_ops), len(brackets)))
        annot_matrix = []

        for i, op in enumerate(top_ops):
            row_ann = []
            for j, b in enumerate(brackets):
                b_res = openings_data[op]["brackets"][b]
                b_tot = sum(b_res)
                if b_tot > 0:
                    pct = round(((b_res[0] + 0.5 * b_res[2]) / b_tot) * 100, 1)
                else:
                    pct = 0.0
                heat_matrix[i, j] = pct
                row_ann.append(f"{pct:.0f}%\n({b_tot}G)")
            annot_matrix.append(row_ann)

        im = ax_heat.imshow(heat_matrix, cmap="RdYlGn", vmin=10, vmax=95, aspect="auto")

        ax_heat.set_xticks(np.arange(len(brackets)))
        ax_heat.set_xticklabels(brackets, fontsize=9.5, fontweight="bold")
        ax_heat.set_yticks(np.arange(len(top_ops)))
        ax_heat.set_yticklabels([])  # Hide duplicate y-labels, shared with left plot
        ax_heat.set_title("Score Matrix Across Rating Brackets", fontsize=12, fontweight="bold", color="#f0f6fc", pad=12)
        ax_heat.set_xlabel("Opponent Elo Bracket", fontsize=10.5, fontweight="bold", color="#c9d1d9", labelpad=8)

        for i in range(len(top_ops)):
            for j in range(len(brackets)):
                val = heat_matrix[i, j]
                text_color = "#000000" if 40 <= val <= 75 else "#ffffff"
                ax_heat.text(j, i, annot_matrix[i][j], ha="center", va="center",
                             fontsize=8.5, fontweight="bold", color=text_color)

        cbar = fig2.colorbar(im, ax=ax_heat, fraction=0.046, pad=0.04)
        cbar.ax.set_ylabel("Score (%)", rotation=-90, va="bottom", fontsize=9.5, color="#c9d1d9")
        cbar.ax.yaxis.set_tick_params(color="#c9d1d9", labelcolor="#c9d1d9")

        fig2.suptitle("pawnGO Opening Repertoire Performance Across Rating Levels", fontsize=15, fontweight="bold", color="#58a6ff", y=0.98)
        plt.tight_layout()
        plt.subplots_adjust(top=0.84)
        out2 = os.path.join(ASSETS_DIR, "benchmark_openings.png")
        fig2.savefig(out2, facecolor=fig2.get_facecolor(), edgecolor="none", bbox_inches="tight")
        plt.close(fig2)

    # Save structured summary for automated README generation
    summary_data = {
        "total_games": len(win_lengths) + len(loss_lengths) + len(draw_lengths),
        "phase_bins": {
            k: {
                "wins": phase_bins[k][0],
                "losses": phase_bins[k][1],
                "draws": phase_bins[k][2],
                "total": phase_bins[k][0] + phase_bins[k][1] + phase_bins[k][2],
                "score": round(((phase_bins[k][0] + 0.5 * phase_bins[k][2]) / max(1, phase_bins[k][0] + phase_bins[k][1] + phase_bins[k][2])) * 100, 1)
            } for k in phase_bins
        },
        "med_w": round(float(med_w), 1),
        "mean_w": round(float(mean_w), 1),
        "med_l": round(float(med_l), 1),
        "mean_l": round(float(mean_l), 1),
        "tiers": {}
    }
    for t in tiers:
        tier_ops = []
        for op in top_ops:
            w, l, d = openings_data[op]["tiers"][t]
            tot = w + l + d
            if tot > 0:
                pct = round(((w + 0.5 * d) / tot) * 100, 1)
                tier_ops.append([op, pct, tot])
        tier_ops.sort(key=lambda x: x[1], reverse=True)
        summary_data["tiers"][t] = tier_ops

    summary_file = os.path.join(BENCH_DIR, "analytics_summary.json")
    with open(summary_file, "w") as fp:
        json.dump(summary_data, fp, indent=2)

    print("Advanced analytics charts generated:")
    print(f"  - {out1}")
    if top_ops:
        print(f"  - {out2}")
    print(f"  - {summary_file}")

if __name__ == "__main__":
    generate_advanced_charts()

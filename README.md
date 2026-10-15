# pawnGO

**pawnGO** is a ~2900 Elo chess engine and web application. It combines a modular C++20 UCI engine — executable natively or compiled to WebAssembly — with a React web client. The engine runs locally in the browser using multi-threaded WebAssembly with SIMD acceleration.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-SIMD%20%7C%20Threads-orange.svg)](https://webassembly.org/)
[![React](https://img.shields.io/badge/React-19-cyan.svg)](https://react.dev/)
[![Engine Rating](https://img.shields.io/badge/Engine%20Rating-~2900%20Elo-green.svg)](#empirical-benchmark-and-validation)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Live Deployment**: [pawngo.mybsu.online](https://pawngo.mybsu.online)

---

## Key Highlights

- **~2900 Elo Benchmark Rating**: Validated across a 2,550-game gauntlet tournament against Stockfish 17.1 calibrated from 2200 to 3600 Elo under UCI conditions.
- **Competitive Parity vs SF 2900**: 51.8% overall score against Stockfish 2900 across all time controls, with 60.0% in slower blitz.
- **NNUE Positional Evaluation**: Embedded neural network `nn-62ef826d1a6d.nnue` with dual-perspective accumulator caches and incremental stack updates.
- **Syzygy Endgame Tablebases**: Direct probing of 3-4-5 piece Syzygy tablebases and dedicated endgame heuristics.
- **Dual Compilation Target**: Compiles to a native executable and WebAssembly for in-browser execution.

---

## Engine Architecture

### 1. Board Representation & Move Generation
- **Bitboard Representation**: 64-bit unsigned integers represent piece positions, square attacks, and board occupancy.
- **Magic Bitboards**: Precomputed magic hashing algorithms for bishop and rook move generation.
- **Zobrist Hashing**: Incremental 64-bit position keys for transposition lookups, 50-move rule tracking, and 3-fold repetition detection.
- **Move Validation**: Pseudo-legal move validation ensuring correct destination occupancy, en-passant legality, and promotion sanity.

### 2. Evaluation System
- **NNUE Evaluation**: Neural network inference pipeline using `nn-62ef826d1a6d.nnue`.
- **Incremental Accumulators**: Accumulator state updates are computed incrementally during `make_move` / `unmake_move`, with cache recalculation on irreversible moves.
- **Syzygy Tablebases**: Direct probing of 3-, 4-, and 5-man Syzygy endgame tablebases for root WDL/DTZ lookups and in-tree search adjudication.
- **Endgame Heuristics**:
  - *King Centralization*: Rewards king proximity to the center as non-pawn material decreases.
  - *Rule of the Square*: Fast breakthrough detection for unstoppable passed pawns.
  - *Theoretical Draw Scaling*: Score zeroing for theoretical draws such as $KR$ vs $KR$ or opposite-colored bishop endgames with rook pawns.
  - *Lone King Cornering*: Manhattan distance minimization to drive bare kings toward board edges.
  - *Tarrasch Rule*: Penalizes friendly rooks obstructing passed pawns from the front.

### 3. Search Engine
- **Principal Variation Search**: Aspiration windows around the previous iteration score with adaptive delta widening.
- **Transposition Table**: 64-byte aligned entries with depth-preferred replacement, generation aging, and fail-low best move preservation.
- **Singular Extensions**: Re-searches the transposition move with reduced depth to detect forced lines, extending search depth when a move is strictly superior to alternatives.
- **Multi-Cut Pruning**: Early beta cutoffs when non-PV nodes confirm multiple refutations during singular verification.
- **Null Move Pruning**: Adaptive depth reduction based on search depth and position improvement status, guarded by zugzwang piece presence checks.
- **Reverse Futility Pruning**: Static evaluation margins scaled by search depth.
- **Futility Pruning & Late Move Pruning**: Prunes non-promising quiet moves at low depths while exempting tactical checks and advanced passed pawns.
- **Late Move Reductions**: Logarithmic reduction table adjusted for killers, countermoves, tactical checks, history scores, and advanced passed pawns.
- **Static Exchange Evaluation**: Prunes losing captures in search and quiescence.
- **Move Ordering Pipeline**:
  1. Transposition table move.
  2. Winning and equal captures ordered by MVV-LVA and Capture History.
  3. Pawn promotions.
  4. Killer moves (two slots per ply).
  5. Countermove heuristic indexed by previous move source and target.
  6. Continuation history (1-ply and 2-ply context tables).
  7. Main history table with smooth gravity decay:
     $$h \leftarrow h + \text{bonus} - \frac{h \cdot |\text{bonus}|}{16384}$$
  8. Losing captures (negative SEE).
- **Quiescence Search**: Evaluates tactical captures, checks, and queen promotions, protected by delta pruning and stand-pat checks.

---

## Empirical Benchmark and Validation

To establish pawnGO's playing strength, a multi-run tournament benchmark of up to 7,650 games (510 games per level across 3 independent runs) was conducted using `cutechess-cli` against official calibrated Stockfish 17.1 rating levels from 2200 to 3600 Elo across Fast Blitz (`2+0.05`), Slower Blitz (`5+0.1`), and Ultra Slow (`10+0.25`).

### Grand Performance Matrix Across All Games

<p align="center">
  <img src="assets/benchmark_overall.png" alt="pawnGO Overall Gauntlet Performance Summary" width="100%" />
</p>

| Opponent Target | Fast Blitz (`2+0.05`) | Slower Blitz (`5+0.1`) | Ultra Slow (`10+0.25`) | Playing White | Playing Black | Total Score | Win Rate | Performance Rating |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Stockfish 2200** | +287 -10 =3 (96.2%) | +247 -2 =1 (99.0%) | +98 -1 =1 (98.5%) | 320.0/325 (98.5%) | 314.5/325 (96.8%) | **634.5 / 650** | **97.6%** | ~2844 Elo |
| **Stockfish 2300** | +289 -8 =3 (96.8%) | +247 -3 =0 (98.8%) | +99 -1 =0 (99.0%) | 322.5/325 (99.2%) | 314.0/325 (96.6%) | **636.5 / 650** | **97.9%** | ~2967 Elo |
| **Stockfish 2400** | +284 -15 =1 (94.8%) | +232 -13 =5 (93.8%) | +96 -2 =2 (97.0%) | 310.0/325 (95.4%) | 306.0/325 (94.2%) | **616.0 / 650** | **94.8%** | ~2904 Elo |
| **Stockfish 2500** | +270 -15 =15 (92.5%) | +233 -13 =4 (94.0%) | +94 -4 =2 (95.0%) | 308.5/325 (94.9%) | 299.0/325 (92.0%) | **607.5 / 650** | **93.5%** | ~2963 Elo |
| **Stockfish 2600** | +247 -35 =18 (85.3%) | +226 -14 =10 (92.4%) | +93 -2 =5 (95.5%) | 298.0/325 (91.7%) | 284.5/325 (87.5%) | **582.5 / 650** | **89.6%** | ~2974 Elo |
| **Stockfish 2700** | +205 -61 =34 (74.0%) | +192 -41 =17 (80.2%) | +83 -8 =9 (87.5%) | 259.0/325 (79.7%) | 251.0/325 (77.2%) | **510.0 / 650** | **78.5%** | ~2925 Elo |
| **Stockfish 2800** | +142 -109 =49 (55.5%) | +119 -85 =46 (56.8%) | +59 -21 =20 (69.0%) | 191.0/325 (58.8%) | 186.5/325 (57.4%) | **377.5 / 650** | **58.1%** | ~2857 Elo |
| **Stockfish 2900** | +124 -130 =46 (49.0%) | +111 -89 =50 (54.4%) | +42 -35 =23 (53.5%) | 183.0/325 (56.3%) | 153.5/325 (47.2%) | **336.5 / 650** | **51.8%** | ~2913 Elo |
| **Stockfish 3000** | +109 -145 =46 (44.0%) | +87 -103 =60 (46.8%) | +26 -44 =30 (41.0%) | 145.0/325 (44.6%) | 145.0/325 (44.6%) | **290.0 / 650** | **44.6%** | ~2962 Elo |
| **Stockfish 3100** | +79 -180 =41 (33.2%) | +54 -138 =58 (33.2%) | +20 -59 =21 (30.5%) | 115.0/325 (35.4%) | 98.0/325 (30.2%) | **213.0 / 650** | **32.8%** | ~2975 Elo |
| **Stockfish 3200** | +62 -196 =42 (27.7%) | +47 -153 =50 (28.8%) | +15 -62 =23 (26.5%) | 104.0/325 (32.0%) | 77.5/325 (23.8%) | **181.5 / 650** | **27.9%** | ~3035 Elo |
| **Stockfish 3300** | +70 -183 =47 (31.2%) | +72 -132 =46 (38.0%) | +21 -54 =25 (33.5%) | 125.0/325 (38.5%) | 97.0/325 (29.8%) | **222.0 / 650** | **34.2%** | ~3186 Elo |
| **Stockfish 3400** | +61 -181 =58 (30.0%) | +47 -160 =43 (27.4%) | +13 -61 =26 (26.0%) | 100.0/325 (30.8%) | 84.5/325 (26.0%) | **184.5 / 650** | **28.4%** | ~3239 Elo |
| **Stockfish 3500** | +49 -209 =42 (23.3%) | +42 -170 =38 (24.4%) | +14 -60 =26 (27.0%) | 88.0/325 (27.1%) | 70.0/325 (21.5%) | **158.0 / 650** | **24.3%** | ~3303 Elo |
| **Stockfish 3600** | +2 -290 =8 (2.0%) | +0 -244 =6 (1.2%) | +0 -97 =3 (1.5%) | 7.5/325 (2.3%) | 3.0/325 (0.9%) | **10.5 / 650** | **1.6%** | ~2884 Elo |

---

### Stockfish Rating Calibration and Ceiling

Official Stockfish implements `UCI_LimitStrength` and `UCI_Elo` up to an upper limit of 3190 Elo in its UCI configuration code. Setting `UCI_Elo` above 3190 has no effect within the native rating limiter.

To test pawnGO against higher strength levels (3200 through 3600), `UCI_LimitStrength` was set to `false`, and calibrated via the engine's internal `Skill Level` parameter (0–20):
- **Stockfish 3200**: `Skill Level 17`
- **Stockfish 3300**: `Skill Level 18`
- **Stockfish 3400**: `Skill Level 19`
- **Stockfish 3500**: `Skill Level 19` with search modifications
- **Stockfish 3600**: `Skill Level 20` (full-strength Stockfish 17.1 without handicap, ~3550–3600 CCRL Elo)

In official Stockfish, `Skill Level 20` represents maximum unconstrained playing strength. No isolated 'Stockfish 3700' rating level exists. All benchmarks were conducted using official Stockfish 17.1.

The score inflection at level 3300 occurs due to the transition between strength limiting mechanisms: levels up to 3190 use continuous Gaussian error injection via `UCI_Elo`, whereas levels from 3200 switch to discrete search-pruning handicaps via `Skill Level`.

---

### Color Asymmetry & Opening Balance Analysis

<p align="center">
  <img src="assets/benchmark_white_black.png" alt="pawnGO Color Asymmetry & Opening Balance Analysis" width="100%" />
</p>

---

### Time Control Breakdown

<p align="center">
  <img src="assets/benchmark_by_control.png" alt="Performance by Time Control Matrix" width="100%" />
</p>

- **Fast Blitz (`2+0.05`)**: pawnGO maintains high conversion against opponents from 2200 to 2700. Baseline parity (50% score) occurs at ~2880 Elo.
- **Slower Blitz (`5+0.1`)**: Consistent positional play, achieving positive score against Stockfish 2900.
- **Ultra Slow (`10+0.25`)**: High conversion against opponents under 2900 Elo.

---

### Game Length and Phase Analysis

<p align="center">
  <img src="assets/benchmark_game_phases.png" alt="pawnGO Game Length and Phase Analysis" width="100%" />
</p>

- **Under 35 moves**: 77.9% score (+1443 -398 =30, 1871 games). Early tactical sharpness and NNUE evaluation deliver high conversion before complex endgames.
- **35 to 50 moves**: 58.3% score (+1688 -1194 =97, 2979 games). Stable performance in full middlegames.
- **50 to 70 moves**: 47.7% score (+1215 -1343 =259, 2817 games). Transitions into 6- and 7-piece endings prior to 5-piece Syzygy tablebase resolution represent the primary challenge against 3000+ opponents.
- **Over 70 moves**: 49.0% score (+663 -703 =717, 2083 games). High draw frequency (717 draws) and 5-piece tablebase holds.
- **Game Length Distribution**: Wins resolve with a median of 44 moves (mean 48.3 moves); losses resolve with a median of 53 moves (mean 56.2 moves).
- **Adjudication**: Games were adjudicated by `cutechess-cli` using mutual draw rules (within $\pm 10$ cp for 6 plies after move 40), resignation threshold (600 cp deficit for 4 plies), and 5-piece Syzygy tablebases.

---

### Opening Repertoire Performance Across Rating Levels

<p align="center">
  <img src="assets/benchmark_openings.png" alt="pawnGO Opening Repertoire Performance Across Rating Levels" width="100%" />
</p>

Performance across 9,750 tournament games distributed into three opponent rating tiers:
- **2200–2600 Elo**: Scotch Game 98%, Two Knights Defense 96%, Queen's Bishop Game 96%, English Opening 96%, Caro-Kann Defense 95%, Queen's Gambit Declined 95%, French Defense 94%, Sicilian Defense 94%.
- **2700–3100 Elo (peer tier)**: Pirc / Modern Defense 55.8%, English Opening 55.5%, Grünfeld Defense 55.0%, French Defense 54.8%, Scotch Game 54.4%, Queen's Gambit Declined 53.5%, Sicilian Defense 53.1%, Queen's Bishop Game 52.7%.
- **3200–3600 Elo (top Stockfish)**: Scotch Game 30.0%, English Opening 26.5%, Queen's Gambit Declined 26.1%, French Defense 24.8%, Pirc / Modern Defense 23.2%, Sicilian Defense 22.2%, Queen's Bishop Game 21.7%, Caro-Kann Defense 21.2%.

---
### Stockfish 17.1 Parallel Referee Evaluation

> Independent evaluation by official Stockfish 17.1 referee over **9,750 tournament games** and **555,138 pawnGO moves**:

| Metric | pawnGO Engine Performance |
| :--- | :--- |
| **Overall Accuracy** | **88.6%** |
| **Average Centipawn Loss (ACPL)** | **28.5 cp** |
| **White Accuracy / ACPL** | **88.8%** (28.0 cp) |
| **Black Accuracy / ACPL** | **88.4%** (28.9 cp) |
| **Best Moves** | **309,181** (55.7%) |
| **Good Moves** | **120,914** (21.8%) |
| **Inaccuracies** | 86,768 (15.6%) |
| **Mistakes** | 30,176 (5.4%) |
| **Blunders** | **8,099 (1.5%)** |

#### Impact of Time Trouble (Zeitnot) on Move Quality & Endgames

A notable observation in the referee distribution is that the proportion of engine-optimal "Best" moves sits at 55.7% (with good moves at 21.8%), while inaccuracies reach 15.6%. This distribution is directly driven by **severe time trouble (zeitnot)** in fast time controls, particularly in deep endgames:

1. **Sub-Second Search Horizon Degradation**:
   - In Fast Blitz (`2+0.05`) and Slow Blitz (`5+0.1`), games extending beyond move 45 often force the engine to operate exclusively on the increment buffer (0.05s to 0.10s per move).
   - Under acute time pressure, pawnGO dynamically curtails search depth to 6–8 plies (compared to depth 14–18 in calm middlegames), whereas the Stockfish 17.1 referee evaluates every position at an unconstrained `depth=14`.
   - Small positional differences (e.g. 20–40 centipawn deltas between the 1st and 3rd candidate move) are automatically penalized as inaccuracies or non-best moves by the referee, even when practically holding or winning.

2. **Acute Endgame Calculation Pressure**:
   - Complex 6- and 7-piece endings prior to 5-piece Syzygy tablebase resolution require deep, multi-ply tactical calculation (king triangulation, opposition, breakthrough tempo counting).
   - In endgame zeitnot, minor inaccuracies naturally accumulate as non-critical piece shuffles are played to maintain clock stability on increment.
   - Decisive game-losing blunders remain tightly restricted to just **1.5%** across all 555,138 evaluated moves, confirming that endgame time pressure produces positional drift rather than catastrophic tactical blunders.

---

## Web Application

The frontend provides an in-browser interface for play and analysis:

- **Client-Side WebAssembly**: Engine runs inside a dedicated Web Worker using `SharedArrayBuffer` with COOP and COEP compliance.
- **Evaluation Bar**: Real-time evaluation bar with asymptotic win-probability mapping.
- **Move Classification**: Move quality categorization:
  - Best Move: matching top engine line
  - Good Move: sound positional play
  - Inaccuracy: loss $\le 0.50$ cp
  - Mistake: loss $\le 1.50$ cp
  - Blunder: loss $> 1.80$ cp
  - Book Move: Polyglot opening library match
- **Polyglot Opening Book**: Embedded opening book `Perfect2023.bin` for opening phase recognition.
- **Audio Feedback**: Procedural audio for moves and captures.
- **Analysis Tools**: Multi-PV line inspection, custom FEN setup, PGN import/export, and keyboard navigation.

---

## Directory Structure

```
pawnGO/
├── client/                     # React web frontend
│   ├── public/
│   │   └── wasm/               # WebAssembly artifacts (pawngo_wasm.wasm, worker, js)
│   ├── src/
│   │   ├── components/         # Chessboard, eval bar, move list, game controls
│   │   ├── utils/              # Analysis classifier, sound engine, UCI bridge
│   │   ├── App.jsx             # Main application orchestrator
│   │   └── main.jsx
│   └── package.json
├── engine/                     # C++ chess engine core
│   ├── src/
│   │   ├── core/               # Bitboards, move generation, Zobrist hashing
│   │   ├── eval/               # NNUE evaluation, Syzygy probing, endgame heuristics
│   │   ├── search/             # PVS, singular extensions, LMR, history tables
│   │   └── utils/              # Polyglot opening book parsing
│   ├── scripts/                # Tournament gauntlet & Stockfish referee scripts
│   │   ├── benchmark_suite.py  # Automated multi-TC benchmark runner
│   │   ├── plot_benchmark.py   # Publication-quality benchmark chart generator
│   │   ├── run_gauntlet.sh     # Cutechess automated tournament runner
│   │   ├── referee_stockfish.py# Stockfish 17.1 PGN analysis & referee
│   │   └── openings.pgn        # Tournament opening suite
│   ├── CMakeLists.txt          # Native C++ CMake build definition
│   ├── CMakeLists.wasm.txt     # Emscripten WebAssembly build definition
│   └── Dockerfile.wasm         # Reproducible containerized WASM build toolchain
├── assets/                     # Benchmark charts and project graphics
├── docker-compose.yml          # Production deployment specification
└── README.md
```

---

## Building and Installation

### Prerequisites
- **Native Engine**: GCC 11+ or Clang 13+ with C++20 support, CMake 3.16+, Make/Ninja.
- **WebAssembly**: Docker (recommended) or Emscripten SDK (`emsdk` 3.1.50+).
- **Web Client**: Node.js 18+ and npm.

---

### 1. Native Engine Build

```bash
mkdir -p engine/build
cd engine/build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

The compiled binary will be placed at `engine/build/pawnGO`. It supports standard UCI commands (`uci`, `isready`, `position`, `go`, `quit`).

---

### 2. WebAssembly Engine Build (Docker)

To compile the C++ engine to WebAssembly using the containerized Emscripten toolchain:

```bash
# Build the WASM image
docker build -f engine/Dockerfile.wasm -t pawngo-wasm-builder engine/

# Extract the compiled artifacts to the web client
CONTAINER_ID=$(docker create pawngo-wasm-builder)
docker cp $CONTAINER_ID:/app/build_wasm/pawngo_wasm.js client/public/wasm/
docker cp $CONTAINER_ID:/app/build_wasm/pawngo_wasm.wasm client/public/wasm/
docker cp $CONTAINER_ID:/app/build_wasm/pawngo_wasm.data client/public/wasm/
docker cp $CONTAINER_ID:/app/build_wasm/pawngo_wasm.worker.js client/public/wasm/
docker rm $CONTAINER_ID
```

---

### 3. Frontend Client Build and Run

```bash
cd client
npm install

# Start local development server (http://localhost:5173)
npm run dev

# Assemble optimized production bundle
npm run build
```

---

### 4. Running Tournament Benchmarks & Verification

To run an automated gauntlet tournament against Stockfish with full referee verification:

```bash
# Run 20 games against Stockfish calibrated to 2900 Elo (10 rounds, 4 concurrent threads)
./engine/scripts/run_gauntlet.sh 2900 "3+0.05" 10 4

# Run the complete multi-control 2,210-game benchmark matrix (2400-3600 Elo)
python3 engine/scripts/benchmark_suite.py
```

Tournament games are written in real-time to `engine/scripts/benchmarks/`, and charts can be updated at any time via:
```bash
uv run --with matplotlib --with numpy python3 engine/scripts/plot_benchmark.py
```

---

## License

This project is open-source software licensed under the [MIT License](LICENSE).

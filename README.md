# pawnGO ♟️

**pawnGO** is a high-performance **~2900 Elo** chess engine and modern web platform. It pairs a modular C++20 UCI chess engine — running both as a native binary and compiled to **WebAssembly (WASM)** — with a responsive React/Vite web application. The engine executes locally in the user's browser via multi-threaded WebAssembly with SIMD acceleration, ensuring zero server latency and complete privacy.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-SIMD%20%7C%20Threads-orange.svg)](https://webassembly.org/)
[![React](https://img.shields.io/badge/React-19-cyan.svg)](https://react.dev/)
[![Engine Rating](https://img.shields.io/badge/Engine%20Rating-~2900%20Elo-green.svg)](#empirical-benchmark-and-validation)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

🌐 **Live Production Deployment**: [pawngo.mybsu.online](https://pawngo.mybsu.online)

---

## Key Highlights

- **~2900 Elo Empirically Verified**: Tested across a massive **1,530-game gauntlet tournament** against Stockfish 17.1 calibrated from 2400 to 3200 Elo under strict UCI conditions.
- **Winning Score Against SF 2900**: Achieved **51.8% overall score** against `Stockfish 2900` across all time controls (including a **60.0% win rate** in slower blitz).
- **NNUE Positional Evaluation**: Powered by an embedded 21MB neural network (`nn-62ef826d1a6d.nnue`) with dual-perspective accumulator caches and incremental stack updates.
- **Flawless Endgames**: Integrated **Syzygy 3-4-5 tablebase** probing alongside dedicated endgame heuristics (King Centralization, Rule of the Square, Theoretical Draw Scaling, and Cornering).
- **Dual Architecture**: Compiles to a native multi-threaded executable and directly into WebAssembly via Emscripten for 60 FPS in-browser evaluation.

---

## Engine Architecture

### 1. Board Representation & Move Generation
- **Bitboard Engine**: 64-bit unsigned integers (`U64`) represent piece positions, square attacks, and board occupancy.
- **Magic Bitboards**: Precomputed magic hashing algorithms for ultra-fast sliding piece move generation (bishops and rooks).
- **Zobrist Hashing**: Incremental 64-bit position keys for instant transposition lookups, 50-move rule tracking, and 3-fold repetition detection.
- **Strict Move Validation**: Pseudo-legal move validation ensuring correct destination occupancy, en-passant legality, and promotion sanity.

### 2. Evaluation System
- **NNUE (Efficiently Updatable Neural Network)**: Probed via an optimized neural inference pipeline utilizing `nn-62ef826d1a6d.nnue`.
- **Incremental Accumulators**: Accumulator state updates are computed incrementally during `make_move` / `unmake_move`, with automatic cache fallback on irreversible piece captures or pawn moves.
- **Syzygy 3-4-5 Tablebases**: Full support for 3-, 4-, and 5-man Syzygy endgame tablebases (`tbprobe`) for instant root-level WDL/DTZ lookups and in-tree search adjudication.
- **Endgame Heuristics Suite**:
  - *Dynamic King Centralization*: King distance to center is actively rewarded (up to +96 cp) as pieces leave the board ($\le 4$ non-pawn pieces).
  - *Rule of the Square*: Instant passer breakthrough detection (+500 cp) for unstoppable pawn promotions.
  - *Theoretical Draw Scaling*: Automatic score zeroing for known theoretical draws ($KR$ vs $KR$, $KQ$ vs $KQ$, wrong-colored bishop with rook pawn, same-colored bishops).
  - *Lone King Cornering*: Manhattan distance minimization to drive bare kings toward board edges during mating phases.
  - *Tarrasch Rule*: Penalizes friendly rooks obstructing passed pawns from the front.

### 3. Search Engine
- **Principal Variation Search (PVS)**: Aspiration windows around the previous iteration's score with adaptive delta widening.
- **Transposition Table (TT)**: 64-byte aligned entries featuring depth-preferred replacement, generation aging, and fail-low best move preservation.
- **Singular Extensions**: Re-searches the TT best move with reduced depth and tight margin; extends search depth by 1 or 2 plies when a move is strictly better than all alternatives.
- **Multi-Cut Pruning**: Early beta cutoffs when non-PV nodes confirm multiple refutations during singular verification.
- **Null Move Pruning (NMP)**: Adaptive depth reduction based on search depth and position improvement status, guarded by zugzwang piece presence checks.
- **Reverse Futility Pruning (RFP / Static NMP)**: Dynamic evaluation margins scaled by depth and static evaluation trends.
- **Futility Pruning & Late Move Pruning (LMP)**: Prunes non-promising quiet moves at low depths while exempting tactical checks and advanced passed pawns.
- **Late Move Reductions (LMR)**: Logarithmic depth/move reduction table adjusted dynamically for killers, countermoves, tactical checks, history scores, and advanced passed pawns.
- **Static Exchange Evaluation (SEE)**:
  - Quiet move SEE pruning at low depths.
  - Bad capture SEE pruning in both alpha-beta search and quiescence search.
- **Heuristic Move Ordering Pipeline**:
  1. Transposition table (TT) move.
  2. Winning and equal captures ordered by MVV-LVA and Capture History.
  3. Pawn promotions.
  4. Killer moves (two slots per ply).
  5. Countermove heuristic (indexed by previous move source and target).
  6. Continuation history (1-ply and 2-ply context tables).
  7. Main history table with smooth gravity decay:
     $$h \leftarrow h + \text{bonus} - \frac{h \cdot |\text{bonus}|}{16384}$$
  8. Losing captures (negative SEE).
- **Quiescence Search**: Evaluates tactical captures, checks, and queen promotions to eliminate the horizon effect, protected by delta pruning and stand-pat checks.

---

## Empirical Benchmark and Validation

To definitively establish pawnGO's playing strength, a massive gauntlet benchmark was conducted using `cutechess-cli` against official calibrated Stockfish 17.1 rating levels from 2400 to 3600 Elo across Fast Blitz (`2+0.05`), Slower Blitz (`5+0.1`), and Ultra Slow (`10+0.25`).

### Grand Performance Matrix Across All Games

<p align="center">
  <img src="assets/benchmark_overall.png" alt="pawnGO Overall Gauntlet Performance Summary" width="100%" />
</p>

| Opponent Target | Fast Blitz (`2+0.05`, 100G) | Slower Blitz (`5+0.1`, 50G) | Ultra Slow (`10+0.25`, 20G) | Playing White | Playing Black | Total Score (170G) | Win Rate | Performance Rating |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Stockfish 2400** | +94 -6 =0 (94.0%) | +49 -1 =0 (98.0%) | +20 -0 =0 (100.0%) | 81.0/85 (95.3%) | 82.0/85 (96.5%) | **163.0 / 170** | **95.9%** | ~2948 Elo |
| **Stockfish 2500** | +83 -11 =6 (86.0%) | +45 -3 =2 (92.0%) | +19 -1 =0 (95.0%) | 79.0/85 (92.9%) | 72.0/85 (84.7%) | **151.0 / 170** | **88.8%** | ~2860 Elo |
| **Stockfish 2600** | +90 -6 =4 (92.0%) | +43 -5 =2 (88.0%) | +19 -0 =1 (97.5%) | 80.5/85 (94.7%) | 75.0/85 (88.2%) | **155.5 / 170** | **91.5%** | ~3013 Elo |
| **Stockfish 2700** | +74 -18 =8 (78.0%) | +38 -6 =6 (82.0%) | +17 -1 =2 (90.0%) | 69.5/85 (81.8%) | 67.5/85 (79.4%) | **137.0 / 170** | **80.6%** | ~2947 Elo |
| **Stockfish 2800** | +49 -37 =14 (56.0%) | +27 -15 =8 (62.0%) | +12 -4 =4 (70.0%) | 56.5/85 (66.5%) | 44.5/85 (52.4%) | **101.0 / 170** | **59.4%** | ~2866 Elo |
| **Stockfish 2900** | +42 -46 =12 (48.0%) | +26 -16 =8 (60.0%) | +8 -8 =4 (50.0%) | 49.0/85 (57.6%) | 39.0/85 (45.9%) | **88.0 / 170** | **51.8%** | ~2913 Elo |
| **Stockfish 3000** | +38 -51 =11 (43.5%) | +12 -24 =14 (38.0%) | +6 -7 =7 (47.5%) | 38.0/85 (44.7%) | 34.0/85 (40.0%) | **72.0 / 170** | **42.4%** | ~2947 Elo |
| **Stockfish 3100** | +26 -61 =13 (32.5%) | +8 -28 =14 (30.0%) | +3 -13 =4 (25.0%) | 27.0/85 (31.8%) | 25.5/85 (30.0%) | **52.5 / 170** | **30.9%** | ~2960 Elo |
| **Stockfish 3200** | +21 -63 =16 (29.0%) | +10 -32 =8 (28.0%) | +3 -11 =6 (30.0%) | 27.0/85 (31.8%) | 22.0/85 (25.9%) | **49.0 / 170** | **28.8%** | ~3043 Elo |
| **Stockfish 3300** | +26 -61 =13 (32.5%) | +15 -23 =12 (42.0%) | +5 -10 =5 (37.5%) | 35.0/85 (41.2%) | 26.0/85 (30.6%) | **61.0 / 170** | **35.9%** | ~3199 Elo |
| **Stockfish 3400** | +20 -63 =17 (28.5%) | +10 -33 =7 (27.0%) | +4 -10 =6 (35.0%) | 25.0/85 (29.4%) | 24.0/85 (28.2%) | **49.0 / 170** | **28.8%** | ~3243 Elo |
| **Stockfish 3500** | +18 -69 =13 (24.5%) | +6 -39 =5 (17.0%) | +4 -13 =3 (27.5%) | 24.0/85 (28.2%) | 14.5/85 (17.1%) | **38.5 / 170** | **22.6%** | ~3286 Elo |
| **Stockfish 3600** | +1 -96 =3 (2.5%) | +0 -47 =3 (3.0%) | +0 -20 =0 (0.0%) | 2.5/85 (2.9%) | 1.5/85 (1.8%) | **4.0 / 170** | **2.4%** | ~2956 Elo |

---

### Color Asymmetry & Opening Balance Analysis

<p align="center">
  <img src="assets/benchmark_white_black.png" alt="pawnGO Color Asymmetry & Opening Balance Analysis" width="100%" />
</p>

---

### Time Control Breakdown & Empirical Analysis

<p align="center">
  <img src="assets/benchmark_by_control.png" alt="Performance by Time Control Matrix" width="100%" />
</p>

#### Technical Observations Across Time Controls:
1. **⚡ Fast Blitz (`2+0.05`, 100 Games / Level)**:
   - **Early Tactical Refutation**: pawnGO crushes tactical inaccuracies rapidly against opponents from 2400 to 2700 (78%–94% win rate).
   - **NNUE Advantage**: Singular Extensions and fast evaluation punish positional hesitations before endgames arise.
   - **50% Parity**: Reached at **~2880 Elo**.

2. **⏱️ Slower Blitz (`5+0.1`, 50 Games / Level)**:
   - **Master-Level Stability**: High consistency across opening and middlegame structures, achieving **60.0% against Stockfish 2900** and **42.0% against Stockfish 3300**.
   - **Syzygy Conversion**: 5-man tablebases cleanly finish off simplified advantages.

3. **⏳ Ultra Slow (`10+0.25`, 20 Games / Level)**:
   - **Peak Accuracy Against Sub-2900**: Contrary to common intuition, pawnGO achieves its **highest peak win rates** on Ultra Slow against opponents under 2900 Elo (**100% vs 2400, 95% vs 2500, 97.5% vs 2600, 90% vs 2700, 70% vs 2800**). At 20+ plies of search depth, blunder rates drop to virtually zero.
   - **Parity Baseline**: Holds solid **50.0% parity against Stockfish 2900**.

---

### Game Phase Dynamics: Tactical Knockouts vs Endgame Grinds

<p align="center">
  <img src="assets/benchmark_game_phases.png" alt="pawnGO Phase Analysis: Tactical Middlegame Dominance vs Deep Technical Endgames" width="100%" />
</p>

#### Why pawnGO Dominates Early and Stutters in Late Endgames (>70 moves):
- **💥 Tactical Knockouts (<35 moves)**: pawnGO records an overwhelming **67.6% win rate** (+213 -99 =12) across the entire gauntlet up to 3600 Elo. In early tactics and middlegame complications, the engine excels at finding tactical shots, king attacks, and decisive piece sacrifices.
- **⚔️ Tactical Middlegames (35–50 moves)**: Sustains a solid **51.1% win rate** (+327 -312 =24).
- **♟️ Early Endgames (50–70 moves)**: At **43.1% win rate** (+275 -371 =51), transition into 6- and 7-piece endings before Syzygy triggers is the engine's primary bottleneck against 3000+ opponents.
- **⏳ Deep Technical Grinds (>70 moves)**: Achieves **50.0% win rate** (+176 -176 =174). A massive proportion of long games resolve into theoretical draws (174 draws) or Syzygy 5-piece tablebase holds.
- **Conversion Velocity**: pawnGO wins decisively and quickly with a median game length of **48 moves** (mean 52.7m), whereas losses drag out to a median of **54 moves** (mean 55.8m) when opponents survive the middlegame storm and maneuver into deep technical endgames.

---

### Opening Repertoire Robustness

<p align="center">
  <img src="assets/benchmark_openings.png" alt="Performance Across Major Opening Repertoires" width="100%" />
</p>

pawnGO demonstrates exceptional opening versatility across 2,210 tournament games against grandmaster-level competition:
- **French Defense**: **55.2% win rate** (+75 -59 =20, 154 Games)
- **English Opening**: **52.7% win rate** (+101 -89 =31, 221 Games)
- **Queen's Gambit Declined**: **50.7% win rate** (+98 -95 =28, 221 Games)
- **Sicilian Defense**: **50.4% win rate** (+159 -156 =47, 362 Games)
- **Grünfeld Defense**: **50.0% win rate** (+32 -32 =10, 74 Games)
- **Catalan Opening**: **49.4% win rate** (+33 -34 =11, 78 Games)
- **Caro-Kann Defense**: **49.0% win rate** (+67 -70 =13, 150 Games)
- **Queen's Bishop Game**: **48.6% win rate** (+30 -32 =12, 74 Games)
- **Giuoco Piano**: **45.5% win rate** (+31 -38 =8, 77 Games)
- **Two Knights Defense**: **44.2% win rate** (+30 -39 =8, 77 Games)

---
### Stockfish 17.1 Parallel Referee Evaluation

> Independent evaluation by official Stockfish 17.1 referee over **1,574 tournament games** and **92,659 pawnGO moves**:

| Metric | pawnGO Engine Performance |
| :--- | :--- |
| **Overall Accuracy** | **90.1%** |
| **Average Centipawn Loss (ACPL)** | **24.7 cp** |
| **White Accuracy / ACPL** | **90.4%** (24.1 cp) |
| **Black Accuracy / ACPL** | **89.8%** (25.4 cp) |
| **★ Best Moves** | **57,746** (62.3%) |
| **✓ Good Moves** | **18,361** (19.8%) |
| **?! Inaccuracies** | 11,323 (12.2%) |
| **? Mistakes** | 3,900 (4.2%) |
| **?? Blunders** | **1,329 (1.4%)** |

---

## Web Application

The frontend delivers an interactive, zero-latency desktop and mobile chess experience:

- **Client-Side WASM Execution**: The C++ engine executes entirely inside a dedicated Web Worker via `SharedArrayBuffer` with COOP/COEP compliance.
- **Dynamic Win-Rate Evaluation Bar**: Real-time evaluation bar with smooth asymptotic win-probability mapping.
- **Post-Game Game Review**: Automatic move quality classification engine with move badges:
  - `★` **Best Move** (matching top engine line or negligible delta)
  - `✓` **Good Move** (sound positional play)
  - `?!` **Inaccuracy** ($\Delta \le -0.50$ cp)
  - `?` **Mistake** ($\Delta \le -1.50$ cp or unjustified material sacrifice)
  - `??` **Blunder** ($\Delta > -1.80$ cp)
  - `📖` **Book Move** (Polyglot opening library)
- **Polyglot Opening Book**: Embedded opening book (`Perfect2023.bin`) with instant recognition of major opening lines.
- **Audio Feedback**: Streamlined procedural audio system for moves and captures with instant mute controls.
- **Analysis Tools**: Multi-PV line inspection, custom FEN setup, PGN import/export, and keyboard arrow navigation.

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

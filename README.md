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

- **~2900 Elo Verified Strength**: Defeated calibrated `Stockfish 2900` in a verified 20-game cutechess gauntlet with a **72.5% win rate** (`+12 -3 =5`, 14.5 / 20 points).
- **Grandmaster Accuracy**: Matches evaluated by full **Stockfish 17.1 referee** show **92.6% overall accuracy**, an average centipawn loss (**ACPL**) of only **18.5 cp**, and **70.5% Best Moves**.
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

The engine was evaluated using `cutechess-cli` and a 2-sided adjudication protocol with official Syzygy tablebase adjudication. All matches were independently evaluated by a full **Stockfish 17.1 Referee** across every move:

### 20-Game Gauntlet vs Stockfish 2900 (Time Control: `3+0.05`)

| Parameter | Result |
| :--- | :--- |
| **Final Match Score** | **14.5 / 20 points (72.5%)** |
| **Match Record** | **+12 Wins, -3 Losses, =5 Draws** |
| **White Performance** | 7.5 / 10 points (Accuracy: 91.7%) |
| **Black Performance** | 7.0 / 10 points (Accuracy: 93.4%) |
| **Overall Accuracy** | **92.6%** |
| **Average Centipawn Loss (ACPL)** | **18.5 cp** |

#### Move Quality Breakdown (1,436 pawnGO moves analyzed by Stockfish 17.1)
- **★ Best Moves**: 1,013 (**70.5%**)
- **✓ Good Moves**: 230 (**16.0%**)
- **?! Inaccuracies**: 133 (**9.3%**)
- **? Mistakes**: 46 (**3.2%**)
- **?? Blunders**: 14 (**1.0%**)

### Historical Calibrated Benchmark Summary (`5+0.1` TC)

| Opponent | Target Rating | Score | Win Rate | Performance Rating |
| :--- | :---: | :---: | :---: | :---: |
| **Stockfish** | 2200 Elo | **4.0 - 0.0** | 100.0% | > 2600 Elo |
| **Stockfish** | 2400 Elo | **6.0 - 0.0** | 100.0% | > 2800 Elo |
| **Stockfish** | 2600 Elo | **5.0 - 1.0** | 83.3% | ~2880 Elo |
| **Stockfish** | 2800 Elo | **3.5 - 0.5** | 87.5% | ~3138 Elo |
| **Stockfish** | 2900 Elo | **14.5 - 5.5** | 72.5% | **~2900+ Elo** |

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
│   │   ├── run_gauntlet.sh     # Cutechess automated tournament runner
│   │   ├── referee_stockfish.py# Stockfish 17.1 PGN analysis & referee
│   │   └── openings.pgn        # Tournament opening suite
│   ├── CMakeLists.txt          # Native C++ CMake build definition
│   ├── CMakeLists.wasm.txt     # Emscripten WebAssembly build definition
│   └── Dockerfile.wasm         # Reproducible containerized WASM build toolchain
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

To compile the C++ engine to WebAssembly using the reproducible containerized Emscripten toolchain:

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

# Run against full unconstrained Stockfish 17.1
./engine/scripts/run_gauntlet.sh full "5+0.1" 5 2
```

Tournament games are written in real-time to `engine/scripts/games/match.pgn`, and detailed referee reports are logged to `engine/scripts/tournament.log`.

---

## License

This project is open-source software licensed under the [MIT License](LICENSE).

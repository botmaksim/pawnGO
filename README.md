# pawnGO

pawnGO is a high-performance chess platform featuring a custom C++ UCI chess engine compiled to WebAssembly, paired with a modern React/Vite web application. The engine runs locally in the browser with multi-threaded SIMD execution or as a standalone native binary.

---

## Architectural Overview

The repository consists of two main components:
1. **Engine (`engine/`)**: A modular C++20 chess engine implementing modern search techniques, NNUE positional evaluation, Syzygy endgame tablebase probing, and Polyglot opening book integration.
2. **Client (`client/`)**: A responsive React interface utilizing Web Workers to communicate with the WebAssembly-compiled engine over the Universal Chess Interface (UCI) protocol.

---

## Engine Features and Heuristics

### Board Representation and Move Generation
- **Bitboard Framework**: 64-bit unsigned integers (`U64`) represent piece locations, occupancy, and attack masks.
- **Magic Bitboards**: Precomputed magic hashing algorithms for slider move generation (bishops and rooks).
- **Zobrist Hashing**: Incremental 64-bit position keys for transposition detection and 3-fold repetition validation.
- **Strict Move Validation**: Pseudo-legal move validation ensuring correct destination occupancy, en-passant legality, and promotion sanity.

### Evaluation
- **NNUE (Efficiently Updatable Neural Network)**: Integrates embedded neural network evaluation (`nn-62ef826d1a6d.nnue`), featuring dual-perspective accumulator caches.
- **Incremental Accumulators**: Accumulator updates are performed incrementally during make/unmake operations with fallback resets on irreversible transformations.
- **Syzygy Tablebases**: Full support for 3-, 4-, and 5-man Syzygy endgame tablebases (`tbprobe`) for instant root-level and in-tree WDL resolution.
- **PeSTO Handcrafted Evaluation**: Fallback evaluation used when NNUE weights are unavailable.

### Search Architecture
- **Principal Variation Search (PVS)**: Aspiration windows around the previous iteration's score with adaptive delta widening.
- **Transposition Table (TT)**: 64-byte aligned entries with depth-preferred replacement, generation aging, and fail-low best move preservation.
- **Double Singular Extensions**: Re-searches the TT best move with reduced depth and tight margin; extends search depth by 1 or 2 plies when a move is strictly better than all alternatives.
- **Multi-Cut Pruning**: Early beta cutoffs when non-PV nodes confirm multiple refutations during singular verification.
- **Null Move Pruning (NMP)**: Adaptive depth reduction based on search depth and position improvement status, guarded by zugzwang piece presence checks.
- **Reverse Futility Pruning (RFP / Static NMP)**: Dynamic evaluation margins scaled by depth and static evaluation trends.
- **Futility Pruning & Late Move Pruning (LMP)**: Prunes non-promising quiet moves at low depths while exempting checks and advanced passed pawns.
- **Late Move Reductions (LMR)**: Logarithmic depth/move reduction table adjusted dynamically for killers, countermoves, tactical checks, history scores, and advanced pawns (6th and 7th ranks).
- **Static Exchange Evaluation (SEE)**:
  - Quiet move SEE pruning at low depths.
  - Bad capture SEE pruning in both alpha-beta search and quiescence search.
- **Heuristic Move Ordering**:
  - Transposition table move.
  - Good captures ordered by MVV-LVA and Capture History.
  - Promotions.
  - Killer moves (two slots per ply).
  - Countermove heuristic (keyed by previous move source and target).
  - Continuation history (1-ply and 2-ply context tables).
  - Main history table with smooth gravity decay:
    $$h \leftarrow h + \text{bonus} - \frac{h \cdot |\text{bonus}|}{16384}$$
  - Bad captures (negative SEE).
  - Capture Malus: penalizes non-cutoff captures evaluated prior to a beta cutoff.
  - History Malus: penalizes quiet moves that failed to produce cutoffs.
- **Quiescence Search**: Evaluates tactical captures and queen promotions to eliminate the horizon effect, protected by delta pruning and stand-pat checks.

---

## Empirical Benchmark and Validation

The engine's strength was empirically validated via automated round-robin and gauntlet tournaments using Cutechess-CLI at `5+0.1` time control against Stockfish 17.1 under official UCI calibrated rating limits (`UCI_LimitStrength = true`, `UCI_Elo`):

| Opponent | Calibrated Elo | Score | Win Rate | Performance Rating |
| :--- | :---: | :---: | :---: | :---: |
| **Stockfish** | 2200 | **4.0 - 0.0** | 100.0% | > 2600 Elo |
| **Stockfish** | 2400 | **6.0 - 0.0** | 100.0% | > 2800 Elo |
| **Stockfish** | 2600 | **5.0 - 1.0** | 83.3% | ~2880 Elo |
| **Stockfish** | 2700 | **3.5 - 0.5** | 87.5% | ~3038 Elo |
| **Stockfish** | 2800 | **3.5 - 0.5** | 87.5% | ~3138 Elo |
| **Stockfish** | 2900 | **7.5 - 2.5** | 75.0% | ~3090 Elo |

- **Effective Engine Rating**: Evaluated at **~2900 - 3050 Elo** (Super-Grandmaster strength).
- **Search Throughput**: 1.0 to 2.0+ Million Nodes Per Second (NPS) depending on host hardware and thread allocation.

---

## Web Application

The frontend provides an interactive platform running the engine client-side:
- **Client-Side Engine Execution**: Runs the WebAssembly binary inside a dedicated Web Worker via `SharedArrayBuffer` with COOP/COEP compliance.
- **Evaluation Bar**: Non-linear win-probability mapping based on pawnGO centipawn evaluations.
- **Move Quality Analysis**: Post-game classification identifying best moves, inaccuracies, mistakes, and blunders.
- **Opening Book Exploration**: Automatic Polyglot opening detection.
- **Position Setup**: Custom FEN and castling configuration editor.
- **Game Navigation & Controls**: Keyboard arrow navigation (Left/Right for move history), audio feedback with sound toggles, and multi-PV depth inspection.

---

## Directory Structure

```
pawnGO/
├── client/                     # React web frontend
│   ├── public/wasm/            # Compiled WebAssembly engine artifacts
│   ├── src/                    # UI components, state management, worker adapters
│   └── package.json
├── engine/                     # C++ chess engine core
│   ├── src/
│   │   ├── core/               # Board, bitboards, move types, Zobrist hashing
│   │   ├── eval/               # NNUE evaluation, incremental stack, Syzygy probing
│   │   ├── search/             # Alpha-beta search, move ordering, time allocation
│   │   └── utils/              # Polyglot opening book parsing
│   ├── scripts/                # Automated testing and gauntlet scripts
│   ├── CMakeLists.txt          # Native CMake build definition
│   ├── CMakeLists.wasm.txt     # Emscripten build configuration
│   └── Dockerfile.wasm         # Containerized WASM compilation toolchain
├── docker-compose.yml          # Production deployment specification
└── README.md
```

---

## Building and Installation

### Prerequisites
- **Native compilation**: GCC 11+ or Clang 13+ with C++20 support, CMake 3.16+, Make or Ninja.
- **WebAssembly compilation**: Docker (recommended) or Emscripten SDK (emsdk 3.1.50+).
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

### 2. WebAssembly Engine Build

To compile the C++ source code to WebAssembly using the containerized Emscripten environment:

```bash
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

### 3. Frontend Client Build and Execution

```bash
cd client
npm install
npm run dev
```

The development server starts at `http://localhost:5173`.

To assemble the production build:
```bash
npm run build
```

---

### 4. Running Tournament Benchmarks

To benchmark pawnGO against Stockfish or other engines:

```bash
# Test against calibrated Stockfish Elo 2600 (5 rounds, 10 games, concurrency 2)
./engine/scripts/run_gauntlet.sh 2600 5+0.1 5 2

# Test against calibrated Stockfish Elo 2800
./engine/scripts/run_gauntlet.sh 2800 5+0.1 5 2
```

Match results and PGN records are output to `engine/scripts/tournament.log` and `engine/scripts/games/match.pgn`.

---

## License

This project is licensed under the MIT License.

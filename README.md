# pawnGO: Standalone High-Performance C++ UCI Chess Engine

`pawnGO` is a modern, high-performance chess engine written in C++17. It implements bitboard board representations, SIMD-accelerated NNUE evaluation, Principal Variation Search (PVS) with extensive pruning heuristics, Lazy SMP multi-threading, and Syzygy endgame tablebase probing.

---

## Technical Specifications & Architecture

### Board Representation & Bitboards
- **Bitboard Engine**: 64-bit integer bitmasks (`uint64_t`) for board state and piece locations.
- **Magic Bitboards**: Precomputed hashing for sliding pieces (bishops, rooks, queens) with AVX2 and BMI2 popcount optimizations.
- **Zobrist Hashing**: 64-bit incremental state hashing for transposition table indexing and repetition detection.

### Search Algorithm
- **Search Strategy**: Alpha-Beta Principal Variation Search (PVS) with Iterative Deepening.
- **Selectivity & Pruning**:
  - Quiescence search with delta pruning for tactical stability.
  - Null Move Pruning (adaptive R depending on depth and static eval).
  - Reverse Futility Pruning (static null move pruning).
  - Late Move Reductions (LMR) based on depth, move order, and capture status.
  - Futility Pruning at frontier nodes.
  - Razoring and Internal Iterative Reductions (IIR).
- **Move Ordering**:
  - Hash move from Transposition Table.
  - Most Valuable Victim - Least Valuable Attacker (MVV-LVA) for tactical captures.
  - Killer move heuristics (2 slots per ply).
  - History heuristic and Countermove tables.
- **Concurrency**: Lazy SMP multi-threaded search scaling across multi-core architectures.

### Evaluation & Endgames
- **NNUE (Efficiently Updatable Neural Network)**:
  - Architecture: HalfKP_768 dual-accumulator feature transformer.
  - Acceleration: Hand-optimized AVX2 and BMI2 SIMD vector intrinsics.
- **Syzygy Tablebases**:
  - Native 3-4-5-man WDL (Win-Draw-Loss) and DTZ (Distance-to-Zero) probing via Fathom integration.
  - Dynamic root and leaf tablebase probing during alpha-beta traversal.

---

## Building from Source

### Prerequisites
- Modern C++17 compiler (`g++` >= 9.0 or `clang++` >= 10.0)
- `cmake` >= 3.20
- CPU supporting AVX2, BMI2, and POPCNT (x86-64-v3)

### Compilation
```bash
# Clone the engine-core branch
git clone -b engine-core git@github.com:botmaksim/pawnGO.git
cd pawnGO

# Configure and build in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The resulting binary will be generated at `build/engine/pawnGO`.

---

## UCI Protocol Interface

`pawnGO` communicates using the standard Universal Chess Interface (UCI) protocol:

```bash
$ ./build/engine/pawnGO
uci
id name pawnGO 1.0.0
id author Maksim
option name Hash type spin default 64 min 1 max 65536
option name Threads type spin default 1 min 1 max 512
option name SyzygyPath type string default <empty>
option name SyzygyProbeDepth type spin default 1 min 1 max 100
uciok
isready
readyok
position startpos moves e2e4 e7e5
go depth 14
info depth 14 score cp 32 time 142 nodes 218543 nps 1539035 pv g1f3 b8c6 f1b5
bestmove g1f3 ponder b8c6
quit
```

---

## Benchmarking & Testing

The `engine/scripts/` directory includes automated benchmarking and tournament harnesses:
- **`benchmark_suite.py`**: Multi-run tournament gauntlet against Stockfish Elo-calibrated engines via Cutechess-cli.
- **`parallel_referee.py`**: Parallel Stockfish 17.1 referee verifying Average Centipawn Loss (ACPL), move accuracy, and blunder rates.
- **`plot_benchmark.py` & `plot_advanced_analytics.py`**: High-resolution SVG and PNG telemetry generators for tournament results.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

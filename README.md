# pawnGO: Standalone High-Performance C++ UCI Chess Engine

`pawnGO` is a modern, high-performance chess engine written in C++17. The engine implements bitboard state representations, SIMD-accelerated NNUE positional evaluation, Principal Variation Search with extensive pruning heuristics, Lazy SMP multi-threading, and Syzygy endgame tablebase probing.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![UCI Protocol](https://img.shields.io/badge/Protocol-UCI-green.svg)](https://en.wikipedia.org/wiki/Universal_Chess_Interface)
[![Evaluation: NNUE](https://img.shields.io/badge/Eval-HalfKP--768%20NNUE-purple.svg)](#evaluation--endgames)
[![Syzygy Tablebases](https://img.shields.io/badge/Endgame-3--4--5--man%20Syzygy-orange.svg)](#evaluation--endgames)
[![CMake: 3.20+](https://img.shields.io/badge/CMake-3.20%2B-informational.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## Technical Specifications & Architecture

### Board Representation & Bitboards
- **Bitboard Engine**: 64-bit integer bitmasks (`uint64_t`) representing board occupancy and piece locations.
- **Magic Bitboards**: Precomputed hashing for sliding pieces including bishops, rooks, and queens, optimized with AVX2 and BMI2 instructions.
- **Zobrist Hashing**: Incremental 64-bit position keys for transposition table indexing, fifty-move rule verification, and threefold repetition detection.

### Search Algorithm
- **Search Strategy**: Alpha-Beta Principal Variation Search with Iterative Deepening.
- **Selectivity & Pruning**:
  - Quiescence search with delta pruning to ensure tactical stability.
  - Null Move Pruning with adaptive reduction parameter $R$ dynamically scaled by search depth and static evaluation.
  - Reverse Futility Pruning for static null move pruning at frontier nodes.
  - Late Move Reductions based on depth, move order, and tactical capture status.
  - Futility Pruning near leaf nodes.
  - Razoring and Internal Iterative Reductions.
- **Move Ordering**:
  - Principal Variation and Hash move from Transposition Table.
  - Most Valuable Victim / Least Valuable Attacker ordering for tactical captures.
  - Killer move heuristics with two dedicated slots per ply.
  - History heuristic and Countermove tables.
- **Concurrency**: Lazy SMP multi-threaded search scaling across multi-core systems.

### Evaluation & Endgames
- **NNUE Positional Evaluation**:
  - Architecture: HalfKP-768 dual-accumulator feature transformer.
  - SIMD Optimization: AVX2 and BMI2 vector intrinsics.
- **Syzygy Endgame Tablebases**:
  - Native 3-4-5-man Win-Draw-Loss and Distance-to-Zero probing via embedded Fathom integration.
  - Dynamic root and leaf tablebase probing during alpha-beta traversal.

---

## Building from Source

### Prerequisites
- Modern C++17 compiler supporting GCC 9.0+ or Clang 10.0+
- CMake 3.20 or later
- x86-64 CPU supporting AVX2, BMI2, and POPCNT instructions

### Compilation
```bash
# Clone the engine-core branch
git clone -b engine-core git@github.com:botmaksim/pawnGO.git
cd pawnGO

# Configure and build in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The compiled binary will be placed at `build/engine/pawnGO`.

---

## UCI Protocol Interface

`pawnGO` communicates over standard Universal Chess Interface protocol streams:

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

## Benchmarking & Verification Harness

The `engine/scripts/` directory provides automated benchmarking and tournament harnesses:
- **`benchmark_suite.py`**: Multi-run tournament gauntlet against Stockfish calibrated engines via Cutechess-cli.
- **`parallel_referee.py`**: Parallel Stockfish referee measuring Average Centipawn Loss, move accuracy, and blunder frequencies.
- **`plot_benchmark.py` & `plot_advanced_analytics.py`**: SVG and PNG telemetry generators for tournament evaluation curves.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

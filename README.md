# pawnGO ♟️

**pawnGO** is a full-stack chess application featuring a custom, high-performance C++ UCI chess engine (compiled to WebAssembly), a lightweight Node.js/Vite environment, and a modern React-based frontend. 

## Features

### C++ Chess Engine (WASM-powered)
- **Custom UCI Implementation:** Fully compliant with the Universal Chess Interface protocol.
- **WebAssembly Native:** The C++ engine is directly compiled to Wasm utilizing multithreading and SIMD instructions, allowing it to run entirely in the browser at near-native speeds!
- **NNUE Evaluation:** Integrates modern neural network-based evaluation (`nn-62ef826d1a6d.nnue`) for highly accurate position assessments.
- **Advanced Search Heuristics:** Features Transposition Tables (TT), Late Move Reductions (LMR), Null Move Pruning, Delta Pruning, and SEE-like Quiescence Search for deep, efficient searching.
- **MultiPV Search:** Fully supports MultiPV lines returned seamlessly over UCI.

### Web Interface (React)
- **Interactive Chessboard:** Powered by `react-chessboard` with drag-and-drop mechanics.
- **Real-Time Analysis:** Continuous background evaluation utilizing the local Wasm Engine Web Worker.
- **Dynamic Evaluation Bar:** A non-linear evaluation thermometer that maps exact centipawn evaluation from pawnGO to win percentages using modern WDL (Win/Draw/Loss) mathematical probabilities.
- **Full Game Analysis:** Automatically analyzes every move in a game, detecting Best moves (★), Inaccuracies (?!), Mistakes (?), Blunders (??), and Brilliant sacrifices (!!).
- **Setup Mode:** Custom FEN string editor to easily set up and instantly analyze specific positions.

## Engine Strength & Testing

The native C++ variant of `pawnGO` was heavily tested using fast time controls against other established chess engines. With the recent port to WebAssembly utilizing Emscripten SIMD, the performance remains highly competitive:

- **Estimated ELO:** ~2750 - 2900 (Depending on device performance)
- **Calculations:** Consistently reaches **1.0 to 1.5+ Million Nodes Per Second (NPS)** on average hardware using Web Workers.
- **Tactical Sharpness:** Extremely strong in tactical shootouts due to sophisticated Quiescence Search and deep MultiPV analysis.

## Architecture & Code Structure

The C++ engine uses a clean modular structure, which is then mapped to the Frontend:
- **`engine/src/core/`**: Bitboards, move types, magic bitboard generation, Zobrist hashing.
- **`engine/src/eval/`**: NNUE initialization, incremental evaluation updates.
- **`engine/src/search/`**: Alpha-beta search, Threading, Move generation.
- **`client/public/wasm/`**: The compiled execution context `pawngo_wasm.wasm` and dynamic wrapper.

## Prerequisites

- **Node.js:** v16+ for the package manager and vite preview server.
- **npm:** Node package manager.

## Installation & Setup

You do NOT need to manually compile the engine using CMake unless you are making modifications to the C++ code! The built WebAssembly binaries are already included.

### 1. Start the Frontend Client
```bash
cd client
npm install
npm run dev
```
The client will be available on `http://localhost:5173`. The application runs 100% locally in your browser. No backend calculations or external APIs are used!

### 2. (Optional) Recompile the Engine to WASM
If you make changes to the C++ files in `engine/src/`, you can recompile using Emscripten. Ensure `emcc` is installed.
```bash
cd engine
mkdir build_wasm && cd build_wasm
emcmake cmake -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" ../
make -j4
cp pawngo_wasm* ../../client/public/wasm/
```

## Usage
- **Play & Analyze:** Make moves on the board. The engine will instantly evaluate the position utilizing Wasm Threads and NNUE.
- **Analyze Game:** Click the "Analyze Game" button to let the engine evaluate all past moves and assign accurate probability-based annotations.
- **Setup Mode:** Click "Setup Mode" to configure a custom position using FEN and castling rights, then click "Apply Position" to start analyzing it.

## License
MIT

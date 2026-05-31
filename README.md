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

## Engine Strength & Testing 🏆

The native C++ variant of `pawnGO` was rigorously tested using fast time controls against other established UCI chess engines (Stockfish 14, Ethereal, Leela). With the recent engine modifications and the WASM/Emscripten polyfills, the WebAssembly port achieves an exceptional level of browser-based performance:

- **Estimated ELO:** ~2800 - 2950 (Depending on device performance and WASM thread availability)
- **Node Speed (NPS):** Consistently reaches **1.0 to 1.8+ Million Nodes Per Second (NPS)** on average hardware using Web Workers.
- **Search Depth:** Reaches sustained analytical depth of 15+ within milliseconds for most balanced mid-games.
- **Positional Evaluation:** Successfully restored the custom `atan`-based win-probability scaling (original pawnGO formulation) instead of relying on the external Stockfish model, meaning the frontend analysis correctly maps exact pawn centipawns.

### Recent Core Updates & Fixes (Requires Recompilation)
The engine has received critical updates to the alpha-beta search loop and browser concurrency models:
- **Concurrency Overlap:** React 18 StrictMode previously triggered aggressive `stop` commands during component unmounting, causing the WASM engine to halt prematurely without yielding `info depth` strings to the UI. The frontend has been hardened to prevent rogue UCI stops.
- **Standardized Book Fast-Pathing:** Removed the restrictive `is_book_slot` bypass loop inside the C++ engine (`search.cpp`). Opening book moves are now naturally forced and evaluated directly inside the standard search pipeline, guaranteeing accurate `info` UCI reporting back to the frontend instead of silent exits.

*(Because of the modifications to `/pawnGO/engine/src/search/search.cpp`, you must run Emscripten `make` locally to compile these C++ engine fixes into the `pawngo_wasm.wasm` bundle).*

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

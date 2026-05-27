# pawnGO ♟️

**pawnGO** is a full-stack chess application featuring a custom, high-performance C++ UCI chess engine, a Node.js WebSocket backend, and a modern React-based frontend. 

## Features

### C++ Chess Engine
- **Custom UCI Implementation:** Fully compliant with the Universal Chess Interface protocol.
- **NNUE Evaluation:** Integrates modern neural network-based evaluation (`nn-62ef826d1a6d.nnue`) for highly accurate position assessments.
- **Advanced Search Heuristics:** Features Transposition Tables (TT), Late Move Reductions (LMR), Null Move Pruning, Delta Pruning, and SEE-like Quiescence Search for deep, efficient searching.
- **Syzygy Tablebases:** Built-in support for 3-4-5 piece endgame tablebases for perfect endgame play.
- **Polyglot Opening Book:** Parses `.bin` books, providing multiple book lines to the client in the opening, and auto-playing random book moves for the first 2 full moves of a game.
- **MultiPV Search:** Fully supports MultiPV lines returned seamlessly over UCI.

### Web Interface (React)
- **Interactive Chessboard:** Powered by `react-chessboard` with drag-and-drop mechanics.
- **Real-Time Analysis:** Continuous background evaluation connected via WebSockets, dynamically parsing MultiPV.
- **Book Move Integration:** Identifies opening book moves ("Book") and displays them alongside evaluations.
- **Full Game Analysis:** Automatically analyzes every move in a game, detecting Best moves (★), Inaccuracies (?!), Mistakes (?), Blunders (??), and Brilliant sacrifices (!!).
- **Dynamic Evaluation Bar:** A non-linear evaluation thermometer that accurately visualizes small advantages in centipawns.
- **Setup Mode:** Custom FEN string editor to easily set up and instantly analyze specific positions.
- **Move Tree & Variations:** Keeps track of the main line and variations in a structured game tree.

## Architecture & Code Structure

The C++ engine uses a clean modular structure:
- **`engine/src/core/`**: Bitboards, move types, magic bitboard generation, Zobrist hashing.
- **`engine/src/eval/`**: NNUE initialization, incremental evaluation updates, Syzygy probing.
- **`engine/src/search/`**: Alpha-beta search, Quiescence search, Transposition Tables (TT), Move generation.
- **`engine/src/utils/`**: Polyglot book reading.

The project is split into three main parts:
1. **`engine/`**: The C++ source code for the pawnGO UCI engine.
2. **`server/`**: A Node.js server that spawns the engine process and communicates with the frontend over WebSockets.
3. **`client/`**: A React single-page application (built with Vite) that provides the user interface.

## Prerequisites

- **C++ Compiler:** `g++` (or `clang`) with C++17 support.
- **CMake:** For building the C++ engine (version 3.20+).
- **Node.js:** v16+ for the backend and frontend.
- **npm:** Node package manager.

## Installation & Setup

### 1. Build the Engine
```bash
cd engine
mkdir build && cd build
cmake ..
make -j4
```
Ensure that the `3-4-5` Syzygy tablebases are located in `engine/3-4-5/`. 
Ensure your polyglot book is at `engine/perfect/Perfect2023.bin` (or adjust it in the UCI config).

### 2. Start the Backend Server
```bash
cd server
npm install
node index.js
```
The server will run on `http://localhost:8080` and automatically bind to the compiled C++ engine.

### 3. Start the Frontend Client
```bash
cd client
npm install
npm run dev
```
The client will be available on `http://localhost:5173`.

## Usage
- **Play & Analyze:** Make moves on the board. The engine will automatically evaluate the current position using NNUE.
- **Opening Explorer:** Standard opening variations will show up marked as "Book".
- **Analyze Game:** Click the "Analyze Game" button to let the engine evaluate all past moves and assign annotations.
- **Setup Mode:** Click "Setup Mode" to configure a custom position using FEN and castling rights, then click "Apply Position" to start analyzing it immediately.

## Engine Tournament Testing
A script is provided to test `pawnGO` against other engines like Stockfish:
```bash
cd engine/scripts
./run_gauntlet.sh 15 "30+0.5"
```
This tests the engine against Stockfish level 15 at 30 seconds + 0.5s increment. It outputs games into `engine/scripts/games/` and a log in `tournament.log`.

## License
MIT

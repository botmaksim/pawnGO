# pawnGO ♟️

**pawnGO** is a full-stack chess application featuring a custom, high-performance C++ UCI chess engine, a Node.js WebSocket backend, and a modern React-based frontend. 

## Features

### C++ Chess Engine
- **Custom UCI Implementation:** Fully compliant with the Universal Chess Interface protocol.
- **NNUE Evaluation:** Integrates modern neural network-based evaluation (`nn-62ef826d1a6d.nnue`) for highly accurate position assessments.
- **Advanced Search Heuristics:** Features Transposition Tables (TT) and Late Move Reductions (LMR) for deep, efficient searching.
- **Syzygy Tablebases:** Built-in support for 3-4-5 piece endgame tablebases for perfect endgame play.

### Web Interface (React)
- **Interactive Chessboard:** Powered by `react-chessboard` with drag-and-drop mechanics.
- **Real-Time Analysis:** Continuous background evaluation connected via WebSockets.
- **Full Game Analysis:** Automatically analyzes every move in a game, detecting Best moves (★), Inaccuracies (?!), Mistakes (?), Blunders (??), and Brilliant sacrifices (!!).
- **Dynamic Evaluation Bar:** A non-linear (sigmoid-based) evaluation thermometer that accurately visualizes small advantages in centipawns.
- **Setup Mode:** Custom FEN string editor to easily set up and instantly analyze specific positions.
- **Move Tree & Variations:** Keeps track of the main line and variations in a structured game tree.

## Architecture

1. **`engine/`**: The C++ source code for the pawnGO UCI engine.
2. **`server/`**: A Node.js server that spawns the engine process and communicates with the frontend over WebSockets.
3. **`client/`**: A React single-page application (built with Vite) that provides the user interface.

## Prerequisites

- **C++ Compiler:** `g++` (or `clang`) with C++17 support.
- **CMake:** For building the C++ engine.
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

### 2. Start the Backend Server
```bash
cd server
npm install
index.js
```
The server will run on `http://localhost:3000` and automatically bind to the compiled C++ engine.

### 3. Start the Frontend Client
```bash
cd client
npm install
node index.js
```
The client will be available on `http://localhost:5173`.

## Usage
- **Play & Analyze:** Make moves on the board. The engine will automatically evaluate the current position.
- **Analyze Game:** Click the "Analyze Full Game" button to let the engine evaluate all past moves and assign annotations.
- **Setup Mode:** Click "Setup Mode" to configure a custom position using FEN, then click "Apply Position" to start analyzing it immediately.

## License
MIT

# pawnGO ♟️🚀

pawnGO is an incredibly fast, highly optimized, and visually stunning modern Chess Engine and Web Application. 
It combines a blazing fast C++ core with a dynamic React frontend, providing a completely unique experience through its **Visual Engine** and **Explainable AI (LLM Coach)** capabilities.

## 🌟 Key Features

### 1. Visual Engine (Thought Visualization)
Unlike standard engines that output dry text strings, pawnGO visualizes its thought process in real-time directly on the board.
- 🟢 **Green Arrow**: The engine's top choice (Multi-PV 1)
- 🔵 **Blue Arrow**: The second best choice (Multi-PV 2)
- 🟠 **Orange Arrow**: The third best choice (Multi-PV 3)
*You can also draw your own custom arrows (Right-Click + Drag)!*

### 2. Explainable AI & LLM Coach (Shadow HCE)
Modern chess engines rely heavily on Neural Networks (NNUE) which are powerful but act as "black boxes". pawnGO solves this by introducing a **Shadow HCE** (Hand-Crafted Evaluation) module. 
- While the NNUE finds the best moves, the Shadow HCE extracts human-understandable features (Material, Piece Activity, King Safety, Center Control).
- These factors are sent to an **Isolated LLM Coach** (Node.js backend) which generates human-readable advice in real-time.
- Supports local LLMs (e.g., LLaMA 3 via Ollama) out of the box with a built-in fallback template system if the local LLM is offline.

### 3. High-Performance C++ Core
- **NNUE Evaluation**: Lightning fast neural network evaluation.
- **Magic Bitboards**: Instant pseudo-legal move generation without loops.
- **Lazy SMP Multithreading**: Shared transposition table across multiple persistent threads for immense speedups.
- **Advanced Search Heuristics**: Alpha-Beta pruning, Quiescence Search, Null Move Pruning, Late Move Reductions (LMR), Futility Pruning, and History Heuristics.

---

## 🏗️ Architecture

pawnGO is split into three decoupled layers:

1. **`engine/` (C++17 Core)**
   - The heart of the application. Uses the standard UCI protocol.
   - Includes custom commands like `explain` which triggers the Shadow HCE extraction.
2. **`server/` (Node.js WebSocket API)**
   - Bridges the engine and the frontend.
   - Hosts the `LLMProvider` logic to intercept engine JSONs and query local language models for natural language coaching.
3. **`client/` (React + Vite)**
   - Beautiful, responsive glassmorphism UI.
   - Parses Multi-PV data to draw thought arrows and renders real-time evaluations and coach advice.

---

## 🚀 Getting Started

### Prerequisites
- `CMake` (>= 3.10)
- `C++17` compatible compiler (GCC, Clang, MSVC)
- `Node.js` (>= 18)
- *Optional:* [Ollama](https://ollama.com/) for LLM Coach integration.

### 1. Build the Engine
```bash
cd engine
mkdir build && cd build
cmake ..
make -j4
```

### 2. Start the Server
```bash
cd server
npm install
node index.js
```
*The server will start on port 8080 and automatically boot the compiled C++ engine.*

### 3. Start the Frontend
Open a new terminal:
```bash
cd client
npm install
npm run dev
```
Navigate to `http://localhost:5173` (or the port Vite provides) in your browser.

---

## 🤖 Using the LLM Coach with Ollama

By default, the LLM Coach uses a built-in fast template generator to give you advice based on the Shadow HCE evaluation. If you want true, dynamic AI comments:
1. Install [Ollama](https://ollama.com/).
2. Run a lightweight model: `ollama run llama3` (or `phi3`).
3. Refresh pawnGO. The Node.js server will automatically detect the local API (`http://localhost:11434`) and begin streaming real-time AI thoughts into the Coach panel!

## ⚡ Performance Toggle
If you want to allocate 100% of your CPU to deep engine calculations, simply uncheck the **"Visual Engine & AI Coach"** toggle in the UI. This will disable visual thought parsing, LLM generation, and Shadow HCE extraction, allowing the engine to reach its maximum Nodes Per Second (NPS).

---
*Created with ❤️ by Maksim*

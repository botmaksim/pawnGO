# pawnGO: Web Application & WebAssembly Engine

`pawnGO Web` is an interactive, browser-based chess platform powered by the C++ `pawnGO` chess engine compiled to WebAssembly (WASM). It provides real-time client-side evaluation, responsive move calculation via Web Workers, dynamic evaluation graphing, and audio feedback.

---

## Architectural Overview

### Client Architecture (`client/`)
- **Framework**: React 18 with Vite bundling.
- **Engine Execution**: Dedicated Web Worker (`engineWorker_v3.js`) running `pawngo_wasm.js` and `pawngo_wasm.wasm`.
  - Non-blocking asynchronous calculation prevents main UI thread stutter.
  - Zero server round-trips for move calculation: 100% of engine computation happens locally in the user's browser.
- **Real-Time Evaluation Bar**: Live centipawn and mate score rendering synced with engine search iterations.
- **Move Sanity & Legal Moves**: Validated via `chess.js` with sound effects and board animations.

### Server Architecture (`server/`)
- **Runtime**: Node.js with Express.
- **Role**: Lightweight API gateway and static asset serving for production deployments.

### WebAssembly Engine Build (`engine/`)
- **Cross-Compiler**: Emscripten (via `engine/Dockerfile.wasm` and `engine/CMakeLists.wasm.txt`).
- **Optimization**: WebAssembly SIMD and thread support configured for modern browsers.
- **Preloaded Data**: Packaged binary data (`pawngo_wasm.data`) containing precomputed piece tables and opening parameters.

---

## Quickstart & Deployment

### Running with Docker Compose (Recommended)
To launch the full production environment:

```bash
docker compose up --build
```

The application will be accessible at `http://localhost:3000` (or configured port).

### Local Development Setup

#### 1. Client Setup
```bash
cd client
npm install
npm run dev
```

#### 2. Server Setup
```bash
cd server
npm install
npm start
```

---

## Rebuilding WebAssembly Engine

To recompile the C++ engine to WASM from source:

```bash
docker build -f engine/Dockerfile.wasm -t pawngo-wasm engine/
docker create --name temp-wasm pawngo-wasm
docker cp temp-wasm:/build/pawngo_wasm.js client/public/wasm/
docker cp temp-wasm:/build/pawngo_wasm.wasm client/public/wasm/
docker cp temp-wasm:/build/pawngo_wasm.data client/public/wasm/
docker rm temp-wasm
```

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

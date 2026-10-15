# pawnGO: Web Application & WebAssembly Engine

`pawnGO Web` is an interactive, browser-based chess platform powered by the C++ `pawnGO` chess engine compiled to WebAssembly. The platform provides real-time client-side evaluation, responsive move calculation via Web Workers, dynamic evaluation graphs, and audio feedback.

[![React 18](https://img.shields.io/badge/React-18-cyan.svg)](https://react.dev/)
[![Vite](https://img.shields.io/badge/Vite-5.x-646CFF.svg)](https://vitejs.dev/)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-SIMD%20%7C%20Threads-orange.svg)](https://webassembly.org/)
[![Docker Compose](https://img.shields.io/badge/Docker-Compose-2496ED.svg)](https://www.docker.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Live Deployment**: [pawngo.mybsu.online](https://pawngo.mybsu.online)

---

## Architectural Overview

### Client Architecture: `client/`
- **Framework**: React 18 bundled with Vite.
- **Engine Execution**: Dedicated Web Worker `engineWorker_v3.js` executing `pawngo_wasm.js` and `pawngo_wasm.wasm`.
  - Asynchronous background calculation prevents UI thread blocking.
  - Zero server round-trips for move evaluation: engine computation executes entirely within the browser.
- **Real-Time Evaluation Bar**: Live centipawn and mate score rendering synchronized with engine search depth iterations.
- **Move Legality & State**: Validated using `chess.js` with integrated sound synthesis and smooth board animations.

### Server Architecture: `server/`
- **Runtime**: Node.js with Express.
- **Role**: Lightweight HTTP static file server and reverse proxy for containerized deployments.

### WebAssembly Engine Build: `engine/`
- **Cross-Compiler**: Emscripten toolchain orchestrated via `engine/Dockerfile.wasm` and `engine/CMakeLists.wasm.txt`.
- **Instruction Set**: WebAssembly SIMD and thread extensions targeted for modern browsers.
- **Embedded Binary Assets**: Bundled binary payload `pawngo_wasm.data` containing precomputed piece-square tables and opening books.

---

## Quickstart & Deployment

### Production Deployment with Docker Compose
To launch the complete application stack:

```bash
docker compose up --build
```

The application will be accessible at `http://localhost:3000`.

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

To recompile the native C++ engine to WebAssembly from source:

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

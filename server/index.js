const { spawn } = require('child_process');
const WebSocket = require('ws');
const express = require('express');
const http = require('http');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// Spawn the C++ engine process
// Assuming the engine is built in ../engine/build/pawnGO
const enginePath = path.join(__dirname, '../engine/build/pawnGO');
let engineProcess = null;

const log = (msg) => {
    const t = new Date().toISOString().replace('T', ' ').substr(0, 19);
    console.log(`[${t}] ${msg}`);
};
const errLog = (msg) => {
    const t = new Date().toISOString().replace('T', ' ').substr(0, 19);
    console.error(`[${t}] ${msg}`);
};

function startEngine() {
    log(`Starting engine from ${enginePath}`);
    engineProcess = spawn(enginePath, [], { cwd: path.join(__dirname, '../engine/build') });

    engineProcess.stdout.on('data', (data) => {
        const output = data.toString();
        const lines = output.split('\n');
        
        lines.forEach(line => {
            if (!line) return;
            log(`Engine OUT: ${line.trim()}`);
            // Broadcast standard lines to all connected clients
            wss.clients.forEach((client) => {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(line);
                }
            });
        });
    });

    engineProcess.stderr.on('data', (data) => {
        errLog(`Engine ERR: ${data.toString().trim()}`);
    });

    engineProcess.on('close', (code) => {
        log(`Engine process exited with code ${code}`);
        // Optional: restart engine?
    });
}

wss.on('connection', (ws) => {
    log('Client connected');

    ws.on('message', (message) => {
        const cmd = message.toString();
        log(`Client IN: ${cmd}`);
        if (engineProcess && engineProcess.stdin) {
            engineProcess.stdin.write(cmd + '\n');
        }
    });

    ws.on('close', () => {
        log('Client disconnected');
    });
});

const PORT = process.env.PORT || 8080;
server.listen(PORT, () => {
    log(`Server listening on port ${PORT}`);
    // Start engine when server starts
    startEngine();
});

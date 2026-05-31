const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');
const path = require('path');

const app = express();
app.use(cors());

const enginePath = '/app/engine/pawnGO';

app.get('/api/tablebase', (req, res) => {
    const fen = req.query.fen;
    if (!fen) {
        return res.status(400).json({ error: 'FEN is required' });
    }

    console.log(`[Tablebase Request] FEN: ${fen}`);

    const engineProcess = spawn(enginePath, []);

    engineProcess.on('error', (err) => {
        console.error('Failed to start engine process:', err);
        clearTimeout(timeout);
        if (!res.headersSent) {
            res.status(500).json({ error: 'Failed to start engine' });
        }
    });

    let bestmove = null;
    let outputBuffer = '';

    // Timeout to kill engine if it takes too long
    const timeout = setTimeout(() => {
        engineProcess.kill();
        if (!res.headersSent) {
            res.status(504).json({ error: 'Engine timeout' });
        }
    }, 2000);

    engineProcess.stdout.on('data', (data) => {
        const output = data.toString();
        outputBuffer += output;
        
        const lines = output.split('\n');
        for (const line of lines) {
            if (line.startsWith('bestmove')) {
                bestmove = line.split(' ')[1];
                
                clearTimeout(timeout);
                engineProcess.kill();
                
                if (!res.headersSent) {
                    res.json({ bestmove });
                }
                return;
            }
        }
    });

    engineProcess.stderr.on('data', (data) => {
        console.error(`Engine Error: ${data.toString()}`);
    });

    // Initialize engine
    engineProcess.stdin.write('setoption name Threads value 1\n');
    engineProcess.stdin.write('setoption name Hash value 16\n');
    engineProcess.stdin.write('setoption name SyzygyPath value /app/syzygy\n');
    
    // Set position and search
    engineProcess.stdin.write(`position fen ${fen}\n`);
    // movetime 100 ensures that if Syzygy is missing, it won't hang the server forever
    engineProcess.stdin.write('go movetime 100\n');
});

const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`Tablebase server listening on port ${PORT}`);
});

const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');
const readline = require('readline');

const app = express();
app.use(cors());

const enginePath = process.env.ENGINE_PATH || '/app/engine/pawnGO';
const syzygyPath = process.env.SYZYGY_PATH || '/app/syzygy';

class EngineDaemon {
    constructor() {
        this.process = null;
        this.rl = null;
        this.isReady = false;
        this.currentJob = null;
        this.queue = [];
        this.restartCount = 0;
        this.init();
    }

    init() {
        console.log(`[EngineDaemon] Spawning persistent engine process: ${enginePath}`);
        try {
            this.process = spawn(enginePath, [], {
                stdio: ['pipe', 'pipe', 'pipe']
            });
        } catch (err) {
            console.error('[EngineDaemon] Failed to spawn engine:', err);
            return;
        }

        this.rl = readline.createInterface({
            input: this.process.stdout,
            crlfDelay: Infinity
        });

        this.rl.on('line', (line) => this.handleLine(line));

        this.process.stderr.on('data', (data) => {
            console.error(`[Engine Error]: ${data.toString().trim()}`);
        });

        this.process.on('close', (code) => {
            console.warn(`[EngineDaemon] Engine exited with code ${code}`);
            this.isReady = false;
            if (this.currentJob) {
                clearTimeout(this.currentJob.timeout);
                this.currentJob.reject(new Error('Engine process exited unexpectedly'));
                this.currentJob = null;
            }
            // Reject all queued jobs
            while (this.queue.length > 0) {
                const job = this.queue.shift();
                clearTimeout(job.timeout);
                job.reject(new Error('Engine process exited'));
            }
            // Auto restart with backoff
            setTimeout(() => this.init(), 1000);
        });

        // Initialize engine options once
        this.send('uci');
        this.send('setoption name Threads value 1');
        this.send('setoption name Hash value 16');
        this.send(`setoption name SyzygyPath value ${syzygyPath}`);
        this.send('isready');
    }

    send(cmd) {
        if (this.process && this.process.stdin.writable) {
            this.process.stdin.write(cmd + '\n');
        }
    }

    handleLine(line) {
        const trimmed = line.trim();
        if (trimmed === 'readyok') {
            this.isReady = true;
            console.log('[EngineDaemon] Engine is initialized and ready');
            this.processNext();
            return;
        }

        if (this.currentJob) {
            if (trimmed.startsWith('info string TB')) {
                this.currentJob.tbInfo = trimmed;
            }
            if (trimmed.startsWith('bestmove')) {
                const parts = trimmed.split(' ');
                const bestmove = parts[1] || 'none';
                const job = this.currentJob;
                this.currentJob = null;
                clearTimeout(job.timeout);
                job.resolve({ bestmove, tbInfo: job.tbInfo || null });
                this.processNext();
            }
        }
    }

    processNext() {
        if (!this.isReady || this.currentJob || this.queue.length === 0) {
            return;
        }

        this.currentJob = this.queue.shift();
        const { fen, timeoutMs } = this.currentJob;

        this.currentJob.timeout = setTimeout(() => {
            console.warn(`[EngineDaemon] Query timeout for FEN: ${fen}`);
            const job = this.currentJob;
            this.currentJob = null;
            this.send('stop');
            job.reject(new Error('Engine query timed out'));
            this.processNext();
        }, timeoutMs);

        this.send(`position fen ${fen}`);
        // Quick query: if Syzygy hits, tb_probe_root immediately responds; otherwise 100ms search
        this.send('go movetime 100');
    }

    queryFen(fen, timeoutMs = 1500) {
        return new Promise((resolve, reject) => {
            this.queue.push({ fen, timeoutMs, resolve, reject, tbInfo: null });
            this.processNext();
        });
    }
}

const engineDaemon = new EngineDaemon();

app.get('/api/health', (req, res) => {
    res.json({
        status: 'ok',
        engineReady: engineDaemon.isReady,
        queueLength: engineDaemon.queue.length
    });
});

app.get('/api/tablebase', async (req, res) => {
    const fen = req.query.fen;
    if (!fen) {
        return res.status(400).json({ error: 'FEN is required' });
    }

    try {
        const result = await engineDaemon.queryFen(fen, 1500);
        res.json(result);
    } catch (err) {
        console.error(`[Tablebase Request Failed]: ${err.message}`);
        res.status(504).json({ error: err.message });
    }
});

const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`Tablebase daemon server listening on port ${PORT}`);
});

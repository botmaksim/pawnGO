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

// LLM Provider / Coach Service
class LLMProvider {
    static async generate(explainData) {
        // Prepare prompt
        const factors = [
            { name: 'Материал', key: 'material', value: explainData.material },
            { name: 'Активность', key: 'activity', value: explainData.activity },
            { name: 'Безопасность Короля', key: 'king_safety', value: explainData.king_safety },
            { name: 'Контроль Центра', key: 'center_control', value: explainData.center_control },
        ];
        
        let maxDelta = 0;
        let dominantFactor = null;
        let isWhiteAdvantage = true;

        factors.forEach(f => {
            if (Math.abs(f.value) > maxDelta) {
                maxDelta = Math.abs(f.value);
                dominantFactor = f;
                isWhiteAdvantage = f.value > 0;
            }
        });

        // 1. Try hitting local Ollama (LLaMA 3 or phi3)
        try {
            const prompt = `Ты шахматный тренер. Объясни текущую позицию очень коротко (1 предложение). Главный фактор: ${dominantFactor?.name || 'Нет'} с перевесом ${maxDelta} в пользу ${isWhiteAdvantage ? 'белых' : 'черных'}. Не используй сложные форматы, только текст.`;
            
            const response = await fetch('http://localhost:11434/api/generate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model: 'llama3', // or 'phi3'
                    prompt: prompt,
                    stream: false
                }),
                // short timeout so it doesn't hang if Ollama is not running
                signal: AbortSignal.timeout(1000)
            });
            if (response.ok) {
                const data = await response.json();
                return data.response;
            }
        } catch (e) {
            // Fallback if Ollama is unreachable
        }

        // 2. Fallback Template Generator
        let message = "Позиция примерно равна, идет сложная позиционная борьба.";
        if (dominantFactor && maxDelta >= 0.5) {
            const side = isWhiteAdvantage ? "белых" : "черных";
            const otherSide = isWhiteAdvantage ? "черных" : "белых";
            
            if (dominantFactor.key === 'material') message = `У ${side} серьезное материальное преимущество.`;
            else if (dominantFactor.key === 'activity') message = `Фигуры ${side} расположены намного активнее и простреливают важные поля.`;
            else if (dominantFactor.key === 'king_safety') message = `У ${otherSide} серьезные проблемы с безопасностью короля!`;
            else if (dominantFactor.key === 'center_control') message = `${side.charAt(0).toUpperCase() + side.slice(1)} полностью доминируют в центре доски.`;
        }
        return message;
    }
}

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
    engineProcess = spawn(enginePath);

    engineProcess.stdout.on('data', (data) => {
        const output = data.toString();
        const lines = output.split('\n');
        
        lines.forEach(line => {
            if (!line) return;
            log(`Engine OUT: ${line.trim()}`);
            
            if (line.startsWith('info string Explain:')) {
                try {
                    const jsonStr = line.replace('info string Explain:', '').trim();
                    const explainData = JSON.parse(jsonStr);
                    
                    // Trigger LLM asynchronously
                    LLMProvider.generate(explainData).then(text => {
                        const coachMsg = `info string Coach: ${text}`;
                        log(`Sending to client: ${coachMsg}`);
                        wss.clients.forEach((client) => {
                            if (client.readyState === WebSocket.OPEN) {
                                client.send(coachMsg);
                                // Also send the raw JSON so client can render factors if desired
                                client.send(line);
                            }
                        });
                    });
                } catch (e) {
                    errLog(`Failed to parse Explain JSON: ${e.message}`);
                }
            } else {
                // Broadcast standard lines to all connected clients
                wss.clients.forEach((client) => {
                    if (client.readyState === WebSocket.OPEN) {
                        client.send(line);
                    }
                });
            }
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

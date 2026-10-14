import { Chess } from 'chess.js';

export function countMaterial(fen) {
    const pieces = fen.split(' ')[0];
    const values = { 'p': 1, 'n': 3, 'b': 3, 'r': 5, 'q': 9 };
    let whiteMat = 0, blackMat = 0;
    for (let char of pieces) {
        if (values[char.toLowerCase()]) {
            if (char === char.toUpperCase()) whiteMat += values[char.toLowerCase()];
            else blackMat += values[char.toLowerCase()];
        }
    }
    return { white: whiteMat, black: blackMat };
}

export function parseScoreToPawns(scoreStr) {
    if (!scoreStr) return undefined;
    if (scoreStr === 'TBW') return 199.00;
    if (scoreStr === 'TBL') return -199.00;
    if (scoreStr === 'Book') return 0;
    if (scoreStr.startsWith('M')) {
        const moves = parseInt(scoreStr.substring(1), 10);
        return 100.00 - moves; // Mate is very high score
    }
    if (scoreStr.startsWith('-M')) {
        const moves = parseInt(scoreStr.substring(2), 10);
        return -100.00 + moves; // Negative mate
    }
    const pf = parseFloat(scoreStr);
    return isNaN(pf) ? undefined : pf;
}

// Winning probability model: W(p) = 1 / (1 + 10^(-p/4))
export function winProbability(pawns) {
    if (pawns === undefined || isNaN(pawns)) return 0.5;
    const clamped = Math.max(-20, Math.min(20, pawns));
    return 1 / (1 + Math.pow(10, -clamped / 4));
}

// Classify a move from Node A to Node B
export function classifyMove(nodeA, nodeB, sanMove) {
    if (!nodeA || !nodeB) return '';

    // 1. Opening Book Detection
    if (nodeA.isBook || nodeB.isBook || nodeA.pvLines?.['1']?.score === 'Book' || nodeB.pvLines?.['1']?.score === 'Book') {
        return '📖'; // Book move
    }

    if (!nodeA.pvLines || !nodeB.pvLines) return '';

    // Convert scores to absolute perspective (white is positive)
    let scoreA = parseScoreToPawns(nodeA.pvLines['1']?.score);
    let scoreB = parseScoreToPawns(nodeB.pvLines['1']?.score);
    
    // If node B has no engine score (e.g. checkmate reached), evaluate it manually
    if (scoreB === undefined) {
        const gameB = new Chess(nodeB.fen);
        if (gameB.isCheckmate()) {
            scoreB = gameB.turn() === 'w' ? -100.00 : 100.00;
        } else if (gameB.isDraw() || gameB.isStalemate() || gameB.isInsufficientMaterial()) {
            scoreB = 0;
        } else {
            scoreB = 0; // Fallback
        }
    }
    if (scoreA === undefined) scoreA = 0;
    
    const turnA = nodeA.fen.split(' ')[1];
    
    // Player advantage in pawns (from the perspective of the player making the move)
    const playerScoreA = turnA === 'w' ? scoreA : -scoreA;
    const playerScoreB = turnA === 'w' ? scoreB : -scoreB;
    const pawnsDelta = playerScoreB - playerScoreA;

    // Win probability delta: Delta Win% = W(B) - W(A)
    const winProbA = winProbability(playerScoreA);
    const winProbB = winProbability(playerScoreB);
    const winProbDelta = winProbB - winProbA;

    // Get top moves of parent
    const topMoves = [];
    if (nodeA.pvLines) {
        for (let i = 1; i <= Object.keys(nodeA.pvLines).length; i++) {
            if (nodeA.pvLines[i]) topMoves.push(nodeA.pvLines[i]);
            else if (nodeA.pvLines[i.toString()]) topMoves.push(nodeA.pvLines[i.toString()]);
        }
    }
    
    // Extract first move in PV
    let topMoveSan = null;
    if (topMoves.length > 0 && topMoves[0].san) {
        topMoveSan = topMoves[0].san.trim().split(' ')[0];
    }
    const isStrictTopMove = topMoveSan === sanMove;

    // Check material sacrifice for Brilliant move
    const matA = countMaterial(nodeA.fen);
    const matB = countMaterial(nodeB.fen);
    const playerMatA = turnA === 'w' ? matA.white : matA.black;
    const playerMatB = turnA === 'w' ? matB.white : matB.black;
    const oppMatA = turnA === 'w' ? matA.black : matA.white;
    const oppMatB = turnA === 'w' ? matB.black : matB.white;
    const sacrificedMaterial = (playerMatA - playerMatB) > (oppMatA - oppMatB) + 1;

    // Brilliant (!!): piece sacrifice with sound continuation
    if (sacrificedMaterial && playerScoreB >= -0.2 && winProbDelta >= -0.04) {
        return '!!';
    }

    // Best Move (★): engine primary line or minimal win-probability loss
    if (isStrictTopMove || winProbDelta >= -0.02 || pawnsDelta >= -0.10) {
        return '★';
    }

    // Good Move (✓)
    if (winProbDelta >= -0.06 || pawnsDelta >= -0.35) {
        return '✓';
    }

    // Inaccuracy (?!)
    if (winProbDelta >= -0.12) {
        return '?!';
    }

    // Mistake (?)
    if (winProbDelta >= -0.22) {
        return '?';
    }

    // Cap classification severity if position remains decisively winning
    if (playerScoreB >= 3.5) {
        return '?!';
    }
    if (playerScoreB >= 1.8) {
        return '?';
    }

    // Blunder (??): game-altering loss of winning advantage or turning draw into loss
    return '??';
}

export function getAnnotationDescription(annotation) {
    switch (annotation) {
        case '📖': return 'Book Move';
        case '!!': return 'Brilliant Move';
        case '!': return 'Great Move';
        case '★': return 'Best Move';
        case '✓': return 'Good Move';
        case '?!': return 'Inaccuracy';
        case '?': return 'Mistake';
        case '??': return 'Blunder';
        default: return '';
    }
}

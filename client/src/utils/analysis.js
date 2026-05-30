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
    if (scoreStr.startsWith('M')) {
        const moves = parseInt(scoreStr.substring(1), 10);
        return 100.00 - moves; // Mate is very high score
    }
    if (scoreStr.startsWith('-M')) {
        const moves = parseInt(scoreStr.substring(2), 10);
        return -100.00 + moves; // Negative mate
    }
    return parseFloat(scoreStr);
}

function getWinProbability(pawns) {
    if (pawns === undefined) return undefined;
    const cp = pawns * 100;
    return 50 + 50 * (2 / (1 + Math.exp(-0.00368208 * Math.max(-4000, Math.min(4000, cp)))) - 1);
}

// Classify a move from Node A to Node B
export function classifyMove(nodeA, nodeB, sanMove) {
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
    
    const winProbA = getWinProbability(scoreA);
    const winProbB = getWinProbability(scoreB);
    
    // Delta in win probability from perspective of the player to move
    let probDelta = turnA === 'w' ? (winProbB - winProbA) : (winProbA - winProbB);

    // Get top moves of parent
    const topMoves = [];
    for (let i = 1; i <= Object.keys(nodeA.pvLines).length; i++) {
        if (nodeA.pvLines[i]) topMoves.push(nodeA.pvLines[i]);
    }
    
    const topMoveSan = topMoves.length > 0 ? topMoves[0].san.split(' ')[0] : null;
    const isTopMove = topMoveSan === sanMove;
    
    let probAdvantageOverOthers = 0;
    if (isTopMove && topMoves.length > 1) {
        const score1 = parseScoreToPawns(topMoves[0].score);
        const score2 = parseScoreToPawns(topMoves[1].score);
        const prob1 = getWinProbability(score1);
        const prob2 = getWinProbability(score2);
        probAdvantageOverOthers = turnA === 'w' ? (prob1 - prob2) : (prob2 - prob1);
    }

    // Classification Logic
    if (isTopMove) {
        if (probAdvantageOverOthers >= 15) return '!!'; // Brilliant (found a >15% win prob only move)
        if (probAdvantageOverOthers >= 5) return '!'; // Strong move
        return '★'; // Best move
    }

    // Drop in win percentage -> classification
    if (probDelta > -3) return '✓'; // Good
    if (probDelta > -10) return '?!'; // Inaccuracy
    if (probDelta > -20) return '?'; // Mistake
    return '??'; // Blunder
}

export function getAnnotationDescription(annotation) {
    switch (annotation) {
        case '!!': return 'Brilliant Move';
        case '!': return 'Strong Move';
        case '★': return 'Best Move';
        case '✓': return 'Good Move';
        case '?!': return 'Inaccuracy';
        case '?': return 'Mistake';
        case '??': return 'Blunder';
        default: return '';
    }
}

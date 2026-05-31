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
    
    // Delta in raw pawns from perspective of the player to move
    let pawnsDelta = turnA === 'w' ? (scoreB - scoreA) : (scoreA - scoreB);

    // Get top moves of parent
    const topMoves = [];
    for (let i = 1; i <= Object.keys(nodeA.pvLines).length; i++) {
        if (nodeA.pvLines[i]) topMoves.push(nodeA.pvLines[i]);
    }
    
    const topMoveSan = topMoves.length > 0 ? topMoves[0].san.split(' ')[0] : null;
    const isTopMove = topMoveSan === sanMove;
    
    let pawnsAdvantageOverOthers = 0;
    if (isTopMove && topMoves.length > 1) {
        const score1 = parseScoreToPawns(topMoves[0].score);
        const score2 = parseScoreToPawns(topMoves[1].score);
        pawnsAdvantageOverOthers = turnA === 'w' ? (score1 - score2) : (score2 - score1);
    }

    // Classification Logic
    if (isTopMove) {
        if (pawnsAdvantageOverOthers >= 2.0 && Math.abs(scoreA) < 5.0) return '!!'; // Brilliant (found a far superior move in competitive position)
        if (pawnsAdvantageOverOthers >= 1.0) return '!'; // Strong move
        return '★'; // Best move
    }

    // Drop in pawn evaluation -> classification
    if (pawnsDelta > -0.5) return '✓'; // Good
    if (pawnsDelta > -1.2) return '?!'; // Inaccuracy
    if (pawnsDelta > -2.5) return '?'; // Mistake
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

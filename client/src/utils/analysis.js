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

export function parseScoreToCp(scoreStr) {
    if (!scoreStr) return undefined;
    if (scoreStr === 'TBW') return 19900;
    if (scoreStr === 'TBL') return -19900;
    if (scoreStr.startsWith('M')) {
        const moves = parseInt(scoreStr.substring(1), 10);
        return 10000 - moves; // Mate is very high score
    }
    if (scoreStr.startsWith('-M')) {
        const moves = parseInt(scoreStr.substring(2), 10);
        return -10000 + moves;
    }
    return parseFloat(scoreStr);
}

// Classify a move from Node A to Node B
export function classifyMove(nodeA, nodeB, sanMove) {
    if (!nodeA.pvLines || !nodeB.pvLines) return '';

    // Convert scores to absolute perspective (white is positive)
    let scoreA = parseScoreToCp(nodeA.pvLines['1']?.score);
    let scoreB = parseScoreToCp(nodeB.pvLines['1']?.score);
    
    // If node B has no engine score (e.g. checkmate reached), evaluate it manually
    if (scoreB === undefined) {
        const gameB = new Chess(nodeB.fen);
        if (gameB.isCheckmate()) {
            scoreB = gameB.turn() === 'w' ? -10000 : 10000;
        } else if (gameB.isDraw() || gameB.isStalemate() || gameB.isInsufficientMaterial()) {
            scoreB = 0;
        } else {
            scoreB = 0; // Fallback
        }
    }
    if (scoreA === undefined) scoreA = 0;
    
    const turnA = nodeA.fen.split(' ')[1];
    
    // Engine scores in App.jsx are already converted to absolute (White is positive)
    let absA = scoreA;
    let absB = scoreB;
    
    // Delta from the perspective of the player who made the move
    // If White moved, Delta = absB - absA (positive means White improved or engine found better line)
    // If Black moved, Delta = absA - absB (positive means Black improved)
    let delta = turnA === 'w' ? (absB - absA) : (absA - absB);
    
    // Get top moves of parent
    const topMoves = [];
    for (let i = 1; i <= Object.keys(nodeA.pvLines).length; i++) {
        if (nodeA.pvLines[i]) topMoves.push(nodeA.pvLines[i]);
    }
    
    const topMoveSan = topMoves.length > 0 ? topMoves[0].san.split(' ')[0] : null;
    const isTopMove = topMoveSan === sanMove;
    
    let advantageOverOthers = 0;
    if (isTopMove && topMoves.length > 1) {
        const score1 = parseScoreToCp(topMoves[0].score);
        const score2 = parseScoreToCp(topMoves[1].score);
        // From the perspective of the player to move
        advantageOverOthers = turnA === 'w' ? (score1 - score2) : (score2 - score1);
    }

    // Check material sacrifice
    const matA = countMaterial(nodeA.fen);
    const matB = countMaterial(nodeB.fen);
    let isSacrifice = false;
    if (turnA === 'w' && matB.white < matA.white && delta >= -0.5) isSacrifice = true;
    if (turnA === 'b' && matB.black < matA.black && delta >= -0.5) isSacrifice = true;

    // Classification Logic
    if (isTopMove) {
        if (isSacrifice && advantageOverOthers >= 1.0) return '!!'; // Brilliant
        if (advantageOverOthers >= 1.0) return '!'; // Strong move
        return '★'; // Best move
    }

    if (delta > -0.5) return '✓'; // Good
    if (delta > -1.0) return '?!'; // Inaccuracy
    if (delta > -2.0) return '?'; // Mistake
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

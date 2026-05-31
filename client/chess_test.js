import { Chess } from 'chess.js';

const sanGame = new Chess();
try {
    const moveObj = { from: 'e2', to: 'e4' };
    const m = sanGame.move(moveObj);
    console.log("Legal move returns:", m);
} catch (e) {
    console.log("Exception:", e);
}

try {
    const m2 = sanGame.move({from: 'e2', to: 'e5'});
    console.log("Illegal move returns:", m2);
} catch (e) {
    console.log("Exception:", e.message);
}

import { useState, useEffect, useRef } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';
import EvalBar from './EvalBar';
import CoachCard from './CoachCard';

// Helper to generate IDs
const genId = () => Math.random().toString(36).substr(2, 9);

function App() {
  const [chessHelper] = useState(new Chess());
  
  // PGN Tree State
  const [nodes, setNodes] = useState({
    'root': { id: 'root', fen: chessHelper.fen(), san: 'Start', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
  });
  const [currentNodeId, setCurrentNodeId] = useState('root');
  
  // Modes
  const [isSetupMode, setIsSetupMode] = useState(false);
  const [setupFenInput, setSetupFenInput] = useState(chessHelper.fen());
  const [boardOrientation, setBoardOrientation] = useState('white');

  // UX States
  const [moveFrom, setMoveFrom] = useState('');
  const [optionSquares, setOptionSquares] = useState({});
  const [isShaking, setIsShaking] = useState(false);
  
  // Engine States
  const [pvLines, setPvLines] = useState({}); // map of multipv index -> line object
  const [multiPvCount, setMultiPvCount] = useState(3);
  const [analysisDepth, setAnalysisDepth] = useState(15);
  const [explainData, setExplainData] = useState(null);
  const [coachMessage, setCoachMessage] = useState('');
  const [isVisualEngineEnabled, setIsVisualEngineEnabled] = useState(true);
  const [isDragging, setIsDragging] = useState(false);
  const wsRef = useRef(null);

  // Derived state
  const currentNode = nodes[currentNodeId];
  const currentFen = isSetupMode ? setupFenInput : currentNode.fen;
  const game = new Chess();
  // Safe load
  try {
    game.load(currentFen);
  } catch (e) {
    // ignore invalid fen in setup
  }
  const turnColor = game.turn() === 'w' ? 'White' : 'Black';

  const currentFenRef = useRef(currentFen);
  useEffect(() => {
    currentFenRef.current = currentFen;
  }, [currentFen]);

  useEffect(() => {
    wsRef.current = new WebSocket('ws://localhost:8080');

    wsRef.current.onopen = () => {
      console.log('Connected to Engine Server');
      wsRef.current.send('uci');
      wsRef.current.send('isready');
      wsRef.current.send(`setoption name MultiPV value ${multiPvCount}`);
      // Start analysis for the initial position immediately
      wsRef.current.send(`position fen ${currentFenRef.current}`);
      wsRef.current.send(`go depth ${analysisDepth}`);
      if (isVisualEngineEnabled) {
          wsRef.current.send(`explain`);
      }
    };

    wsRef.current.onmessage = (event) => {
      const msg = event.data;
      
      if (msg.startsWith('info')) {
        const depthMatch = msg.match(/depth (\d+)/);
        const multipvMatch = msg.match(/multipv (\d+)/) || [null, '1'];
        const scoreCpMatch = msg.match(/score cp (-?\d+)/);
        const scoreMateMatch = msg.match(/score mate (-?\d+)/);
        const pvMatch = msg.match(/\bpv (.*)/);

        if (depthMatch && pvMatch) {
          const multipv = multipvMatch[1];
          let scoreStr = '';
          
          const tempGame = new Chess();
          try { tempGame.load(currentFenRef.current); } catch(e) {}
          const isBlackTurn = tempGame.turn() === 'b';

          if (scoreCpMatch) {
            let cp = parseInt(scoreCpMatch[1]);
            if (isBlackTurn) cp = -cp;
            scoreStr = (cp / 100).toFixed(2);
          }
          if (scoreMateMatch) {
            let mate = parseInt(scoreMateMatch[1]);
            if (isBlackTurn) mate = -mate;
            scoreStr = `M${mate}`;
          }

          let sanPv = pvMatch[1];
          try {
              const sanGame = new Chess();
              try { sanGame.load(currentFenRef.current); } catch(e) {}
              const rawPv = pvMatch[1].trim().split(' ');
              const sanMoves = rawPv.map(move => {
                  if (move.length < 4) return move;
                  const moveObj = {
                      from: move.substring(0, 2),
                      to: move.substring(2, 4),
                      promotion: move.length === 5 ? move.substring(4, 5) : undefined
                  };
                  try {
                      const m = sanGame.move(moveObj);
                      return m ? m.san : move;
                  } catch(e) {
                      return move;
                  }
              });
              sanPv = sanMoves.join(' ');
          } catch(e) {
              console.error('[Engine] Failed to translate PV to SAN', e);
          }

          setPvLines(prev => ({
            ...prev,
            [multipv]: {
              depth: depthMatch[1],
              score: scoreStr,
              pv: pvMatch[1],
              san: sanPv
            }
          }));
        }
      } else if (msg.startsWith('info string Explain:')) {
        try {
            console.log(`[Engine Explain] Received raw factors:`, msg);
            const jsonStr = msg.replace('info string Explain:', '').trim();
            const data = JSON.parse(jsonStr);
            setExplainData(data);
        } catch (e) {
            console.error('[Engine Explain] Failed to parse Explain JSON', e);
        }
      } else if (msg.startsWith('info string Coach:')) {
        const coachMsg = msg.replace('info string Coach:', '').trim();
        console.log(`[AI Coach] Comment received: ${coachMsg}`);
        setCoachMessage(coachMsg);
      }
    };

    return () => {
      if (wsRef.current) wsRef.current.close();
    };
  }, [multiPvCount]);

  function getMoveOptions(square) {
    const moves = game.moves({ square, verbose: true });
    if (moves.length === 0) return;

    const newSquares = {};
    moves.map((move) => {
      newSquares[move.to] = {
        background:
          game.get(move.to) && game.get(move.to).color !== game.get(square).color
            ? 'radial-gradient(circle, rgba(0,0,0,.1) 85%, transparent 85%)'
            : 'radial-gradient(circle, rgba(0,0,0,.1) 25%, transparent 25%)',
        borderRadius: '50%'
      };
    });
    newSquares[square] = { background: 'rgba(255, 255, 0, 0.4)' };
    setOptionSquares(newSquares);
  }

  function handleMove(moveObj) {
    let move = null;
    try {
      move = game.move(moveObj);
    } catch (e) {
      return false;
    }

    if (move === null) return false;

    // Check if this move already exists as a child
    const existingChildId = currentNode.children.find(childId => nodes[childId].san === move.san);
    
    if (existingChildId) {
      setCurrentNodeId(existingChildId);
    } else {
      // Create new branch
      const newId = genId();
      
      // Remove dummy annotation logic to avoid false blunders
      let annotation = '';

      const newNode = {
        id: newId,
        fen: game.fen(),
        san: move.san,
        parentId: currentNodeId,
        children: [],
        annotation,
        ply: currentNode.ply + 1
      };

      setNodes(prev => ({
        ...prev,
        [currentNodeId]: { ...prev[currentNodeId], children: [...prev[currentNodeId].children, newId] },
        [newId]: newNode
      }));
      setCurrentNodeId(newId);
    }

    // Reset Engine PVs and request analysis
    setPvLines({});
    setExplainData(null);
    setCoachMessage('');
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(`position fen ${game.fen()}`);
      wsRef.current.send(`go depth ${analysisDepth}`);
      if (isVisualEngineEnabled) {
          wsRef.current.send(`explain`);
      }
    }

    setMoveFrom('');
    setOptionSquares({});
    return true;
  }

  function onSquareClick(square) {
    if (isSetupMode) return;

    if (!moveFrom) {
      const hasPiece = game.get(square);
      if (hasPiece && hasPiece.color === game.turn()) {
        setMoveFrom(square);
        getMoveOptions(square);
      }
      return;
    }

    if (moveFrom === square) {
        setMoveFrom('');
        setOptionSquares({});
        return;
    }
    
    const success = handleMove({ from: moveFrom, to: square, promotion: 'q' });

    if (!success) {
      const hasPiece = game.get(square);
      if (hasPiece && hasPiece.color === game.turn()) {
          setMoveFrom(square);
          getMoveOptions(square);
      } else {
          triggerError();
      }
    }
  }

  function onDrop(sourceSquare, targetSquare, piece) {
    if (isSetupMode) {
      const gameCopy = new Chess(setupFenInput);
      try {
        if (sourceSquare === 'spare') {
          const type = piece[1].toLowerCase();
          const color = piece[0];
          gameCopy.put({ type, color }, targetSquare);
        } else if (targetSquare === 'spare' || targetSquare === 'trash') {
          gameCopy.remove(sourceSquare);
        } else {
          // If we are dragging a piece to another square, chess.js remove/put overwrites the target implicitly.
          // However, if the user eats a piece, react-chessboard usually gives us 'trash' or off-board when thrown away.
          const p = gameCopy.remove(sourceSquare);
          if (p) gameCopy.put(p, targetSquare);
        }
        setSetupFenInput(gameCopy.fen());
        return true;
      } catch(e) {
        return false;
      }
    }

    const success = handleMove({
      from: sourceSquare,
      to: targetSquare,
      promotion: piece && piece[1] ? piece[1].toLowerCase() : 'q',
    });

    if (!success) triggerError();
    return success;
  }

  function onPieceDropSetup(source, target, piece) {
     return onDrop(source, target, piece);
  }

  function triggerError() {
    setIsShaking(true);
    setMoveFrom('');
    setOptionSquares({});
    setTimeout(() => setIsShaking(false), 400);
  }

  function jumpTo(id) {
    setCurrentNodeId(id);
    setMoveFrom('');
    setOptionSquares({});
    setPvLines({});
    setExplainData(null);
    setCoachMessage('');
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(`position fen ${nodes[id].fen}`);
      wsRef.current.send(`go depth ${analysisDepth}`);
      if (isVisualEngineEnabled) {
          wsRef.current.send(`explain`);
      }
    }
  }

  function resetGame() {
    const startFen = new Chess().fen();
    setNodes({
      'root': { id: 'root', fen: startFen, san: 'Start', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    setPvLines({});
    setMoveFrom('');
    setOptionSquares({});
  }

  function applySetup() {
    setNodes({
      'root': { id: 'root', fen: setupFenInput, san: 'Setup Position', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    setIsSetupMode(false);
  }

  const renderMoves = (nodeId) => {
    const node = nodes[nodeId];
    if (!node || node.children.length === 0) return null;

    const mainChildId = node.children[0];
    const mainChild = nodes[mainChildId];
    const variations = node.children.slice(1);

    const moveNum = Math.floor((mainChild.ply - 1) / 2) + 1;
    const isWhite = mainChild.ply % 2 !== 0;
    const numStr = isWhite ? `${moveNum}. ` : (node.id === 'root' ? `${moveNum}... ` : ' ');
    
    // Add visual styling for annotations
    let annotationClass = '';
    if (mainChild.annotation === '!!') annotationClass = 'anno-brilliant';
    else if (mainChild.annotation === '!') annotationClass = 'anno-good';
    else if (mainChild.annotation === '??') annotationClass = 'anno-blunder';
    else if (mainChild.annotation === '?!') annotationClass = 'anno-inaccuracy';

    return (
      <span key={mainChildId}>
        <span 
          className={`move-item ${currentNodeId === mainChildId ? 'active' : ''} ${annotationClass}`}
          onClick={() => jumpTo(mainChildId)}
        >
          {numStr}{mainChild.san}{mainChild.annotation}
        </span>
        
        {variations.length > 0 && (
          <span className="variations-container">
            {variations.map(varId => {
               const vChild = nodes[varId];
               const vMoveNum = Math.floor((vChild.ply - 1) / 2) + 1;
               const vIsWhite = vChild.ply % 2 !== 0;
               const vNumStr = vIsWhite ? `${vMoveNum}. ` : `${vMoveNum}... `;
               
               let vAnnotationClass = '';
               if (vChild.annotation === '!!') vAnnotationClass = 'anno-brilliant';
               else if (vChild.annotation === '??') vAnnotationClass = 'anno-blunder';

               return (
                <span key={varId} className="variation-block">
                  (
                  <span 
                    className={`move-item variation ${currentNodeId === varId ? 'active' : ''} ${vAnnotationClass}`}
                    onClick={() => jumpTo(varId)}
                  >
                    {vNumStr}{vChild.san}{vChild.annotation}
                  </span>
                  {renderMoves(varId)}
                  )
                </span>
               )
            })}
          </span>
        )}
        
        {renderMoves(mainChildId)}
      </span>
    );
  };

  function updateSetupFen(index, value) {
    const parts = setupFenInput.split(' ');
    while (parts.length < 6) parts.push('-');
    if (parts.length === 6 && parts[4] === '-') parts[4] = '0';
    if (parts.length === 6 && parts[5] === '-') parts[5] = '1';
    
    parts[index] = value || '-';
    setSetupFenInput(parts.join(' '));
  }

  function toggleCastling(char) {
    const parts = setupFenInput.split(' ');
    let castling = parts[2] || '-';
    let newCastling = castling.replace('-', '');
    if (newCastling.includes(char)) {
        newCastling = newCastling.replace(char, '');
    } else {
        newCastling += char;
    }
    const order = 'KQkq';
    newCastling = newCastling.split('').sort((a,b) => order.indexOf(a) - order.indexOf(b)).join('');
    updateSetupFen(2, newCastling || '-');
  }

  // Find king square if in check
  let checkSquare = null;
  if (!isSetupMode && game.inCheck()) {
    const board = game.board();
    for (let r = 0; r < 8; r++) {
      for (let c = 0; c < 8; c++) {
        if (board[r][c] && board[r][c].type === 'k' && board[r][c].color === game.turn()) {
          checkSquare = String.fromCharCode(97 + c) + (8 - r);
        }
      }
    }
  }

  const customStyles = { ...optionSquares };
  if (checkSquare) {
    customStyles[checkSquare] = {
      background: 'radial-gradient(circle, rgba(255,0,0,0.8), rgba(255,0,0,0) 60%)'
    };
  }

  // Annotation overlay calculation
  let annotationOverlay = null;
  if (!isSetupMode && currentNode.annotation) {
    // Determine the target square from SAN or move history
    // Since chess.js move history isn't directly exposed for the node,
    // we can parse the move from the FEN or we could have stored 'to' square.
    // Let's store 'to' square in handleMove next time, but for now we can compute it
    // by comparing currentFen and parent fen, or just parsing SAN roughly.
    // A quick hack: extract the last 2 chars if it's a standard move
    const san = currentNode.san;
    const match = san.match(/[a-h][1-8]/);
    if (match) {
      const sq = match[0];
      const files = ['a','b','c','d','e','f','g','h'];
      const fIndex = files.indexOf(sq[0]);
      const rIndex = parseInt(sq[1], 10) - 1;
      
      const x = boardOrientation === 'white' ? fIndex : 7 - fIndex;
      const y = boardOrientation === 'white' ? 7 - rIndex : rIndex;
      
      let badgeColor = '#888';
      let badgeText = currentNode.annotation;
      if (badgeText === '!!') badgeColor = '#1baca6';
      else if (badgeText === '??') badgeColor = '#e53935';
      else if (badgeText === '!') badgeColor = '#5c8bb0';
      else if (badgeText === '?!') badgeColor = '#e58e26';

      annotationOverlay = (
        <div style={{
          position: 'absolute',
          left: `${(x * 100) / 8}%`,
          top: `${(y * 100) / 8}%`,
          width: '12.5%',
          height: '12.5%',
          pointerEvents: 'none',
          display: 'flex',
          justifyContent: 'flex-end',
          alignItems: 'flex-start'
        }}>
          <div style={{
            background: badgeColor,
            color: '#fff',
            fontWeight: 'bold',
            fontSize: '1rem',
            padding: '2px 6px',
            borderRadius: '50%',
            transform: 'translate(30%, -30%)',
            boxShadow: '0 2px 5px rgba(0,0,0,0.5)'
          }}>
            {badgeText}
          </div>
        </div>
      );
    }
  }

  // Text description for annotation
  const getAnnotationDescription = (anno) => {
      if (anno === '!!') return 'Brilliant Move!';
      if (anno === '??') return 'Blunder!';
      if (anno === '!') return 'Good Move';
      if (anno === '?!') return 'Inaccuracy';
      return '';
  };

  // Calculate current depth
  const currentDepth = Object.values(pvLines).length > 0 
      ? Math.max(...Object.values(pvLines).map(l => parseInt(l.depth, 10))) 
      : 0;

  // Visual Engine Arrows
  let customArrows = [];
  if (isVisualEngineEnabled && Object.keys(pvLines).length > 0) {
      const colors = ['rgba(0, 255, 0, 0.6)', 'rgba(0, 100, 255, 0.6)', 'rgba(255, 165, 0, 0.6)'];
      // sort keys to ensure 1 is first
      const keys = Object.keys(pvLines).sort();
      keys.forEach((key, index) => {
          if (index < 3) {
             const pv = pvLines[key].pv;
             if (pv) {
                 const firstMove = pv.split(' ')[0]; // e.g. e2e4
                 if (firstMove && firstMove.length >= 4) {
                     const from = firstMove.substring(0, 2);
                     const to = firstMove.substring(2, 4);
                     customArrows.push([from, to, colors[index]]);
                 }
             }
          }
      });
  }
  const parseScoreForSort = (scoreStr) => {
    if (!scoreStr) return 0;
    if (scoreStr.startsWith('M')) {
      const moves = parseInt(scoreStr.substring(1), 10);
      return 10000 - moves;
    }
    if (scoreStr.startsWith('-M')) {
      const moves = parseInt(scoreStr.substring(2), 10);
      return -10000 + moves;
    }
    return parseFloat(scoreStr) || 0;
  };

  return (
    <div className="app-container">
      <div className={`board-container ${isShaking ? 'shake' : ''}`}>
        {!isSetupMode && (
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1rem' }}>
            <div className={`turn-indicator ${game.turn() === 'w' ? 'white' : 'black'}`} style={{ margin: 0 }}>
              {turnColor} to Move
            </div>
            <button 
              onClick={() => setBoardOrientation(prev => prev === 'white' ? 'black' : 'white')}
              style={{ background: 'rgba(255,255,255,0.1)', border: 'none', padding: '0.5rem 1rem', borderRadius: '8px', cursor: 'pointer', color: '#fff' }}
            >
              🔄 Flip Board
            </button>
          </div>
        )}
        
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'stretch' }}>
          <EvalBar 
            score={pvLines['1'] ? pvLines['1'].score : "0.00"} 
            boardOrientation={boardOrientation} 
          />
          <div style={{ position: 'relative', flexGrow: 1 }}>
            <Chessboard 
              position={currentFen} 
              boardOrientation={boardOrientation}
              onPieceDrop={isSetupMode ? onPieceDropSetup : onDrop}
              onPieceDragBegin={() => setIsDragging(true)}
              onPieceDragEnd={() => setIsDragging(false)}
              onSquareClick={onSquareClick}
              customSquareStyles={customStyles}
              customDarkSquareStyle={{ backgroundColor: '#476375' }}
              customLightSquareStyle={{ backgroundColor: '#a6bfce' }}
              animationDuration={200}
              sparePieces={isSetupMode}
              dropOffBoard={isSetupMode ? 'trash' : 'snapback'}
              customArrows={customArrows}
            />
            {annotationOverlay}
          </div>
        </div>

        {!isSetupMode && currentNode.annotation && (
          <div style={{
             marginTop: '1rem', padding: '1rem', borderRadius: '8px',
             background: currentNode.annotation === '!!' ? 'rgba(27, 172, 166, 0.2)' : 
                         currentNode.annotation === '??' ? 'rgba(229, 57, 53, 0.2)' : 'rgba(255,255,255,0.1)',
             textAlign: 'center', fontWeight: 'bold', fontSize: '1.1rem'
          }}>
             {currentNode.san} {currentNode.annotation} — {getAnnotationDescription(currentNode.annotation)}
          </div>
        )}

        {isSetupMode && (
          <div className="setup-controls">
            <input 
              className="modern-input"
              value={setupFenInput} 
              onChange={e => setSetupFenInput(e.target.value)} 
              placeholder="Paste FEN here..."
            />
            
            <div className="setup-row">
              <span className="setup-label">Turn:</span>
              <div className="btn-group">
                <button 
                  className={(setupFenInput.split(' ')[1] || 'w') === 'w' ? 'active' : ''} 
                  onClick={() => updateSetupFen(1, 'w')}
                >White</button>
                <button 
                  className={(setupFenInput.split(' ')[1] || 'w') === 'b' ? 'active' : ''} 
                  onClick={() => updateSetupFen(1, 'b')}
                >Black</button>
              </div>
            </div>

            <div className="setup-row">
              <span className="setup-label">Castling:</span>
              <div className="checkbox-group" style={{ display: 'flex', flexDirection: 'column', gap: '0.3rem', alignItems: 'flex-start' }}>
                <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer', color: '#fff' }}>
                  <input type="checkbox" checked={(setupFenInput.split(' ')[2] || '').includes('K')} onChange={() => toggleCastling('K')} />
                  White O-O
                </label>
                <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer', color: '#fff' }}>
                  <input type="checkbox" checked={(setupFenInput.split(' ')[2] || '').includes('Q')} onChange={() => toggleCastling('Q')} />
                  White O-O-O
                </label>
                <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer', color: '#fff' }}>
                  <input type="checkbox" checked={(setupFenInput.split(' ')[2] || '').includes('k')} onChange={() => toggleCastling('k')} />
                  Black O-O
                </label>
                <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer', color: '#fff' }}>
                  <input type="checkbox" checked={(setupFenInput.split(' ')[2] || '').includes('q')} onChange={() => toggleCastling('q')} />
                  Black O-O-O
                </label>
              </div>
            </div>

            <div className="setup-row">
              <span className="setup-label">En Passant:</span>
              <input 
                style={{ background: 'rgba(255,255,255,0.1)', color: '#fff', border: '1px solid rgba(255,255,255,0.2)', padding: '5px', borderRadius: '4px', width: '50px', textAlign: 'center' }}
                value={setupFenInput.split(' ')[3] || '-'} 
                onChange={(e) => updateSetupFen(3, e.target.value)} 
                maxLength={2}
              />
            </div>

            <button onClick={applySetup} style={{ width: '100%', background: '#4CAF50' }}>Apply Position</button>
          </div>
        )}
      </div>

      <div className="panel-container">
        <div className="glass-panel">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <h2>Analysis Board</h2>
            <button onClick={() => setIsSetupMode(!isSetupMode)} style={{ fontSize: '0.8rem', padding: '0.3rem 0.6rem' }}>
              {isSetupMode ? 'Cancel Setup' : 'Setup Mode'}
            </button>
          </div>
          <p>Status: {wsRef.current && wsRef.current.readyState === WebSocket.OPEN ? 'Connected' : 'Connecting...'}</p>
          
          <div className="history-controls">
            <button onClick={() => jumpTo('root')}>&lt;&lt;</button>
            <button onClick={() => jumpTo(currentNode.parentId || 'root')}>&lt;</button>
            <button onClick={() => jumpTo(currentNode.children[0] || currentNodeId)}>&gt;</button>
            <button onClick={() => {
               // Jump to end of main line
               let c = currentNodeId;
               while (nodes[c].children.length > 0) c = nodes[c].children[0];
               jumpTo(c);
            }}>&gt;&gt;</button>
          </div>

          <div className="move-list">
            {nodes['root'].children.length === 0 ? <span style={{color: '#888'}}>No moves yet</span> : renderMoves('root')}
          </div>

          <button onClick={resetGame} style={{ marginTop: '1rem', width: '100%' }}>New Game</button>
        </div>

        <div className="glass-panel">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1rem', flexWrap: 'wrap', gap: '1rem' }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <h2 style={{ margin: 0 }}>Engine Lines (Multi-PV)</h2>
                <label style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '0.9rem', color: '#1baca6', cursor: 'pointer' }}>
                    <input 
                        type="checkbox" 
                        checked={isVisualEngineEnabled} 
                        onChange={(e) => {
                            setIsVisualEngineEnabled(e.target.checked);
                            if (e.target.checked && wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
                                wsRef.current.send(`explain`);
                            }
                        }} 
                    />
                    Visual Engine & AI Coach
                </label>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
              <span style={{ fontSize: '1.2rem', fontWeight: 'bold', color: '#1baca6', backgroundColor: 'rgba(27, 172, 166, 0.1)', padding: '4px 10px', borderRadius: '6px' }}>
                Depth: {currentDepth}/{analysisDepth}
              </span>
              <input 
                type="range" 
                min="5" 
                max="50" 
                value={analysisDepth} 
                onChange={(e) => {
                  const d = parseInt(e.target.value, 10);
                  setAnalysisDepth(d);
                  if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
                    wsRef.current.send(`go depth ${d}`);
                  }
                }}
                style={{ width: '100px' }}
              />
              <div className="btn-group">
                <button className={multiPvCount === 1 ? 'active' : ''} onClick={() => { setMultiPvCount(1); if (wsRef.current) wsRef.current.send('setoption name MultiPV value 1'); }}>1</button>
                <button className={multiPvCount === 3 ? 'active' : ''} onClick={() => { setMultiPvCount(3); if (wsRef.current) wsRef.current.send('setoption name MultiPV value 3'); }}>3</button>
                <button className={multiPvCount === 5 ? 'active' : ''} onClick={() => { setMultiPvCount(5); if (wsRef.current) wsRef.current.send('setoption name MultiPV value 5'); }}>5</button>
              </div>
            </div>
          </div>
          <div className="engine-output">
            {Object.keys(pvLines).length === 0 ? <span style={{color: '#888'}}>Waiting for engine...</span> : null}
            {Object.values(pvLines)
              .sort((a,b) => game.turn() === 'w' 
                  ? parseScoreForSort(b.score) - parseScoreForSort(a.score) 
                  : parseScoreForSort(a.score) - parseScoreForSort(b.score))
              .map((line, i) => (
              <div key={i} style={{ borderBottom: '1px solid rgba(255,255,255,0.1)', paddingBottom: '4px', marginBottom: '4px' }}>
                <span style={{ color: '#ffb74d', marginRight: '8px', display: 'inline-block', width: '40px' }}>
                  {line.score}
                </span>
                <span style={{ color: '#888', fontSize: '0.8em', marginRight: '8px' }}>
                  d{line.depth}
                </span>
                <span style={{ flex: 1, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                  {line.san || line.pv}
                </span>
              </div>
            ))}
          </div>
          
          {isVisualEngineEnabled && <CoachCard coachMessage={coachMessage} explainData={explainData} />}
        </div>
      </div>
    </div>
  );
}

export default App;

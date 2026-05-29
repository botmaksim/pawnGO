import { useState, useEffect, useRef } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';
import EvalBar from './EvalBar';
import { classifyMove, getAnnotationDescription } from './utils/analysis';

// Helper to generate IDs
const genId = () => Math.random().toString(36).substr(2, 9);

function App() {
  const [chessHelper] = useState(new Chess());
  
  // PGN Tree State
  const [nodes, setNodes] = useState({
    'root': { id: 'root', fen: chessHelper.fen(), san: 'Start', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
  });
  const nodesRef = useRef(nodes);
  useEffect(() => { nodesRef.current = nodes; }, [nodes]);
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
  
  // Full Game Analysis
  const [isAnalyzingGame, setIsAnalyzingGame] = useState(false);
  const [analyzeProgress, setAnalyzeProgress] = useState({ current: 0, total: 0 });
  const analyzeQueueRef = useRef([]);
  const currentAnalyzeNodeIdRef = useRef(null);
  const isAnalyzingGameRef = useRef(false);
  const latestPvLinesRef = useRef({});
  const updatePendingRef = useRef(false);
  const [isDragging, setIsDragging] = useState(false);
  const wsRef = useRef(null);

  // New UI features
  const [showArrows, setShowArrows] = useState(true);
  const [playMode, setPlayMode] = useState('Analysis');
  const playModeRef = useRef(playMode);
  useEffect(() => { 
    playModeRef.current = playMode; 
    
    // If we just switched to playing as the side whose turn it is, force the engine to think!
    if (playMode !== 'Analysis' && !isSetupMode) {
      const currentTurn = game.turn();
      if ((playMode === 'Play as White' && currentTurn === 'w') ||
          (playMode === 'Play as Black' && currentTurn === 'b')) {
          if (wsRef.current && wsRef.current.readyState === 1) {
              wsRef.current.send('stop');
              setTimeout(() => {
                  wsRef.current.send(`position fen ${game.fen()}`);
                  wsRef.current.send(`go depth ${analysisDepth}`);
              }, 50);
          }
      }
    }
  }, [playMode, game, isSetupMode, analysisDepth]);
  const [pendingEngineMove, setPendingEngineMove] = useState(null);

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
    if (pendingEngineMove) {
      const from = pendingEngineMove.substring(0, 2);
      const to = pendingEngineMove.substring(2, 4);
      const promotion = pendingEngineMove.length > 4 ? pendingEngineMove[4] : undefined;
      handleMove({ from, to, promotion });
      setPendingEngineMove(null);
    }
  }, [pendingEngineMove]);

  useEffect(() => {
    // Web Worker for WebAssembly Engine
    const worker = new Worker('engineWorker.js');
    
    // Simulate WebSocket API for existing code
    wsRef.current = {
      readyState: 1, // OPEN
      isTablebasePending: null,
      send: (cmd) => {
          if (cmd.startsWith('position fen ')) {
              const fen = cmd.substring('position fen '.length);
              const pieceCount = fen.split(' ')[0].replace(/[^a-zA-Z]/g, '').length;
              if (pieceCount <= 5) {
                  wsRef.current.isTablebasePending = fen;
                  return; // Hold the command
              }
              wsRef.current.isTablebasePending = null;
          }
          if (cmd.startsWith('go ') && wsRef.current.isTablebasePending) {
              const fen = wsRef.current.isTablebasePending;
              fetch(`/api/tablebase?fen=${encodeURIComponent(fen)}`)
                  .then(res => res.json())
                  .then(data => {
                      if (data.bestmove) {
                          // Inject tablebase move
                          if (wsRef.current.onmessage) {
                              wsRef.current.onmessage({ data: `info depth 64 score cp 19999 pv ${data.bestmove}` });
                              wsRef.current.onmessage({ data: `bestmove ${data.bestmove}` });
                          }
                      } else {
                          // Fallback to Wasm
                          wsRef.current.isTablebasePending = null;
                          worker.postMessage(`position fen ${fen}`);
                          worker.postMessage(cmd);
                      }
                  })
                  .catch(err => {
                      console.error('Tablebase error', err);
                      // Fallback to Wasm
                      wsRef.current.isTablebasePending = null;
                      worker.postMessage(`position fen ${fen}`);
                      worker.postMessage(cmd);
                  });
              return;
          }
          if (cmd === 'stop' && wsRef.current.isTablebasePending) {
              return; // Nothing to stop on the server
          }
          worker.postMessage(cmd);
      },
      close: () => worker.terminate()
    };

    worker.onmessage = (event) => {
      const msg = event.data;
      if (msg === 'isready') {
          console.log('Engine is ready (Wasm)');
          // Send initial setup
          wsRef.current.send('uci');
          wsRef.current.send(`setoption name MultiPV value ${multiPvCount}`);
          requestAnalysis(currentFenRef.current);
          return;
      }
      
      // Pass the message to the existing handler
      if (wsRef.current.onmessage) {
          wsRef.current.onmessage({ data: msg });
      }
    };

    // We don't have an exact "onopen" for Workers, it's ready immediately
    // but the engine takes time to initialize (download NNUE, etc.).
    // So we wait for 'isready' from the worker above.

    wsRef.current.onmessage = (event) => {
      const msg = event.data;
      
      if (msg.startsWith('info')) {
        const depthMatch = msg.match(/depth (\d+)/);
        const multipvMatch = msg.match(/multipv (\d+)/) || [null, '1'];
        const scoreCpMatch = msg.match(/score cp (-?\d+)/);
        const scoreMateMatch = msg.match(/score mate (-?\d+)/);
        const bookMatch = msg.match(/book 1/);
        const pvMatch = msg.match(/\bpv (.*)/);

        if (depthMatch && pvMatch) {
          const multipv = multipvMatch[1];
          let scoreStr = '';
          
          const tempGame = new Chess();
          try { tempGame.load(isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current ? nodesRef.current[currentAnalyzeNodeIdRef.current].fen : currentFenRef.current); } catch(e) {}
          const isBlackTurn = tempGame.turn() === 'b';

          if (bookMatch) {
            scoreStr = 'Book';
          } else if (scoreCpMatch) {
            let cp = parseInt(scoreCpMatch[1]);
            if (isBlackTurn) cp = -cp;
            
            if (cp > 19000) {
              scoreStr = 'TBW';
            } else if (cp < -19000) {
              scoreStr = 'TBL';
            } else {
              scoreStr = (cp / 100).toFixed(2);
            }
          }
          if (scoreMateMatch) {
            let mate = parseInt(scoreMateMatch[1]);
            if (isBlackTurn) mate = -mate;
            if (mate < 0) {
                scoreStr = `-M${-mate}`;
            } else {
                scoreStr = `M${mate}`;
            }
          }

          let sanPv = pvMatch[1];
          try {
              const sanGame = new Chess();
              try { sanGame.load(isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current ? nodesRef.current[currentAnalyzeNodeIdRef.current].fen : currentFenRef.current); } catch(e) {}
              const rawPv = pvMatch[1].trim().split(' ');
              const newSanMoves = rawPv.map(move => {
                  if (move.length < 4) return move;
                  const moveObj = {
                      from: move.substring(0, 2),
                      to: move.substring(2, 4),
                      promotion: move.length === 5 ? move.substring(4, 5) : undefined
                  };
                  const m = sanGame.move(moveObj);
                  if (!m) throw new Error("Illegal move for current fen");
                  return m.san;
              });
              sanPv = newSanMoves.join(' ');
          } catch(e) {
              // Stale info depth for previous fen, ignore it
              return;
          }

          latestPvLinesRef.current = {
            ...latestPvLinesRef.current,
            [multipv]: {
              depth: depthMatch[1],
              score: scoreStr,
              pv: pvMatch[1],
              san: sanPv
            }
          };
          if (!isAnalyzingGameRef.current) {
             if (!updatePendingRef.current) {
                 updatePendingRef.current = true;
                 requestAnimationFrame(() => {
                     // Only update pv lines if in analysis mode to prevent hints during play
                     if (playModeRef.current === 'Analysis') {
                         setPvLines(latestPvLinesRef.current);
                     }
                     updatePendingRef.current = false;
                 });
             }
          }
        }
      } else if (msg.startsWith('bestmove')) {
        if (isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current) {
          setNodes(prev => ({
            ...prev,
            [currentAnalyzeNodeIdRef.current]: {
              ...prev[currentAnalyzeNodeIdRef.current],
              pvLines: { ...latestPvLinesRef.current }
            }
          }));
          processNextAnalysisNode();
        } else if (!isAnalyzingGameRef.current && playModeRef.current !== 'Analysis') {
          // Play against engine mode
          try {
              const tempGame = new Chess();
              tempGame.load(currentFenRef.current);
              const turn = tempGame.turn(); // 'w' or 'b'
              if ((playModeRef.current === 'Play as White' && turn === 'b') ||
                  (playModeRef.current === 'Play as Black' && turn === 'w')) {
                  const bestmove = msg.split(' ')[1];
                  if (bestmove && bestmove !== '(none)') {
                      setPendingEngineMove(bestmove);
                  }
              }
          } catch (e) {
              console.error('[Engine] Failed to process play mode bestmove', e);
          }
        }
      }
    };

    return () => {
      if (wsRef.current) wsRef.current.close();
    };
  }, [multiPvCount]);

  const processNextAnalysisNode = () => {
    if (analyzeQueueRef.current.length === 0) {
      // Done analyzing! Now classify moves
      setIsAnalyzingGame(false);
      isAnalyzingGameRef.current = false;
      setNodes(prevNodes => {
        const nextNodes = { ...prevNodes };
        // Traverse all nodes that have parents
        for (let key in nextNodes) {
          if (nextNodes[key].parentId && nextNodes[nextNodes[key].parentId]) {
            let parent = nextNodes[nextNodes[key].parentId];
            let child = nextNodes[key];
            let annotation = classifyMove(parent, child, child.san);
            if (annotation) {
              nextNodes[key] = { ...child, annotation };
            }
          }
        }
        return nextNodes;
      });
      // Resume normal analysis for current node
      requestAnalysis(nodesRef.current[currentNodeId].fen);
      return;
    }
    const nextId = analyzeQueueRef.current.shift();
    currentAnalyzeNodeIdRef.current = nextId;
    setAnalyzeProgress(p => ({ ...p, current: p.total - analyzeQueueRef.current.length }));
    
    // Clear old pvLines so they don't leak
    latestPvLinesRef.current = {};
    if (wsRef.current && wsRef.current.readyState === 1) {
      wsRef.current.send(`position fen ${nodesRef.current[nextId].fen}`);
      wsRef.current.send(`go depth ${analysisDepth}`); // analyze current node
    }
  };

  const startFullGameAnalysis = () => {
    // Collect all nodes in the tree
    const queue = Object.keys(nodesRef.current);
    analyzeQueueRef.current = queue;
    setAnalyzeProgress({ current: 0, total: queue.length });
    setIsAnalyzingGame(true);
    isAnalyzingGameRef.current = true;
    
    if (wsRef.current && wsRef.current.readyState === 1) {
        wsRef.current.send('stop'); // Stop any ongoing background analysis
        setTimeout(() => {
            processNextAnalysisNode();
        }, 100);
    }
  };

  const cancelFullGameAnalysis = () => {
    analyzeQueueRef.current = [];
    setIsAnalyzingGame(false);
    isAnalyzingGameRef.current = false;
    
    if (wsRef.current && wsRef.current.readyState === 1) {
        wsRef.current.send('stop');
        setTimeout(() => {
            requestAnalysis(nodesRef.current[currentNodeId].fen); // Resume normal analysis
        }, 100);
    }
  };

  const requestAnalysis = (fen) => {
    if (isAnalyzingGameRef.current) return; // don't interrupt full game analysis
    if (wsRef.current && wsRef.current.readyState === 1) {
      wsRef.current.send('stop');
      // Set a small delay to ignore old messages and start fresh
      setTimeout(() => {
        latestPvLinesRef.current = {};
        setPvLines({});
        wsRef.current.send(`position fen ${fen}`);
        wsRef.current.send(`go depth ${analysisDepth}`);
      }, 50);
    }
  };

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
    requestAnalysis(game.fen());

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
  
  // Disable drag if it's engine's turn
  const isEngineTurn = !isSetupMode && (
    (playMode === 'Play as White' && game.turn() === 'w') ||
    (playMode === 'Play as Black' && game.turn() === 'b')
  );

  function triggerError() {
    setIsShaking(true);
    setMoveFrom('');
    setOptionSquares({});
    setTimeout(() => setIsShaking(false), 400);
  }

  function jumpTo(id) {
    setCurrentNodeId(id);
    requestAnalysis(nodes[id].fen);
    setMoveFrom('');
    setOptionSquares({});
  }

  function resetGame() {
    const startFen = new Chess().fen();
    setNodes({
      'root': { id: 'root', fen: startFen, san: 'Start', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    latestPvLinesRef.current = {};
    setPvLines({});
    setMoveFrom('');
    setOptionSquares({});
    requestAnalysis(startFen);
  }

  function applySetup() {
    setNodes({
      'root': { id: 'root', fen: setupFenInput, san: 'Setup Position', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    setIsSetupMode(false);
    requestAnalysis(setupFenInput);
  }

  function toggleSetupMode() {
    if (isSetupMode) {
      setIsSetupMode(false);
      requestAnalysis(nodes[currentNodeId].fen); // Resume normal analysis on cancel
    } else {
      setIsSetupMode(true);
      if (wsRef.current && wsRef.current.readyState === 1) {
        wsRef.current.send('stop'); // Stop engine while setting up board
      }
    }
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
  if (showArrows && playMode === 'Analysis' && Object.keys(pvLines).length > 0 && !isDragging) {
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
        <div style={{ display: 'flex', justifyContent: 'flex-end', marginBottom: '0.5rem' }}>
            <a href="https://github.com/botmaksim/pawnGO" target="_blank" rel="noreferrer" style={{ color: '#aaa', textDecoration: 'none', display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.9rem' }}>
                <svg height="20" viewBox="0 0 16 16" version="1.1" width="20" aria-hidden="true" fill="currentColor">
                    <path fillRule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path>
                </svg>
                GitHub
            </a>
        </div>
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
              id="MainBoard"
              position={currentFen} 
              isDraggablePiece={() => !isEngineTurn}
              boardOrientation={boardOrientation}
              onPieceDrop={isSetupMode ? onPieceDropSetup : onDrop}
              onPieceDragBegin={() => setIsDragging(true)}
              onPieceDragEnd={() => setIsDragging(false)}
              onSquareClick={isSetupMode ? null : onSquareClick}
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
            <button onClick={toggleSetupMode} style={{ fontSize: '0.8rem', padding: '0.3rem 0.6rem' }}>
              {isSetupMode ? 'Cancel Setup' : 'Setup Mode'}
            </button>
          </div>
          <p>Status: {wsRef.current && wsRef.current.readyState === WebSocket.OPEN ? 'Connected' : 'Connecting...'}</p>
          
          <div style={{ display: 'flex', gap: '1rem', flexWrap: 'wrap', marginBottom: '1rem', padding: '0.5rem', background: 'rgba(255,255,255,0.05)', borderRadius: '8px' }}>
            <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer', color: '#fff' }}>
              <input type="checkbox" checked={showArrows} onChange={e => setShowArrows(e.target.checked)} />
              Show Arrows
            </label>
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
              <span style={{ color: '#aaa' }}>Mode:</span>
              <select 
                value={playMode} 
                onChange={e => setPlayMode(e.target.value)}
                style={{ background: '#2c3e50', color: '#fff', border: '1px solid #34495e', padding: '4px 8px', borderRadius: '4px' }}
              >
                <option value="Analysis">Analysis</option>
                <option value="Play as White">Play as White</option>
                <option value="Play as Black">Play as Black</option>
              </select>
            </div>
          </div>
          
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

        {playMode === 'Analysis' && (
          <div className="glass-panel">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1rem', flexWrap: 'wrap', gap: '1rem' }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <div style={{ display: 'flex', alignItems: 'baseline', gap: '0.5rem' }}>
                    <h2 style={{ margin: 0 }}>Engine Lines (Multi-PV)</h2>
                    <span style={{ fontSize: '0.8rem', color: '#aaa', fontStyle: 'italic' }}>
                        (Analyzes each move up to Depth {analysisDepth})
                    </span>
                </div>
                <div style={{ display: 'flex', gap: '0.5rem' }}>
                    <button 
                      onClick={startFullGameAnalysis} 
                      disabled={isAnalyzingGame || Object.keys(nodes).length <= 1}
                      className="modern-button"
                      style={{ padding: '6px 12px', fontSize: '0.9rem', width: 'fit-content', background: 'rgba(27, 172, 166, 0.2)', border: '1px solid #1baca6' }}
                    >
                      Analyze Game
                    </button>
                    {isAnalyzingGame && (
                        <button 
                          onClick={cancelFullGameAnalysis} 
                          className="modern-button"
                          style={{ padding: '6px 12px', fontSize: '0.9rem', width: 'fit-content', background: 'rgba(229, 57, 53, 0.2)', border: '1px solid #e53935' }}
                        >
                          Cancel
                        </button>
                    )}
                </div>
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
              .sort((a,b) => {
                  if (a.score === 'Book' && b.score !== 'Book') return -1;
                  if (b.score === 'Book' && a.score !== 'Book') return 1;
                  return game.turn() === 'w' 
                      ? parseScoreForSort(b.score) - parseScoreForSort(a.score) 
                      : parseScoreForSort(a.score) - parseScoreForSort(b.score);
              })
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
        </div>
        )}
      </div>

      {isAnalyzingGame && (
        <div style={{
          position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
          background: 'rgba(0,0,0,0.8)', display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center', zIndex: 9999, color: 'white'
        }}>
          <h2>Analyzing Game...</h2>
          <div style={{ width: '300px', height: '10px', background: '#333', borderRadius: '5px', overflow: 'hidden', margin: '20px 0' }}>
            <div style={{ 
              width: `${(analyzeProgress.current / analyzeProgress.total) * 100}%`, 
              height: '100%', background: '#1baca6', transition: 'width 0.3s' 
            }} />
          </div>
          <p>{analyzeProgress.current} / {analyzeProgress.total} moves analyzed</p>
        </div>
      )}
    </div>
  );
}

export default App;

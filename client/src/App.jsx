import { useState, useEffect, useRef, useMemo } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';
import EvalBar from './EvalBar';
import { classifyMove, getAnnotationDescription } from './utils/analysis';
import { playMoveSound, playCaptureSound, playCheckSound, playCastleSound, playEndSound, isSoundEnabled, setSoundEnabled, getMasterVolume, setMasterVolume } from './utils/sound';

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
  const currentNodeIdRef = useRef(currentNodeId);
  useEffect(() => { currentNodeIdRef.current = currentNodeId; }, [currentNodeId]);
  
  // Last move highlight & Sound
  const [lastMove, setLastMove] = useState(null);
  const [soundOn, setSoundOn] = useState(() => isSoundEnabled());
  const [volume, setVolume] = useState(() => getMasterVolume());
  const toggleSound = () => {
    setSoundOn(prev => {
      const next = !prev;
      setSoundEnabled(next);
      if (next) {
        if (volume === 0) {
          setVolume(0.8);
          setMasterVolume(0.8);
        }
        playMoveSound();
      }
      return next;
    });
  };
  const handleVolumeChange = (newVal) => {
    const val = parseFloat(newVal);
    setVolume(val);
    setMasterVolume(val);
    if (val > 0 && !soundOn) {
      setSoundOn(true);
      setSoundEnabled(true);
    } else if (val === 0 && soundOn) {
      setSoundOn(false);
      setSoundEnabled(false);
    }
  };

  // Modes
  const [isSetupMode, setIsSetupMode] = useState(false);
  const [setupFenInput, setSetupFenInput] = useState(chessHelper.fen());
  const [boardOrientation, setBoardOrientation] = useState('white');

  // UX States
  const [moveFrom, setMoveFrom] = useState('');
  const [optionSquares, setOptionSquares] = useState({});
  const [isShaking, setIsShaking] = useState(false);
  
  // Engine States
  const maxHardwareThreads = typeof navigator !== 'undefined' ? Math.min(navigator.hardwareConcurrency || 4, 16) : 4;
  const [engineReady, setEngineReady] = useState(false);
  const [pvLines, setPvLines] = useState({}); // map of multipv index -> line object
  const [multiPvCount, setMultiPvCount] = useState(3);
  const [analysisDepth, setAnalysisDepth] = useState(15);
  const [engineThreads, setEngineThreads] = useState(() => {
    try {
      const saved = localStorage.getItem('pawngo_threads');
      return saved ? parseInt(saved, 10) : 2;
    } catch (e) {
      return 2;
    }
  });
  const isEngineSearchingRef = useRef(false);
  const pendingGoRef = useRef(null);
  const ignoringStaleInfoRef = useRef(false);

  
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
  const workerRef = useRef(null);
  const reinitTimerRef = useRef(null);
  const retryCountRef = useRef(0);
  const tempGameRef = useRef(new Chess());
  const sanGameRef = useRef(new Chess());

  // New UI features
  const [showArrows, setShowArrows] = useState(true);
  const [playMode, setPlayMode] = useState('Analysis');

  // Helper to construct UCI position command with full history
  const getUciPosition = (treeNodes, targetNodeId, fallbackFen = null) => {
    if (!targetNodeId || !treeNodes || !treeNodes[targetNodeId]) {
      if (fallbackFen) {
        return `position fen ${fallbackFen}`;
      }
      return 'position startpos';
    }
    const path = [];
    let curr = treeNodes[targetNodeId];
    while (curr && curr.parentId) {
      path.unshift(curr);
      curr = treeNodes[curr.parentId];
    }
    const rootNode = curr;
    let posCmd = '';
    if (rootNode && rootNode.fen && rootNode.fen !== 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1') {
      posCmd = `position fen ${rootNode.fen}`;
    } else {
      posCmd = `position startpos`;
    }
    const moves = [];
    for (const n of path) {
      const uciStr = n.uci || (n.from && n.to ? `${n.from}${n.to}${n.promotion || ''}` : null);
      if (uciStr) moves.push(uciStr);
    }
    if (moves.length > 0) {
      posCmd += ` moves ${moves.join(' ')}`;
    }
    return posCmd;
  };

  // Derived state
  const currentNode = nodes[currentNodeId];
  const currentFen = isSetupMode ? setupFenInput : currentNode.fen;

  // Reconstruct game with full move history so chess.js can track threefold repetition & 50-move rule
  const game = useMemo(() => {
    if (isSetupMode) {
      const g = new Chess();
      try { g.load(setupFenInput); } catch (e) {}
      return g;
    }
    const path = [];
    let curr = nodes[currentNodeId];
    while (curr && curr.parentId) {
      path.unshift(curr);
      curr = nodes[curr.parentId];
    }
    const rootNode = curr;
    const g = new Chess();
    if (rootNode && rootNode.fen && rootNode.fen !== 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1') {
      try { g.load(rootNode.fen); } catch (e) { g.load('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'); }
    }
    for (const n of path) {
      if (n.uci) {
        const from = n.uci.substring(0, 2);
        const to = n.uci.substring(2, 4);
        const promotion = n.uci.length > 4 ? n.uci[4] : undefined;
        try { g.move({ from, to, promotion }); } catch (e) { try { g.move(n.san); } catch (e2) {} }
      } else if (n.san && n.san !== 'Start') {
        try { g.move(n.san); } catch (e) {}
      }
    }
    return g;
  }, [nodes, currentNodeId, isSetupMode, setupFenInput]);

  const turnColor = game.turn() === 'w' ? 'White' : 'Black';

  const playModeRef = useRef(playMode);
  const isBotTurnSearchRef = useRef(false);
  const prevPlayModeRef = useRef(playMode);
  useEffect(() => { 
    playModeRef.current = playMode; 
    
    if (wsRef.current && wsRef.current.readyState === 1) {
      if (playMode !== 'Analysis') {
        // Full maximum strength for play against engine: single line for full search depth
        wsRef.current.send(`setoption name MultiPV value 1`);
        wsRef.current.send(`setoption name Threads value ${maxHardwareThreads}`);
      } else {
        // Standard threads & MultiPV for analysis
        wsRef.current.send(`setoption name MultiPV value ${multiPvCount}`);
        wsRef.current.send(`setoption name Threads value ${engineThreads}`);
      }
    }

    // If we just switched to playing as the side whose turn it is, force the engine to think!
    if (playMode !== 'Analysis' && !isSetupMode) {
      if (prevPlayModeRef.current !== playMode) {
          if (playMode === 'Play as Black') {
              setBoardOrientation('black');
          } else if (playMode === 'Play as White') {
              setBoardOrientation('white');
          }
          const tempGame = new Chess();
          try { tempGame.load(currentFenRef.current); } catch(e) {}
          const currentTurn = tempGame.turn();
          if ((playMode === 'Play as White' && currentTurn === 'b') ||
              (playMode === 'Play as Black' && currentTurn === 'w')) {
              requestAnalysis(currentFenRef.current);
          } else {
              // It's player's turn: don't let engine search in background
              if (wsRef.current && isEngineSearchingRef.current) {
                  wsRef.current.send('stop');
              }
          }
      }
    } else if (playMode === 'Analysis' && prevPlayModeRef.current !== 'Analysis') {
      // Switched back to Analysis: resume analysis
      requestAnalysis(currentFenRef.current);
    }
    prevPlayModeRef.current = playMode;
  }, [playMode, isSetupMode, engineThreads, maxHardwareThreads, multiPvCount]);
  const [pendingEngineMove, setPendingEngineMove] = useState(null);



  const currentFenRef = useRef(currentFen);
  useEffect(() => {
    currentFenRef.current = currentFen;
  }, [currentFen]);

  useEffect(() => {
    if (pendingEngineMove) {
      const from = pendingEngineMove.substring(0, 2);
      const to = pendingEngineMove.substring(2, 4);
      const promotion = pendingEngineMove.length > 4 ? pendingEngineMove[4] : undefined;
      const success = handleMove({ from, to, promotion });
      if (!success) {
        console.warn(`[Engine] Play mode move failed: ${pendingEngineMove} on FEN: ${game.fen()}`);
      }
      setPendingEngineMove(null);
    }
  }, [pendingEngineMove]);

  useEffect(() => {
    let active = true;

    const initWorker = () => {
      if (!active) return;
      if (workerRef.current) {
        try { workerRef.current.terminate(); } catch (e) {}
        workerRef.current = null;
      }

      const worker = new Worker('wasm/engineWorker_v3.js?v=10');
      workerRef.current = worker;
      
      worker.onerror = (err) => {
        console.error("Worker error:", err);
        if (!active) return;
        try { worker.terminate(); } catch (e) {}
        workerRef.current = null;
        setEngineReady(false);
        isEngineSearchingRef.current = false;
        if (retryCountRef.current < 3) {
          retryCountRef.current += 1;
          clearTimeout(reinitTimerRef.current);
          reinitTimerRef.current = setTimeout(initWorker, 1000);
        }
      };

      wsRef.current = {
        readyState: 1, // OPEN
        isTablebasePending: null,
        send: (cmd) => {
            if (cmd.startsWith('position fen ')) {
                const fen = cmd.substring('position fen '.length);
                const pieceCount = fen.split(' ')[0].replace(/[^a-zA-Z]/g, '').length;
                if (pieceCount <= 5) {
                    wsRef.current.isTablebasePending = fen;
                    return;
                }
                wsRef.current.isTablebasePending = null;
            }
            if (cmd.startsWith('go ') && wsRef.current.isTablebasePending) {
                const fen = wsRef.current.isTablebasePending;
                // 1. Try local in-memory Syzygy daemon first:
                fetch(`/api/tablebase?fen=${encodeURIComponent(fen)}`)
                    .then(res => {
                        if (!res.ok) throw new Error('Local tablebase miss');
                        return res.json();
                    })
                    .then(data => {
                        if (data && data.bestmove && data.bestmove !== 'none') {
                            return { bestmove: data.bestmove };
                        }
                        throw new Error('Local tablebase miss');
                    })
                    .catch(() => {
                        // 2. Fall back to lichess.ovh:
                        return fetch(`https://tablebase.lichess.ovh/standard?fen=${encodeURIComponent(fen)}`)
                            .then(res => res.json())
                            .then(data => {
                                if (data && data.moves && data.moves.length > 0) {
                                    return { bestmove: data.moves[0].uci };
                                }
                                return null;
                            });
                    })
                    .then(result => {
                        if (result && result.bestmove) {
                            if (wsRef.current && wsRef.current.onmessage) {
                                wsRef.current.onmessage({ data: `info depth 64 score cp 19999 pv ${result.bestmove}` });
                                wsRef.current.onmessage({ data: `bestmove ${result.bestmove}` });
                            }
                            wsRef.current.isTablebasePending = null;
                        } else {
                            wsRef.current.isTablebasePending = null;
                            if (workerRef.current) {
                                workerRef.current.postMessage(`position fen ${fen}`);
                                workerRef.current.postMessage(cmd);
                            }
                        }
                    })
                    .catch(() => {
                        wsRef.current.isTablebasePending = null;
                        if (workerRef.current) {
                            workerRef.current.postMessage(`position fen ${fen}`);
                            workerRef.current.postMessage(cmd);
                        }
                    });
                return;
            }
            if (cmd === 'stop' && wsRef.current.isTablebasePending) {
                return;
            }
            if (workerRef.current) {
                workerRef.current.postMessage(cmd);
            }
        },
        close: () => {
          if (workerRef.current) {
            try { workerRef.current.terminate(); } catch (e) {}
            workerRef.current = null;
          }
        }
      };

      worker.onmessage = (event) => {
        if (!active) return;
        const msg = event.data;
        if (msg === 'isready') {
            retryCountRef.current = 0;
            setEngineReady(true);
            wsRef.current.send('uci');
            wsRef.current.send(`setoption name Threads value ${engineThreads}`);
            return;
        }
        
        if (wsRef.current && wsRef.current.onmessage) {
            wsRef.current.onmessage({ data: msg });
        }
      };
    };

    initWorker();

    wsRef.current.onmessage = (event) => {
      const msg = event.data;
      if (typeof msg !== 'string') return;
      
      if (msg.startsWith('info')) {
        if (ignoringStaleInfoRef.current) return;
        
        if (msg.includes('Book move') || msg.includes('book 1')) {
          latestPvLinesRef.current = {
            ...latestPvLinesRef.current,
            '1': {
              depth: 'Book',
              score: 'Book',
              pv: 'Book',
              san: 'Book'
            }
          };
        }

        const depthMatch = msg.match(/depth (\d+)/);
        const multipvMatch = msg.match(/multipv (\d+)/) || [null, '1'];
        const scoreCpMatch = msg.match(/score cp (-?\d+)/);
        const scoreMateMatch = msg.match(/score mate (-?\d+)/);
        const bookMatch = msg.match(/book 1/);
        const pvMatch = msg.match(/\bpv (.*)/);

        if (depthMatch && pvMatch) {
          const multipv = multipvMatch[1];
          let scoreStr = '';
          
          const tempGame = tempGameRef.current;
          try { 
            tempGame.load(isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current 
              ? nodesRef.current[currentAnalyzeNodeIdRef.current].fen 
              : currentFenRef.current); 
          } catch(e) {}
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
              const sanGame = sanGameRef.current;
              sanGame.load(isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current 
                ? nodesRef.current[currentAnalyzeNodeIdRef.current].fen 
                : currentFenRef.current);
              const rawPv = pvMatch[1].trim().split(' ');
              const newSanMoves = [];
              for (const move of rawPv) {
                  if (move.length < 4) continue;
                  const moveObj = {
                      from: move.substring(0, 2),
                      to: move.substring(2, 4),
                      promotion: move.length === 5 ? move.substring(4, 5) : undefined
                  };
                  try {
                      const m = sanGame.move(moveObj);
                      if (m && m.san) {
                          newSanMoves.push(m.san);
                      }
                  } catch (err) {
                      break;
                  }
              }
              sanPv = newSanMoves.join(' ');
          } catch(e) {
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
                     setPvLines(latestPvLinesRef.current);
                     updatePendingRef.current = false;
                 });
             }
          }
        }
      } else if (msg.startsWith('bestmove')) {
        isEngineSearchingRef.current = false;
        if (pendingGoRef.current) {
            const fen = pendingGoRef.current.fen;
            const depth = pendingGoRef.current.depth;
            const isBotTurn = pendingGoRef.current.isBotTurn;
            const posCmd = pendingGoRef.current.posCmd || (isSetupMode ? `position fen ${fen}` : getUciPosition(nodesRef.current, currentNodeIdRef.current, fen));
            pendingGoRef.current = null;
            ignoringStaleInfoRef.current = false;
            
            latestPvLinesRef.current = {};
            setPvLines({});
            
            isEngineSearchingRef.current = true;
            isBotTurnSearchRef.current = isBotTurn;

            const pieceCount = fen.split(' ')[0].replace(/[^a-zA-Z]/g, '').length;
            if (pieceCount <= 5) {
                wsRef.current.isTablebasePending = fen;
            } else {
                wsRef.current.isTablebasePending = null;
            }

            wsRef.current.send(posCmd);
            if (isBotTurn) {
                wsRef.current.send(`go movetime 3200`);
            } else {
                wsRef.current.send(`go depth ${depth}`);
            }
            return;
        }

        if (isAnalyzingGameRef.current && currentAnalyzeNodeIdRef.current) {
          const analyzedId = currentAnalyzeNodeIdRef.current;
          let analyzedPvs = { ...latestPvLinesRef.current };
          const isBook = Object.keys(analyzedPvs).length === 0 || analyzedPvs['1']?.score === 'Book' || msg.includes('Book');
          if (isBook) {
            analyzedPvs = {
              '1': { depth: 'Book', score: 'Book', pv: msg.split(' ')[1] || '', san: '' }
            };
          }
          setNodes(prev => ({
            ...prev,
            [analyzedId]: {
              ...prev[analyzedId],
              pvLines: analyzedPvs,
              isBook: isBook
            }
          }));
          processNextAnalysisNode();
        } else if (!isAnalyzingGameRef.current && playModeRef.current !== 'Analysis') {
          // Play against engine mode
          try {
              if (isBotTurnSearchRef.current) {
                  const bestmove = msg.split(' ')[1];
                  if (bestmove && bestmove !== '(none)') {
                      // Smooth natural delay for opening book moves and bot moves:
                      const isBook = latestPvLinesRef.current['1']?.score === 'Book' || msg.includes('Book');
                      const delay = isBook ? 400 : 120;
                      setTimeout(() => {
                          setPendingEngineMove(bestmove);
                      }, delay);
                  }
              }
          } catch (e) {
              console.error('[Engine] Failed to process play mode bestmove', e);
          }
        }
      }
    };

    return () => {
      active = false;
      clearTimeout(reinitTimerRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, []);

  useEffect(() => {
    if (engineReady && wsRef.current && wsRef.current.readyState === 1 && !isEngineSearchingRef.current) {
       wsRef.current.send(`setoption name MultiPV value ${multiPvCount}`);
       requestAnalysis(currentFenRef.current);
    }
  }, [multiPvCount, engineReady]);

  useEffect(() => {
    if (engineReady && wsRef.current && wsRef.current.readyState === 1) {
       wsRef.current.send(`setoption name Threads value ${engineThreads}`);
       try {
         localStorage.setItem('pawngo_threads', engineThreads.toString());
       } catch (e) {}
    }
  }, [engineThreads, engineReady]);

  const processNextAnalysisNode = () => {
    if (analyzeQueueRef.current.length === 0) {
      // Game analysis complete: classify moves
      setIsAnalyzingGame(false);
      isAnalyzingGameRef.current = false;
      currentAnalyzeNodeIdRef.current = null;
      setNodes(prevNodes => {
        const nextNodes = { ...prevNodes };
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
      // Restore threads after full game analysis
      if (wsRef.current && wsRef.current.readyState === 1) {
        const restoreThreads = playModeRef.current !== 'Analysis' ? maxHardwareThreads : engineThreads;
        wsRef.current.send(`setoption name Threads value ${restoreThreads}`);
      }
      requestAnalysis(nodesRef.current[currentNodeIdRef.current].fen, null, currentNodeIdRef.current);
      return;
    }
    const nextId = analyzeQueueRef.current.shift();
    currentAnalyzeNodeIdRef.current = nextId;
    setAnalyzeProgress(p => ({ ...p, current: p.total - analyzeQueueRef.current.length }));
    
    // Depth 14 for swift and accurate game review
    requestAnalysis(nodesRef.current[nextId].fen, 14, nextId);
  };

  const startFullGameAnalysis = () => {
    // Utilize maximum hardware threads for full game analysis
    if (wsRef.current && wsRef.current.readyState === 1) {
      wsRef.current.send(`setoption name Threads value ${maxHardwareThreads}`);
    }

    // Collect all nodes in topological tree order (root first, then children)
    const queue = [];
    const visited = new Set();
    const dfs = (id) => {
      if (visited.has(id)) return;
      visited.add(id);
      queue.push(id);
      const node = nodesRef.current[id];
      if (node && node.children) {
        for (const childId of node.children) {
          dfs(childId);
        }
      }
    };
    dfs('root');
    for (const id in nodesRef.current) {
      if (!visited.has(id)) queue.push(id);
    }

    analyzeQueueRef.current = queue;
    setAnalyzeProgress({ current: 0, total: queue.length });
    setIsAnalyzingGame(true);
    isAnalyzingGameRef.current = true;
    
    processNextAnalysisNode();
  };

  const cancelFullGameAnalysis = () => {
    analyzeQueueRef.current = [];
    setIsAnalyzingGame(false);
    isAnalyzingGameRef.current = false;
    currentAnalyzeNodeIdRef.current = null;
    if (wsRef.current && wsRef.current.readyState === 1) {
      wsRef.current.send('stop');
      const restoreThreads = playModeRef.current !== 'Analysis' ? maxHardwareThreads : engineThreads;
      wsRef.current.send(`setoption name Threads value ${restoreThreads}`);
    }
    requestAnalysis(nodesRef.current[currentNodeIdRef.current].fen, null, currentNodeIdRef.current);
  };

  const requestAnalysis = (fen, overrideDepth = null, targetNodeId = null) => {
    const depthToUse = overrideDepth !== null ? overrideDepth : analysisDepth;
    const nodeId = targetNodeId || currentNodeIdRef.current;
    const posCmd = isSetupMode ? `position fen ${fen}` : getUciPosition(nodesRef.current, nodeId, fen);
    
    let isBotTurn = false;
    if (playModeRef.current !== 'Analysis') {
        try {
            const tempGame = new Chess();
            tempGame.load(fen);
            if (tempGame.isGameOver()) {
                if (wsRef.current && wsRef.current.readyState === 1 && isEngineSearchingRef.current) {
                    wsRef.current.send('stop');
                }
                return;
            }
            const turn = tempGame.turn();
            if ((playModeRef.current === 'Play as White' && turn === 'b') || 
                (playModeRef.current === 'Play as Black' && turn === 'w')) {
                isBotTurn = true;
            }
        } catch (e) {}

        if (!isBotTurn && !isAnalyzingGameRef.current) {
            // Engine idling while awaiting player move
            if (wsRef.current && wsRef.current.readyState === 1 && isEngineSearchingRef.current) {
                wsRef.current.send('stop');
            }
            return;
        }
    }

    if (wsRef.current && wsRef.current.readyState === 1) {
      if (isEngineSearchingRef.current) {
        pendingGoRef.current = { fen, depth: depthToUse, isBotTurn, posCmd };
        ignoringStaleInfoRef.current = true;
        if (!targetNodeId || !nodesRef.current[targetNodeId]?.pvLines) {
          latestPvLinesRef.current = {};
          setPvLines({});
        }
        wsRef.current.send('stop');
      } else {
        latestPvLinesRef.current = {};
        setPvLines({});
        isEngineSearchingRef.current = true;
        ignoringStaleInfoRef.current = false;
        isBotTurnSearchRef.current = isBotTurn;

        const pieceCount = fen.split(' ')[0].replace(/[^a-zA-Z]/g, '').length;
        if (pieceCount <= 5) {
            wsRef.current.isTablebasePending = fen;
        } else {
            wsRef.current.isTablebasePending = null;
        }

        wsRef.current.send(posCmd);
        if (isBotTurn) {
            // Allocate 3.2s per move in play against engine mode
            wsRef.current.send(`go movetime 3200`);
        } else {
            wsRef.current.send(`go depth ${depthToUse}`);
        }
      }
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

    // Play sounds
    if (move.captured) {
      playCaptureSound();
    } else if (move.flags && (move.flags.includes('k') || move.flags.includes('q'))) {
      playCastleSound();
    } else {
      playMoveSound();
    }

    if (game.isCheckmate() || game.isDraw()) {
      setTimeout(playEndSound, 150);
    } else if (game.inCheck()) {
      setTimeout(playCheckSound, 120);
    }

    setLastMove({ from: move.from, to: move.to });

    const parentId = currentNodeIdRef.current;
    const parentNode = nodesRef.current[parentId] || nodesRef.current['root'] || { ply: 0, children: [] };

    // Check if this move already exists as a child
    const existingChildId = (parentNode.children || []).find(childId => nodesRef.current[childId]?.san === move.san);
    
    let activeId = existingChildId;
    if (existingChildId) {
      setCurrentNodeId(existingChildId);
      currentNodeIdRef.current = existingChildId;
    } else {
      // Create new branch
      const newId = genId();
      activeId = newId;

      const uci = move.from + move.to + (move.promotion || '');

      const newNode = {
        id: newId,
        fen: game.fen(),
        san: move.san,
        uci: uci,
        from: move.from,
        to: move.to,
        parentId: parentId,
        children: [],
        annotation: '',
        ply: (parentNode.ply || 0) + 1
      };

      const updatedNodes = {
        ...nodesRef.current,
        [parentId]: { ...parentNode, children: [...(parentNode.children || []), newId] },
        [newId]: newNode
      };
      nodesRef.current = updatedNodes;
      setNodes(updatedNodes);
      setCurrentNodeId(newId);
      currentNodeIdRef.current = newId;
    }

    // Reset Engine PVs and clear stale arrows immediately
    latestPvLinesRef.current = {};
    setPvLines({});
    requestAnalysis(game.fen(), null, activeId);

    setMoveFrom('');
    setOptionSquares({});
    return true;
  }

  // Disable drag if it's engine's turn or opponent's pieces
  const isEngineTurn = !isSetupMode && playMode !== 'Analysis' && (
    (playMode === 'Play as White' && game.turn() === 'b') ||
    (playMode === 'Play as Black' && game.turn() === 'w')
  );

  const isDraggablePiece = ({ piece }) => {
    if (isSetupMode) return true;
    if (playMode === 'Analysis') return piece[0] === game.turn();
    if (isEngineTurn) return false;
    if (playMode === 'Play as White') return piece[0] === 'w' && game.turn() === 'w';
    if (playMode === 'Play as Black') return piece[0] === 'b' && game.turn() === 'b';
    return false;
  };

  function onSquareClick(square) {
    if (isSetupMode) return;
    if (isEngineTurn) return;
    if (playMode === 'Play as White' && game.turn() !== 'w') return;
    if (playMode === 'Play as Black' && game.turn() !== 'b') return;

    if (!moveFrom) {
      const hasPiece = game.get(square);
      if (hasPiece && hasPiece.color === game.turn()) {
        if (playMode === 'Play as White' && hasPiece.color !== 'w') return;
        if (playMode === 'Play as Black' && hasPiece.color !== 'b') return;
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
          if (playMode === 'Play as White' && hasPiece.color !== 'w') return;
          if (playMode === 'Play as Black' && hasPiece.color !== 'b') return;
          setMoveFrom(square);
          getMoveOptions(square);
      } else {
          triggerError();
      }
    }
  }

  function onDrop(sourceSquare, targetSquare, piece) {
    if (isEngineTurn) return false;
    if (!isSetupMode && playMode !== 'Analysis') {
      if (playMode === 'Play as White' && (game.turn() !== 'w' || (piece && piece[0] !== 'w'))) return false;
      if (playMode === 'Play as Black' && (game.turn() !== 'b' || (piece && piece[0] !== 'b'))) return false;
    }

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
    if (!id || !nodesRef.current[id]) return;
    setCurrentNodeId(id);
    currentNodeIdRef.current = id;
    const n = nodesRef.current[id];
    if (n && n.from && n.to) {
      setLastMove({ from: n.from, to: n.to });
    } else if (n && n.uci && n.uci.length >= 4) {
      setLastMove({ from: n.uci.substring(0, 2), to: n.uci.substring(2, 4) });
    } else {
      setLastMove(null);
    }
    latestPvLinesRef.current = n && n.pvLines ? { ...n.pvLines } : {};
    setPvLines(latestPvLinesRef.current);
    requestAnalysis(n.fen, null, id);
    setMoveFrom('');
    setOptionSquares({});
  }

  // Keyboard navigation for moves using arrow keys
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (['INPUT', 'TEXTAREA'].includes(e.target?.tagName) || e.target?.isContentEditable) {
        return;
      }
      if (isSetupMode) return;

      if (e.key === 'ArrowLeft') {
        e.preventDefault();
        const curr = nodesRef.current[currentNodeIdRef.current];
        if (curr && curr.parentId) {
          jumpTo(curr.parentId);
        }
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        const curr = nodesRef.current[currentNodeIdRef.current];
        if (curr && curr.children && curr.children.length > 0) {
          jumpTo(curr.children[0]);
        }
      } else if (e.key === 'ArrowUp' || e.key === 'Home') {
        e.preventDefault();
        jumpTo('root');
      } else if (e.key === 'ArrowDown' || e.key === 'End') {
        e.preventDefault();
        let curr = nodesRef.current[currentNodeIdRef.current];
        while (curr && curr.children && curr.children.length > 0) {
          curr = nodesRef.current[curr.children[0]];
        }
        if (curr && curr.id) {
          jumpTo(curr.id);
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isSetupMode]);

  function resetGame() {
    const startFen = new Chess().fen();
    setNodes({
      'root': { id: 'root', fen: startFen, san: 'Start', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    setLastMove(null);
    latestPvLinesRef.current = {};
    setPvLines({});
    setMoveFrom('');
    setOptionSquares({});
    requestAnalysis(startFen, null, 'root');
  }

  function applySetup() {
    try {
        new Chess(setupFenInput);
    } catch(e) {
        alert("Invalid FEN: " + e.message);
        return;
    }
    setNodes({
      'root': { id: 'root', fen: setupFenInput, san: 'Setup Position', parentId: null, children: [], evalStr: null, annotation: '', ply: 0 }
    });
    setCurrentNodeId('root');
    setLastMove(null);
    latestPvLinesRef.current = {};
    setPvLines({});
    setMoveFrom('');
    setOptionSquares({});
    setIsSetupMode(false);
    requestAnalysis(setupFenInput, null, 'root');
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

  const getAnnotationClass = (anno) => {
    if (anno === '📖') return 'anno-book';
    if (anno === '!!') return 'anno-brilliant';
    if (anno === '!') return 'anno-great';
    if (anno === '★') return 'anno-best';
    if (anno === '✓') return 'anno-good';
    if (anno === '?!') return 'anno-inaccuracy';
    if (anno === '?') return 'anno-mistake';
    if (anno === '??') return 'anno-blunder';
    return '';
  };

  const renderMoves = (nodeId) => {
    const node = nodes[nodeId];
    if (!node || node.children.length === 0) return null;

    const mainChildId = node.children[0];
    const mainChild = nodes[mainChildId];
    const variations = node.children.slice(1);

    const moveNum = Math.floor((mainChild.ply - 1) / 2) + 1;
    const isWhite = mainChild.ply % 2 !== 0;
    const numStr = isWhite ? `${moveNum}. ` : (node.id === 'root' ? `${moveNum}... ` : ' ');
    
    const annotationClass = getAnnotationClass(mainChild.annotation);

    return (
      <span key={mainChildId}>
        <span 
          className={`move-item ${currentNodeId === mainChildId ? 'active' : ''} ${annotationClass}`}
          onClick={() => jumpTo(mainChildId)}
        >
          {numStr}{mainChild.san}{mainChild.annotation}
        </span>{' '}
        
        {variations.length > 0 && (
          <span className="variations-container">
            {variations.map(varId => {
               const vChild = nodes[varId];
               const vMoveNum = Math.floor((vChild.ply - 1) / 2) + 1;
               const vIsWhite = vChild.ply % 2 !== 0;
               const vNumStr = vIsWhite ? `${vMoveNum}. ` : `${vMoveNum}... `;
               
               const vAnnotationClass = getAnnotationClass(vChild.annotation);

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

  // Memoized custom styles for square highlights & check & last move
  const customStyles = useMemo(() => {
    const styles = {};

    // 1. Highlight last move (from & to)
    if (!isSetupMode && lastMove && lastMove.from && lastMove.to) {
      styles[lastMove.from] = {
        backgroundColor: 'rgba(205, 210, 106, 0.45)'
      };
      styles[lastMove.to] = {
        backgroundColor: 'rgba(205, 210, 106, 0.55)'
      };
    }

    // 2. Click option squares
    Object.keys(optionSquares).forEach(sq => {
      styles[sq] = {
        ...styles[sq],
        ...optionSquares[sq]
      };
    });

    // 3. Red glow on king in check
    if (!isSetupMode && game.inCheck()) {
      const board = game.board();
      for (let r = 0; r < 8; r++) {
        for (let c = 0; c < 8; c++) {
          if (board[r][c] && board[r][c].type === 'k' && board[r][c].color === game.turn()) {
            const checkSquare = String.fromCharCode(97 + c) + (8 - r);
            styles[checkSquare] = {
              ...styles[checkSquare],
              background: 'radial-gradient(circle, rgba(255,0,0,0.8), rgba(255,0,0,0) 60%)'
            };
          }
        }
      }
    }

    return styles;
  }, [optionSquares, isSetupMode, currentFen, lastMove]);

  // Memoized annotation overlay
  const annotationOverlay = useMemo(() => {
    if (isSetupMode || !currentNode.annotation) return null;
    const san = currentNode.san;
    const match = san ? san.match(/[a-h][1-8]/) : null;
    if (!match) return null;

    const sq = match[0];
    const files = ['a','b','c','d','e','f','g','h'];
    const fIndex = files.indexOf(sq[0]);
    const rIndex = parseInt(sq[1], 10) - 1;
    
    const x = boardOrientation === 'white' ? fIndex : 7 - fIndex;
    const y = boardOrientation === 'white' ? 7 - rIndex : rIndex;
    
    let badgeColor = '#888';
    let badgeText = currentNode.annotation;
    if (badgeText === '📖') badgeColor = '#85a947';
    else if (badgeText === '!!') badgeColor = '#1baca6';
    else if (badgeText === '!') badgeColor = '#5c8bb0';
    else if (badgeText === '★') badgeColor = '#81b64c';
    else if (badgeText === '✓') badgeColor = '#87ab63';
    else if (badgeText === '?!') badgeColor = '#f5b041';
    else if (badgeText === '?') badgeColor = '#eb984e';
    else if (badgeText === '??') badgeColor = '#e53935';

    return (
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
  }, [isSetupMode, currentNode.annotation, currentNode.san, boardOrientation]);

  // Calculate current depth
  const currentDepth = Object.values(pvLines).length > 0 
      ? Math.max(0, ...Object.values(pvLines).map(l => parseInt(l.depth, 10) || 0)) 
      : 0;

  // Memoized Custom Engine Arrows SVG Overlay
  const engineArrowsOverlay = useMemo(() => {
    if (isSetupMode || !showArrows || playMode !== 'Analysis' || isDragging || !pvLines) {
      return null;
    }

    const arrowConfigs = [
      { color: 'rgba(34, 197, 94, 0.85)', strokeWidth: 1.8, headLen: 4.2, headWidth: 3.8 }, // 1st line: green
      { color: 'rgba(59, 130, 246, 0.80)', strokeWidth: 1.6, headLen: 3.8, headWidth: 3.4 }, // 2nd line: blue
      { color: 'rgba(245, 158, 11, 0.75)', strokeWidth: 1.4, headLen: 3.4, headWidth: 3.0 }, // 3rd line: orange
    ];

    const currentTurn = game.turn();
    const legalMoves = game.moves({ verbose: true });
    const sortedKeys = Object.keys(pvLines).sort((a, b) => parseInt(a, 10) - parseInt(b, 10));
    const candidateMoves = [];

    sortedKeys.forEach((key) => {
      const line = pvLines[key];
      if (!line || !line.pv || line.pv === 'Book') return;
      const firstMove = line.pv.trim().split(/\s+/)[0];
      if (firstMove && firstMove.length >= 4) {
        const from = firstMove.substring(0, 2);
        const to = firstMove.substring(2, 4);
        const piece = game.get(from);
        if (piece && piece.color === currentTurn) {
          // Strictly verify the move is legal for the active board state
          const isLegal = legalMoves.some(m => m.from === from && m.to === to);
          if (isLegal && !candidateMoves.some(m => m.from === from && m.to === to)) {
            candidateMoves.push({ from, to });
          }
        }
      }
    });

    if (candidateMoves.length === 0) return null;

    // Helper to calculate square center coordinates (0..100)
    const getSquareCenter = (sq) => {
      const fileIdx = 'abcdefgh'.indexOf(sq[0]);
      const rankIdx = parseInt(sq[1], 10) - 1;
      if (fileIdx < 0 || rankIdx < 0 || rankIdx > 7) return null;
      const col = boardOrientation === 'white' ? fileIdx : 7 - fileIdx;
      const row = boardOrientation === 'white' ? 7 - rankIdx : rankIdx;
      return {
        x: col * 12.5 + 6.25,
        y: row * 12.5 + 6.25
      };
    };

    const renderedArrows = candidateMoves.slice(0, 3).map((move, index) => {
      const fromCenter = getSquareCenter(move.from);
      const toCenter = getSquareCenter(move.to);
      if (!fromCenter || !toCenter) return null;

      const config = arrowConfigs[index] || arrowConfigs[2];
      const dx = toCenter.x - fromCenter.x;
      const dy = toCenter.y - fromCenter.y;
      const dist = Math.hypot(dx, dy);
      if (dist < 0.1) return null;

      const ux = dx / dist;
      const uy = dy / dist;
      const nx = -uy;
      const ny = ux;

      // Shaft starts slightly away from piece center so piece is not obscured
      const startX = fromCenter.x + ux * 2.5;
      const startY = fromCenter.y + uy * 2.5;

      // Tip lands just 1.2% before target square center
      const tipX = toCenter.x - ux * 1.2;
      const tipY = toCenter.y - uy * 1.2;

      // Base of arrowhead
      const baseCenterX = tipX - ux * config.headLen;
      const baseCenterY = tipY - uy * config.headLen;

      // Arrowhead wings
      const halfWidth = config.headWidth / 2;
      const leftX = baseCenterX + nx * halfWidth;
      const leftY = baseCenterY + ny * halfWidth;
      const rightX = baseCenterX - nx * halfWidth;
      const rightY = baseCenterY - ny * halfWidth;

      // Shaft ends slightly inside arrowhead base so no gap
      const endX = baseCenterX + ux * 0.5;
      const endY = baseCenterY + uy * 0.5;

      return {
        key: `${move.from}-${move.to}-${index}`,
        startX,
        startY,
        endX,
        endY,
        tipX,
        tipY,
        leftX,
        leftY,
        rightX,
        rightY,
        color: config.color,
        strokeWidth: config.strokeWidth
      };
    }).filter(Boolean);

    if (renderedArrows.length === 0) return null;

    // Render in reverse so best move (index 0) is drawn last (on top)
    const reversed = [...renderedArrows].reverse();

    return (
      <svg
        style={{
          position: 'absolute',
          top: 0,
          left: 0,
          width: '100%',
          height: '100%',
          pointerEvents: 'none',
          zIndex: 10
        }}
        viewBox="0 0 100 100"
      >
        {reversed.map((arrow) => (
          <g key={arrow.key}>
            <line
              x1={arrow.startX}
              y1={arrow.startY}
              x2={arrow.endX}
              y2={arrow.endY}
              stroke={arrow.color}
              strokeWidth={arrow.strokeWidth}
              strokeLinecap="round"
            />
            <polygon
              points={`${arrow.tipX},${arrow.tipY} ${arrow.leftX},${arrow.leftY} ${arrow.rightX},${arrow.rightY}`}
              fill={arrow.color}
              stroke={arrow.color}
              strokeWidth="0.3"
              strokeLinejoin="round"
            />
          </g>
        ))}
      </svg>
    );
  }, [isSetupMode, showArrows, playMode, isDragging, pvLines, game, boardOrientation]);
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
          <div className="board-header">
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem', flexWrap: 'wrap' }}>
              <div className={`turn-indicator ${game.turn() === 'w' ? 'white' : 'black'}`} style={{ margin: 0 }}>
                {turnColor} to Move
              </div>
              {game.inCheck() && !game.isCheckmate() && (
                <span style={{ color: '#ef5350', fontWeight: 'bold', fontSize: '0.95rem' }}>
                  ⚠️ Check!
                </span>
              )}
              {game.isCheckmate() && (
                <div className="checkmate-status-badge">
                  ♚ Checkmate — {game.turn() === 'w' ? 'Black won' : 'White won'}
                </div>
              )}
              {game.isThreefoldRepetition() && (
                <div className="draw-status-badge">
                  🤝 Draw: Threefold Repetition
                </div>
              )}
              {game.isDrawByFiftyMoves() && (
                <div className="draw-status-badge">
                  🤝 Draw: 50-Move Rule
                </div>
              )}
              {game.isStalemate() && (
                <div className="draw-status-badge">
                  🤝 Draw: Stalemate
                </div>
              )}
              {game.isInsufficientMaterial() && (
                <div className="draw-status-badge">
                  🤝 Draw: Insufficient Material
                </div>
              )}
            </div>
            <div style={{ display: 'flex', gap: '0.5rem', alignItems: 'center' }}>
              <div className="volume-control" title={`Volume: ${Math.round((soundOn ? volume : 0) * 100)}%`}>
                <button 
                  className="volume-btn"
                  onClick={toggleSound}
                  title={soundOn ? 'Mute sound' : 'Unmute sound'}
                >
                  {!soundOn || volume === 0 ? '🔇' : (volume < 0.5 ? '🔉' : '🔊')}
                </button>
                <input 
                  type="range"
                  className="volume-slider"
                  min="0"
                  max="1"
                  step="0.05"
                  value={soundOn ? volume : 0}
                  onChange={(e) => handleVolumeChange(e.target.value)}
                  aria-label="Volume slider"
                />
              </div>
              <button 
                onClick={() => setBoardOrientation(prev => prev === 'white' ? 'black' : 'white')}
                style={{ background: 'rgba(255,255,255,0.1)', border: 'none', padding: '0.5rem 0.9rem', borderRadius: '8px', cursor: 'pointer', color: '#fff', fontSize: '0.9rem' }}
              >
                🔄 Flip Board
              </button>
            </div>
          </div>
        )}
        
        <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'stretch' }}>
          <EvalBar 
            score={
              pvLines['1'] 
                ? pvLines['1'].score 
                : (currentNode && currentNode.pvLines && currentNode.pvLines['1'] 
                    ? currentNode.pvLines['1'].score 
                    : "0.00")
            } 
            boardOrientation={boardOrientation} 
          />
          <div style={{ position: 'relative', flexGrow: 1 }}>
            <Chessboard 
              id="MainBoard"
              position={currentFen} 
              isDraggablePiece={isDraggablePiece}
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
              customArrows={[]}
            />
            {annotationOverlay}
            {engineArrowsOverlay}
          </div>
        </div>

        {!isSetupMode && (
          <div className="annotation-slot">
            {currentNode.annotation ? (
              <div style={{
                 width: '100%',
                 padding: '0.6rem 1rem', borderRadius: '8px',
                 background: 
                   currentNode.annotation === '📖' ? 'rgba(133, 169, 71, 0.2)' :
                   currentNode.annotation === '!!' ? 'rgba(27, 172, 166, 0.2)' : 
                   currentNode.annotation === '!'  ? 'rgba(92, 139, 176, 0.2)' :
                   currentNode.annotation === '★'  ? 'rgba(129, 182, 76, 0.2)' :
                   currentNode.annotation === '✓'  ? 'rgba(135, 171, 99, 0.2)' :
                   currentNode.annotation === '?!' ? 'rgba(245, 176, 65, 0.2)' :
                   currentNode.annotation === '?'  ? 'rgba(235, 152, 78, 0.2)' :
                   currentNode.annotation === '??' ? 'rgba(229, 57, 53, 0.2)' : 'rgba(255,255,255,0.1)',
                 border: `1px solid ${
                   currentNode.annotation === '📖' ? '#85a947' :
                   currentNode.annotation === '!!' ? '#1baca6' : 
                   currentNode.annotation === '!'  ? '#5c8bb0' :
                   currentNode.annotation === '★'  ? '#81b64c' :
                   currentNode.annotation === '✓'  ? '#87ab63' :
                   currentNode.annotation === '?!' ? '#f5b041' :
                   currentNode.annotation === '?'  ? '#eb984e' :
                   currentNode.annotation === '??' ? '#e53935' : 'rgba(255,255,255,0.2)'
                 }`,
                 textAlign: 'center', fontWeight: 'bold', fontSize: '1.05rem',
                 display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '0.6rem'
              }}>
                 <span style={{
                   fontSize: '1.25rem',
                   color: 
                     currentNode.annotation === '📖' ? '#85a947' :
                     currentNode.annotation === '!!' ? '#1baca6' : 
                     currentNode.annotation === '!'  ? '#5c8bb0' :
                     currentNode.annotation === '★'  ? '#81b64c' :
                     currentNode.annotation === '✓'  ? '#87ab63' :
                     currentNode.annotation === '?!' ? '#f5b041' :
                     currentNode.annotation === '?'  ? '#eb984e' : '#e53935'
                 }}>
                   {currentNode.annotation}
                 </span>
                 <span>{currentNode.san}</span>
                 <span style={{ color: '#aaa', fontWeight: 'normal' }}>—</span>
                 <span>{getAnnotationDescription(currentNode.annotation)}</span>
              </div>
            ) : null}
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
          <p>please don't use private mode</p>

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

        {playMode !== 'Analysis' && (
          <div className="glass-panel" style={{ background: 'rgba(27, 172, 166, 0.08)', border: '1px solid rgba(27, 172, 166, 0.3)', padding: '1rem', borderRadius: '12px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem', color: '#1baca6', fontWeight: 'bold', fontSize: '1.05rem' }}>
              <span>⚡</span> Game Mode: Maximum Strength
            </div>
            <p style={{ margin: '0.5rem 0 0', fontSize: '0.85rem', color: '#ddd', lineHeight: '1.5' }}>
              Engine playing at full strength: <b>{maxHardwareThreads} threads</b>, depth <b>unlimited</b>, Polyglot opening book & Syzygy tablebases.
            </p>
          </div>
        )}

        {playMode === 'Analysis' && (
          <div className="glass-panel">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1rem', flexWrap: 'wrap', gap: '1rem' }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                <div style={{ display: 'flex', alignItems: 'baseline', gap: '0.5rem' }}>
                    <h2 style={{ margin: 0 }}>Position Analysis</h2>
                    <span style={{ fontSize: '0.8rem', color: '#aaa', fontStyle: 'italic' }}>
                        (depth {analysisDepth})
                    </span>
                </div>
                <div style={{ display: 'flex', gap: '0.5rem' }}>
                    <button 
                      onClick={startFullGameAnalysis} 
                      disabled={isAnalyzingGame || Object.keys(nodes).length <= 1}
                      className="modern-button"
                      style={{ padding: '6px 14px', fontSize: '0.9rem', width: 'fit-content', background: 'rgba(27, 172, 166, 0.2)', border: '1px solid #1baca6' }}
                    >
                      Full Game Analysis
                    </button>
                    {isAnalyzingGame && (
                        <button 
                          onClick={cancelFullGameAnalysis} 
                          className="modern-button"
                          style={{ padding: '6px 14px', fontSize: '0.9rem', width: 'fit-content', background: 'rgba(229, 57, 53, 0.2)', border: '1px solid #e53935' }}
                        >
                          Cancel
                        </button>
                    )}
                </div>
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '0.5rem', background: 'rgba(0,0,0,0.2)', padding: '0.6rem 0.8rem', borderRadius: '8px' }}>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: '1rem', flexWrap: 'wrap' }}>
                <span style={{ fontSize: '0.95rem', fontWeight: 'bold', color: '#1baca6', display: 'flex', alignItems: 'center', gap: '0.4rem' }}>
                  Depth: <span style={{ fontSize: '1.1rem', color: '#fff' }}>{currentDepth}</span> / {analysisDepth}
                </span>
                <div style={{ display: 'flex', alignItems: 'center', gap: '0.6rem', flexWrap: 'wrap' }}>
                  <input 
                    type="range" 
                    min="6" 
                    max="35" 
                    value={analysisDepth} 
                    onChange={(e) => {
                      const d = parseInt(e.target.value, 10);
                      setAnalysisDepth(d);
                      if (wsRef.current && wsRef.current.readyState === 1) {
                        requestAnalysis(currentFenRef.current, d);
                      }
                    }}
                    style={{ width: '100px', cursor: 'pointer' }}
                  />
                  <div className="btn-group">
                    {[10, 15, 20, 25, 30].map(preset => (
                      <button 
                        key={preset}
                        className={analysisDepth === preset ? 'active' : ''}
                        onClick={() => {
                          setAnalysisDepth(preset);
                          if (wsRef.current && wsRef.current.readyState === 1) {
                            requestAnalysis(currentFenRef.current, preset);
                          }
                        }}
                        style={{ padding: '0.2rem 0.45rem', fontSize: '0.78rem' }}
                      >
                        {preset}
                      </button>
                    ))}
                  </div>
                </div>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: '0.82rem', color: '#aaa', marginTop: '0.2rem' }}>
                <span>Multi-PV:</span>
                <div className="btn-group">
                  <button className={multiPvCount === 1 ? 'active' : ''} onClick={() => setMultiPvCount(1)}>1</button>
                  <button className={multiPvCount === 3 ? 'active' : ''} onClick={() => setMultiPvCount(3)}>3</button>
                  <button className={multiPvCount === 5 ? 'active' : ''} onClick={() => setMultiPvCount(5)}>5</button>
                </div>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: '0.82rem', color: '#aaa' }}>
                <span>Threads:</span>
                <div className="btn-group">
                  <button className={engineThreads === 1 ? 'active' : ''} onClick={() => setEngineThreads(1)}>1</button>
                  <button className={engineThreads === 2 ? 'active' : ''} onClick={() => setEngineThreads(2)}>2</button>
                  <button className={engineThreads === 4 ? 'active' : ''} onClick={() => setEngineThreads(4)}>4</button>
                </div>
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
          <button 
            className="modern-button" 
            style={{ marginTop: '20px' }} 
            onClick={cancelFullGameAnalysis}
          >
            Cancel
          </button>
        </div>
      )}
      {!engineReady && (
        <div style={{
          position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
          background: 'rgba(0,0,0,0.9)', display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center', zIndex: 10000, color: 'white'
        }}>
          <h2>Downloading Engine...</h2>
          <p style={{ color: '#aaa', marginTop: '10px' }}>Loading WebAssembly components (approx. 21 MB)</p>
          <div className="spinner" style={{ 
            marginTop: '20px', width: '40px', height: '40px', 
            border: '4px solid rgba(255,255,255,0.2)', 
            borderTop: '4px solid #1baca6', borderRadius: '50%', 
            animation: 'spin 1s linear infinite' 
          }} />
          <style>
            {`
              @keyframes spin {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
              }
            `}
          </style>
        </div>
      )}
    </div>
  );
}

export default App;

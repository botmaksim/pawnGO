#!/bin/bash

# Compute script and project directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

PAWNGO_CMD="$PROJECT_DIR/engine/build/pawnGO"
PAWNGO_DIR="$PROJECT_DIR/engine/build"
CUTECHESS_CLI="/home/maksim/cutechess-cli/cutechess-cli"
LOG_FILE="$SCRIPT_DIR/tournament.log"
GAMES_DIR="$SCRIPT_DIR/games"

# Arguments
SF_LEVEL=${1:-15}   # Default to Stockfish level 15 (GM level ~2500 Elo)
TC=${2:-"5+0.1"}   # Default fast time control 5s + 0.1s increment for swift evaluation
ROUNDS=${3:-5}     # 5 rounds of 2 games = 10 games total
CONCURRENCY=${4:-2}

# Setup
mkdir -p "$GAMES_DIR"
rm -f "$GAMES_DIR"/*

echo "==========================================================" | tee "$LOG_FILE"
echo "Starting Tournament: pawnGO vs Stockfish Level $SF_LEVEL" | tee -a "$LOG_FILE"
echo "Time Control: $TC | Rounds: $ROUNDS ($(($ROUNDS * 2)) games) | Concurrency: $CONCURRENCY" | tee -a "$LOG_FILE"
echo "Engine Binary: $PAWNGO_CMD" | tee -a "$LOG_FILE"
echo "==========================================================" | tee -a "$LOG_FILE"

if [ "$SF_LEVEL" = "full" ] || [ "$SF_LEVEL" = "20" ]; then
    SF_ENGINE_ARGS=(-engine name="Stockfish_Full_Power_17.1" cmd=stockfish "option.Skill Level=20" "option.UCI_LimitStrength=false")
    echo "Using FULL UNCONSTRAINED Stockfish 17.1 (Maximum Strength ~3600+ Elo)" | tee -a "$LOG_FILE"
elif [ "$SF_LEVEL" -gt 100 ] 2>/dev/null; then
    SF_ENGINE_ARGS=(-engine name="Stockfish_Elo_$SF_LEVEL" cmd=stockfish "option.UCI_LimitStrength=true" "option.UCI_Elo=$SF_LEVEL")
    echo "Using Calibrated Stockfish Strength: UCI_Elo = $SF_LEVEL" | tee -a "$LOG_FILE"
else
    SF_ENGINE_ARGS=(-engine name="Stockfish_Level_$SF_LEVEL" cmd=stockfish "option.Skill Level=$SF_LEVEL")
    echo "Using Stockfish Skill Level: $SF_LEVEL" | tee -a "$LOG_FILE"
fi

SYZYGY_OPTS=()
if [ -d "/home/maksim/CV/pawnGO/engine/3-4-5" ]; then
    SYZYGY_OPTS=("option.SyzygyPath=/home/maksim/CV/pawnGO/engine/3-4-5")
    SF_ENGINE_ARGS+=("${SYZYGY_OPTS[@]}")
    echo "Syzygy 3-4-5 tablebases enabled for both engines." | tee -a "$LOG_FILE"
fi

OPENING_ARG=()
if [ -f "$SCRIPT_DIR/openings.pgn" ]; then
    OPENING_ARG=(-openings file="$SCRIPT_DIR/openings.pgn" format=pgn order=random)
fi

TB_ADJUDICATION=()
if [ -d "/home/maksim/CV/pawnGO/engine/3-4-5" ]; then
    TB_ADJUDICATION=(-tb /home/maksim/CV/pawnGO/engine/3-4-5 -tbpieces 5)
fi

$CUTECHESS_CLI \
    -engine name=pawnGO cmd="$PAWNGO_CMD" dir="$PAWNGO_DIR" option.MultiPV=1 "${SYZYGY_OPTS[@]}" \
    "${SF_ENGINE_ARGS[@]}" \
    -each proto=uci tc=$TC \
    "${OPENING_ARG[@]}" \
    "${TB_ADJUDICATION[@]}" \
    -draw movenumber=40 movecount=6 score=10 \
    -resign movecount=4 score=600 twosided=true \
    -ratinginterval 1 \
    -games 2 -rounds $ROUNDS -concurrency $CONCURRENCY \
    -pgnout "$GAMES_DIR/match.pgn" \
    2>&1 | tee -a "$LOG_FILE"

echo "==========================================================" | tee -a "$LOG_FILE"
echo "Tournament Completed! Games saved to $GAMES_DIR/match.pgn" | tee -a "$LOG_FILE"
echo "Invoking Full Stockfish 17.1 Referee for match verification..." | tee -a "$LOG_FILE"
echo "==========================================================" | tee -a "$LOG_FILE"

if [ -f "$GAMES_DIR/match.pgn" ]; then
    python3 "$SCRIPT_DIR/referee_stockfish.py" "$GAMES_DIR/match.pgn" 12 2>&1 | tee -a "$LOG_FILE"
fi

#!/bin/bash

# Configuration
PAWNGO_CMD="/home/maksim/Programming/Projects/pawnGO/engine/build/pawnGO"
CUTECHESS_CLI="/home/maksim/cutechess-cli/cutechess-cli"
LOG_FILE="tournament.log"
GAMES_DIR="games"

# Arguments
SF_LEVEL=${1:-15}   # Default to Stockfish level 5 if not provided
TC=${2:-"10+0.5"}  # Default to 10s + 0.5s increment if not provided

# Setup
mkdir -p "$GAMES_DIR"
echo "Starting Tournament: pawnGO vs Stockfish Level $SF_LEVEL" > "$LOG_FILE"
echo "Time Control: $TC" >> "$LOG_FILE"
echo "=================================" >> "$LOG_FILE"

for i in {1..10}; do
    echo "Running Game $i..." | tee -a "$LOG_FILE"
    
    # Alternate colors
    if [ $((i % 2)) -eq 1 ]; then
        # Game 1, 3, 5... pawnGO is White
        $CUTECHESS_CLI \
            -engine name=pawnGO cmd="$PAWNGO_CMD" dir="/home/maksim/Programming/Projects/pawnGO/engine/build" option.MultiPV=1 \
            -engine name="Stockfish_Level_$SF_LEVEL" cmd=stockfish "option.Skill Level=$SF_LEVEL" \
            -each proto=uci tc=$TC \
            -openings file=/home/maksim/Programming/Projects/pawnGO/engine/scripts/openings.pgn format=pgn order=random \
            -games 1 -rounds 1 \
            -pgnout "$GAMES_DIR/${i}.pgn" \
            >> "$LOG_FILE" 2>&1
    else
        # Game 2, 4, 6... Stockfish is White
        $CUTECHESS_CLI \
            -engine name="Stockfish_Level_$SF_LEVEL" cmd=stockfish "option.Skill Level=$SF_LEVEL" \
            -engine name=pawnGO cmd="$PAWNGO_CMD" dir="/home/maksim/Programming/Projects/pawnGO/engine/build" option.MultiPV=1 \
            -each proto=uci tc=$TC \
            -openings file=/home/maksim/Programming/Projects/pawnGO/engine/scripts/openings.pgn format=pgn order=random \
            -games 1 -rounds 1 \
            -pgnout "$GAMES_DIR/${i}.pgn" \
            >> "$LOG_FILE" 2>&1
    fi
        
    echo "Game $i finished. Saved to $GAMES_DIR/${i}.pgn" | tee -a "$LOG_FILE"
done

echo "=================================" >> "$LOG_FILE"
echo "Tournament Completed!" | tee -a "$LOG_FILE"

#!/bin/bash

# Configuration
PAWNGO_CMD="/home/maksim/Programming/Projects/pawnGO/engine/build/pawnGO"
CUTECHESS_CLI="/home/maksim/cutechess-cli/cutechess-cli"
LOG_FILE="tournament.log"
GAMES_DIR="games"

# Arguments
SF_LEVEL=${1:-15}   # Default to Stockfish level 15 if not provided
TC=${2:-"10+0.5"}  # Default to 10s + 0.5s increment if not provided
ROUNDS=${3:-5}     # 5 rounds of 2 games = 10 games total
CONCURRENCY=${4:-2}

# Setup
rm -rf "$GAMES_DIR"/*
mkdir -p "$GAMES_DIR"

echo "Starting Tournament: pawnGO vs Stockfish Level $SF_LEVEL" > "$LOG_FILE"
echo "Time Control: $TC" >> "$LOG_FILE"
echo "=================================" >> "$LOG_FILE"

echo "Running Tournament with $ROUNDS rounds (2 games per round = $(($ROUNDS * 2)) games)..." | tee -a "$LOG_FILE"

$CUTECHESS_CLI \
    -engine name=pawnGO cmd="$PAWNGO_CMD" dir="/home/maksim/Programming/Projects/pawnGO/engine/build" option.MultiPV=1 \
    -engine name="Stockfish_Level_$SF_LEVEL" cmd=stockfish "option.Skill Level=$SF_LEVEL" \
    -each proto=uci tc=$TC \
    -games 2 -rounds $ROUNDS -concurrency $CONCURRENCY \
    -pgnout "$GAMES_DIR/match.pgn" \
    >> "$LOG_FILE" 2>&1

echo "=================================" >> "$LOG_FILE"
echo "Tournament Completed! Games saved nicely to $GAMES_DIR/match.pgn" | tee -a "$LOG_FILE"

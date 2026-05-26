#!/bin/bash

# Configuration
PAWNGO_CMD="/home/maksim/Programming/Projects/pawnGO/engine/build/pawnGO"
CUTECHESS_CLI="/home/maksim/cutechess-cli/cutechess-cli"
RESULTS_FILE="gauntlet_results.txt"

# Clear previous results
echo "pawnGO Massive Gauntlet Results" > $RESULTS_FILE
echo "=================================" >> $RESULTS_FILE

# Time controls
time_controls=("10+0.5" "10+1.0" "10+2.0")

# Thread settings
thread_settings=(1 2)

for threads in "${thread_settings[@]}"; do
    echo "=================================" | tee -a $RESULTS_FILE
    echo "Starting Gauntlet with $threads THREADS" | tee -a $RESULTS_FILE
    echo "=================================" | tee -a $RESULTS_FILE
    
    # Configure pawnGO thread option if supported, else concurrency handles it for independent games
    pawngo_opt=""
    if [ $threads -gt 1 ]; then
        pawngo_opt="option.MultiPV=$threads"
    fi
    
    for level in {1..10}; do
        echo "Testing against Stockfish Level $level..." | tee -a $RESULTS_FILE
        
        for tc in "${time_controls[@]}"; do
            echo "  Time Control: $tc" | tee -a $RESULTS_FILE
            
            # Run cutechess-cli
            $CUTECHESS_CLI \
                -engine name=pawnGO cmd=$PAWNGO_CMD $pawngo_opt \
                -engine name=stockfish$level cmd=stockfish option.Skill\ Level=$level option.Threads=$threads \
                -each proto=uci tc=$tc \
                -games 10 -rounds 1 -repeat \
                -concurrency 4 -ratinginterval 1 \
                > tmp_result.txt
            
            # Extract score
            score_line=$(grep "Score of pawnGO vs stockfish$level" tmp_result.txt | tail -1)
            elo_line=$(grep "Elo difference:" tmp_result.txt | tail -1)
            
            echo "    $score_line" | tee -a $RESULTS_FILE
            echo "    $elo_line" | tee -a $RESULTS_FILE
        done
        echo "" | tee -a $RESULTS_FILE
    done
done

rm tmp_result.txt
echo "Gauntlet Finished!" | tee -a $RESULTS_FILE

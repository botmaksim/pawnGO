#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include "bitboard.h"
#include "movegen.h"
#include "board.h"
#include "search.h"
#include "zobrist.h"
#include "tt.h"
#include "evaluation.h"

std::ofstream engine_logger("engine.log", std::ios_base::app);

void log(const std::string& msg) {
    if (engine_logger.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::string t = std::ctime(&now_time);
        t.pop_back(); // remove newline
        engine_logger << "[" << t << "] " << msg << std::endl;
    }
}

void uciLoop() {
    std::string line;
    std::thread search_thread;

    log("UCI Loop started.");

    while (std::getline(std::cin, line)) {
        log("IN: " + line);

        if (line == "quit") {
            Search::stopped = true;
            Search::stop_threads();
            if (search_thread.joinable()) search_thread.join();
            break;
        } else if (line == "stop") {
            Search::stopped = true;
            if (search_thread.joinable()) search_thread.join();
        } else if (line == "uci") {
            std::cout << "id name pawnGO 1.0" << std::endl;
            std::cout << "id author Maksim" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (line.find("setoption name MultiPV value") != std::string::npos) {
            size_t val_idx = line.find("value ");
            if (val_idx != std::string::npos) {
                Search::multi_pv = std::stoi(line.substr(val_idx + 6));
                log("Multi-PV set to " + std::to_string(Search::multi_pv));
            }
        } else if (line.find("position") == 0) {
            std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            size_t fen_idx = line.find("fen ");
            if (fen_idx != std::string::npos) {
                std::string fen = line.substr(fen_idx + 4);
                size_t moves_idx = fen.find(" moves");
                if (moves_idx != std::string::npos) fen = fen.substr(0, moves_idx);
                Bitboard::parse_fen(fen);
            } else if (line.find("startpos") != std::string::npos) {
                Bitboard::parse_fen(startpos);
            }
        } else if (line.find("perft ") == 0) {
            int depth = std::stoi(line.substr(6));
            Board::perft_test(depth);
        } else if (line == "d") {
            Bitboard::print_board();
        } else if (line.find("go") == 0) {
            // Stop any ongoing search first
            Search::stopped = true;
            if (search_thread.joinable()) search_thread.join();

            int depth = 6;
            size_t depth_idx = line.find("depth ");
            if (depth_idx != std::string::npos) {
                depth = std::stoi(line.substr(depth_idx + 6));
            }
            
            // Launch search in a background thread so UCI remains responsive
            search_thread = std::thread(Search::search_position, depth);
            log("Search started in background thread for depth " + std::to_string(depth));
        } else if (line == "explain") {
            std::string expl = Evaluation::shadow_evaluate();
            log("Explain called, output: " + expl);
            std::cout << "info string Explain: " << expl << std::endl;
        }
    }
}

int main() {
    // Disable buffering for standard I/O streams for immediate communication
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    log("Engine started.");

    MoveGen::init_all();
    Evaluation::init_nnue("../src/nnue/nn-62ef826d1a6d.nnue");
    Search::init_threads(4);

    uciLoop();

    log("Engine shutting down cleanly.");
    return 0;
}

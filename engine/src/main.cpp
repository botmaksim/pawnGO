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
#include "syzygy/tbprobe.h"

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
            size_t moves_idx = line.find(" moves");
            
            if (fen_idx != std::string::npos) {
                std::string fen = line.substr(fen_idx + 4);
                if (moves_idx != std::string::npos) fen = line.substr(fen_idx + 4, moves_idx - (fen_idx + 4));
                Bitboard::parse_fen(fen);
            } else if (line.find("startpos") != std::string::npos) {
                Bitboard::parse_fen(startpos);
            }
            
            if (moves_idx != std::string::npos) {
                std::string moves_str = line.substr(moves_idx + 6);
                std::stringstream ss(moves_str);
                std::string move_str;
                while (ss >> move_str) {
                    MoveList move_list;
                    MoveGen::generate_moves(move_list);
                    Move matched_move = 0;
                    for (int i = 0; i < move_list.count; i++) {
                        Move m = move_list.moves[i];
                        int src = GET_MOVE_SOURCE(m);
                        int tgt = GET_MOVE_TARGET(m);
                        int promoted = GET_MOVE_PROMOTED(m);
                        std::string m_str = "";
                        m_str += char((src % 8) + 'a');
                        m_str += char((src / 8) + '1');
                        m_str += char((tgt % 8) + 'a');
                        m_str += char((tgt / 8) + '1');
                        if (promoted) {
                            int p_type = promoted % 6;
                            if (p_type == 1) m_str += "n";
                            if (p_type == 2) m_str += "b";
                            if (p_type == 3) m_str += "r";
                            if (p_type == 4) m_str += "q";
                        }
                        if (m_str == move_str) {
                            matched_move = m;
                            break;
                        }
                    }
                    if (matched_move != 0) {
                        Board::make_move(matched_move, 0);
                        U64 gen_hash = Zobrist::generate_hash_key();
                        if (Bitboard::hash_key != gen_hash) {
                            log("HASH MISMATCH! Incremental: " + std::to_string(Bitboard::hash_key) + " Generated: " + std::to_string(gen_hash));
                            std::cout << "info string HASH MISMATCH!" << std::endl;
                        }
                    }
                }
            }
        } else if (line.find("perft ") == 0) {
            int depth = std::stoi(line.substr(6));
            Board::perft_test(depth);
        } else if (line == "d") {
            Bitboard::print_board();
        } else if (line == "eval") {
            std::cout << "Eval (evaluate): " << Evaluation::evaluate() << std::endl;
            std::cout << "Eval (incremental 0): " << Evaluation::evaluate_incremental(0) << std::endl;
            if (Evaluation::use_nnue) {
                std::cout << "Eval (incremental 1): " << Evaluation::evaluate_incremental(1) << std::endl;
            }
        } else if (line.find("go") == 0) {
            // Stop any ongoing search first
            Search::stopped = true;
            if (search_thread.joinable()) search_thread.join();

            int depth = 100;
            size_t depth_idx = line.find("depth ");
            if (depth_idx != std::string::npos) {
                depth = std::stoi(line.substr(depth_idx + 6));
            }
            
            long long time_for_move = -1;
            size_t wtime_idx = line.find("wtime ");
            size_t btime_idx = line.find("btime ");
            size_t winc_idx = line.find("winc ");
            size_t binc_idx = line.find("binc ");
            
            if (wtime_idx != std::string::npos && btime_idx != std::string::npos) {
                long long wtime = std::stoll(line.substr(wtime_idx + 6));
                long long btime = std::stoll(line.substr(btime_idx + 6));
                long long winc = (winc_idx != std::string::npos) ? std::stoll(line.substr(winc_idx + 5)) : 0;
                long long binc = (binc_idx != std::string::npos) ? std::stoll(line.substr(binc_idx + 5)) : 0;
                
                long long time_left = (Bitboard::side == WHITE) ? wtime : btime;
                long long increment = (Bitboard::side == WHITE) ? winc : binc;
                
                time_for_move = time_left / 30 + increment / 2;
                if (time_for_move < 50) time_for_move = 50;
            }
            
            // Reset stopped flag before launching!
            Search::stopped = false;
            
            static int search_id = 0;
            search_id++;
            int current_search_id = search_id;

            // Capture the current main thread's board state
            U64 t_pieceBB[12];
            for (int i = 0; i < 12; i++) t_pieceBB[i] = Bitboard::pieceBB[i];
            U64 t_occ[3];
            for (int i = 0; i < 3; i++) t_occ[i] = Bitboard::occupancies[i];
            int t_side = Bitboard::side;
            int t_ep = Bitboard::enpassant;
            int t_cas = Bitboard::castle;
            U64 t_hash = Bitboard::hash_key;

            // Launch search in a background thread so UCI remains responsive
            search_thread = std::thread([=]() {
                for (int i = 0; i < 12; i++) Bitboard::pieceBB[i] = t_pieceBB[i];
                for (int i = 0; i < 3; i++) Bitboard::occupancies[i] = t_occ[i];
                Bitboard::side = t_side;
                Bitboard::enpassant = t_ep;
                Bitboard::castle = t_cas;
                Bitboard::hash_key = t_hash;
                Search::search_position(depth);
            });
            
            if (time_for_move != -1 && depth_idx == std::string::npos) {
                // Time-based search
                std::thread timer_thread([time_for_move, current_search_id]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(time_for_move));
                    if (search_id == current_search_id) {
                        Search::stopped = true;
                    }
                });
                timer_thread.detach();
                log("Search started in background thread for " + std::to_string(time_for_move) + " ms");
            } else {
                log("Search started in background thread for depth " + std::to_string(depth));
            }
        } else if (line.find("setoption name SyzygyPath value ") != std::string::npos) {
            std::string path = line.substr(32);
            if (tb_init(path.c_str())) {
                log("Syzygy Tablebases initialized from " + path + " (Max pieces: " + std::to_string(TB_LARGEST) + ")");
            } else {
                log("Failed to initialize Syzygy Tablebases from " + path);
            }
        }
    }

    if (search_thread.joinable()) {
        Search::stopped = true;
        search_thread.join();
    }
    Search::stop_threads();
}

int main() {
    // Disable buffering for standard I/O streams for immediate communication
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    log("Engine started.");

    MoveGen::init_all();
    Zobrist::init();
    TT::init(64);
    Search::init_lmr_table();
    Evaluation::init_nnue("../src/nnue/nn-62ef826d1a6d.nnue");
    if (tb_init("../3-4-5")) {
        log("Syzygy Tablebases automatically initialized from ../3-4-5 (Max pieces: " + std::to_string(TB_LARGEST) + ")");
    }

    Search::init_threads(4);

    uciLoop();

    log("Engine shutting down cleanly.");
    return 0;
}

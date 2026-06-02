#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <random>
#include <chrono>
#include <array>
#include "bitboard.h"
#include "movegen.h"
#include "board.h"
#include "search.h"
#include "zobrist.h"
#include "tt.h"
#include "evaluation.h"
#include "syzygy/tbprobe.h"

#include "polyglot.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

std::ofstream engine_logger("engine.log", std::ios_base::app);

bool UseOwnBook = true;
#ifdef __EMSCRIPTEN__
std::string BookFile = "perfect/Perfect2023.bin";
#else
std::string BookFile = "../perfect/Perfect2023.bin";
#endif
int game_ply = 0;
BoardState pos;

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
    int wtime = 0;

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
            std::cout << "option name Threads type spin default 4 min 1 max 256" << std::endl;
            std::cout << "option name MultiPV type spin default 1 min 1 max 500" << std::endl;
            std::cout << "option name OwnBook type check default true" << std::endl;
            std::cout << "option name BookFile type string default " << BookFile << std::endl;
            std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (line.find("setoption name OwnBook value ") != std::string::npos) {
            std::string val = line.substr(29);
            UseOwnBook = (val == "true" || val == "1");
            log("OwnBook set to " + std::to_string(UseOwnBook));
        } else if (line.find("setoption name BookFile value ") != std::string::npos) {
            BookFile = line.substr(30);
            log("BookFile set to " + BookFile);
        } else if (line.find("setoption name MultiPV value") != std::string::npos) {
            size_t val_idx = line.find("value ");
            if (val_idx != std::string::npos) {
                Search::multi_pv = std::stoi(line.substr(val_idx + 6));
                log("Multi-PV set to " + std::to_string(Search::multi_pv));
            }
        } else if (line.find("setoption name Threads value") != std::string::npos) {
            size_t val_idx = line.find("value ");
            if (val_idx != std::string::npos) {
                int threads = std::stoi(line.substr(val_idx + 6));
                Search::init_threads(threads);
                log("Threads set to " + std::to_string(threads));
            }
        } else if (line.find("position") == 0) {
            std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            size_t fen_idx = line.find("fen ");
            size_t moves_idx = line.find(" moves");
            
            if (fen_idx != std::string::npos) {
                std::string fen = line.substr(fen_idx + 4);
                if (moves_idx != std::string::npos) fen = line.substr(fen_idx + 4, moves_idx - (fen_idx + 4));
                Bitboard::parse_fen(pos, fen);
                game_ply = 0; // If custom FEN, assume ply 0 or handle accordingly. Assuming 0 is safe for now.
            } else if (line.find("startpos") != std::string::npos) {
                Bitboard::parse_fen(pos, startpos);
                game_ply = 0;
            }
            
            if (moves_idx != std::string::npos) {
                std::string moves_str = line.substr(moves_idx + 6);
                std::stringstream ss(moves_str);
                std::string move_str;
                game_ply = 0; // Reset game ply when parsing moves
                while (ss >> move_str) {
                    game_ply++;
                    MoveList move_list;
                    MoveGen::generate_moves(pos, move_list);
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
                        Board::make_move(pos, matched_move, 0);
                    }
                }
            }
        } else if (line.find("perft ") == 0) {
            int depth = std::stoi(line.substr(6));
            Board::perft_test(pos, depth);
        } else if (line == "d") {
            Bitboard::print_board(pos);
        } else if (line == "test_q") {
            MoveList ml;
            MoveGen::generate_moves(pos, ml, true);
            std::cout << "Captures count: " << ml.count << std::endl;

        } else if (line == "eval") {
            Evaluation::allocate_nnue_stack();
            std::cout << "Eval (evaluate): " << Evaluation::evaluate(pos) << std::endl;
            std::cout << "Eval (incremental 0): " << Evaluation::evaluate_incremental(pos, 0) << std::endl;
            if (Evaluation::use_nnue) {
                std::cout << "Eval (incremental 1): " << Evaluation::evaluate_incremental(pos, 1) << std::endl;
            }
        } else if (line.find("go perft ") == 0) {
            int depth = std::stoi(line.substr(9));
            Board::perft_test(pos, depth);
        } else if (line.find("go") == 0) {
            bool is_game = (line.find("wtime") != std::string::npos || line.find("btime") != std::string::npos || line.find("movetime") != std::string::npos);
            if (UseOwnBook && !BookFile.empty() && is_game && game_ply <= 4) {
                auto b_moves = Polyglot::get_all_book_moves(pos, BookFile);
                Move book_move = 0;
                if (!b_moves.empty()) {
                    std::sort(b_moves.begin(), b_moves.end(), [](const std::pair<Move, int>& a, const std::pair<Move, int>& b) {
                        return a.second > b.second;
                    });
                    
                    int best_weight = b_moves[0].second;
                    std::vector<Move> best_moves;
                    for (auto& bm : b_moves) {
                        if (bm.second >= best_weight / 2 || best_moves.size() < 3) {
                            best_moves.push_back(bm.first);
                        } else {
                            break;
                        }
                    }
                    
                    if (!best_moves.empty()) {
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_int_distribution<> dis(0, best_moves.size() - 1);
                        book_move = best_moves[dis(gen)];
                    }
                }
                
                if (book_move != 0) {
                    int src = GET_MOVE_SOURCE(book_move);
                    int tgt = GET_MOVE_TARGET(book_move);
                    int promoted = GET_MOVE_PROMOTED(book_move);
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
                    std::cout << "info string Book move" << std::endl;
                    std::cout << "bestmove " << m_str << std::endl;
                    log("Book move played automatically: " + m_str);
                    continue; // Skip the search
                }
            }
            
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
            size_t movetime_idx = line.find("movetime ");
            
            if (movetime_idx != std::string::npos) {
                time_for_move = std::stoll(line.substr(movetime_idx + 9));
            } else if (wtime_idx != std::string::npos && btime_idx != std::string::npos) {
                long long wtime = std::stoll(line.substr(wtime_idx + 6));
                long long btime = std::stoll(line.substr(btime_idx + 6));
                long long winc = (winc_idx != std::string::npos) ? std::stoll(line.substr(winc_idx + 5)) : 0;
                long long binc = (binc_idx != std::string::npos) ? std::stoll(line.substr(binc_idx + 5)) : 0;
                
                long long time_left = (pos.side == WHITE) ? wtime : btime;
                long long increment = (pos.side == WHITE) ? winc : binc;
                
                time_for_move = time_left / 30 + increment / 2;
                if (time_for_move < 50) time_for_move = 50;
            }
            
            // Reset stopped flag before launching!
            Search::stopped = false;
            
            Search::search_id.fetch_add(1, std::memory_order_relaxed);
            int current_search_id = Search::search_id.load(std::memory_order_relaxed);

            // Capture the current main thread's board state
            std::array<U64, 12> t_pieceBB;
            for (int i = 0; i < 12; i++) t_pieceBB[i] = pos.pieceBB[i];
            std::array<U64, 3> t_occ;
            for (int i = 0; i < 3; i++) t_occ[i] = pos.occupancies[i];
            int t_side = pos.side;
            int t_ep = pos.enpassant;
            int t_cas = pos.castle;
            U64 t_hash = pos.hash_key;

            // Launch search in a background thread so UCI remains responsive
            search_thread = std::thread([=]() {
                BoardState thread_pos;
                for (int i = 0; i < 12; i++) thread_pos.pieceBB[i] = t_pieceBB[i];
                for (int i = 0; i < 3; i++) thread_pos.occupancies[i] = t_occ[i];
                thread_pos.side = t_side;
                thread_pos.enpassant = t_ep;
                thread_pos.castle = t_cas;
                thread_pos.hash_key = t_hash;
                Search::thread_search_id = current_search_id;
                Search::search_position(thread_pos, depth, current_search_id, (depth_idx == std::string::npos) ? time_for_move : -1);
            });
            
            if (time_for_move != -1 && depth_idx == std::string::npos) {
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
#ifdef __EMSCRIPTEN__
        search_thread.detach();
#else
        search_thread.join();
#endif
    }
    Search::stop_threads();
}

#ifdef __EMSCRIPTEN__
#include <mutex>
#include <condition_variable>

std::thread wasm_search_thread_instance;
std::mutex wasm_search_mtx;
bool wasm_search_ready = false;
bool wasm_search_quit = false;

BoardState wasm_thread_pos;
int wasm_thread_depth = 15;
int wasm_thread_search_id = 0;
long long wasm_thread_time_for_move = -1;

void wasm_search_worker() {
    while (true) {
        bool ready;
        bool quit;
        {
            std::lock_guard<std::mutex> lock(wasm_search_mtx);
            ready = wasm_search_ready;
            quit = wasm_search_quit;
        }
        if (quit) break;
        if (!ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        
        {
            std::lock_guard<std::mutex> lock(wasm_search_mtx);
            wasm_search_ready = false;
        }
        
        BoardState p = wasm_thread_pos;
        int d = wasm_thread_depth;
        int s_id = wasm_thread_search_id;
        long long t_ms = wasm_thread_time_for_move;

        Search::thread_search_id = s_id;
        Search::search_position(p, d, s_id, t_ms);
    }
}
#endif

void execute_uci_command(const std::string& line, std::thread& search_thread) {
    // We basically copy the inside of the while loop into here for the Wasm API
    log("IN: " + line);

    if (line == "quit") {
        Search::stopped = true;
        Search::stop_threads();
#ifdef __EMSCRIPTEN__
        {
            std::lock_guard<std::mutex> lock(wasm_search_mtx);
            wasm_search_quit = true;
        }
#else
        if (search_thread.joinable()) search_thread.join();
#endif
    } else if (line == "stop") {
        Search::stopped = true;
#ifndef __EMSCRIPTEN__
        if (search_thread.joinable()) search_thread.join();
#endif
    } else if (line == "uci") {
        std::cout << "id name pawnGO 1.0" << std::endl;
        std::cout << "id author Maksim" << std::endl;
        std::cout << "option name Threads type spin default 4 min 1 max 256" << std::endl;
        std::cout << "option name MultiPV type spin default 1 min 1 max 500" << std::endl;
        std::cout << "option name OwnBook type check default true" << std::endl;
        std::cout << "option name BookFile type string default " << BookFile << std::endl;
        std::cout << "option name SyzygyPath type string default <empty>" << std::endl;
        std::cout << "uciok" << std::endl;
    } else if (line == "isready") {
        std::cout << "readyok" << std::endl;
    } else if (line.find("setoption name OwnBook value ") != std::string::npos) {
        std::string val = line.substr(29);
        UseOwnBook = (val == "true" || val == "1");
        log("OwnBook set to " + std::to_string(UseOwnBook));
    } else if (line.find("setoption name BookFile value ") != std::string::npos) {
        BookFile = line.substr(30);
        log("BookFile set to " + BookFile);
    } else if (line.find("setoption name MultiPV value") != std::string::npos) {
        size_t val_idx = line.find("value ");
        if (val_idx != std::string::npos) {
            Search::multi_pv = std::stoi(line.substr(val_idx + 6));
            log("Multi-PV set to " + std::to_string(Search::multi_pv));
        }
    } else if (line.find("setoption name Threads value") != std::string::npos) {
        size_t val_idx = line.find("value ");
        if (val_idx != std::string::npos) {
            int threads = std::stoi(line.substr(val_idx + 6));
#ifdef __EMSCRIPTEN__
            threads = 1; // Lazy SMP is buggy in Emscripten Wasm
#endif
            Search::init_threads(threads);
            log("Threads set to " + std::to_string(threads));
        }
    } else if (line.find("position") == 0) {
        std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        size_t fen_idx = line.find("fen ");
        size_t moves_idx = line.find(" moves");
        
        if (fen_idx != std::string::npos) {
            std::string fen = line.substr(fen_idx + 4);
            if (moves_idx != std::string::npos) fen = line.substr(fen_idx + 4, moves_idx - (fen_idx + 4));
            Bitboard::parse_fen(pos, fen);
            game_ply = 0; 
        } else if (line.find("startpos") != std::string::npos) {
            Bitboard::parse_fen(pos, startpos);
            game_ply = 0;
        }
        
        if (moves_idx != std::string::npos) {
            std::string moves_str = line.substr(moves_idx + 6);
            std::stringstream ss(moves_str);
            std::string move_str;
            game_ply = 0; 
            while (ss >> move_str) {
                game_ply++;
                MoveList move_list;
                MoveGen::generate_moves(pos, move_list);
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
                    Board::make_move(pos, matched_move, 0);
                }
            }
        }
    } else if (line.find("perft ") == 0) {
        int depth = std::stoi(line.substr(6));
        Board::perft_test(pos, depth);
    } else if (line == "d") {
        Bitboard::print_board(pos);
    } else if (line == "test_q") {
        MoveList ml;
        MoveGen::generate_moves(pos, ml, true);
        std::cout << "Captures count: " << ml.count << std::endl;

    } else if (line == "eval") {
        Evaluation::allocate_nnue_stack();
        std::cout << "Eval (evaluate): " << Evaluation::evaluate(pos) << std::endl;
        std::cout << "Eval (incremental 0): " << Evaluation::evaluate_incremental(pos, 0) << std::endl;
        if (Evaluation::use_nnue) {
            std::cout << "Eval (incremental 1): " << Evaluation::evaluate_incremental(pos, 1) << std::endl;
        }
    } else if (line.find("go perft ") == 0) {
        int depth = std::stoi(line.substr(9));
        Board::perft_test(pos, depth);
    } else if (line.find("go") == 0) {
        bool is_game = (line.find("wtime") != std::string::npos || line.find("btime") != std::string::npos || line.find("movetime") != std::string::npos);
        if (UseOwnBook && !BookFile.empty() && is_game && game_ply <= 4) {
            auto b_moves = Polyglot::get_all_book_moves(pos, BookFile);
            Move book_move = 0;
            if (!b_moves.empty()) {
                std::sort(b_moves.begin(), b_moves.end(), [](const std::pair<Move, int>& a, const std::pair<Move, int>& b) {
                    return a.second > b.second;
                });
                
                int best_weight = b_moves[0].second;
                std::vector<Move> best_moves;
                for (auto& bm : b_moves) {
                    if (bm.second >= best_weight / 2 || best_moves.size() < 3) {
                        best_moves.push_back(bm.first);
                    } else {
                        break;
                    }
                }
                
                if (!best_moves.empty()) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, best_moves.size() - 1);
                    book_move = best_moves[dis(gen)];
                }
            }
            
            if (book_move != 0) {
                int src = GET_MOVE_SOURCE(book_move);
                int tgt = GET_MOVE_TARGET(book_move);
                int promoted = GET_MOVE_PROMOTED(book_move);
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
                std::cout << "info string Book move" << std::endl;
                std::cout << "bestmove " << m_str << std::endl;
                log("Book move played automatically: " + m_str);
                return; // Skip the search
            }
        }
        
        // Stop any ongoing search first
        Search::stopped = true;
#ifndef __EMSCRIPTEN__
        if (search_thread.joinable()) search_thread.join();
#endif

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
        size_t movetime_idx = line.find("movetime ");
        
        if (movetime_idx != std::string::npos) {
            time_for_move = std::stoll(line.substr(movetime_idx + 9));
        } else if (wtime_idx != std::string::npos && btime_idx != std::string::npos) {
            long long wtime = std::stoll(line.substr(wtime_idx + 6));
            long long btime = std::stoll(line.substr(btime_idx + 6));
            long long winc = (winc_idx != std::string::npos) ? std::stoll(line.substr(winc_idx + 5)) : 0;
            long long binc = (binc_idx != std::string::npos) ? std::stoll(line.substr(binc_idx + 5)) : 0;
            
            long long time_left = (pos.side == WHITE) ? wtime : btime;
            long long increment = (pos.side == WHITE) ? winc : binc;
            
            time_for_move = time_left / 30 + increment / 2;
            if (time_for_move < 50) time_for_move = 50;
        }
        
        // Reset stopped flag before launching!
        Search::stopped = false;
        
        Search::search_id.fetch_add(1, std::memory_order_relaxed);
        int current_search_id = Search::search_id.load(std::memory_order_relaxed);

        // Capture the current main thread's board state
        std::array<U64, 12> t_pieceBB;
        for (int i = 0; i < 12; i++) t_pieceBB[i] = pos.pieceBB[i];
        std::array<U64, 3> t_occ;
        for (int i = 0; i < 3; i++) t_occ[i] = pos.occupancies[i];
        int t_side = pos.side;
        int t_ep = pos.enpassant;
        int t_cas = pos.castle;
        U64 t_hash = pos.hash_key;

        // Launch search in a background thread so UCI remains responsive
#ifdef __EMSCRIPTEN__
        BoardState thread_pos;
        for (int i = 0; i < 12; i++) thread_pos.pieceBB[i] = t_pieceBB[i];
        for (int i = 0; i < 3; i++) thread_pos.occupancies[i] = t_occ[i];
        thread_pos.side = t_side;
        thread_pos.enpassant = t_ep;
        thread_pos.castle = t_cas;
        thread_pos.hash_key = t_hash;
        
        {
            std::lock_guard<std::mutex> lock(wasm_search_mtx);
            wasm_thread_pos = thread_pos;
            wasm_thread_depth = depth;
            wasm_thread_search_id = current_search_id;
            wasm_thread_time_for_move = (depth_idx == std::string::npos) ? time_for_move : -1;
            wasm_search_ready = true;
        }
#else
        search_thread = std::thread([=]() {
            BoardState thread_pos;
            for (int i = 0; i < 12; i++) thread_pos.pieceBB[i] = t_pieceBB[i];
            for (int i = 0; i < 3; i++) thread_pos.occupancies[i] = t_occ[i];
            thread_pos.side = t_side;
            thread_pos.enpassant = t_ep;
            thread_pos.castle = t_cas;
            thread_pos.hash_key = t_hash;
            Search::thread_search_id = current_search_id;
            Search::search_position(thread_pos, depth, current_search_id, (depth_idx == std::string::npos) ? time_for_move : -1);
        });
#endif
        
        if (time_for_move != -1 && depth_idx == std::string::npos) {
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

int main() {
    // Disable buffering for standard I/O streams for immediate communication
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    log("Engine started.");

    MoveGen::init_all();
    Zobrist::init();
    TT::init(64);
    Search::init_lmr_table();
    const char* nnue_paths[] = {
        "../src/nnue/nn-62ef826d1a6d.nnue",
        "src/nnue/nn-62ef826d1a6d.nnue",
        "nnue/nn-62ef826d1a6d.nnue",
        "nn-62ef826d1a6d.nnue"
    };
    
    bool loaded = false;
    for (int i = 0; i < 4; i++) {
        std::ifstream f(nnue_paths[i]);
        if (f.good()) {
            Evaluation::init_nnue(nnue_paths[i]);
            loaded = true;
            break;
        }
    }
    
    if (!loaded) {
        std::cout << "Engine OUT: Failed to load NNUE from any standard path." << std::endl;
    }
    if (tb_init("../3-4-5")) {
        log("Syzygy Tablebases automatically initialized from ../3-4-5 (Max pieces: " + std::to_string(TB_LARGEST) + ")");
    }

#ifdef __EMSCRIPTEN__
    Search::init_threads(1);
#else
    Search::init_threads(4);
#endif

#ifdef __EMSCRIPTEN__
    // Do nothing here, Wasm engine is waiting for execute_uci_command
    wasm_search_thread_instance = std::thread(wasm_search_worker);
    log("Wasm Engine initialized. Waiting for commands.");
    emscripten_exit_with_live_runtime();
#else
    uciLoop();
    log("Engine shutting down cleanly.");
    return 0;
#endif
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" {

    std::thread wasm_search_thread;
    EMSCRIPTEN_KEEPALIVE
    void wasm_send_command(const char* cmd) {
        std::string line(cmd);
        execute_uci_command(line, wasm_search_thread);
    }
}
#endif

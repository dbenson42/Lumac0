#include <fstream>

using namespace std;
std::string version = "14.0";
std::string ucistring = "id name Lumac0 "+version+"\nid author Davide Bencini\n\n"
						"option name Move Overhead type spin default 10 min 0 max 65535\n"
						"option name Hash type spin default 256 min 16 max 8192\n"
						"option name Threads type spin default 2 min 2 max 16\n"
						"option name Ponder type check default false\n"
						"option name Clear Hash type button\n"
						"option name UCI_AnalyseMode type check default false\n"
						"option name UCI_ShowWDL type check default false\n"
						"option name Log File type string default "+logpath+"\n"
						"uciok\n";

namespace UCI {
	int Start() {
		uint8_t timetipe;
		std::string command;
		strstack splitcm;
		unsigned int cmlen, d, t, i;
		Board board = initboard;
		SearchResult lmcout;
		std::cout << "Lumac0 " << version << " by Davide Bencini\n";
		TTables::create_tt(UCIparams::Hash);
		while (1) {
			board.set_zobrist();
			command = readUCI(splitcm);
			if (splitcm.stack[0] == "position") {
				if (splitcm.stack[1] == "startpos") {
					board = initboard;
					search::reps.clear();
					if (splitcm.len > 2 && splitcm.stack[2] == "moves") {
						for (int i=3;i<splitcm.len;i++) {
							board.push_san(splitcm.stack[i]);
							search::reps[board.string_id()] = 1;
						}
					}
				} else if (splitcm.stack[1] == "fen") {
					board.set_from_fen_command(splitcm);
					if (splitcm.stack[8] == "moves") {
						for (int i=9;i<splitcm.len;i++) {
							board.push_san(splitcm.stack[i]);
							search::reps[board.string_id()] = 1;
						}
					}
				} else if (splitcm.stack[1] == "moves") {
					for (int i=2;i<splitcm.len;i++) {
						board.push_san(splitcm.stack[i]);
						search::reps[board.string_id()] = 1;
					}
				} else if (splitcm.stack[1] == "undo") {
					board.pop();
				}
			}
			else if (splitcm.stack[0] == "uci") {
				std::cout << ucistring;
				writelog("<< "+ucistring);
			}
			else if (splitcm.stack[0] == "isready") {
				std::cout << "readyok\n";
				writelog("<< readyok\n");
			}
			else if (splitcm.stack[0] == "quit") {return 0;}
			else if (splitcm.stack[0] == "setoption") {
				std::string option = "";
				std::string value = "";
				for (uint8_t i=2;i<splitcm.len;i++) {
					option += splitcm.stack[i];
					if (i == splitcm.len-1) {break;}
					if (splitcm.stack[i+1] == "value") {
						value = splitcm.stack[i+2];
						break;
					}
					option += " ";
				}
				if (option == "UCI_Variant") {
					;
				} else if (option == "Ponder") {
					UCIparams::Ponder = (value=="true"?true:false);
				} else if (option == "Move Overhead") {
					UCIparams::MoveOverhead = std::stoi(value);
				} else if (option == "Hash") {
					UCIparams::Hash = std::stoi(value);
					TTables::create_tt(UCIparams::Hash);
				} else if (option == "Threads") {
					UCIparams::Threads = std::max(1,std::stoi(value)-1);
				} else if (option == "Clear Hash") {
					TTables::create_tt(UCIparams::Hash);
				} else if (option == "UCI_AnalyseMode") {
					UCIparams::UCI_AnalyseMode = (value=="true"?true:false);
				} else if (option == "UCI_ShowWDL") {
					UCIparams::UCI_ShowWDL = (value=="true"?true:false);
				} else if (option == "Log File") {
					logpath = value;
				} else {
					std::cout << "Unknown option: '" << option << "'\n";
					logfile.open(logpath, std::ios::app);
					logfile << "<< " << "Unknown option: '" << option << "'\n";
					logfile.close();
				}
			}
			else if (splitcm.stack[0] == "go") {
				if (splitcm.stack[1] == "depth") {
					d = std::stoi(splitcm.stack[2]);
					run_search(board, d, 0, UCIparams::Threads);
					print_bestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "movetime") {
					Time t = std::stoi(splitcm.stack[2])-UCIparams::MoveOverhead;
					run_search(board, 0, t, UCIparams::Threads);
					print_bestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "wtime") {
					t = std::stoi(splitcm.stack[4-2*board.turn]);
					if (splitcm.stack[3] != "btime") {
						i = std::stoi(splitcm.stack[8-2*board.turn]);
					} else {
						i = 0;
					}
					Time tm = Tuning::timecomp(t,i,UCIparams::Ponder)-UCIparams::MoveOverhead;
					run_search(board, 0, tm, UCIparams::Threads);
					print_bestmove(search::results[0], board);
				}
				else if (splitcm.stack[1] == "infinite" || splitcm.len==1) {
					run_search(board, 0, 0, UCIparams::Threads);
					print_bestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "ponder") {
					t = std::stoi(splitcm.stack[5-2*board.turn]);
					if (splitcm.stack[4] != "btime") {
						i = std::stoi(splitcm.stack[9-2*board.turn]);
					} else {
						i = 0;
					}
					Time tm = Tuning::timecomp(t, i, 1);
					run_pondering(board, tm-UCIparams::MoveOverhead, UCIparams::Threads);
					print_bestmove(search::bestresult, board);
				}
			} else if (splitcm.stack[0] == "fen") {
				std::cout << board.get_fen() << '\n';
			} else if (splitcm.stack[0] == "eval") {
				std::cout << evaluate(board.board, board.turn) << " cp\n";
			} else if (splitcm.stack[0] == "ucinewgame") {
				board.set_from_fen_string(startingfen);
				search::reps.clear();
			} else if (splitcm.stack[0] == "bench") {
				uint64_t total_nodes = 0;
				uint8_t points = 0;
				uint8_t num = 0;
				uint64_t nps = 0;
				map<std::string, std::string>::iterator it;
				std::string pos;	
				for (it=bench_positions.begin();it!=bench_positions.end();it++) {
					TTables::create_tt(UCIparams::Hash);
					pos = it->first;
					std::string right = it->second;
					board.set_from_fen_string(pos);
					std::cout << "Position: " << pos << '\n';
					run_search(board, 10, 0, 1);
					SearchResult res = search::bestresult;
					std::string mov = strmove(res.move, board.board);
					std::cout << "Move: " << mov << " Nodes: " << res.nodes << "\n\n\n";
					total_nodes += res.nodes;
					points += (right.find(mov) != std::string::npos);
					nps += res.nps;
					++num;
				}
				nps = nps/num;
				board.set_from_fen_string(startingfen);
				std::cout << "==============================================\nNodes: " << total_nodes << " | Points: " << int(points) << "/" << int(num) << " | Nps: " << nps << '\n';
			} else if (splitcm.stack[0] == "d") {
				board.printout();
				std::cout << "Zobrist Key: " << board.zobrist << '\n';
				std::cout << "En passant square: " << (board.ep == NULLSQ ? "None":intosq(board.ep)) << '\n';
				arrayret lgs = board.gen_moves();
				arrayret cps = board.gen_captures();
				std::cout << "Pseudo legal moves: ";
				print_moves(lgs,board.board);
				std::cout << "Pseudo legal captures: ";
				print_moves(cps,board.board);
				std::cout << '\n';
			} else {
				if (command.length()) {
					std::cout << "Unknown command: " << command << '\n';
					logfile.open(logpath, std::ios::app);
					logfile << "<< " << "Unknown command: " << command << '\n';
					logfile.close();
				}
			}
		}	
	}
}

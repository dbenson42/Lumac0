#include <iostream>
#include <fstream>

using namespace std;
std::string version = "12.14";
std::string ucistring = "id name Lumac0 "+version+"\nid author Davide Bencini\n\n"
						"option name Move Overhead type spin default 10 min 0 max 65535\n"
						"option name Hash type spin default 256 min 16 max 8192\n"
						"option name Threads type spin default 2 min 2 max 16\n"
						"option name Ponder type check default false\n"
						"option name Clear Hash type button\n"
						"option name UCI_AnalyseMode type check default false\n"
						"option name UCI_ShowWDL type check default false\n"
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
			board.setzobrist();
			command = readUCI(splitcm);
			if (splitcm.stack[0] == "position") {
				if (splitcm.stack[1] == "startpos") {
					board = initboard;
					search::reps.clear();
					if (splitcm.len > 2 && splitcm.stack[2] == "moves") {
						for (int i=3;i<splitcm.len;i++) {
							board.push_san(splitcm.stack[i]);
							search::reps[board.zobrist] = 1;
						}
					}
				} else if (splitcm.stack[1] == "fen") {
					board.setposfromfencommand(splitcm);
					if (splitcm.stack[8] == "moves") {
						for (int i=9;i<splitcm.len;i++) {
							board.push_san(splitcm.stack[i]);
							search::reps[board.zobrist] = 1;
						}
					}
				} else if (splitcm.stack[1] == "moves") {
					for (int i=2;i<splitcm.len;i++) {
						board.push_san(splitcm.stack[i]);
						search::reps[board.zobrist] = 1;
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
					UCIparams::Threads = std::stoi(value)-1;
				} else if (option == "Clear Hash") {
					TTables::create_tt(UCIparams::Hash);
				} else if (option == "UCI_AnalyseMode") {
					UCIparams::UCI_AnalyseMode = (value=="true"?true:false);
				} else if (option == "UCI_ShowWDL") {
					UCIparams::UCI_ShowWDL = (value=="true"?true:false);
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
					runSearch(board, d, 0, UCIparams::Threads);
					printbestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "movetime") {
					Time t = std::stoi(splitcm.stack[2])-UCIparams::MoveOverhead;
					runSearch(board, 0, t, UCIparams::Threads);
					printbestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "wtime") {
					t = std::stoi(splitcm.stack[4-2*board.turn]);
					if (splitcm.stack[3] != "btime") {
						i = std::stoi(splitcm.stack[8-2*board.turn]);
					} else {
						i = 0;
					}
					Time tm = Tuning::timecomp(t,i,UCIparams::Ponder)-UCIparams::MoveOverhead;
					runSearch(board, 0, tm, UCIparams::Threads);
					printbestmove(search::results[0], board);
				}
				else if (splitcm.stack[1] == "infinite" || splitcm.len==1) {
					runSearch(board, 0, 0, UCIparams::Threads);
					printbestmove(search::bestresult, board);
				}
				else if (splitcm.stack[1] == "ponder") {
					t = std::stoi(splitcm.stack[5-2*board.turn]);
					if (splitcm.stack[4] != "btime") {
						i = std::stoi(splitcm.stack[9-2*board.turn]);
					} else {
						i = 0;
					}
					Time tm = Tuning::timecomp(t, i, 1);
					runPonderSearch(board, tm-UCIparams::MoveOverhead, UCIparams::Threads);
					printbestmove(search::bestresult, board);
				}
			} else if (splitcm.stack[0] == "fen") {
				std::cout << board.getfen() << '\n';
			} else if (splitcm.stack[0] == "eval") {
				std::cout << evaluate(board.board, board.turn) << " cp\n";
			} else if (splitcm.stack[0] == "ucinewgame") {
				board.setposfromfenstring(startingfen);
				search::reps.clear();
			} else {
				if (command.length()) {
					std::cout << "Unknown command: " << command << '\n';
					logfile.open(logpath, std::ios::app);
					logfile << "<< " << "Unknown command: " << command << '\n';
					logfile.close();
				}
			}
			#if PRINTBOARD
			board.printout();
			#endif
			#if PRINTLEGALS
			std::cout << int(board.ep) << '\n';
			arrayret lgs = board.gen_moves();
			arrayret cps = board.gen_captures();
			printmoves(lgs,board.board);
			printmoves(cps,board.board);
			#endif
		}	
	}
}
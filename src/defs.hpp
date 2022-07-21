#include <fstream>
#include <iostream>

//defs.hpp contains some options and definitions.
// Here we have some options. 0 if inactive, 1 if active.

//GENERAL
#define TTABLES 1

//REDUCTIONS
#define LMR 1
#define RAZORING 1

//Move ordering
#define QUIESCE_MVV_LVA 1

#define MVV_LVA 1
#define HISTORY 1
#define KILLERS 1
#define COUNTERMOVE 1

//PRUNING
#define FUTILITY 1
#define STATIC_NULL_MOVE 1
#define NULLMOVE 1

//DEBUGGING
#define PRINTBOARD 0
#define PRINTLEGALS 0
#define PRINTPVS 0
#define PRINTCURRMOVE 1

//EVALUATION
#define PAWN_STRUCTURE_EVAL 1

#define stopsearch(src) search::stopsearch = true;src.join();search::stopsearch = false;
#define timecount(start) (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start).count())


//define some types
typedef int16_t Value;
typedef uint16_t Move;
typedef int8_t Depth;
typedef uint64_t Time;
typedef uint64_t Key;
typedef uint8_t Index;
enum TTflag {ALL, CUT, PV};

typedef std::map<Key, bool> RepetitionTable;
typedef std::map<Key, Value> PositionMap;

std::string logpath = "./log.txt";
std::ofstream logfile;

//Here we have the parameters for the search.

namespace Tuning {
	const Time dividends[2] = {60, 50}; //No ponder or ponder
	Time timecomp(Time base, Time incr, bool pond) {return base/dividends[pond]+incr;}
	
	const Value PAWN_VAL = 100;
	const Value KNIGHT_VAL = 378;
	const Value BISHOP_VAL = 397;
	const Value ROOK_VAL = 591;
	const Value QUEEN_VAL = 987;
	
	const Value RANKVALUE[8] = {0,8,15,26,30,41,53,0}; //Rank values for passed pawn
	const Value ISOLATED_PAWN_VALUE = -12;
	const Value DOUBLED_PAWN_VALUE = -9;
	
	const Depth MAXDEPTH = 29;
	const Depth MAX_FUTILITY_DEPTH = 3;
	
	const Value WIDENING = 15; //Window for first guess around guessed value
	const Value WINDOW_SAMEW = 20; // The higher, the more we "correct" the odd-even  FRACTION OF GUESS (SAME %2)
	const Value WINDOW_DIFFW = 3; // Opposite
	const Value REOPENING = 40; //In case of fail, we widen the side that failed by this value.
	const Value DELTA_FRACTION = 4; //Still in case of fail, we shrink the side that didn't fail by this fraction of beta-alpha. (the higher, the wider)
	const Value THREAD_WIDENING = 5; //How much we widen the window for each thread.
	
	const Value RAZORING_MARGIN = 125;
	const Value FUT_MARGIN[Tuning::MAX_FUTILITY_DEPTH+1] = {0, 200, 300, 500};
	const Value STATICNULLMOVEMARGIN = 120;
	
	const Index LMR_MOVE = 5;

	const Value LOWER_MATE = (2*(KNIGHT_VAL+20))+(2*(BISHOP_VAL+10)+(2*(ROOK_VAL+10))+(9*(QUEEN_VAL+5))) + 1; //If we have an advantage >= to MATE, we forcefully have the king more.
	const Value pieceval[13] = {0,PAWN_VAL,PAWN_VAL,KNIGHT_VAL,KNIGHT_VAL,BISHOP_VAL,BISHOP_VAL,ROOK_VAL,ROOK_VAL,QUEEN_VAL,QUEEN_VAL,0,0}; //Values. You cannot capture the king; no point giving it a value
	const Value INV_VALUE = -LOWER_MATE - 1; //Impossible to obtain.
	const Value INF = -INV_VALUE;
	
	const Value KILLER_BONUS = 90; //Killer bonus
	const Value COUNTERMOVE_BONUS = 7; //Countermove bonus
	
	const Depth MAX_STATIC_NULL_MOVE_DEPTH = 3;
	const Depth MIN_LMR_DEPTH = 2;
	
	const Value Q_DELTA_VAL = QUEEN_VAL + 29;
}

namespace UCIparams {
	bool UCI_ShowWDL = false;
	bool Ponder = false;
	bool UCI_AnalyseMode = false;
	uint8_t Threads = 1;
	uint16_t MoveOverhead = 10;
	uint32_t Hash = 512;
}

struct Stack {
	Value staticEval = 0;
	Move lastMove = 0;
	bool inCheck = false;
	bool pv = false;
};

struct Stats {
	Stack stacks[100];
	uint64_t nodes = 0;
	Depth seldepth = 1;
	uint8_t ply = 0;
};

struct Node {
	Value val;
	TTflag type:2;
	Depth depth:6;
	Key hash;
	Move move:12;
};

struct MoveDepthNode {
	Move move;
	Depth depth;
	Key hash;
};

struct WDLresult {
	Value win = 0;
	Value draw = 1000;
	Value loss = 0;
};

struct arrayret {
	short int arr[256];
	uint8_t len = 0;
};

struct strstack {
	std::string stack[256];
	uint8_t len = 0;
};

struct posstack {
	char pc[600][2]; //Piece moved, piece captured
	uint8_t ix[600][2]; //Starting square, destination square
	uint8_t cs[600][2]; //Starting square of rook in castle and its destination
	bool hascastled[600] = {false}; //Have we castled?
	int8_t eps[600] = {0};  //The en possants
	uint8_t epcapt[600] = {0}; //The ep capture squares
	uint16_t len = 0;
};

struct SearchResult {
	Move pv[Tuning::MAXDEPTH]; //Principal variation
	Move move;  //best move (pv[0])
	Move ponder;  //counter move by opponent (pv[1])
	uint64_t nodes;  //no of nodes searched
	Depth depth;  //depth searched
	Depth seldepth; //Maximum depth reached in Quiesce
	Value cp;  //score
	Time time;  //elapsed time in ns
	WDLresult wdl; //WDl probabilities
};

strstack splitstring(std::string s) {
	strstack r;
	std::string curr;
	char c;
	for (int i=0;i<s.length();i++) {
		c = s[i];
		if (c == ' ') {
			r.stack[r.len++] = curr;
			curr = "";
			continue;
		}
		curr += c;
	}
	r.stack[r.len++] = curr;
	return r;
}

std::string readUCI(strstack& splitcm) {
	std::string command;
	std::getline(std::cin, command);
	if (command.length()) {
		logfile.open(logpath, std::ios::app);
		logfile << ">> " << command << '\n';
		logfile.close();
	}
	splitcm = splitstring(command);
	return command;
}

void writelog(std::string msg) {
	logfile.open(logpath, std::ios::app);
	logfile << msg;
	logfile.close();
}

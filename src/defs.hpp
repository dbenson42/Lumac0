#include <fstream>

//defs.hpp contains some options and definitions.
// Here we have some options. 0 if inactive, 1 if active.

//GENERAL
#define TTABLES 1

//REDUCTIONS
#define LMR 1
#define ROOT_LMR 1
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

typedef std::map<std::string, bool> RepetitionTable;

std::string logpath = "./log.txt";
std::ofstream logfile;
std::map<std::string, std::string> bench_positions = { //Best 5 stockfish moves at depth 22 with max -0.3/.4 points from best.
	{"r4rk1/1bpqnpbp/p1n1p1p1/1p1pP3/3P4/1BN1BN1P/PPP1QPP1/R4RK1 b - - 0 12","a6a5 e7f5 f7f6 c6a5 a8e8"}, //Generic middlegames with slightly better moves
	{"r4rk1/p2nbp1p/1qp1bpp1/3p4/3P4/2NBPN2/PPQ2PPP/R4RK1 w - - 0 1","f1c1 c3a4 h2h3 a1c1 d3e2"},
	{"r1b2rk1/ppb2ppp/2n2q2/3pp3/1P1P4/P2BPN2/3N1PPP/R2Q1RK1 w - - 0 13","d4e5 b4b5"},
	{"6k1/1R3ppp/8/8/8/8/8/7K w - - 0 1","b7b8"}, //Mate in 1
	{"r6k/6np/8/8/8/8/5R2/5RK1 w - - 0 1","f2f8"}, //Mate in 2
	{"1r1n1rk1/5qpp/R7/1pp5/3p2N1/2PQ3P/1P3PP1/4R1K1 w - - 0 1","d3g3 g4f6"}, //Tactical positions (last 3 from bratko-kopec test).
	{"rnbqkb1r/pppp1ppp/8/4P3/6n1/7P/PPPNPPP1/R1BQKBNR b KQkq - 0 4","g4e3"},
	{"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1","d6d1"},
	{"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1","g4g7"},
	{"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1","f5h6"},
};

//Here we have the parameters for the search.

namespace Tuning {
	Time timecomp(Time base, Time incr, bool pond) {return base/(44-pond)+incr;}

	const Value PAWN_VAL = 100; //The values in cp of the pieces
	const Value KNIGHT_VAL = 378;
	const Value BISHOP_VAL = 397;
	const Value ROOK_VAL = 591;
	const Value QUEEN_VAL = 987;
	const Value PIECE_VAL[13] = {0,PAWN_VAL,PAWN_VAL,KNIGHT_VAL,KNIGHT_VAL,BISHOP_VAL,BISHOP_VAL,ROOK_VAL,ROOK_VAL,QUEEN_VAL,QUEEN_VAL,0,0}; //Values. You cannot capture the king; no point giving it a value

	const Value RANKVALUE[8] = {0,8,15,26,30,41,53,0}; //Rank values for passed pawn
	const Value ISOLATED_PAWN_VALUE = -11;
	const Value DOUBLED_PAWN_VALUE = -9;
	
	const Depth MAXDEPTH = 29;

	const Depth FUTILITY_MAX_DEPTH = 3;
	const Value FUTILITY_MARGIN[Tuning::FUTILITY_MAX_DEPTH+1] = {0, 200, 300, 500};

	const Depth STATIC_NULL_MOVE_MAX_DEPTH = 4;
	const Value STATIC_NULL_MOVE_MARGIN = 160;

	const Depth RAZORING_MAX_DEPTH = 4;
	const Value RAZORING_MARGIN = 250;

	const Depth LMR_MIN_DEPTH = 1;
	const Index LMR_MOVE = 2;

	const Depth ROOT_LMR_MIN_DEPTH = 5;
	const Depth ROOT_LMR_MOVE = 7;

	const Index QUIESCE_DELTA_MOVE = 3;
	const Value QUIESCE_DELTA_MARGIN = 200;

	const Value KILLER_BONUS = 90; //Killer bonus
	const Value COUNTERMOVE_BONUS = 7; //Countermove bonus
	const Value CAPTURE_BONUS = 750; //Value to add to MVV_LVA for captures
	
	const Value WINDOW_WIDENING = 15; //Window for first guess around guessed value
	const Value WINDOW_SAMEW = 20; // The higher, the more we "correct" the odd-even effect
	const Value WINDOW_DIFFW = 3; // Opposite
	const Value WINDOW_REOPENING = 40; //In case of fail, we widen the side that failed by this value.
	const Value WINDOW_DELTA_FRACTION = 4; //Still in case of fail, we shrink the side that didn't fail by this fraction of beta-alpha. (the higher, the wider)
	const Value WINDOW_THREAD_WIDENING = 5; //How much we widen the window for each thread.

	const Value LOWER_MATE = (2*(KNIGHT_VAL+10))+(2*(BISHOP_VAL+13)+(2*(ROOK_VAL+4))+(9*(QUEEN_VAL+29))) + 51; //If we have an advantage >= to MATE, we forcefully have the king more.
	const Value INV_VALUE = -LOWER_MATE - 1; //Impossible to obtain.
	const Value INF = -INV_VALUE+1;
}

namespace UCIparams {
	bool UCI_ShowWDL = false;
	bool Ponder = false;
	bool UCI_AnalyseMode = false;
	uint8_t Threads = 1;
	uint16_t MoveOverhead = 10;
	uint32_t Hash = 256;
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
	std::string stack[512];
	uint16_t len = 0;
};

struct posstack {
	char pc[600][2]; //Piece moved, piece captured
	uint8_t ix[600][2]; //Starting square, destination square
	uint8_t cs[600][2]; //Starting square of rook in castle and its destination
	bool hascastled[600] = {false}; //Have we castled?
	int8_t eps[600] = {0};  //The en possants
	uint8_t epcapt[600] = {0}; //The ep capture squares
	bool wkc[600] = {0};
	bool bkc[600] = {0};
	bool wqc[600] = {0};
	bool bqc[600] = {0};
	uint16_t len = 0;
};

struct SearchResult {
	Move pv[Tuning::MAXDEPTH+1]; //Principal variation
	Move move;  //best move (pv[0])
	Move ponder;  //counter move by opponent (pv[1])
	uint64_t nodes;  //no of nodes searched
	uint64_t nps; //Nodes per second
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
	for (uint16_t i=0;i<s.length();i++) {
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

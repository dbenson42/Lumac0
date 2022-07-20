using namespace std;

/* ORDERING.HPP
	contains general functions to order the moves and related to check and ordering heuristics. */
	
namespace Ordering {
	bool killers[300][4096] = {0};
	Value history[2][4096] = {0};
	Move countermove[4096] = {0};
}

Value MVV_LVA_Value(Move move, char board[64]) {
	return Tuning::pieceval[board[move%64]^START]-Tuning::pieceval[board[move/64]^START];
}

arrayret setasnth(arrayret legals, Move b, uint8_t n) {
	uint8_t idx = 0;
	while (idx < legals.len) {
		if (legals.arr[idx] == b) {break;}
		idx++;
	}
	legals.arr[idx] = legals.arr[n];
	legals.arr[n] = b;
	return legals;
}

arrayret setasnth_safe(arrayret legals, Move b, uint8_t n) {
	uint8_t idx = 255;
	for (uint8_t i=0;i<legals.len;i++) {
		if (legals.arr[i] == b) {
			idx = i;
			break;
		}
	}
	if (idx == 255) {return legals;}
	legals.arr[idx] = legals.arr[n];
	legals.arr[n] = b;
	return legals;
}

template <typename NUM>
arrayret order_MoveVal(arrayret legals, NUM table[4096]) {
	bool found[4096] = {0};
	arrayret res;
	Move curr, m;
	const NUM lowest = std::numeric_limits<NUM>::min();
	for (uint8_t idx=0;idx<legals.len;idx++) {
		NUM bst = lowest;
		for (uint8_t i=0;i<legals.len;i++) {
			m = legals.arr[i];
			if (found[m]) {continue;}
			NUM v = table[m];
			if (v > bst) {
				bst = v;
				curr = m;
			}
		}
		found[curr] = true;
		res.arr[res.len++] = curr;
	}
	return res;
}

bool isattacked(arrayret oppmoves, uint8_t sq) {
	for (uint8_t i=0;i<oppmoves.len;i++) {
		if (oppmoves.arr[i]%64 == sq) {return 1;}
	}
	return 0;
}

bool iscapt(Move move, char board[64]) {
	return board[move%64]^NONE;
}

uint8_t locateking(std::string board, bool turn) {
	char k = turn?WK:BK;
	for (uint8_t i=0;i<64;i++) {
		if (board[i] == k) {return i;}
	}
	return 255;
}

bool leaves_check(Board board, Move move) { //Detects if a move is irregular (leaves the king in check to be eaten by the opponent
	board.push(move);
	uint8_t myking = locateking(board.board, !board.turn);
	arrayret opponent_moves = board.gen_captures();
	if (isattacked(opponent_moves, myking)) {
		return true;
	} else {return false;}
}

bool ischeck(Board board) {
	board.turn = !board.turn;
	arrayret mvs = board.gen_captures();
	board.turn = !board.turn;
	return isattacked(mvs, locateking(board.board, board.turn));
}

arrayret order_MVV_LVA_captureonly(arrayret moves, char board[64]) {
	Value t[4096] = {0};
	for (uint8_t i=0;i<moves.len;i++) {
		Move m = moves.arr[i];
		t[m] = MVV_LVA_Value(m, board);
	}
	moves = order_MoveVal(moves, t);
	return moves;
}

arrayret order_MVV_LVA(arrayret moves, char board[64]) {
	Value t[4096] = {0};
	for (uint8_t i=0;i<moves.len;i++) {
		Move m = moves.arr[i];
		if (iscapt(m, board)) {
			t[m] = MVV_LVA_Value(m, board);
		}
	}
	moves = order_MoveVal(moves, t);
	return moves;
}

arrayret order_BestMove(arrayret moves, Key hash) {
	Move a = movetprobe(hash);
	if (a) {moves = setasnth_safe(moves, a, 0);}
	return moves;
}

void ageHistoryTable() {
	for (uint8_t trn=0;trn<2;trn++) {
		for (Move move=0;move<4096;move++) {
			Ordering::history[trn][move] /= 8;
		}
	}
}

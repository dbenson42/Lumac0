#ifndef CHESS_HPP
#define CHESS_HPP

#define initboard Board(startpos,0)

#define WKC 193
#define WQC 197
#define BKC 3833
#define BQC 3837

#define NONE 16
#define WP (NONE+1)
#define BP (NONE+2)
#define WN (NONE+3)
#define BN (NONE+4)
#define WB (NONE+5)
#define BB (NONE+6)
#define WR (NONE+7)
#define BR (NONE+8)
#define WQ (NONE+9)
#define BQ (NONE+10)
#define WK (NONE+11)
#define BK (NONE+12)

#define START NONE

#define NULLEPS -1

std::map<char, char> piece_to_gui = {{NONE,'.'},{WP,'P'},{BP,'p'},{WN,'N'},{BN,'n'},{WB,'B'},{BB,'b'},{WR,'R'},{BR,'r'},{WQ,'Q'},{BQ,'q'},{WK,'K'},{BK,'k'}};
std::map<char, char> gui_to_piece = {{'.',NONE},{'P',WP},{'p',BP},{'N',WN},{'n',BN},{'B',WB},{'b',BB},{'R',WR},{'r',BR},{'Q',WQ},{'q',BQ},{'K',WK},{'k',BK}};
inline bool getcolor(char piece) {return piece&1;}
inline bool ispiece(char piece) {return piece&15;}
inline bool iswhite(char piece) {return (piece&1);}
inline bool isblack(char piece) {return (piece&15)&&(!(piece&1));}
inline char getblackpiece(char piece) {return (piece+(piece&1));}
inline char getwhitepiece(char piece) {return piece & 30;}
inline char pieceofcolor(char blackpiece, bool turn) {return blackpiece-turn;}

char numtolett(int8_t num) {return (char)((7-num%8)+97);}
char numtocnum(int8_t num) {return (char)(num/8+49);}
int8_t letttonum(char lett) {return 7-((lett-97));}
int8_t cnumtonum(char num) {return (num-49);}
int8_t sqtoint(std::string sq) {return cnumtonum(sq[1])*8+letttonum(sq[0]);}

std::string startingfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
char startpos[65] = "RNBKQBNRPPPPPPPP................................pppppppprnbkqbnr";
int isprom(std::string board, int s, int d) {if ((board[s] == WP && d > 55) || (board[s] == BP && d < 8)){return 1;} else {return 0;}}
int ucitoint(std::string move) {return (cnumtonum(move[1])*8+letttonum(move[0]))*64 + (cnumtonum(move[3])*8+letttonum(move[2]));}
void pr_inttouci(Move m, bool pr, char prompiece) {
	if (!m) {return;}
	std::cout << numtolett(m/64) << numtocnum(m/64) << numtolett(m%64) << numtocnum(m%64) << (pr?std::string(1, piece_to_gui[prompiece]):"");
}
std::string fentostring(std::string fen) {
	std::string position;
	for (int8_t i=fen.length()-1;i>=0;--i) {
		if (fen[i] == '/') {continue;}
		if (std::isdigit(fen[i])) {
			for (int j=0;j<(fen[i]-48);j++) {
				position += '.';
			}
		} else {position += fen[i];}
	}
	return position;
}

arrayret rookmvs(int idx) {
	arrayret r;
	for (int8_t i=idx-1;i>=((idx/8)*8);i-=1) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+1;i<((idx/8+1)*8);i+=1) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+8;i<64;i+=8) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx-8;i>=0;i-=8) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	return r;
}
arrayret bishopmvs(int idx) {
	arrayret r;
	for (int8_t i=idx-9;i>=0&&i%8 < 7;i-=9) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx-7;i>=0&&i%8 > 0;i-=7) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+9;i<64&&i%8 > 0;i+=9) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+7;i<64&&i%8 < 7;i+=7) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	return r;
}
arrayret knightmvs(int idx) {
	arrayret r;
	if (idx % 8 > 0 && idx > 16) {r.arr[r.len++] = idx-17;}
	if (idx % 8 < 7 && idx > 14) {r.arr[r.len++] = idx-15;}
	if (idx % 8 < 7 && idx < 47) {r.arr[r.len++] = idx+17;}
	if (idx % 8 > 0 && idx < 49) {r.arr[r.len++] = idx+15;}
	if (idx % 8 > 1 && idx > 9) {r.arr[r.len++] = idx-10;}
	if (idx % 8 < 6 && idx > 5) {r.arr[r.len++] = idx-6;}
	if (idx % 8 < 6 && idx < 54) {r.arr[r.len++] = idx+10;}
	if (idx % 8 > 1 && idx < 58) {r.arr[r.len++] = idx+6;}
	return r;
}
arrayret queenmvs(int idx) {
	arrayret r;
	for (int8_t i=idx-9;i>=0&&i%8 < 7;i-=9) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx-7;i>=0&&i%8 > 0;i-=7) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+9;i<64&&i%8 > 0;i+=9) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+7;i<64&&i%8 < 7;i+=7) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx-1;i>=((idx/8)*8);i-=1) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+1;i<((idx/8+1)*8);i+=1) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx+8;i<64;i+=8) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	for (int8_t i=idx-8;i>=0;i-=8) {r.arr[r.len++] = i;}
	r.arr[r.len++] = -1;
	return r;
}
arrayret kingmvs(int idx) {
	arrayret r;
	if (idx > 7) {
		r.arr[r.len++] = idx-8;
		if ((idx+8)%8 > 0) {r.arr[r.len++] = idx-9;}
		if ((idx+8)%8 < 7) {r.arr[r.len++] = idx-7;}
	}
	if ((idx+8)%8 > 0) {r.arr[r.len++] = idx-1;}
	if ((idx+8)%8 < 7) {r.arr[r.len++] = idx+1;}
	if (idx < 56) {
		r.arr[r.len++] = idx+8;
		if ((idx+8)%8 > 0) {r.arr[r.len++] = idx+7;}
		if ((idx+8)%8 < 7) {r.arr[r.len++] = idx+9;}
	}
	return r;
}

arrayret rook[64], bishop[64], knight[64], king[64], queen[64];

namespace Chess {
	void Init() {
		for (uint8_t i=0;i<64;i++) {
			rook[i] = rookmvs(i);
			bishop[i] = bishopmvs(i);
			knight[i] = knightmvs(i);
			queen[i] = queenmvs(i);
			king[i] = kingmvs(i);
		}
	}
	char defaultprom = BQ;
}

class Board {
	public:
	int8_t ep;
	int eval = 0;
	bool turn,wkc,wqc,bkc,bqc;
	char board[65];
	posstack stack;
	Key zobrist;
	uint16_t* ply = &stack.len;
	Board(char brd[65], int eps) {
		for (uint8_t i=0;i<65;i++) {
			board[i] = gui_to_piece[brd[i]];
		}
		wkc = 1;
		wqc = 1;
		bkc = 1;
		bqc = 1;
		ep = eps;
		turn = 1; // 1 for W 0 for B
	}
	arrayret gen_moves() {
		char p;
		char toup;
		char ds;
		bool broken;
		int s;
		bool notempty;
		arrayret res;
		arrayret dirs;
		arrayret kac;
		int8_t dsq;
		for (uint8_t i=0;i<64;i++) {
			p = board[i];
			if (p == NONE || getcolor(p) != turn) {continue;}
			toup = p+turn;
			s = i*64;
			broken = 0;
			if (toup == BQ) {
				dirs = queen[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds^NONE) {
						broken = 1;
						if (getcolor(ds)==turn) {continue;}
					}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BR) {
				dirs = rook[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds^NONE) {
						broken = 1;
						if (getcolor(ds)==turn) {continue;}
					}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BB) {
				dirs = bishop[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds^NONE) {
						broken = 1;
						if (getcolor(ds)==turn) {continue;}
					}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BK) {
				dirs = king[i];
				for (int j=0;j<dirs.len;j++) {
					ds = board[dirs.arr[j]];
					if (ds^NONE && getcolor(ds)==turn) {
						continue;
					}
					res.arr[res.len++] = s+dirs.arr[j];
				}
			}
			else if (toup == BN) {
				dirs = knight[i];
				for (int j=0;j<dirs.len;j++) {
					ds = board[dirs.arr[j]];
					if (ds ^ NONE && getcolor(ds)==turn) {
						continue;
					}
					res.arr[res.len++] = s+dirs.arr[j];
				}
			}
			else {
				if (turn) {
					if (board[i+8] == NONE) {
						res.arr[res.len++] = s+i + 8;
						if (i < 16 && board[i+16] == NONE) {
							res.arr[res.len++] = s+i+16;
						}
					}
					if (i+7 == ep && i%8 > 0) {res.arr[res.len++] = s+i+7;}
					else if (i+9 == ep && i%8 < 7) {res.arr[res.len++] = s+i+9;}
					if (i < 56 && i%8 > 0 && isblack(board[i+7])) {res.arr[res.len++] = s+i+7;}
					if (i < 56 && i%8 < 7 && isblack(board[i+9])) {res.arr[res.len++] = s+i+9;}
				} else {
					if (board[i-8] == NONE) {
						res.arr[res.len++] = s+i - 8;
						if (i > 47 && board[i-16] == NONE) {
							res.arr[res.len++] = s+i-16;
						}
					}
					if (i-7 == ep && i%8 < 7) {res.arr[res.len++] = s+i-7;}
					else if (i-9 == ep && i%8 > 0) {res.arr[res.len++] = s+i-9;}
					if (i > 7 && i%8 > 0 && iswhite(board[i-9])) {res.arr[res.len++] = s+i-9;}
					if (i > 7 && i%8 < 7 && iswhite(board[i-7])) {res.arr[res.len++] = s+i-7;}
				}
			}
		}
		if (turn) {
			if (wkc && board[2] == NONE && board[1] == NONE) {res.arr[res.len++] = WKC;}
			if (wqc && board[4] == NONE && board[5] == NONE && board[6] == NONE) {res.arr[res.len++] = WQC;}
		} else {
			if (bkc && board[57] == NONE && board[58] == NONE) {res.arr[res.len++] = BKC;}
			if (bqc && board[60] == NONE && board[61] == NONE && board[62] == NONE) {res.arr[res.len++] = BQC;}
		}
		return res;
	}	
	arrayret gen_captures() {
		char p;
		char toup;
		char ds;
		bool broken;
		int s;
		bool notempty;
		arrayret res;
		arrayret dirs;
		arrayret kac;
		int8_t dsq;
		for (uint8_t i=0;i<64;i++) {
			p = board[i];
			if (p == NONE || getcolor(p) != turn) {continue;}
			toup = p+turn;
			s = i*64;
			broken = 0;
			if (toup == BQ) {
				dirs = queen[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds == NONE) {continue;}
					broken = 1;
					if (getcolor(ds)==turn) {continue;}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BR) {
				dirs = rook[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds == NONE) {continue;}
					broken = 1;
					if (getcolor(ds)==turn) {continue;}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BB) {
				dirs = bishop[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					ds = board[dsq];
					if (ds == NONE) {continue;}
					broken = 1;
					if (getcolor(ds)==turn) {continue;}
					res.arr[res.len++] = s+dsq;
				}
			}
			else if (toup == BK) {
				dirs = king[i];
				for (int j=0;j<dirs.len;j++) {
					ds = board[dirs.arr[j]];
					if (ds==NONE || getcolor(ds)==turn) {
						continue;
					}
					res.arr[res.len++] = s+dirs.arr[j];
				}
			}
			else if (toup == BN) {
				dirs = knight[i];
				for (int j=0;j<dirs.len;j++) {
					ds = board[dirs.arr[j]];
					if (ds == NONE || getcolor(ds)==turn) {
						continue;
					}
					res.arr[res.len++] = s+dirs.arr[j];
				}
			}
			else {
				if (turn) {
					if (i+7 == ep && i%8 > 0) {res.arr[res.len++] = s+i+7;}
					else if (i+9 == ep && i%8 < 7) {res.arr[res.len++] = s+i+9;}
					if (i < 56 && i%8 > 0 && isblack(board[i+7])) {res.arr[res.len++] = s+i+7;}
					if (i < 56 && i%8 < 7 && isblack(board[i+9])) {res.arr[res.len++] = s+i+9;}
				} else {
					if (i-7 == ep && i%8 < 7) {res.arr[res.len++] = s+i-7;}
					else if (i-9 == ep && i%8 > 0) {res.arr[res.len++] = s+i-9;}
					if (i > 7 && i%8 > 0 && iswhite(board[i-9])) {res.arr[res.len++] = s+i-9;}
					if (i > 7 && i%8 < 7 && iswhite(board[i-7])) {res.arr[res.len++] = s+i-7;}
				}
			}
		}
		return res;
	}
	bool can_capture_on_square(uint8_t square) {
		char p, toup;
		char ds;
		bool broken;
		int s;
		bool notempty;
		arrayret dirs;
		int8_t dsq;
		for (uint8_t i=0;i<64;i++) {
			p = board[i];
			if (p == NONE || getcolor(p) != turn) {continue;}
			toup = p+turn;
			broken = 0;
			if (toup == BQ) {
				dirs = queen[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					if (dsq == square) {return 1;}
					ds = board[dsq];
					if (ds^NONE) {broken = 1;}
				}
			}
			else if (toup == BR) {
				dirs = rook[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					if (dsq == square) {return 1;}
					ds = board[dsq];
					if (ds^NONE) {broken = 1;}
				}
			}
			else if (toup == BB) {
				dirs = bishop[i];
				for (int j=0;j<dirs.len;j++) {
					dsq = dirs.arr[j];
					if (dsq == -1) {
						broken = 0;
						continue;
					}
					if (broken) {continue;}
					if (dsq == square) {return 1;}
					ds = board[dsq];
					if (ds^NONE) {broken = 1;}
				}
			}
			else if (toup == BK) {
				dirs = king[i];
				for (int j=0;j<dirs.len;j++) {
					if (dirs.arr[j] == square) {return 1;}
				}
			}
			else if (toup == BN) {
				dirs = knight[i];
				for (int j=0;j<dirs.len;j++) {
					if (dirs.arr[j] == square) {return 1;}
				}
			}
			else {
				if (turn) {
					if (i < 56 && i%8 > 0 && i+7 == square) {return 1;}
					if (i < 56 && i%8 < 7 && i+9 == square) {return 1;}
				} else {
					if (i > 7 && i%8 > 0 && i-9 == square) {return 1;}
					if (i > 7 && i%8 < 7 && i-7 == square) {return 1;}
				}
			}
		}
		return 0;
	}
	bool in_check() { //returns wether is the current side in check
		changeturn();
		uint8_t kl;
		char k = turn?BK:WK;
		for (uint8_t i=0;i<64;++i) {
			if (board[i] == k) {
				kl = i;
				break;
			}
		}
		bool r = can_capture_on_square(kl);
		changeturn();
		return r;
	}
	bool opp_in_check() { //returns wether is the opposite side in check
		uint8_t kl;
		char k = turn?BK:WK;
		for (uint8_t i=0;i<64;++i) {
			if (board[i] == k) {
				kl = i;
				break;
			}
		}
		bool r = can_capture_on_square(kl);
		return r;
	}
	bool iscastling(Move move) {
		if (!(move == BQC || move == WQC || move == BKC || move == WKC)) {return 0;}
		if (turn) {return board[3]==WK;}
		else {return board[59]==BK;}
	}
	inline void changeturn() {
		turn = !turn;
	}
	void push(uint16_t move, char prompiece=Chess::defaultprom) {
		uint8_t s = move/64;
		uint8_t d = move%64;
		char sp = board[s];
		stack.ix[stack.len][0] = s;
		stack.ix[stack.len][1] = d;
		stack.pc[stack.len][0] = sp;
		stack.pc[stack.len][1] = board[d];
		stack.eps[stack.len] = ep;
		stack.len++;
		zobrist ^= TTables::color;
		zobrist ^= TTables::zobristkeys[sp^START][s];
		zobrist ^= TTables::zobristkeys[board[d]^START][d];
		zobrist ^= TTables::zobristkeys[sp^START][d];
		if (ep) {
			zobrist ^= TTables::zobristeps[ep%8];
		}
		char tolw = getblackpiece(sp);
		if (tolw == BP && d == ep) {
			uint8_t capt = (turn ? d-8 : d+8);
			board[capt] = NONE;
			zobrist ^= TTables::zobristkeys[(WP+turn)^START][capt];
			stack.epcapt[stack.len-1] = capt;
		}
		ep = NULLEPS;
		if (tolw == BP && (d-s == 16 || s-d == 16)) {
			ep = (turn? d-8 : d+8);
			zobrist ^= TTables::zobristeps[ep%8];
		}
		else if (sp == WK) {wkc = wqc = 0;}
		else if (sp == BK) {bkc = bqc = 0;}
		else if (s == 0) {wkc = 0;}
		else if (s == 7) {wqc = 0;}
		else if (s == 63) {bqc = 0;}
		else if (s == 56) {bkc = 0;}
		if ((sp == BP && d < 8) || (sp == WP && d > 55)) {
			board[d] = pieceofcolor(prompiece, turn);
		} else {
			board[d] = sp;
		}
		board[s] = NONE;
		if (tolw == BK && (s-d == 2 || d-s == 2)) {
			stack.hascastled[stack.len-1] = true;
			uint8_t rs;
			uint8_t rd;
			if (d % 8 == 1) {rs = (d/8)*8;rd = rs+2;}
			else if (d % 8 == 5) {rs = (d/8)*8+7;rd = rs-3;}
			stack.cs[stack.len-1][0] = rs;
			stack.cs[stack.len-1][1] = rd;
			board[rd] = board[rs];
			board[rs] = NONE;
			zobrist ^= TTables::zobristkeys[board[rd]^START][rd];
			zobrist ^= TTables::zobristkeys[board[rd]^START][rs];
		}
		turn = !turn;		
	}
	void push_capture(uint16_t move, char prompiece=Chess::defaultprom) {
		uint8_t s = move/64;
		uint8_t d = move%64;
		char sp = board[s];
		stack.ix[stack.len][0] = s;
		stack.ix[stack.len][1] = d;
		stack.pc[stack.len][0] = sp;
		stack.pc[stack.len][1] = board[d];
		stack.eps[stack.len] = ep;
		stack.len++;
		zobrist ^= TTables::color;
		zobrist ^= TTables::zobristkeys[sp^START][s];
		zobrist ^= TTables::zobristkeys[board[d]^START][d];
		zobrist ^= TTables::zobristkeys[sp^START][d];
		char tolw = getblackpiece(sp);
		if (ep) {
			zobrist ^= TTables::zobristeps[ep%8];
		}
		if (tolw == BP && d == ep) {
			uint8_t capt = (turn ? d-8 : d+8);
			board[capt] = NONE;
			zobrist ^= TTables::zobristkeys[(WP+turn)^START][capt];
			stack.epcapt[stack.len-1] = capt;
		}
		ep = NULLEPS;
		if ((sp == BP && d < 8) || (sp == WP && d > 55)) {
			board[d] = pieceofcolor(prompiece, turn);
		} else {
			board[d] = sp;
		}
		board[s] = NONE;
		turn = !turn;		
	}
	void push_san(std::string move) {
		if (move.length() == 4) {
			push(ucitoint(move));
		} else {
			push(ucitoint(move.substr(0,4)), gui_to_piece[move[4]]);
		}
	}
	void pop() {
		stack.len--;
		board[stack.ix[stack.len][0]] = stack.pc[stack.len][0];
		board[stack.ix[stack.len][1]] = stack.pc[stack.len][1];
		zobrist ^= TTables::zobristkeys[stack.pc[stack.len][0]^START][stack.ix[stack.len][0]];
		zobrist ^= TTables::zobristkeys[stack.pc[stack.len][0]^START][stack.ix[stack.len][1]];
		zobrist ^= TTables::zobristkeys[stack.pc[stack.len][1]^START][stack.ix[stack.len][1]];
		zobrist ^= TTables::color;
		if (stack.hascastled[stack.len]) {
			board[stack.cs[stack.len][0]] = board[stack.cs[stack.len][1]];
			board[stack.cs[stack.len][1]] = NONE;
			zobrist ^= TTables::zobristkeys[board[stack.cs[stack.len][0]]^START][stack.cs[stack.len][1]];
			zobrist ^= TTables::zobristkeys[board[stack.cs[stack.len][0]]^START][stack.cs[stack.len][0]];
		} else if (stack.epcapt[stack.len]) {
			board[stack.epcapt[stack.len]] = (BP-turn);
			zobrist ^= TTables::zobristkeys[(BP-turn)^START][stack.epcapt[stack.len]];
		}
		if (ep) {
			zobrist ^= TTables::zobristeps[ep%8];
		}
		stack.hascastled[stack.len] = false;
		ep = stack.eps[stack.len];
		if (ep) {
			zobrist ^= TTables::zobristeps[ep%8];
		}
		turn = !turn;
	}
	void printout() {
		for (int i=0;i<64;i++) {
			if (i%8 == 0) {std::cout << '\n';}
			std::cout << piece_to_gui[board[i]];
		}
		std::cout << '\n';
	}
	void setposfromfencommand(strstack splitcm) {
		std::string t = fentostring(splitcm.stack[2]);
		for (int i=0;i<64;i++) {
			board[i] = gui_to_piece[t[i]];
		}
		turn = splitcm.stack[3]=="w"?1:0;
		ep = splitcm.stack[5]!="-"?sqtoint(splitcm.stack[5]):-1;
		wkc = 0;
		bkc = 0;
		wqc = 0;
		bqc = 0;
		for (int i=0;i<splitcm.stack[4].length();i++) {
			if (splitcm.stack[4][i] == 'K') {wkc = 1;}
			if (splitcm.stack[4][i] == 'k') {bkc = 1;}
			if (splitcm.stack[4][i] == 'Q') {wqc = 1;}
			if (splitcm.stack[4][i] == 'q') {bqc = 1;}
		}
		setzobrist();
	}
	void setposfromfenstring(std::string fen) {
		strstack splitcm = splitstring(fen);
		std::string t = fentostring(splitcm.stack[0]);
		for (int i=0;i<64;i++) {
			board[i] = gui_to_piece[t[i]];
		}
		turn = splitcm.stack[1]=="w"?1:0;
		ep = splitcm.stack[5]!="-"?sqtoint(splitcm.stack[3]):-1;
		wkc = 0;
		bkc = 0;
		wqc = 0;
		bqc = 0;
		for (int i=0;i<splitcm.stack[2].length();i++) {
			if (splitcm.stack[2][i] == 'K') {wkc = 1;}
			if (splitcm.stack[2][i] == 'k') {bkc = 1;}
			if (splitcm.stack[2][i] == 'Q') {wqc = 1;}
			if (splitcm.stack[2][i] == 'q') {bqc = 1;}
		}
		setzobrist();
	}
	std::string getfen() {
		std::string fen = "";
		char nullcount = '0';
		for (int8_t sqidx=63;sqidx>=0;sqidx--) {
			char c = board[sqidx];
			if (c == NONE) {
				nullcount++;
			} else {
				if (nullcount != '0') {fen += nullcount;}
				nullcount = '0';
				fen += piece_to_gui[c];
			}
			if (sqidx%8 == 0) {
				if (nullcount != '0') {fen += nullcount;}
				nullcount = '0';
				if (sqidx) {fen += '/';}
			}
		}
		fen += ' ';
		fen += (turn?'w':'b');
		fen += ' ';
		if (wkc) {fen += 'K';}
		if (wkc) {fen += 'Q';}
		if (bkc) {fen += 'k';}
		if (bqc) {fen += 'q';}
		if (fen[fen.length()-1] == ' ') {
			fen += '-';
		}
		fen += ' ';
		if (ep != -1 && ep) {
			fen += numtolett(ep);
			fen += numtocnum(ep);
		} else {
			fen += '-';
		}
		fen += " 0 1";
		return fen;
	}
	inline Key getkey() {
		return zobrist;
	}
	void setzobrist() {
		zobrist = 0;
		for (Index s=0;s<64;s++) {
			zobrist ^= TTables::zobristkeys[board[s]-START][s];
		}
		if (!turn) {
			zobrist ^= TTables::color;
		}
	}
};

std::string strmove(Move m, std::string b) {
	bool pr = isprom(b, m/64, m%64);
	std::string prchr = "";
	if (pr) {prchr += piece_to_gui[Chess::defaultprom];}
	std::string r = "";
	r += numtolett(m/64);
	r += numtocnum(m/64);
	r += numtolett(m%64);
	r += numtocnum(m%64);
	r += prchr;
	return r;
}

void printmoveinfo(SearchResult lmcout, Board b, Time totime, Value alpha=-Tuning::INF, Value beta=Tuning::INF) {
	Time pt = std::max(lmcout.time,Time(1));
	std::string t = std::to_string(totime);
	std::string d = std::to_string(int(lmcout.depth));
	std::string sd = std::to_string(int(lmcout.seldepth));
	std::string nps = std::to_string(lmcout.nodes*1000/pt);
	std::string boundfail = "";
	if (lmcout.cp <= alpha) {boundfail = " lowerbound";}
	else if (lmcout.cp >= beta) {boundfail = " upperbound";}
	std::string score = "cp "+std::to_string(lmcout.cp)+boundfail;
	if (lmcout.cp+lmcout.depth+1 >= Tuning::LOWER_MATE) {
		int8_t ply = Tuning::LOWER_MATE-lmcout.cp;
		score = "mate "+std::to_string(ply/2+1);
	} else if (lmcout.cp-lmcout.depth-1 <= -Tuning::LOWER_MATE) {
		int8_t ply = -Tuning::LOWER_MATE-lmcout.cp;
		score = "mate "+std::to_string(ply/2+1);
	} else {
		if (UCIparams::UCI_ShowWDL) {
			score += " wdl ";
			WDLresult wdl = lmcout.wdl;
			score += std::to_string(wdl.win) + " " + std::to_string(wdl.draw) + " " + std::to_string(wdl.loss);
		}
	}
	std::string message = "info depth " + d + " seldepth " + sd + " multipv 1 score " + score + " nodes " + std::to_string(lmcout.nodes) + " nps " + nps + " time " + t + " pv ";
	for (Depth d=0;d<lmcout.depth;d++) {
		Move move = lmcout.pv[d];
		if (move == 0) {break;}
		std::string sm = strmove(move, b.board);
		b.push(move);
		if (b.opp_in_check()) {break;}
		message += sm + " ";
	}
	message += "\n";
	std::cout << message;
	writelog("<< "+message);
}

void printbestmove(SearchResult lmcout, Board b) {
	std::string message = "bestmove " + strmove(lmcout.move, b.board) + " ponder " + strmove(lmcout.ponder, b.board) + "\n";
	std::cout << message;
	writelog("<< "+message);
}

void printmoves(arrayret lgs, char board[64]) {
	for (uint8_t i=0;i<lgs.len;i++) {;
		std::cout << strmove(lgs.arr[i], board) << ' ';
	}
	std::cout << '\n';
}
#endif

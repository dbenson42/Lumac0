Value pawnt[64] = {7, 7, 7, 7, 7, 7, 7, 7, 57, 57, 57, 57, 57, 57, 57, 57, 17, 17, 27, 37, 37, 27, 17, 17, 12, 12, 17, 34, 34, 17, 12, 12, 7, 7, 7, 33, 33, 7, 7, 7, 12, 2, -3, 7, 7, -3, 2, 12, 12, 17, 17, -13, -13, 17, 17, 12, 7, 7, 7, 7, 7, 7, 7, 7};
Value knightt[64] = {-60, -50, -40, -40, -40, -40, -50, -60, -50, -30, -10, -10, -10, -10, -30, -50, -40, -10, 0, 5, 5, 0, -10, -40, -40, -5, 5, 10, 10, 5, -5, -40, -40, -10, 5, 10, 10, 5, -10, -40, -40, -5, 0, 5, 5, 0, -5, -40, -50, -30, -10, -5, -5, -10, -30, -50, -60, -50, -40, -40, -40, -40, -50, -60};
Value bishopt[64] = {-17, -7, -7, -7, -7, -7, -7, -17, -7, 3, 3, 3, 3, 3, 3, -7, -7, 3, 8, 13, 13, 8, 3, -7, -7, 8, 8, 13, 13, 8, 8, -7, -7, 3, 13, 13, 13, 13, 3, -7, -7, 13, 13, 13, 13, 13, 13, -7, -7, 8, 3, 3, 3, 3, 8, -7, -17, -7, -7, -7, -7, -7, -7, -17};
Value rookt[64] = {-16, -6, -6, -6, -6, -6, -6, -16, -1, 4, 4, 4, 4, 4, 4, -1, -11, -6, -6, -6, -6, -6, -6, -11, -11, -6, -6, -6, -6, -6, -6, -11, -11, -6, -6, -6, -6, -6, -6, -11, -11, -6, -6, -6, -6, -6, -6, -11, -11, -6, -6, -6, -6, -6, -6, -11, -16, -6, -6, -1, -1, -6, -6, -16};
Value queent[64] = {4, 14, 14, 19, 19, 14, 14, 4, 14, 24, 24, 24, 24, 24, 24, 14, 14, 24, 29, 29, 29, 29, 24, 14, 19, 24, 29, 29, 29, 29, 24, 19, 19, 24, 29, 29, 29, 29, 24, 24, 14, 24, 29, 29, 29, 29, 29, 14, 14, 24, 24, 24, 24, 24, 24, 14, 4, 14, 14, 19, 19, 14, 14, 4};
Value kingt[64] = {
	-30,-40,-40,-50,-50,-40,-40,-30,
	-30,-40,-40,-50,-50,-40,-40,-30,
	-30,-40,-40,-50,-50,-40,-40,-30,
	-30,-40,-40,-50,-50,-40,-40,-30,
	-20,-30,-30,-40,-40,-30,-30,-20,
	-10,-20,-20,-20,-20,-20,-20,-10,
	 20, 20,  0,  0,  0,  0, 20, 20,
	 20, 30, 10,  0,  0, 10, 30, 20};

Value rpawnt[64], rknightt[64], rbishopt[64], rrookt[64], rqueent[64], rkingt[64];

namespace Eval {
	void Init() {
		Value* pptr[6] = {pawnt, knightt, bishopt, rookt, queent, kingt};
		Value* rpptr[6] = {rpawnt, rknightt, rbishopt, rrookt, rqueent, rkingt};
		for (Index i=0;i<6;i++) {
			for (Index sq=0;sq<32;sq++) {
				Value temp = pptr[i][sq];
				pptr[i][sq] = pptr[i][63-sq];
				pptr[i][63-sq] = temp;
			}
		}
		for (Index i=0;i<6;i++) {
			for (Index sq=0;sq<64;sq++) {
				pptr[i][sq] += Tuning::pieceval[2*i+1];
			}
		}
		for (Index i=0;i<6;i++) {
			for (Index sq=0;sq<64;sq++) {
				rpptr[i][sq] = -pptr[i][63-sq];
			}
		}
	}
}
Value* psqt[13] = {0, pawnt, rpawnt, knightt, rknightt, bishopt, rbishopt, rookt, rrookt, queent, rqueent, kingt, rkingt};

Value evaluate(char board[64], bool turn) {
	Value pts = 0;
	char p;
	Value tmp;
	uint8_t ri;
	#if PAWN_STRUCTURE_EVAL
	uint8_t wpawncounts[8] = {0};
	uint8_t bpawncounts[8] = {0};
	uint8_t wfirstpawn[8] = {63};
	uint8_t bfirstpawn[8] = {0};
	uint8_t column;
	bool passed;
	bool isolated;
	#endif
	for (uint8_t i=0;i<64;i++) {
		p = board[i];
		if (p == NONE) {continue;}
		pts += psqt[p^START][i];
		#if PAWN_STRUCTURE_EVAL
		column = i%8;
		if (p == WP) {
			wpawncounts[column]++;
			if (i < wfirstpawn[column]) {wfirstpawn[column] = i;}
		} else if (p == BP) {
			bpawncounts[column]++;
			if (i > bfirstpawn[column]) {bfirstpawn[column] = i;}
		}
		#endif
	}
	#if PAWN_STRUCTURE_EVAL
	for (uint8_t c=0;c<8;c++) {
		pts += std::max(0, wpawncounts[c]-1)*Tuning::DOUBLED_PAWN_VALUE;
		pts -= std::max(0, bpawncounts[c]-1)*Tuning::DOUBLED_PAWN_VALUE;
	}
	for (uint8_t sq=0;sq<64;sq++) {
		p = board[sq];
		column = sq%8;
		passed = true;
		isolated = true;
		if (p == WP) {
			if (bpawncounts[column]) {passed = false;}
			else {
				if (column > 0) {
					if (bfirstpawn[column-1] > sq) {passed = false;}
					if (wpawncounts[column-1]) {isolated = false;}
				}
				if (column < 7) {
					if (bfirstpawn[column+1] > sq) {passed = false;}
					if (wpawncounts[column+1]) {isolated = false;}
				}
			}
			if (passed) {pts += Tuning::RANKVALUE[sq/8];}
			if (isolated) {pts += Tuning::ISOLATED_PAWN_VALUE;}
		} else if (p == BP) {
			if (wpawncounts[column]) {passed = false;}
			else {
				if (column > 0) {
					if (wfirstpawn[column-1] < sq) {passed = false;}
					if (bpawncounts[column-1]) {isolated = false;}
				}
				if (column < 7) {
					if (wfirstpawn[column+1] < sq) {passed = false;}
					if (bpawncounts[column+1]) {isolated = false;}
				}
			}
			if (passed) {pts -= Tuning::RANKVALUE[8-(sq/8)];}
			if (isolated) {pts -= Tuning::ISOLATED_PAWN_VALUE;}
		}
	}
	#endif
	return (turn ? pts : -pts);
}

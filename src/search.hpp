#include <map>
#include <math.h>

using namespace std;


namespace search {
	bool stopsearch = false;
	Depth reached = 0;
	RepetitionTable reps;
	Stats searchStats[16];
	SearchResult results[16];
	SearchResult bestresult;
}

WDLresult WDLscore(float cp, float ply) {
	WDLresult wdl;
	if (cp == 0) {return wdl;}
	double winprob = pow(cp/10, 3)/80;
	double drawprob = 987+ply/17-pow(winprob, 2);
	double lossprob = 1000-drawprob-winprob;
	if (winprob < 0) {
		wdl.win = Value(winprob >= -20 ? lossprob/200 : 0.0);
		wdl.loss = Value(min(lossprob, 1000.0));
		wdl.draw = 1000-wdl.win-wdl.loss;
	} else {
		wdl.win = Value(min(winprob, 1000.0));
		wdl.loss = Value(lossprob >= -20 ? winprob/200 : 0.0);
		wdl.draw = 1000-wdl.win-wdl.loss;
	}
	return wdl;
}

Depth LMReduction(uint8_t tried, Depth depth, bool pv, Value score) {
	Depth r = Depth(sqrt(float(depth)-1)+sqrt(float(tried)-1)-(score/310));
	if (r < 0) {return 0;}
	if (pv) {r = r*3/5;}
	return r;
}

Depth nullMoveReduction(Depth depth, Value delta) {
	Depth r = 1+(depth>6)+(delta>Tuning::ROOK_VAL);
	return r;
}

Value Quiesce(Board board, Value alpha, Value beta, Stats* st) {
	if (st->ply > st->seldepth) {st->seldepth = st->ply;}
	st->nodes++;
	Value stand_pat = evaluate(board.board, board.turn);
	if (stand_pat >= beta) {return beta;}
	if (stand_pat + Tuning::Q_DELTA_VAL < alpha) {return alpha;}
	arrayret moves = board.gen_captures();
	if (alpha < stand_pat) {alpha = stand_pat;}
	if (moves.len == 0) {return alpha;}
	Value score;
	#if QUIESCE_MVV_LVA
	moves = order_MVV_LVA_captureonly(moves, board.board);
	#endif
	uint8_t moves_tried = 0;
	for (int i=0;i<moves.len;i++) {
		if (search::stopsearch) {return 0;}
		Move move = moves.arr[i];
		board.push_capture(move);
		if (board.opp_in_check()) {
			board.pop();
			continue;
		}
		moves_tried++;		
		st->ply++;
		score = -Quiesce(board, -beta, -alpha, st);
		st->ply--;
		board.pop();
		if (score > alpha) {
			if (score >= beta) {
				return beta;
			}
			alpha = score;
		}
	}
	if (moves_tried == 0) {
		if (board.in_check()) {return -Tuning::LOWER_MATE+st->ply;}
		else {return 0;}
	}
	return alpha;
}

template <bool pv, bool can_null>
Value Search(Board board, Stats* st, Depth depth, Value alpha, Value beta) {
	
	Key hash = board.zobrist; //We get the zpbrist key of our position
	Value eval, score, best; //Some values we want to use later
	Move bestMove, hashMove, counterMove;
	TTflag flag; //The flag telling wether a node is ALL, PV or CUT.
	int8_t R; //Depth reduction. Next search depth is depth-R-1.
	bool in_check, giving_check, is_capture;
	Stack* stk = &(st->stacks[st->ply]);
	Stack* stk1; //Next stack and previous one.
	Stack* pstk;
	uint8_t moves_tried; //How many moves were tried.
	
	/*****************************************************
	*	Before searching a position or checking wether   *
	*	is the depth 0, we look if have we searched this *
	*	position before, and if the depth is >= ours and *
	*	the value is inside the bounds, we return it.    *
	*****************************************************/
	
	#if TTABLES
	Value val = ttprobe(hash, depth, alpha, beta);
	if (val != Tuning::INV_VALUE) {
		if (!pv || (val >= alpha && val < beta)) {
			return val;
		}
	}
	#endif
	
	in_check = board.in_check(); //Are we under check?
	stk->inCheck = in_check; //Update the stack;
	//If we are under check, we extend; meaning we'll never drop to qsearch while in check.
	if (in_check) {
		depth++;
	}
	
	/*****************************************************
	*	If we reach a leaf node, meaning we have reached *
	*	the maximum depth, we drop to Quiescence search, *
	*	a search that continues searching until the board*
	*	is 'quiet', meaning there are no captures.		 *
	*****************************************************/
	
	if (depth <= 0) {
		st->ply++;
		Value ev = Quiesce(board, alpha, beta, st);
		st->ply--;
		return ev;
	}
	
	arrayret moves; //The array of pseudo-legal moves
	++st->nodes;
	eval = evaluate(board.board, board.turn); //We gat a static score
	stk->staticEval = eval; //Update the stack;
	pstk = (stk-1); //Previoius element of the stack
	stk1 = (stk+1); //Next element of the stack
	
	if (in_check || pv) { // In case of check or PV node, wedirectly go to the moves loop.
		moves = board.gen_moves();
		goto MOVES_LOOP;
	}
	
	/*****************************************************
	*	Static null move pruning: here we verify if is	 *
	*	our static score minus a margin, proportional to *
	*	the depth, higher than beta. If it indeed is,	 *
	*	we consider it enough to just return beta.		 *
	*****************************************************/
	
	#if STATIC_NULL_MOVE
	if (depth < Tuning::MAX_STATIC_NULL_MOVE_DEPTH 
		&& eval - Tuning::STATICNULLMOVEMARGIN*depth >= beta) 
	{
		return beta;
	}
	#endif
	
	/*****************************************************
	*	Null Move Pruning: if our static eval is higher  *
	*	or equal to beta and it stays so even if we let  *
	*	the opponent play two moves in a row, then we    *
	*	assume our position is good enough to return.    *
	*****************************************************/
	
	#if NULLMOVE
	if (can_null
		&& eval >= beta)
	{
		R = nullMoveReduction(depth, eval-beta);
		stk1->lastMove = 0;
		board.changeturn();
		Value v = -Search<0,0>(board, st, depth-R-1, -beta, -beta+1);
		board.changeturn();
		if (v >= beta) {
			return beta;
		}
	}
	#endif
	
	/*****************************************************
	*	Razoring: if we are at a pre-frontier node, at	 *
	*	depth 2, we check if the static score plus a	 *
	*	margin is lower than alpha. If it is, reduce.	 *
	*****************************************************/
	
	#if RAZORING
	if (eval + Tuning::RAZORING_MARGIN < alpha 
		&& depth == 2)
	{
		depth--;
	}
	#endif
	
	/*****************************************************
	*	Futility pruning: if we are at a relatively low  *
	*	depth, not searching for any mate and our score  *
	*	is lower than alpha minus a margin, we only		 *
	*	consider tactical moves, thinking quiets futile. *
	*****************************************************/
	
	#if FUTILITY
	if (depth <= Tuning::MAX_FUTILITY_DEPTH 
		&& abs(alpha) < Tuning::LOWER_MATE 
		&& eval+Tuning::FUT_MARGIN[depth] <= alpha) 
	{
		moves = board.gen_captures(); //If we can futility-prune, we would skip non-captures, so to save time we only generate captures.
	} else {
		moves = board.gen_moves();
	}
	#else
	moves = board.gen_moves();
	#endif
	
	MOVES_LOOP:

	/*****************************************************
	*	Here one of the most crucial parts of all the	 *
	*	engine, vital for not searching 100 times the	 *
	*	nodes: move ordering. Here we use a very basic   *
	*	thing, considering MVV_LVA value, best and killer*
	*	moves. You also can turn on history heuristic.	 *
	*****************************************************/
	
	bool capture_moves[4096] = {0};
	int moveTable[4096] = {0};
	hashMove = movetprobe(hash);
	#if COUNTERMOVE
	counterMove = Ordering::countermove[stk->lastMove];
	#endif
	moveTable[hashMove] = 1000000000;
	for (uint8_t i=0;i<moves.len;i++) {
		Move m = moves.arr[i];
		if (board.isCapture(m)) {
			#if MVV_LVA
			moveTable[m] += int(MVV_LVA_Value(m, board.board))+900;
			#endif
			capture_moves[m] = 1;
		} else {
			#if HISTORY
			moveTable[m] += Ordering::history[board.turn][m];
			#endif
			#if KILLERS
			if (Ordering::killers[*board.ply][m]) {
				moveTable[m] += Tuning::KILLER_BONUS;
			}
			#endif
			#if COUNTERMOVE
			if (m == counterMove) {
				moveTable[m] += Tuning::COUNTERMOVE_BONUS;
			}
			#endif
		}
	}
	moves = order_MoveVal(moves, moveTable);
	best = -Tuning::INF; //Best value
	flag = ALL; //Node type flag
	moves_tried = 0;
	uint8_t differentvalidx = 0;
	
	//Now we iterate through the moves.
	for (uint8_t i=0;i<moves.len;i++) {
		if (search::stopsearch) {return 0;}
		Move move = moves.arr[i];
		if (board.iscastling(move) && in_check) {continue;}
 		board.push(move);
		if (board.opp_in_check()) { //If the moves puts the king under check or castles in check, we discard it.
			board.pop();
			continue;
		}
		R = 0;
		st->ply++;
		is_capture = capture_moves[move];
		
		/*****************************************************
		*	Late Move Reductions: if we have a very likely	 *
		*	fail-low node and we have examined the first x   *
		*	moves at full depth, no one raising alpha, we	 *
		*	just assume that we can reduce the rest.		 *
		*****************************************************/
		
		#if LMR
		if (i && moveTable[moves.arr[i-1]] != moveTable[moves.arr[i]]) {
			differentvalidx++;
		}
		if (flag == ALL
			&& differentvalidx >= Tuning::LMR_MOVE
			&& depth >= Tuning::MIN_LMR_DEPTH
			&& !is_capture
			&& !in_check)
		{
			R += LMReduction(depth, differentvalidx-Tuning::LMR_MOVE+1, pv, moveTable[move]);
		}
		#endif
		
		/*****************************************************
		*	Principal Variation Search: before searching a	 *
		*	move full-window, check if does it have the		 *
		*	potential to raise alpha with a zero-window.	 *
		*****************************************************/
		++moves_tried;
		stk1->lastMove = move;
		if (flag == PV) {
			score = -Search<0,1>(board, st, depth-R-1, -alpha-1, -alpha);
			if (score > alpha) {score = -Search<1,1>(board, st, depth-1, -beta, -alpha);} //In case of a success, we do not reduce.
		} else {
			score = -Search<1,1>(board, st, depth-R-1, -beta, -alpha);
			if (score > alpha && R > 0) {   //If the search results higher than alpha and we reduced, we re-search with no reduction.
				score = -Search<1,1>(board, st, depth-1, -beta, -alpha);
			}
		}
		
		st->ply--;
		board.pop();
		if (score > best) {
			best = score;
			bestMove = move;
			if (score > alpha) {
				if (score >= beta) {
					alpha = beta;
					flag = CUT;
					if (!is_capture) {
						#if KILLERS
						Ordering::killers[*board.ply][bestMove] = 1;
						#endif
						#if COUNTERMOVE
						Ordering::countermove[stk->lastMove] = move;
						#endif
						#if HISTORY
						if (depth > 2) {
							Ordering::history[board.turn][bestMove] += depth*depth; }
						#endif
					}
					break;
				}
				flag = PV;
				alpha = score; 
			}
		}
	}
	if (!moves_tried) { //In case of stalemate/checkmate, return the appropriate score.
		if (in_check) {alpha = -Tuning::LOWER_MATE+st->ply;}
		else {alpha = 0;}
	}
	#if TTABLES
	if (search::stopsearch) {return 0;}
	ttsave(hash, depth, alpha, flag, bestMove);
	#endif
	return alpha;
}

SearchResult SearchRoot(Board board, Depth depth, Index idx, arrayret legals, bool hasprevres=false, SearchResult prevres=search::results[0], Value alpha=-Tuning::INF, Value beta=Tuning::INF) {
	
	auto startTime = std::chrono::high_resolution_clock::now();
	Stats st = search::searchStats[idx];
	st.ply = 0;
	st.seldepth = 0;
	st.nodes = 0;
	ageHistoryTable();
	Move bestMove, currMove;
	Value score = -Tuning::INF;
	Value val;
	bool swq,swk,sbq,sbk;
	uint8_t ep;
	arrayret oppmoves;
	board.turn = !board.turn;
	oppmoves = board.gen_moves();
	board.turn = !board.turn;
	SearchResult rs;
	bool sendcurrmove = (hasprevres && prevres.time >= 300);
	swq = board.wqc;
	swk = board.wkc;
	sbk = board.bkc;
	sbq = board.bqc;
	ep = board.ep;
	bestMove = 0;
	for (uint8_t i=0;i<legals.len;i++) {
		if (search::stopsearch) {break;}
		currMove = legals.arr[i];
		uint8_t kl = locateking(board.board, board.turn);
		if (board.iscastling(currMove)) {
			if (isattacked(oppmoves, kl)) {continue;}
			if (currMove == WKC) {
				if (isattacked(oppmoves, 1) || isattacked(oppmoves, 2)) {continue;}
			} else if (currMove == WQC) {
				if (isattacked(oppmoves, 4) || isattacked(oppmoves, 5) || isattacked(oppmoves, 6)) {continue;}
			} else if (currMove == BKC) {
				if (isattacked(oppmoves, 57) || isattacked(oppmoves, 58)) {continue;}
			} else if (currMove == BQC) {
				if (isattacked(oppmoves, 60) || isattacked(oppmoves, 61) || isattacked(oppmoves, 62)) {continue;}
			}
		}
		board.push(currMove);
		if (board.opp_in_check()) {
			board.pop();
			continue;
		}
		#if PRINTCURRMOVE
		if (sendcurrmove) {
			std::cout << "info depth " << int(depth) << " currmove " << strmove(currMove, board.board) << " currmovenumber " << int(i+1) << '\n';
		}
		#endif
		if (search::reps[board.zobrist]) {
			val = 0;
			goto DRAW_LABEL;
		}
		if (bestMove) {
			val = -Search<0,1>(board, &st, depth-1, -alpha-1, -alpha);
			if (val > alpha) {val = -Search<1,1>(board, &st, depth-1, -beta, -alpha);}
		} else {
			val = -Search<1,1>(board, &st, depth-1, -beta, -alpha);
		}
		DRAW_LABEL:
		#if PRINTPVS
		std::cout << strmove(currMove,board.board) << ' ';
		for (Depth d=depth-1;d>0;d--) {
			Move m = movetprobe(board.getkey());
			std::cout << strmove(m,board.board) << ' ';
			board.push(m);
		}
		for (Depth d=depth-1;d>0;d--) {
			board.pop();
		}
		std::cout << '\n';
		#endif
		board.pop();
		board.wqc = swq;
		board.wkc = swk;
		board.bkc = sbk;
		board.bqc = sbq;
		board.ep = ep;
		if (val > score) {
			score = val;
			bestMove = currMove;
			if (val > alpha) {
				alpha = score;
				if (alpha >= beta) {
					break;
				}
			}
		}
	}
	#if PRINTMVORD
	std::cout << '\n';
	#endif
	rs.move = bestMove;
	rs.cp = score;
	rs.time = timecount(startTime);
	rs.pv[0] = bestMove;
	rs.depth = depth;
	rs.nodes = st.nodes;
	for (Depth d=0;d<depth;d++) {
		board.push(rs.pv[d]);
		rs.pv[d+1] = movetprobe(board.getkey());
	}
	rs.ponder = rs.pv[1];
	rs.seldepth = st.seldepth;
	if (UCIparams::UCI_ShowWDL) {
		rs.wdl = WDLscore(score, *board.ply);
	}
	return rs;
}

void iterdeep(Board board, Depth depth, Index idx, Value additional_widen=0, bool verbose=1) {
	SearchResult prevres, trueprev;
	arrayret legals = board.gen_moves();
	prevres = SearchRoot(board, 1, idx, legals);
	int scores[4096];
	for (int i=0;i<legals.len;i++) {
		Move m = legals.arr[i];
		scores[m] = MVV_LVA_Value(m, board.board)/27;
	}
	if (search::reached < 1) {
		search::reached = 1;
		search::bestresult = prevres;
		if (verbose) {
			printmoveinfo(prevres, board, 0);}
	}
	search::results[idx] = prevres;
	Value alpha,beta;
	Value results[Tuning::MAXDEPTH];
	results[0] = 0;
	results[1] = prevres.cp;
	Value guess, mul, delta;
	auto startTime = std::chrono::high_resolution_clock::now();
	for (Depth d=2;d<=depth;d++) {
		scores[prevres.move] += d*(d-1)*(d-1);/*
		for (uint8_t pvx=0;pvx<d;pvx+=2) {
			Move pvm = prevres.pv[pvx];
			if (!(pvm)) {break;}
			scores[pvm] += 0;
		}*/
		legals = order_MoveVal(legals, scores);
		guess = ((results[d-1]*Tuning::WINDOW_DIFFW)+(results[d-2]*Tuning::WINDOW_SAMEW))/(Tuning::WINDOW_SAMEW+Tuning::WINDOW_DIFFW); // First guess.
		alpha = guess-Tuning::WIDENING-additional_widen;
		beta = guess+Tuning::WIDENING+additional_widen;
		if (search::stopsearch) {return;}
		trueprev = prevres;
		prevres = SearchRoot(board, d, idx, legals, true, prevres, alpha, beta);
		if (search::stopsearch) {return;}
		mul = 1;
		delta = beta-alpha;
		while (prevres.cp <= alpha || prevres.cp >= beta) {
			if (verbose) {
				printmoveinfo(prevres, board, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-startTime).count(), alpha, beta);
			}
			if (prevres.cp <= alpha) {
				beta -= (delta/Tuning::DELTA_FRACTION);
				alpha -= Tuning::REOPENING*mul;
			}
			else {
				alpha += (delta/Tuning::DELTA_FRACTION);
				beta += Tuning::REOPENING*mul;
			}
			if (search::stopsearch) {return;}
			prevres = SearchRoot(board, d, idx, legals, true, trueprev, alpha, beta);
			if (search::stopsearch) {return;}
			delta = beta-alpha;
			mul*=2;
		}
		if (search::reached < d) {
			search::reached = d;
			search::bestresult = prevres;
			if (verbose) {
				printmoveinfo(prevres, board, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-startTime).count());
			}
		}
		if (search::stopsearch) {return;}
		results[d] = prevres.cp;
		search::results[idx] = prevres;
	}
}

void runSearch(Board board, Depth depth, Time time, uint8_t threads) {
	search::reached = 0;
	search::stopsearch = 0;
	std::thread processes[16];
	Depth d = depth;
	if (depth == 0) {d = Tuning::MAXDEPTH;}
	for (uint8_t i=0;i<threads;i++) {
		processes[i] = std::thread(iterdeep, board, d, i, Tuning::THREAD_WIDENING*i, 1);
	}
	if (time) {std::this_thread::sleep_for(std::chrono::milliseconds(time));}
	else if (depth) {while (search::reached < depth) {std::cout<<"";}} 
	else {
		std::string command;
		while (1) {
			std::cin >> command;
			if (command.length()){
				writelog(">> "+command+"\n");}
			if (command == "stop") {
				break;
			}
		}
	}
	while (!search::reached) {;}
	for (uint8_t i=0;i<threads;i++) {
		stopsearch(processes[i]);
	}
}

void runPonderSearch(Board board, Time time, uint8_t threads) {
	auto startTime = std::chrono::high_resolution_clock::now();
	search::reached = 0;
	search::stopsearch = 0;
	std::thread processes[16];
	for (uint8_t i=0;i<threads;i++) {
		processes[i] = std::thread(iterdeep, board, Tuning::MAXDEPTH, i, Tuning::THREAD_WIDENING*i, 1);
	}
	std::string command;
	while (1) {
		std::cin >> command;
		if (command.length()){
			writelog(">> "+command+"\n");}
		if (command == "stop") {
			break;
		} else if (command == "ponderhit") {
			Time elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-startTime).count();
			Time remaining = std::max(time-elapsed,Time(0));
			std::this_thread::sleep_for(std::chrono::milliseconds(remaining));
			break;
		}
	}
	for (uint8_t i=0;i<threads;i++) {
		stopsearch(processes[i]);
	}
}

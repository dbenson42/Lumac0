using namespace std;

namespace search {
	bool stopsearch = false;
	Depth reached = 0;
	RepetitionTable reps;
	Stats searchStats[16];
	SearchResult results[16];
	SearchResult bestresult;
}

namespace Ordering {
	bool killers[300][4096] = {0};
	Value history[2][4096] = {0};
	Move countermove[4096] = {0};
}

Value MVV_LVA_Value(Move move, char board[64]) {
	return Tuning::PIECE_VAL[board[move%64]^START]-Tuning::PIECE_VAL[board[move/64]^START];
}

template <typename NUM>
arrayret order_moves_table(arrayret legals, NUM table[4096]) {
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

arrayret order_capture_MVVLVA(arrayret moves, char board[64]) {
	Value t[4096] = {0};
	for (uint8_t i=0;i<moves.len;i++) {
		Move m = moves.arr[i];
		t[m] = MVV_LVA_Value(m, board);
	}
	moves = order_moves_table(moves, t);
	return moves;
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

void age_history_table() {
	for (uint8_t trn=0;trn<2;trn++) {
		for (Move move=0;move<4096;move++) {
			Ordering::history[trn][move] /= 8;
		}
	}
}

Depth LMReduction(uint8_t tried, Depth depth, bool pv, Value score) {
	Depth r = Depth(sqrt(float(depth)-1)+sqrt(float(tried)-1)-(score/310));
	if (r < 0) {return 0;}
	if (pv) {r = r*3/5;}
	return r;
}

Depth LMReduction_root(uint8_t tried, Depth depth) {
	return 1;
}

Depth null_move_reduction(Depth depth, Value delta) {
	Depth r = 1+(depth>6)+(delta>Tuning::ROOK_VAL);
	return r;
}

Value static_null_move_margin(Depth depth) {
	return depth*Tuning::STATIC_NULL_MOVE_MARGIN;
}

Value razoring_margin(Depth depth) {
	return pow(depth, 1.6)*Tuning::RAZORING_MARGIN+100;
}

Value Quiesce(Board board, Value alpha, Value beta, Stats* st) {
	if (st->ply > st->seldepth) {st->seldepth = st->ply;}
	st->nodes++;
	Value stand_pat = evaluate(board.board, board.turn);
	if (stand_pat >= beta) {return beta;}
	arrayret moves = board.gen_captures();
	if (alpha < stand_pat) {alpha = stand_pat;}
	if (moves.len == 0) {return alpha;}
	Value score;
	#if QUIESCE_MVV_LVA
	moves = order_capture_MVVLVA(moves, board.board);
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
		if (moves_tried-1 > Tuning::QUIESCE_DELTA_MOVE && Tuning::PIECE_VAL[board.board[move%64]]+Tuning::QUIESCE_DELTA_MARGIN+stand_pat < alpha) {
			break;
		} 	
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
	
	Key hash = board.zobrist; //We get the zobrist key of our position
	Value eval, score, best; //Some values we want to use later
	Move bestMove, hashMove, counterMove;
	TTflag flag; //The flag telling wether a node is ALL, PV or CUT.
	int8_t R; //Depth reduction. Next search depth is depth-R-1.
	bool in_check, giving_check, is_capture, eval_is_qs;
	Stack* stk = st->stacks + st->ply;
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
	Value val = tt_probe(hash, depth, alpha, beta, hashMove);
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
	eval_is_qs = 0;
	arrayret moves; //The array of pseudo-legal moves
	++st->nodes;
	eval = evaluate(board.board, board.turn); //We gat a static score
	stk->staticEval = eval; //Update the stack;
	pstk = (stk-1); //Previoius element of the stack
	stk1 = (stk+1); //Next element of the stack
	
	if (in_check || pv) { // In case of check or PV node, we directly go to the moves loop.
		moves = board.gen_moves();
		goto MOVES_LOOP;
	}

	/*****************************************************
	*	Razoring: if we have a really bad static eval, we*
	* 	can return alpha.    							 *
	*****************************************************/
	
	#if RAZORING
	if (depth <= Tuning::RAZORING_MAX_DEPTH
		&& eval + razoring_margin(depth) < alpha)
	{
		eval = Quiesce(board, alpha-1, alpha, st);
		if (eval < alpha) {
			return alpha;
		}
		eval_is_qs = 1;
	}
	#endif
	
	/*****************************************************
	*	Static null move pruning: here we verify if is	 *
	*	our static score minus a margin, proportional to *
	*	the depth, higher than beta. If it indeed is,	 *
	*	we consider it enough to just return beta.		 *
	*****************************************************/
	
	#if STATIC_NULL_MOVE
	if (depth <= Tuning::STATIC_NULL_MOVE_MAX_DEPTH 
		&& eval - static_null_move_margin(depth-eval_is_qs) > beta) 
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
		R = null_move_reduction(depth, eval-beta);
		stk1->lastMove = 0;
		board.change_turn();
		Value v = -Search<0,0>(board, st, depth-R-1, -beta, -beta+1);
		board.change_turn();
		if (v >= beta) {
			return beta;
		}
	}
	#endif
	
	/*****************************************************
	*	Futility pruning: if we are at a relatively low  *
	*	depth, not searching for any mate and our score  *
	*	is lower than alpha minus a margin, we only		 *
	*	consider tactical moves, thinking quiets futile. *
	*****************************************************/
	
	#if FUTILITY
	if (depth <= Tuning::FUTILITY_MAX_DEPTH 
		&& abs(alpha) < Tuning::LOWER_MATE 
		&& eval+Tuning::FUTILITY_MARGIN[depth] <= alpha) 
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
	#if COUNTERMOVE
	counterMove = Ordering::countermove[stk->lastMove];
	#endif
	moveTable[hashMove] = 1000000000;
	for (uint8_t i=0;i<moves.len;i++) {
		Move m = moves.arr[i];
		if (board.is_capture(m)) {
			#if MVV_LVA
			moveTable[m] += int(MVV_LVA_Value(m, board.board))+Tuning::CAPTURE_BONUS;
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
	moves = order_moves_table(moves, moveTable);
	best = -Tuning::INF; //Best value
	flag = ALL; //Node type flag
	moves_tried = 0;
	uint8_t differentvalidx = 0;
	
	//Now we iterate through the moves.
	for (uint8_t i=0;i<moves.len;i++) {
		if (search::stopsearch) {return 0;}
		Move move = moves.arr[i];
		if (board.is_castling_move(move) && in_check) {continue;}
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
			&& depth >= Tuning::LMR_MIN_DEPTH
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
	tt_save(hash, depth, alpha, flag, bestMove);
	#endif
	return alpha;
}

SearchResult SearchRoot(Board board, Depth depth, Index idx, arrayret legals, bool hasprevres=false, SearchResult prevres=search::results[0], Value alpha=-Tuning::INF, Value beta=Tuning::INF) {
	
	auto startTime = std::chrono::high_resolution_clock::now();
	Stats st = search::searchStats[idx];
	st.ply = 0;
	st.seldepth = 0;
	st.nodes = 0;
	age_history_table();
	Move bestMove, currMove;
	Value score = -Tuning::INF;
	Value val;
	bool swq,swk,sbq,sbk;
	SearchResult rs;
	bool sendcurrmove = (hasprevres && prevres.time >= 300);
	bestMove = 0;
	uint8_t moves_tried = 0;
	uint8_t R = 0;
	for (uint8_t i=0;i<legals.len;i++) {
		if (search::stopsearch) {break;}
		currMove = legals.arr[i];
		if (!board.is_legal(currMove)) {continue;}
		board.push(currMove);
		++moves_tried;
		#if PRINTCURRMOVE
		if (sendcurrmove) {
			std::cout << "info depth " << int(depth) << " currmove " << strmove(currMove, board.board) << " currmovenumber " << int(i+1) << '\n';
		}
		#endif
		R = 0;
		#if ROOT_LMR
		if (moves_tried > Tuning::ROOT_LMR_MOVE && depth > Tuning::ROOT_LMR_MIN_DEPTH) {
			R = LMReduction_root(moves_tried, depth);
		}
		#endif
		if (search::reps[board.string_id()]) {
			val = 0;
		} else if (bestMove) {
			val = -Search<0,1>(board, &st, depth-R-1, -alpha-1, -alpha);
			if (val > alpha) {val = -Search<1,1>(board, &st, depth-1, -beta, -alpha);}
		} else {
			val = -Search<1,1>(board, &st, depth-R-1, -beta, -alpha);
			if (val > alpha && R > 0) {
				val = -Search<1,1>(board, &st, depth-1, -beta, -alpha);
			}
		}
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
	rs.move = bestMove;
	rs.cp = score;
	rs.time = timecount(startTime);
	rs.pv[0] = bestMove;
	rs.depth = depth;
	rs.nodes = st.nodes;
	if (rs.time) {
		rs.nps = rs.nodes*1000/rs.time;
	} else {
		rs.nps = 1000000;
	}
	for (Depth d=0;d<depth;d++) {
		board.push(rs.pv[d]);
		rs.pv[d+1] = move_probe(board.zobrist);
	}
	rs.ponder = rs.pv[1];
	rs.seldepth = st.seldepth;
	if (UCIparams::UCI_ShowWDL) {
		rs.wdl = WDLscore(score, *board.ply);
	}
	return rs;
}

void iterative_deepening(Board board, Depth depth, Index idx, Value additional_widen=0, bool verbose=1) {
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
			print_moveinfo(prevres, board, 0);}
	}
	search::results[idx] = prevres;
	Value alpha,beta;
	Value results[Tuning::MAXDEPTH+1];
	results[0] = 0;
	results[1] = prevres.cp;
	Value guess, mul, delta;
	auto startTime = std::chrono::high_resolution_clock::now();
	for (Depth d=2;d<=depth;d++) {
		scores[prevres.move] += d*(d-1)*(d-1)*1500+MVV_LVA_Value(prevres.move, board.board)+Ordering::history[board.turn][prevres.move];
		legals = order_moves_table(legals, scores);
		guess = ((results[d-1]*Tuning::WINDOW_DIFFW)+(results[d-2]*Tuning::WINDOW_SAMEW))/(Tuning::WINDOW_SAMEW+Tuning::WINDOW_DIFFW); // First guess.
		alpha = guess-Tuning::WINDOW_WIDENING-additional_widen;
		beta = guess+Tuning::WINDOW_WIDENING+additional_widen;
		if (search::stopsearch) {return;}
		trueprev = prevres;
		prevres = SearchRoot(board, d, idx, legals, true, prevres, alpha, beta);
		if (search::stopsearch) {return;}
		mul = 1;
		delta = beta-alpha;
		while (prevres.cp <= alpha || prevres.cp >= beta) {
			if (verbose) {
				print_moveinfo(prevres, board, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-startTime).count(), alpha, beta);
			}
			if (prevres.cp <= alpha) {
				beta -= (delta/Tuning::WINDOW_DELTA_FRACTION);
				alpha -= Tuning::WINDOW_REOPENING*mul;
			}
			else {
				alpha += (delta/Tuning::WINDOW_DELTA_FRACTION);
				beta += Tuning::WINDOW_REOPENING*mul;
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
				print_moveinfo(prevres, board, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-startTime).count());
			}
		}
		if (search::stopsearch) {return;}
		results[d] = prevres.cp;
		search::results[idx] = prevres;
	}
}

void run_search(Board board, Depth depth, Time time, uint8_t threads) {
	search::reached = 0;
	search::stopsearch = 0;
	std::thread processes[16];
	Depth d = depth;
	if (depth == 0) {d = Tuning::MAXDEPTH;}
	for (uint8_t i=0;i<threads;i++) {
		processes[i] = std::thread(iterative_deepening, board, d, i, Tuning::WINDOW_THREAD_WIDENING*i, 1);
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

void run_pondering(Board board, Time time, uint8_t threads) {
	auto startTime = std::chrono::high_resolution_clock::now();
	search::reached = 0;
	search::stopsearch = 0;
	std::thread processes[16];
	for (uint8_t i=0;i<threads;i++) {
		processes[i] = std::thread(iterative_deepening, board, Tuning::MAXDEPTH, i, Tuning::WINDOW_THREAD_WIDENING*i, 1);
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

namespace TTables {
	Key random = 1;
	Key randkey() {
		TTables::random = TTables::random * 1103515245 + 12345;
		return TTables::random;
	}
	uint64_t size_bytes;
	Key zobristkeys[13][64] = {0};
	Key zobristeps[8] = {0};
	Key color;
	void Init() {
		TTables::color = TTables::randkey();
		for (Index i=1;i<13;i++) {
			for (Index s=0;s<64;s++) {
				TTables::zobristkeys[i][s] = TTables::randkey();
			}
		}
		for (Index i=0;i<8;i++) {
			TTables::zobristeps[i] = TTables::randkey();
		}
	}
	uint64_t entries;
	Node* ttable;
	void create_tt(uint64_t megabytes) {
		delete TTables::ttable;
		uint64_t bytes = megabytes*uint64_t(1048576);
		TTables::entries = bytes/sizeof(Node);
		if (TTables::entries & TTables::entries-1) {
			Key tester = 1;
			uint8_t log2 = 0;
			while (TTables::entries >>= 1) {log2++;}
			TTables::entries = (1<<log2);
		}
		TTables::entries--;
		TTables::ttable = new Node[TTables::entries]();
	}
}

Value tt_probe(Key pos, Depth depth, Value alpha, Value beta, Move& move) {
	Node found_node = TTables::ttable[pos & TTables::entries];
	move = 0;
	if (found_node.hash != pos) {return Tuning::INV_VALUE;}
	move = found_node.move;
	if (found_node.depth < depth) {return Tuning::INV_VALUE;}
	Value real_val = found_node.val;
	if (found_node.type == PV) {return real_val;}
	if (found_node.type == ALL && real_val <= alpha) {return alpha;}
	if (found_node.type == CUT && real_val >= beta) {return beta;}
	return Tuning::INV_VALUE;
}

void tt_save(Key pos, Depth depth, Value val, TTflag flag, Move move) {
	Node n = TTables::ttable[pos & TTables::entries];
	if (n.depth >= depth && n.hash == pos) {
		return;
	}
	Node node = {val, flag, depth, pos, move};
	TTables::ttable[pos & TTables::entries] = node;
}

Move move_probe(Key pos) {
	Node n = TTables::ttable[pos & TTables::entries];
	if (n.hash != pos) {return 0;}
	return n.move;
}

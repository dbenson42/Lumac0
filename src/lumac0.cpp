#include <iostream>
#include <ctime>
#include <chrono>
#include <map>
#include <math.h>
#include <thread>

#include "defs.hpp"
#include "ttables.hpp"
#include "chess.hpp"
#include "evaluate.hpp"
#include "search.hpp"
#include "uci.hpp"

using namespace std;
int main() {
	Chess::Init();
	TTables::Init();
	Eval::Init();
	UCI::Start();
}
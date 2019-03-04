//============================================================
// movelist.cpp
// ChessEngine
//============================================================

#include "movelist.h"
#include <algorithm>
#include <cassert>

using namespace BlendXChess;

//============================================================
// Constructor
//============================================================
MoveList::MoveList(void)
	: moveCnt(0), moveIdx(0)
{}

//============================================================
// Add a move to the list
//============================================================
void MoveList::add(Move move)
{
	assert(moveCnt < MAX_MOVECNT);
	moves[moveCnt++].move = move;
}

void MoveList::add(Move move, MoveScore score)
{
	assert(moveCnt < MAX_MOVECNT);
	moves[moveCnt].move = move;
	moves[moveCnt++].score = score;
}

//============================================================
// Get next move (in the order specified by move scores)
//============================================================
Move MoveList::getNext(void)
{
	return moveIdx < moveCnt ? moves[moveIdx++].move : MOVE_NONE;
}

//============================================================
// Get next best move (finds it among the rest ones, meaning that
// it should be used when moves array is not sorted before)
//============================================================
Move MoveList::getNextBest(void)
{
	assert(moveIdx <= moveCnt);
	if (moveIdx == moveCnt)
		return MOVE_NONE;
	// One selection sort iteration, which reorders the rest moves in a manner that the best one becomes the current
	for (int i = moveIdx + 1; i < moveCnt; ++i)
		if (moves[i].score > moves[moveIdx].score)
			std::swap(moves[i], moves[moveIdx]);
	return moves[moveIdx++].move;
}

//============================================================
// Sort the move list
//============================================================
void MoveList::sort(void)
{
	std::sort(moves, moves + moveCnt, [](const MLNode& ml1,
		const MLNode& ml2) { return ml1.score > ml2.score; });
}

//============================================================
// Access the move by index
//============================================================
MLNode& MoveList::operator[](int idx)
{
	assert(0 <= idx && idx < moveCnt);
	return moves[idx];
}

//============================================================
// Get AN representation of moves
//============================================================
std::vector<std::string> MoveList::toAN(void) const
{
	std::vector<std::string> ret;
	for (int i = 0; i < moveCnt; ++i)
		ret.push_back(moves[i].move.toAN());
	return std::move(ret);
}

//============================================================
// movelist.cpp
// ChessEngine
//============================================================

#include "movelist.h"
#include <cassert>

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

void MoveList::add(Move move, Score score)
{
	assert(moveCnt < MAX_MOVECNT);
	moves[moveCnt].move = move;
	moves[moveCnt++].score = score;
}

//============================================================
// Get next move (in the order specified by move scores)
//============================================================
MLNode MoveList::getNext(void)
{
	return moveIdx < moveCnt ? moves[moveIdx++] : MLNode {MOVE_NONE, SCORE_ZERO};
}

//============================================================
// Get next best move(finds it among the rest ones, meaning that
// it should be used when moves array is not sorted before)
//============================================================
Move MoveList::getNextBest(void)
{
	if (moveIdx == moveCnt)
		return MOVE_NONE;
	// One bubble sort iteration, which reorders the rest moves in a manner that the best one becomes the current
	for (int i = moveIdx + 1; i < moveCnt; ++i)
		if (moves[i].score > moves[i - 1].score)
			std::swap(moves[i], moves[i - 1]);
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
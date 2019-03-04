//============================================================
// move_manager.h
// ChessEngine
//============================================================

#include "move_manager.h"
#include "search.h"

using namespace BlendXChess;

//============================================================
// Constructor
//============================================================
template<bool ROOT>
MoveManager<ROOT>::MoveManager(Searcher& searcher, Move ttMove)
	: searcher(searcher), ttMove(ttMove), state(MMState::TT_MOVE)
{}

//============================================================
// Returns next picked move or MOVE_NONE if none left
//============================================================
template<bool ROOT>
Move MoveManager<ROOT>::next(void)
{
	Move nextMove;
	Position& pos = searcher.pos;
	switch (state)
	{
	case MMState::TT_MOVE:
		state = MMState::GENMOVES;
		if (pos.isPseudoLegal(ttMove)) // we check this because there could be hash collision
		{
#ifdef ENGINE_DEBUG
			pos.generatePseudolegalMoves(moveList);
			bool foundTT = false;
			for (int i = 0; i < moveList.count(); ++i)
				if (moveList[i].move == ttMove)
				{
					foundTT = true;
					break;
				}
			assert(foundTT);
			moveList.clear();
#endif
			if constexpr (ROOT)
			{
				if (pos.isLegal(ttMove))
					return ttMove;
			}
			else
				return ttMove;
		}
#ifdef ENGINE_DEBUG
		else
		{
			pos.generatePseudolegalMoves(moveList);
			bool foundTT = false;
			for (int i = 0; i < moveList.count(); ++i)
				if (moveList[i].move == ttMove)
				{
					foundTT = true;
					break;
				}
			assert(!foundTT);
			moveList.clear();
		}
#endif
		// [[fallthrough]] // if ttMove is inappropriate, we should proceed
	case MMState::GENMOVES:
		if constexpr (ROOT)
			pos.generateLegalMoves(moveList);
		else
			pos.generatePseudolegalMoves(moveList);
		searcher.scoreMoves(moveList);
		state = MMState::GENERATED;
		// [[fallthrough]]
	case MMState::GENERATED:
		nextMove = moveList.getNextBest();
		if (nextMove == ttMove)
			nextMove = moveList.getNextBest();
		if constexpr (ROOT)
		{
			if (nextMove == MOVE_NONE)
				state = MMState::DEFERRED;
			else
				return nextMove;
		}
		else
			return nextMove;
		// [[fallthrough]] // we should proceed if we are to return deferred moves from root chess
	case MMState::DEFERRED:
		assert(ROOT); // Deferred moves are only for root search (currently)
		if constexpr (ROOT)
			return deferredList.getNext(); // ttMove can't be deferred, so no check for that
	default:
		assert(false); // Should not occur
	}
	return MOVE_NONE; // Should not occur
}

//============================================================
// Defers moves that needs to be searched later in root search
//============================================================
template<bool ROOT>
void BlendXChess::MoveManager<ROOT>::defer(Move move)
{
	assert(ROOT);
	if constexpr (ROOT)
		deferredList.add(move, MS_DEFERRED);
}

//============================================================
// Explicit template instantiations
//============================================================
template class MoveManager<true>;
template class MoveManager<false>;
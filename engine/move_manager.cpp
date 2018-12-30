//============================================================
// move_manager.h
// ChessEngine
//============================================================

#include "move_manager.h"
#include "engine.h"

//============================================================
// Constructor
//============================================================
template<bool LEGAL>
MoveManager<LEGAL>::MoveManager(Engine& eng, Move ttMove)
	: eng(eng), ttMove(ttMove), state(MM_TTMOVE)
{}

//============================================================
// Returns next picked move or MOVE_NONE if none left
//============================================================
template<bool LEGAL>
Move MoveManager<LEGAL>::next(void)
{
	Move nextMove;
	switch (state)
	{
	case MM_TTMOVE:
		state = MM_GENMOVES;
		if (eng.isPseudoLegal(ttMove)) // we check this because there could be hash collision
		{
#ifdef ENGINE_DEBUG
			eng.generatePseudolegalMoves(moveList);
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
			if constexpr (LEGAL)
			{
				if (eng.isLegal(ttMove))
					return ttMove;
			}
			else
				return ttMove;
		}
#ifdef ENGINE_DEBUG
		else
		{
			eng.generatePseudolegalMoves(moveList);
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
	case MM_GENMOVES:
		if constexpr (LEGAL)
			eng.generateLegalMoves(moveList);
		else
			eng.generatePseudolegalMoves(moveList);
		eng.scoreMoves(moveList);
		state = MM_GENERATED;
		// [[fallthrough]]
	case MM_GENERATED:
		nextMove = moveList.getNextBest();
		if (nextMove == ttMove)
			return moveList.getNextBest();
		else
			return nextMove;
	default:
		assert(false); // Should not occur
	}
	return MOVE_NONE; // Should not occur
}

//============================================================
// Explicit template instantiations
//============================================================
template class MoveManager<true>;
template class MoveManager<false>;
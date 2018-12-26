//============================================================
// move_manager.h
// ChessEngine
//============================================================

#include "move_manager.h"
#include "engine.h"

//============================================================
// Constructor
//============================================================
MoveManager::MoveManager(Engine& eng, Move ttMove)
	: eng(eng), ttMove(ttMove), state(MM_TTMOVE)
{}

//============================================================
// Returns next picked move or MOVE_NONE if none left
//============================================================
Move MoveManager::next(void)
{
	switch (state)
	{
	case MM_TTMOVE:
		state = MM_GENMOVES;
		if (eng.isPseudoLegal(ttMove)) // we check this because there could be hash collision
			return ttMove;
		// [[fallthrough]] // if ttMove is inappropriate, we should proceed
	case MM_GENMOVES:
		eng.generatePseudolegalMoves(moveList);
		eng.scoreMoves(moveList);
		state = MM_GENERATED;
		// [[fallthrough]]
	case MM_GENERATED:
		return moveList.getNextBest();
	default:
		assert(false); // Should not occur
	}
	return MOVE_NONE;
}

//============================================================
// move_manager.h
// ChessEngine
//============================================================

#pragma once
#ifndef _MOVE_MANAGER
#define _MOVE_MANAGER
#include "movelist.h"

namespace BlendXChess
{

	class Searcher;

	enum MMState
	{
		MM_TTMOVE, MM_GENMOVES, MM_GENERATED
	};

	//============================================================
	// Class for ordered selection of (pseudo-)legal moves during search
	//============================================================

	template<bool LEGAL = false>
	class MoveManager
	{
	public:
		// Constructor
		MoveManager(Searcher& searcher, Move ttMove);
		// Returns next picked move or MOVE_NONE if none left
		Move next(void);
	private:
		Searcher& searcher;
		MMState state;
		Move ttMove;
		MoveList moveList;
	};

}

#endif
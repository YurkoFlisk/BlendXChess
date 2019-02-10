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

	enum class MMState
	{
		TT_MOVE, GENMOVES, GENERATED, DEFERRED
	};

	constexpr MoveScore MS_DEFERRED = std::numeric_limits<MoveScore>::min();

	//============================================================
	// Class for ordered selection of (pseudo-)legal moves during search
	//============================================================

	template<bool ROOT = false>
	class MoveManager
	{
	public:
		// Constructor
		MoveManager(Searcher& searcher, Move ttMove);
		// Get current state
		inline MMState getState(void) const noexcept;
		// Returns next picked move or MOVE_NONE if none left
		Move next(void);
		// Defers moves that needs to be searched later in root search
		Move defer(Move);
	private:
		Searcher& searcher;
		MMState state;
		Move ttMove;
		MoveList moveList;
	};

	//============================================================
	// Implementation of inline functions
	//============================================================

	template<bool LEGAL>
	inline MMState MoveManager<LEGAL>::getState(void) const noexcept
	{
		return state;
	}
}

#endif
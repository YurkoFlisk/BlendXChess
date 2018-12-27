//============================================================
// move_manager.h
// ChessEngine
//============================================================

#pragma once
#ifndef _MOVE_MANAGER
#define _MOVE_MANAGER
#include "movelist.h"

class Engine;

enum MMState
{
	MM_TTMOVE, MM_GENMOVES, MM_GENERATED
};

//============================================================
// Class for ordered selection of pseudolegal moves during search
//============================================================

template<bool LEGAL = false>
class MoveManager
{
public:
	// Constructor
	MoveManager(Engine& eng, Move ttMove);
	// Returns next picked move or MOVE_NONE if none left
	Move next(void);
private:
	Engine& eng;
	MMState state;
	Move ttMove;
	MoveList moveList;
	bool legalTT;
};

#endif
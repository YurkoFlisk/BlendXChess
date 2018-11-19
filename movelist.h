//============================================================
// movelist.h
// ChessEngine
//============================================================

#pragma once
#ifndef MOVELIST_H
#define MOVELIST_H
#include "basic_types.h"
#include <algorithm>

//============================================================
// Struct for storing a move with it's sorting score
//============================================================

struct MLNode
{
	Move move;
	Score score;
};

//============================================================
// Class representing list of moves generated by generateMoves function
// Maintains sorting moves by their score
//============================================================

class MoveList
{
public:
	// Max possible count of moves in a valid chess position
	static constexpr int MAX_MOVECNT = 218;
	// Constructor
	MoveList(void);
	// Getters
	inline int getMoveCnt(void) const;
	// Inline functions
	inline bool empty(void) const;
	inline void reset(void);
	inline void clear(void);
	// Add a move to the list
	void add(Move, Score);
	void add(Move);
	// Get next move (in the order of moves array, so we should sort moves before calling this function)
	MLNode getNext(void);
	// Get next best move (finds it among the rest ones, meaning that
	// it should be used when moves array is not sorted before)
	Move getNextBest(void);
	// Sort the move list
	void sort(void);
private:
	int moveCnt; // Move count
	int moveIdx; // Next move index
	MLNode moves[MAX_MOVECNT]; // The whole move list
};

//============================================================
// Implementation of inline functions
//============================================================

inline int MoveList::getMoveCnt(void) const
{
	return moveCnt;
}

inline bool MoveList::empty(void) const
{
	return moveCnt == 0;
}

inline void MoveList::reset(void)
{
	moveIdx = 0;
}

inline void MoveList::clear(void)
{
	moveIdx = moveCnt = 0;
}

#endif
//============================================================
// transtable.h
// ChessEngine
//============================================================

#pragma once
#ifndef _TRANSTABLE_H
#define _TRANSTABLE_H
#include "basic_types.h"

//============================================================
// Entry of a transposition table, stores various information
//============================================================

struct TTEntry
{
	TTEntry(void)
		: depth(0)
	{}
	Depth depth; // Search depth. depth == 0 means empty entry
	Bound bound; // Was the score exact or some bound
	Score score; // Score
	int16_t age; // Ply at which this entry was written
	Move move; // Best move for this position so far
	Key key; // Position key
	// Stores the info to entry
	inline void store(Key, Depth, Bound, Score, Move, int16_t);
};

constexpr int TTBUCKET_SIZE = 3;
constexpr int TTINDEX_BITS = 20;
constexpr int TTBUCKET_COUNT = 1 << TTINDEX_BITS;
constexpr Key TTINDEX_MASK = TTBUCKET_COUNT - 1;
extern int fr;

//============================================================
// Bucket of a transposition table, stores several entries
//============================================================

class TTBucket
{
public:
	// Stores the info to a corresponding entry if appropriate
	void store(Key, Depth, Bound, Score, Move, int16_t);
	// Probe the given key and return nullptr if there's not such entry or a pointer to one if it's found
	const TTEntry* probe(Key) const;
private:
	TTEntry entries[TTBUCKET_SIZE];
};

//============================================================
// Transposition table
//============================================================

class TranspositionTable
{
public:
	// Stores the info to a corresponding entry if appropriate
	inline void store(Key, Depth, Bound, Score, Move, int16_t);
	// Probe the given key and return nullptr if there's not such entry or a pointer to one if it's found
	inline const TTEntry* probe(Key) const;
private:
	TTBucket table[TTBUCKET_COUNT];
};

//============================================================
// Implementation of inline functions
//============================================================

inline void TTEntry::store(Key k, Depth d, Bound b, Score s, Move m, int16_t a)
{
	key = k, depth = d, bound = b, score = s, move = m, age = a;
}

inline void TranspositionTable::store(Key key, Depth depth, Bound bound, Score score, Move move, int16_t age)
{
	table[key & TTINDEX_MASK].store(key, depth, bound, score, move, age);
}

inline const TTEntry* TranspositionTable::probe(Key key) const
{
	return table[key & TTINDEX_MASK].probe(key);
}

#endif
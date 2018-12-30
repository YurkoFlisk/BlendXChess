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

#ifdef ENGINE_DEBUG
constexpr bool TT_COUNT_FREE_ENTRIES = true;
#else
constexpr bool TT_COUNT_FREE_ENTRIES = false;
#endif
constexpr int TTBUCKET_ENTRIES = 3;
constexpr int TT_INDEX_BITS = 20;
constexpr int TT_BUCKET_COUNT = 1 << TT_INDEX_BITS;
constexpr Key TT_INDEX_MASK = TT_BUCKET_COUNT - 1;
extern int ttFreeEntries; // !! Only for singleton TT (this approach is maybe temporary) !!

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
	TTEntry entries[TTBUCKET_ENTRIES];
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
	// Clear transposition table;
	inline void clear(void);
private:
	TTBucket table[TT_BUCKET_COUNT];
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
	table[key & TT_INDEX_MASK].store(key, depth, bound, score, move, age);
}

inline const TTEntry* TranspositionTable::probe(Key key) const
{
	return table[key & TT_INDEX_MASK].probe(key);
}

inline void TranspositionTable::clear(void)
{
	// TODO maybe sometimes this is too slow and reduntant (probably no)?
	memset(table, 0, sizeof(table));
	ttFreeEntries = TT_BUCKET_COUNT * TTBUCKET_ENTRIES;
}

#endif
//============================================================
// transtable.cpp
// ChessEngine
//============================================================

#include "transtable.h"

namespace BlendXChess
{

	int ttFreeEntries = TT_BUCKET_COUNT * TTBUCKET_ENTRIES;

	//============================================================
	// Stores the info to a corresponding entry if appropriate
	//============================================================
	void TTBucket::store(Key key, Depth depth, Bound bound, Score score, Move move, int16_t age)
	{
		TTEntry* replace = entries;
		for (int i = 0; i < TTBUCKET_ENTRIES; ++i)
		{
			if (entries[i].depth == 0)
			{
				if constexpr (TT_COUNT_FREE_ENTRIES)
					--ttFreeEntries;
				entries[i].store(key, depth, bound, score, move, age);
				return;
			}
			if (entries[i].key == key)
			{
				if (depth >= entries[i].depth)
					entries[i].store(key, depth, bound, score, move, age);
				return;
			}
			if (entries[i].age < replace->age ||
				(entries[i].age == replace->age && (entries[i].depth < replace->depth/* ||
					entries[i].depth == replace->depth && replace->bound == BOUND_EXACT*/)))
				replace = entries + i;
		}
		if (replace->age < age || (replace->age == age && (replace->depth < depth ||
			(replace->depth == depth && bound == BOUND_EXACT))))
			replace->store(key, depth, bound, score, move, age);
	}

	//============================================================
	// Probe the given key and return nullptr if there's no
	// such entry or a pointer to one if it's found
	//============================================================
	const TTEntry* TTBucket::probe(Key key) const
	{
		for (int i = 0; i < TTBUCKET_ENTRIES; ++i)
		{
			if (entries[i].depth == 0) // No entry has been written here and further
				return nullptr;
			if (entries[i].key == key)
				return entries + i;
		}
		return nullptr;
	}

};
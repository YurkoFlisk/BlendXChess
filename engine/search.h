//============================================================
// search.h
// BlendXChess
//============================================================

#pragma once
#ifndef _SEARCHER_H
#define _SEARCHER_H
#include "position.h"
#include "transtable.h"
#include <list>
#include <atomic>
#include <thread>

#if defined(_DEBUG) | defined(DEBUG)
constexpr bool SEARCH_NODES_COUNT_ENABLED = true;
constexpr bool TT_HITS_COUNT_ENABLED = true;
constexpr bool TIME_CHECK_ENABLED = true;
#else
constexpr bool SEARCH_NODES_COUNT_ENABLED = true;
constexpr bool TT_HITS_COUNT_ENABLED = true;
constexpr bool TIME_CHECK_ENABLED = true;
#endif
constexpr int TIME_CHECK_INTERVAL = 10000; // nodes entered by pvs
constexpr int TIME_LIMIT_DEFAULT = 5000; // ms

//============================================================
// Struct for storing some shared search options and stats (TODO: maybe separate?)
//============================================================

struct SearchOptions
{
	TimePoint startTime;
	int timeLimit; // ms
	int threadCount;
	std::atomic_int timeCheckCounter;
	std::atomic_int ttHits;
	std::atomic_int visitedNodes;
	std::atomic_bool stopSearch;
};

//============================================================
// Struct for storing some per-thread search info
//============================================================

struct ThreadInfo
{
	std::thread handle;
	Searcher searcher;
	Score score; // Score of position evaluated by this thread's search
	Depth resDepth; // Last completed search depth in ID loop
	Move bestMove; // !! TODO (PV?) !!
};

//============================================================
// Class for organizing potentially multi-threaded search process
//============================================================

class MultiSearcher
{
public:
	// Constructor
	MultiSearcher(void);
	// Setters
	inline void setThreadCount(int);
	// Starts search with given depth
	void startSearch(Depth depth);
	// Ends started search (throws if there was no one) and returns search information
	Score endSearch(Move&, Depth& = _dummyDepth, int& = _dummyInt, int& = _dummyInt);
	// Internal AI logic (coordinates searching process, launches Searcher threads)
	void search(const Position&, Depth);
private:
	// Shared transposition table
	TranspositionTable transpositionTable;
	// Shared search options
	SearchOptions shared;
	// Main search thread (searches itself and launches/coordinates other threads)
	// std::thread mainSearchThread;
	// Search threads info by index (main search thread has index 0)
	std::vector<ThreadInfo> threads;
	// Whether search is currently launched
	std::atomic_bool inSearch;
	// Dummy variables for out reference parameters
	static Depth _dummyDepth;
	static int _dummyInt;
};

//============================================================
// Main searching class
// Each object represents a separate search thread
//============================================================

class Searcher
{
public:
	static constexpr int MAX_KILLERS_CNT = 3;
	static constexpr MoveScore MS_TT_BONUS = 1500000000;
	static constexpr MoveScore MS_COUNTERMOVE_BONUS = 300000;
	static constexpr MoveScore MS_SEE_MULT = 1500000 / 100;
	static constexpr MoveScore MS_CAPTURE_BONUS_VICTIM[PIECETYPE_CNT] = {
		0, 100000, 285000, 300000, 500000, 1000000, 0 };
	static constexpr MoveScore MS_CAPTURE_BONUS_ATTACKER[PIECETYPE_CNT] = {
		0, 1000000, 800000, 750000, 400000, 200000 };
	static constexpr MoveScore MS_KILLER_BONUS = 1200000;
	typedef std::list<Move> KillerList;
	// Constructor
	Searcher(const Position&, SearchOptions*, TranspositionTable*, ThreadInfo*);
	// Initializer
	void initialize(const Position&, SearchOptions*, TranspositionTable*, ThreadInfo*);
	// Top-level search function that implements iterative deepening with aspiration windows
	void idSearch(Depth);
private:
	// Helpers for ply-adjustment of scores (mate ones) when (extracted from)/(inserted to) a transposition table
	inline Score scoreToTT(Score);
	inline Score scoreFromTT(Score);
	// Performs given move if legal on pos and updates necessary info
	// Returns false if the move is illegal, otherwise
	// returns true and fills position info for undo
	bool doMove(Move, PositionInfo&);
	// Undoes given move and updates necessary position info
	void undoMove(Move, const PositionInfo&);
	// Internal AI logic (Principal Variation Search)
	// Gets position score by searching with given depth and alpha-beta window
	Score pvs(Depth, Score, Score);
	// Static evaluation
	Score evaluate();
	// Static exchange evaluation
	Score SEE(Square, Side);
	// Static exchange evaluation of specified capture move
	Score SEECapture(Square, Square, Side);
	// Quiescent search
	Score quiescentSearch(Score, Score);
	// Update killer moves
	void updateKillers(int, Move);
	// Move scoring
	void scoreMoves(MoveList&);
	// (Re-)score moves and sort
	inline void sortMoves(MoveList&);
	// Data
	// Current position
	Position pos;
	// Shared search options and info
	SearchOptions* shared;
	// Shared transposition table
	TranspositionTable* transpositionTable;
	// Thread-specific info
	ThreadInfo* threadLocal;
	// ID of the thread where this search is launched 
	// int threadID;
	// Ply from the searcher starting position
	int searchPly;
	// Age for TT (typically ply at searcher starting position)
	int ttAge;
	// Previous moves information, useful for countermove heuristics
	Move prevMoves[MAX_SEARCH_PLY];
	// History table
	MoveScore history[SQUARE_CNT][SQUARE_CNT];
	// Countermoves table
	Move countermoves[SQUARE_CNT][SQUARE_CNT];
	// Killer moves
	KillerList killers[MAX_SEARCH_PLY];
};

//============================================================
// Implementation of inline functions
//============================================================

inline void MultiSearcher::setThreadCount(int threadCount)
{
	if (inSearch)
		throw std::runtime_error("Can't change thread count while in search");
	if (threadCount <= 0)
		throw std::runtime_error("Thread count can't be non-positive");
	shared.threadCount = threadCount;
	threads.resize(threadCount);
}

inline Score Searcher::scoreToTT(Score score)
{
	return score > SCORE_WIN_MIN ? score + searchPly :
		score < SCORE_LOSE_MAX ? score - searchPly : score;
}

inline Score Searcher::scoreFromTT(Score score)
{
	return score > SCORE_WIN_MIN ? score - searchPly :
		score < SCORE_LOSE_MAX ? score + searchPly : score;
}

inline void Searcher::sortMoves(MoveList& ml)
{
	// ml.reset();
	scoreMoves(ml);
	ml.sort();
}

#endif
//============================================================
// engine.h
// ChessEngine
//============================================================

#pragma once
#ifndef _ENGINE_H
#define _ENGINE_H
#include <string>
#include <vector>
#include <list>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include "position.h"
#include "transtable.h"

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

template<bool> class MoveManager;

//============================================================
// Main engine class
//============================================================

class Engine
	: public Position
{
	friend class MoveManager<true>;
	friend class MoveManager<false>;
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
	Engine(void);
	// Destructor
	~Engine(void) = default;
	// Getters
	inline bool isInSearch(void) const;
	inline GameState getGameState(void) const;
	inline DrawCause getDrawCause(void) const;
	inline int getTimeLimit(void) const; // ms
	inline int getThreadCount(void) const;
	inline int getMaxThreadCount(void) const;
	// Setters
	inline void setTimeLimit(int);
	inline void setThreadCount(int);
	// Initialization (should be done before creation of any Engine instance)
	static void initialize(void);
	// Clear position and search info (everything except TT)
	void clear(void);
	// Reset game (stops search if there's any)
	void reset(void);
	// Update game state
	void updateGameState(void);
	// Doing and undoing a move
	// These are not well optimized and are being interface for external calls
	// Engine internals use doMove and undoMove instead
	bool DoMove(Move);
	bool DoMove(const std::string&);
	bool DoMoveSAN(const std::string&);
	bool UndoMove(void);
	// Main AI function
	// Returns position score and outputs the best move to the reference parameter
	Score AIMove(Move&, Depth = DEPTH_MAX, Depth& = _dummyDepth, int& = _dummyInt, int& = _dummyInt);
	// Performance test (if MG_LEGAL is true, all moves are tested for legality
	// during generation and promotions to bishops and rooks are included)
	template<bool MG_LEGAL = false>
	int perft(Depth);
	// Load game from the given stream in SAN notation
	void loadGame(std::istream&);
	// Write game to the given stream in SAN notation
	void writeGame(std::ostream&);
protected:
	// Struct for storing game history information
	struct GHRecord
	{
		Move move;
		// Move in SAN format (we store it here because it is easier than build it when needed in writeGame method)
		std::string moveSAN;
	};
	// Internal doing and undoing moves with remembering
	// (assumes searchPly is 1-biased, as in the usual usecase)
	inline void doMove(Move);
	inline void undoMove(Move);
	// Whether the move is a capture
	inline bool isCaptureMove(Move) const;
	// (Re-)score moves and sort
	inline void sortMoves(MoveList&);
	// Helpers for ply-adjustment of scores (mate ones) when (extracted from)/(inserted to) a transposition table
	inline Score scoreToTT(Score) const;
	inline Score scoreFromTT(Score) const;
	// Whether position is draw by insufficient material
	bool drawByMaterial(void) const;
	// Whether position is threefold repeated
	bool threefoldRepetitionDraw(void) const;
	// Internal AI logic (coordinates searching process, launches PVS-threads)
	void search(void);
	// Internal AI logic (Principal Variation Search)
	// Gets position score by searching with given depth and alpha-beta window
	Score pvs(Depth, Score, Score);
	// Static evaluation
	Score evaluate(void);
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
	// Previous moves (for engine purposes)
	Move prevMoves[MAX_SEARCH_PLY];
	// Transposition table
	TranspositionTable transpositionTable;
	// History heuristic table
	MoveScore history[SQUARE_CNT][SQUARE_CNT];
	// Countermoves table
	Move countermoves[SQUARE_CNT][SQUARE_CNT];
	// Killer moves
	KillerList killers[MAX_SEARCH_PLY];
	// Game state
	GameState gameState;
	// Cause of draw game state (valid only if gameState == GS_DRAW)
	DrawCause drawCause;
	// Variables in search function
	int lastSearchNodes;
	int score;
	// Ply at the root of search
	int searchPly;
	// Interval of entries to pvs function between two consecutive timeout checks
	int timeCheckCounter;
	// Timelimit of searching
	int timeLimit;
	// Number of threads to use in search
	int threadCount;
	// Maximum number of threads available
	int maxThreadCount;
	// Count of TT hits
	int ttHits;
	// Whether move search has already timed out
	std::atomic_bool stopSearch;
	// Whether move search is in progress
	std::atomic_bool inSearch;
	// Main search thread (mainly for coordination of search)
	std::thread mainSearchThread;
	// Actual search threads
	std::vector<std::thread> searchThreads;
	// Notifier of search ending
	// std::condition_variable searchStoppedNotifier;
	// Start time of AI thinking in AIMove function
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	// Game history, which consists of all moves made from the starting position
	std::vector<GHRecord> gameHistory;
	// Position (stored in reduced FEN) repetition count, for handling threefold repetition draw rule
	std::unordered_map<std::string, int> positionRepeats;
private:
	static Depth _dummyDepth;
	static int _dummyInt;
};

//============================================================
// Implementation of inline functions
//============================================================

inline void Engine::doMove(Move move)
{
	Position::doMove(move);
	prevMoves[searchPly++] = move;
}

inline void Engine::undoMove(Move move)
{
	Position::undoMove(move);
	--searchPly;
}

inline bool Engine::isCaptureMove(Move move) const
{
	return board[move.to()] != PIECE_NULL;
}

inline void Engine::sortMoves(MoveList& ml)
{
	// ml.reset();
	scoreMoves(ml);
	ml.sort();
}

inline Score Engine::scoreToTT(Score score) const
{
	return score > SCORE_WIN_MIN ? score + searchPly :
		score < SCORE_LOSE_MAX ? score - searchPly : score;
}

inline Score Engine::scoreFromTT(Score score) const
{
	return score > SCORE_WIN_MIN ? score - searchPly :
		score < SCORE_LOSE_MAX ? score + searchPly : score;
}

inline bool Engine::isInSearch(void) const
{
	return inSearch;
}

inline GameState Engine::getGameState(void) const
{
	return gameState;
}

inline DrawCause Engine::getDrawCause(void) const
{
	return drawCause;
}

inline int Engine::getTimeLimit(void) const
{
	return timeLimit;
}

inline int Engine::getThreadCount(void) const
{
	return threadCount;
}

inline int Engine::getMaxThreadCount(void) const
{
	return maxThreadCount;
}

inline void Engine::setTimeLimit(int tL)
{
	timeLimit = tL;
}

inline void Engine::setThreadCount(int threadCnt)
{
	threadCount = std::clamp(threadCnt, 1, maxThreadCount);
}

#endif
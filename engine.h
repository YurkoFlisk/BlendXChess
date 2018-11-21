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

//============================================================
// Main engine class
//============================================================

class Engine
	: public Position
{
public:
	static constexpr Score MS_TT_BONUS = 15000;
	static constexpr Score MS_CAPTURE_BONUS = 3500;
	static constexpr Score MS_KILLER_BONUS = 1500; // Over MS_CAPTURE_BONUS (see scoreMoves)
	typedef std::list<Move> KillerList;
	// Constructor
	Engine(void);
	// Destructor
	~Engine(void) = default;
	// Getters
	inline GameState getGameState(void);
	inline int getTimeLimit(void); // ms
	// Setters
	inline void setTimeLimit(int);
	// Initialization (should be done before creation of any Engine instance)
	static void initialize(void);
	// Reset game
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
	// Performance test
	int perft(Depth);
	// Convert a move from SAN notation to Move. It should be valid in current position
	Move moveFromSAN(const std::string&);
	// Convert a move from Move to SAN notation. It should be valid in current position
	std::string moveToSAN(Move);
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
	// Whether the move is a capture
	inline bool isCaptureMove(Move) const;
	// (Re-)score moves and sort
	inline void sortMoves(MoveList&, Move = MOVE_NONE);
	// Helpers for ply-adjustment of scores (mate ones) when (extracted from)/(inserted to) a transposition table
	inline Score scoreToTT(Score) const;
	inline Score scoreFromTT(Score) const;
	// Whether position is draw by insufficient material
	bool drawByMaterial(void) const;
	// Whether position is threefold repeated
	bool threefoldRepetitionDraw(void) const;
	// Internal AI logic (Principal Variation Search)
	// Get position score by searching with given depth
	Score pvs(Depth, Score, Score);
	// Static evaluation
	Score evaluate(void);
	// Static exchange evaluation
	Score SEE(Square, Side);
	// Static exchange evaluation of specified capture move
	Score SEECapture(Square, Square, Side);
	// Quiescent search
	Score quiescentSearch(Score, Score);
	// Move scoring
	void scoreMoves(MoveList&, Move);
	// Transposition table
	TranspositionTable transpositionTable;
	// Killer moves
	KillerList killers[MAX_SEARCH_PLY];
	// Game state
	GameState gameState;
	// Variables in search function
	int lastSearchNodes;
	int score;
	// Ply at the root of search
	int searchPly;
	// Interval of entries to pvs function between two consecutive timeout checks
	int timeCheckCounter;
	// Timelimit of AI thinking
	int timeLimit;
	// Count of TT hits
	int ttHits;
	// Whether search already timed out
	bool timeout;
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

inline bool Engine::isCaptureMove(Move move) const
{
	return board[move.to()] != Sq::NONE;
}

inline void Engine::sortMoves(MoveList& ml, Move ttMove)
{
	ml.reset();
	scoreMoves(ml, ttMove);
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

inline GameState Engine::getGameState(void)
{
	return gameState;
}

inline int Engine::getTimeLimit(void)
{
	return timeLimit;
}

inline void Engine::setTimeLimit(int tL)
{
	timeLimit = tL;
}

#endif
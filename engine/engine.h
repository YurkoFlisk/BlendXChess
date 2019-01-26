//============================================================
// engine.h
// ChessEngine
//============================================================

#pragma once
#ifndef _ENGINE_H
#define _ENGINE_H
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <chrono>
#include "position.h"
#include "transtable.h"
#include "search.h"

//============================================================
// Main engine class
//============================================================

class Game
	// : public Position
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
	// Initialization (should be done before creation (though some
	// can be reset) of any engine-related object instance)
	static void initialize(void);
	// Constructor
	Game(void);
	// Destructor
	~Game(void) = default;
	// Getters
	inline bool isInSearch(void) const;
	inline GameState getGameState(void) const;
	inline DrawCause getDrawCause(void) const;
	inline const SearchOptions& getSearchOptions(void) const;
	inline const Position& getPosition(void) const;
	inline int getMaxThreadCount(void) const;
	// Setters
	inline void setSearchOptions(const SearchOptions&);
	inline void setSearchProcesser(const EngineProcesser&);
	// Clear game state
	void clear(void);
	// Reset game (stops search if there's any)
	void reset(void);
	// Update game state
	void updateGameState(void);
	// Doing and undoing a move
	// These are not well optimized and are being interface for external calls
	// Engine internals (eg in search) use doMove and undoMove instead
	bool DoMove(Move);
	bool DoMove(const std::string&, MoveFormat);
	bool UndoMove(void);
	// Searching interface functions
	void startSearch(void);
	SearchResults endSearch();
	// Load game from the given stream in SAN notation
	void loadGame(std::istream&, MoveFormat fmt = FMT_SAN);
	// Write game to the given stream in SAN notation
	void writeGame(std::ostream&, MoveFormat fmt = FMT_SAN);
	// Load position from a given stream in FEN notation (bool parameter says whether to omit move counters)
	void loadFEN(std::istream&, bool = false);
	// Load position from a given string in FEN notation (bool parameter says whether to omit move counters)
	void loadFEN(const std::string&, bool = false);
	// Write position to a given stream in FEN notation, possibly omitting half- and full-move counters
	void writeFEN(std::ostream&, bool = false) const;
	// Get FEN representation of current position, possibly omitting last 2 counters
	inline std::string getPositionFEN(bool = false) const;
	// Redirections to Position class
	template<bool MG_LEGAL = false>
	inline int perft(Depth);
protected:
	// Struct for storing game history information
	struct GHRecord
	{
		// Move 
		Move move;
		// State info of position from which 'move' was made
		PositionInfo prevState;
		// Move in various string formats (we store it here because it
		// is easier than retrieve it when needed in writeGame method)
		std::array<std::string, MOVE_FORMAT_CNT> moveStr;
	};
	// Whether position is draw by insufficient material
	bool drawByMaterial(void) const;
	// Whether position is threefold repeated
	bool threefoldRepetitionDraw(void) const;
	// Current game position (!! not the one changed in-search !!)
	Position pos;
	// Move searcher
	MultiSearcher searcher;
	// Search options (they are rarely changed during 1 game)
	SearchOptions searchOptions;
	// Result of last preformed search
	SearchResults lastSearchResults;
	// Game state
	GameState gameState;
	// Cause of draw game state (valid only if gameState == GS_DRAW)
	DrawCause drawCause;
	// Game history, which consists of all moves made from the starting position
	std::vector<GHRecord> gameHistory;
	// Position (stored in reduced FEN) repetition count, for handling threefold repetition draw rule
	std::unordered_map<std::string, int> positionRepeats;

};

//============================================================
// Implementation of inline functions
//============================================================

//inline void Engine::doMove(Move move, SearchState& ss)
//{
//	ss.pos.doMove(move);
//	ss.prevMoves[ss.searchPly++] = move;
//}
//
//inline void Engine::undoMove(Move move, SearchState& ss)
//{
//	ss.pos.undoMove(move);
//	--ss.searchPly;
//}

inline bool Game::isInSearch(void) const
{
	return searcher.isInSearch();
}

inline GameState Game::getGameState(void) const
{
	return gameState;
}

inline DrawCause Game::getDrawCause(void) const
{
	return drawCause;
}

inline const SearchOptions& Game::getSearchOptions(void) const
{
	return searchOptions;
}

inline const Position& Game::getPosition(void) const
{
	return pos;
}

inline int Game::getMaxThreadCount(void) const
{
	return searcher.getMaxThreadCount();
}

inline void Game::setSearchOptions(const SearchOptions& options)
{
	searchOptions = options;
}

inline void Game::setSearchProcesser(const EngineProcesser& proc)
{
	searcher.setProcesser(proc);
}

inline std::string Game::getPositionFEN(bool omitCounters) const
{
	return pos.getFEN(omitCounters);
}

template<bool MG_LEGAL>
inline int Game::perft(Depth depth)
{
	if (isInSearch())
		return 0;
	return pos.perft<MG_LEGAL>(depth);
}

#endif

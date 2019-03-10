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
#include <limits>
#include "position.h"
#include "transtable.h"
#include "search.h"

namespace BlendXChess
{
	//============================================================
	// Helper enums
	//============================================================

	enum class GameState : int8_t {
		ACTIVE,
		DRAW,
		WHITE_WIN,
		BLACK_WIN,
		UNDEFINED
	};

	enum class DrawCause : int8_t {
		RULE_50,
		MATERIAL,
		THREEFOLD_REPETITION
	};

	//============================================================
	// Main game class
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
		static inline int getMaxThreadCount(void);
		// Setters
		inline void setSearchOptions(const SearchOptions&);
		inline void setSearchProcesser(const EngineProcesser&);
		// Clear game state
		void clear(void);
		// Reset game (stops search if there's any)
		void reset(void);
		// Update game state
		void updateGameState(void);
		// Convert move from given string format
		inline Move moveFromStr(const std::string& moveStr, MoveFormat fmt);
		// Convert move to given string format
		inline std::string moveToStr(Move move, MoveFormat fmt) const;
		// Doing and undoing a move
		// These are not well optimized and are being interface for external calls
		// Engine internals (eg in search) use doMove and undoMove instead
		bool DoMove(Move);
		bool DoMove(const std::string&, MoveFormat);
		bool UndoMove(void);
		// Change value of specified option to given (it's parsed here)
		// Throws if no such option found
		void setOption(std::string name, const std::string& value = "");
		// Searching interface functions
		void startSearch(void);
		SearchReturn endSearch(void);
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
		// Convert string to number
		template<typename T>
		static inline T convertTo(const std::string&);
		// Whether position is draw by insufficient material
		bool drawByMaterial(void) const;
		// Whether position is threefold repeated
		bool threefoldRepetitionDraw(void) const;
		// Whether engine core is initialized
		inline static bool initialized = false;
		// Current game position (!! not the one changed in-search !!)
		Position pos;
		// Move searcher
		MultiSearcher searcher;
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
		return searcher.getOptions();
	}

	inline const Position& Game::getPosition(void) const
	{
		return pos;
	}

	inline int Game::getMaxThreadCount(void)
	{
		return MultiSearcher::getMaxThreadCount();
	}

	inline void Game::setSearchOptions(const SearchOptions& options)
	{
		searcher.setOptions(options);
	}

	inline void Game::setSearchProcesser(const EngineProcesser& proc)
	{
		searcher.setProcesser(proc);
	}

	inline Move Game::moveFromStr(const std::string& moveStr, MoveFormat fmt)
	{
		return pos.moveFromStr(moveStr, fmt);
	}

	inline std::string Game::moveToStr(Move move, MoveFormat fmt) const
	{
		return pos.moveToStr(move, fmt);
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

	template<typename T>
	inline T Game::convertTo(const std::string& str)
	{
		static_assert(std::is_arithmetic_v<T>, "'T' should be numeric type");
		try
		{
			if constexpr (std::is_unsigned_v<T>)
			{
				unsigned long long value = std::stoull(str);
				if (value > std::numeric_limits<T>::max() || value < std::numeric_limits<T>::min())
					throw std::runtime_error(str + " is out of 'T' range");
				return static_cast<T>(value);
			}
			else
			{
				long long value = std::stoll(str);
				if (value > std::numeric_limits<T>::max() || value < std::numeric_limits<T>::min())
					throw std::runtime_error(str + " is out of 'T' range");
				return static_cast<T>(value);
			}
		}
		catch (const std::invalid_argument&)
		{
			throw std::runtime_error("Cannot convert " + str + " to 'T'");
		}
		catch (const std::out_of_range&)
		{
			throw std::runtime_error(str + " is out of 'T' range");
		}
	}
};

#endif

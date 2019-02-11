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
#include <functional>
#include <deque>

namespace BlendXChess
{

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
	constexpr int TIME_LIMIT_MIN = 100; // ms
	constexpr int TIME_LIMIT_MAX = 1000000; // ms
	constexpr Depth SEARCH_DEPTH_DEFAULT = 10;
	constexpr Depth SEARCH_DEPTH_MIN = 1;
	constexpr Depth SEARCH_DEPTH_MAX = 60;

	//============================================================
	// Structs for storing results and stats of search returned by endSearch
	//============================================================

	struct SearchResults
	{
		Score score;
		Depth resDepth;
		Move bestMove; // !! TODO (PV?) !!
	};

	struct SearchStats
	{
		int ttHits;
		int visitedNodes;
	};

	typedef std::pair<SearchResults, SearchStats> SearchReturn;

	//============================================================
	// Struct for sending search info to external event processing
	//============================================================

	enum class SearchEventType {
		NONE,
		FINISHED, // Sent if search stop is not forced by endSearch
		INFO // For notifying about updated search results
	};

	struct SearchEvent
	{
		inline SearchEvent(SearchEventType = SearchEventType::NONE,
			SearchResults = SearchResults());
		SearchEventType type;
		SearchResults results; // valid if type == INFO
	};

	using EngineProcesser = std::function<void(const SearchEvent&)>;

	//============================================================
	// Structs for storing some shared search options and info
	//============================================================

	struct SearchOptions
	{
		unsigned int timeLimit; // ms
		unsigned int threadCount;
		Depth depth;
	};

	const SearchOptions DEFAULT_SEARCH_OPTIONS = SearchOptions{
		TIME_LIMIT_DEFAULT, std::thread::hardware_concurrency(), SEARCH_DEPTH_DEFAULT };

	typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

	enum class StopCause {
		END_SEARCH_CALL, TIMEOUT, DEPTH_REACHED
	};

	struct SharedInfo
	{
		struct RootSearchState
		{
			int depth = DEPTH_ZERO;
			Move move;
		};
		TimePoint startTime; // read-only while accessed multithreaded, thus not atomic
		std::atomic_int timeCheckCounter;
		std::atomic_int ttHits;
		std::atomic_int visitedNodes;
		std::atomic_bool stopSearch;
		std::atomic_bool externalStop;
		std::atomic_bool timeout;
		// States of search in the root of a thread with corrersponding ID
		std::vector<RootSearchState> rootSearchStates;
		// Count of threads search(-ing/-ed) specified depth (from root position)
		// std::deque<std::atomic_int> depthSearchedByCnt;
		StopCause stopCause;
		EngineProcesser processer; // External search event processer
	};

	//============================================================
	// Class for organizing potentially multi-threaded search process
	//============================================================

	struct ThreadInfo;
	typedef std::vector<ThreadInfo> ThreadList;

	class MultiSearcher
	{
	public:
		// Constructor
		MultiSearcher(void);
		// Getters
		inline bool isInSearch(void) const;
		static inline int getMaxThreadCount(void);
		// Setup external search event processer
		inline void setProcesser(const EngineProcesser&);
		// Starts search with given depth
		void startSearch(const Position&, const SearchOptions& options = DEFAULT_SEARCH_OPTIONS);
		// Ends started search (throws if there was no one) and returns search information
		SearchReturn endSearch(void);
		// Internal search logic (coordinates searching process, launches Searcher threads)
		void search(void);
	private:
		// Setters (actual search parameters are given in startSearch parameters, thus setter is private)
		inline void setThreadCount(int);
		// Select (currently) best search thread
		ThreadList::iterator bestThread(void);
		// Shared transposition table
		TranspositionTable transpositionTable;
		// Shared search info
		SharedInfo shared;
		// Shared search options
		SearchOptions options;
		// Position
		Position pos;
		// Search threads info by index (main search thread has index 0)
		// Main search thread searches itself and launches/coordinates other threads
		std::vector<ThreadInfo> threads;
		// Whether search is currently launched
		std::atomic_bool inSearch;
		// Maximum thread count
		static inline int maxThreadCount = std::thread::hardware_concurrency();
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
		friend class MoveManager<false>;
		friend class MoveManager<true>;
		static constexpr int MAX_KILLERS_CNT = 3;
		static constexpr MoveScore MS_TT_BONUS = 1500000000;
		static constexpr MoveScore MS_COUNTERMOVE_BONUS = 300000;
		static constexpr MoveScore MS_SEE_MULT = 1500000 / 100;
		static constexpr MoveScore MS_CAPTURE_BONUS_VICTIM[PIECETYPE_CNT] = {
			0, 100000, 285000, 300000, 500000, 1000000, 0 };
		static constexpr MoveScore MS_CAPTURE_BONUS_ATTACKER[PIECETYPE_CNT] = {
			0, 1000000, 800000, 750000, 400000, 200000 };
		static constexpr MoveScore MS_KILLER_BONUS = 1200000;
		using KillerList = Move[MAX_KILLERS_CNT];
		// Default constructor
		Searcher(void) = default;
		// Constructor
		Searcher(const Position&, SearchOptions*, SharedInfo*, TranspositionTable*, ThreadInfo*);
		// Initializer
		void initialize(const Position&, SearchOptions*, SharedInfo*, TranspositionTable*, ThreadInfo*);
		// Top-level search function that implements iterative deepening with aspiration windows
		void idSearch(Depth);
		// Whether this search thread is the main one
		inline bool isMainThread(void) const;
	private:
		// Helpers for ply-adjustment of scores (mate ones) when (extracted from)/(inserted to) a transposition table
		inline Score scoreToTT(Score) const;
		inline Score scoreFromTT(Score) const;
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
		void scoreMoves(MoveList&) const;
		// (Re-)score moves and sort
		inline void sortMoves(MoveList&) const;
		// Data
		// Current position
		Position pos;
		// Shared search options
		SearchOptions* options;
		// Shared search info
		SharedInfo* shared;
		// Shared transposition table
		TranspositionTable* transpositionTable;
		// Thread-specific info
		ThreadInfo* threadLocal;
		// Ply from the searcher starting position
		int searchPly;
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
	// Struct for storing some per-thread search info
	//============================================================

	struct ThreadInfo
	{
		std::thread handle;
		Searcher searcher;
		SearchResults results;
		int ID; // 0 for main thread
	};

	//============================================================
	// Implementation of inline functions
	//============================================================

	inline SearchEvent::SearchEvent(SearchEventType type, SearchResults results)
		: type(type), results(results)
	{}

	inline bool MultiSearcher::isInSearch(void) const
	{
		return inSearch;
	}

	inline int MultiSearcher::getMaxThreadCount(void)
	{
		return maxThreadCount;
	}

	inline void MultiSearcher::setProcesser(const EngineProcesser& proc)
	{
		if (inSearch)
			throw std::runtime_error("Can't change processer during search");
		shared.processer = proc;
	}

	inline void MultiSearcher::setThreadCount(int threadCount)
	{
		if (inSearch)
			throw std::runtime_error("Can't change thread count while in search");
		if (threadCount <= 0)
			throw std::runtime_error("Thread count can't be non-positive");
		// Update max thread count and clamp thread count if needed
		maxThreadCount = std::thread::hardware_concurrency();
		if (threadCount > maxThreadCount)
			threadCount = maxThreadCount;
		options.threadCount = threadCount;
		threads.resize(threadCount);
		shared.rootSearchStates.resize(threadCount);
	}

	inline bool Searcher::isMainThread(void) const
	{
		return threadLocal->ID == 0;
	}

	inline Score Searcher::scoreToTT(Score score) const
	{
		return score > SCORE_WIN_MIN ? score + searchPly :
			score < SCORE_LOSE_MAX ? score - searchPly : score;
	}

	inline Score Searcher::scoreFromTT(Score score) const
	{
		return score > SCORE_WIN_MIN ? score - searchPly :
			score < SCORE_LOSE_MAX ? score + searchPly : score;
	}

	inline void Searcher::sortMoves(MoveList& ml) const
	{
		// ml.reset();
		scoreMoves(ml);
		ml.sort();
	}

};

#endif
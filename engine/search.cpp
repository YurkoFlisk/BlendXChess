//============================================================
// search.cpp
// BlendXChess
//============================================================

#include <chrono>
#include <algorithm>
#include "search.h"
#include "move_manager.h"

using namespace BlendXChess;

//============================================================
// Constructor
//============================================================
MultiSearcher::MultiSearcher(const SearchOptions& opt)
	: inSearch(false)
{
	shared.processer = [](const SearchEvent&) {}; // Default no-op processer
	setOptions(opt);
}

//============================================================
// Destructor
//============================================================
MultiSearcher::~MultiSearcher(void)
{
	// We need this in case there's still some search OR if the last search
	// was finished internally (so main thread was not joined)
	if (!threads.empty() && threads[0].handle.joinable())
	{
		shared.stopSearch = true;
		threads[0].handle.join();
	}
}

//============================================================
// Constructor
//============================================================
Searcher::Searcher(const Position& pos, SearchOptions* options, SharedInfo* shared,
	TranspositionTable* tt, ThreadInfo* threadLocal)
	: pos(pos), options(options), shared(shared),
	transpositionTable(tt), threadLocal(threadLocal)
{
	memset(prevMoves, 0, sizeof(prevMoves));
	memset(history, 0, sizeof(history));
	memset(countermoves, 0, sizeof(countermoves));
}

//============================================================
// Initializer
//============================================================
void Searcher::initialize(const Position& pos, SearchOptions* options, SharedInfo* shared,
	TranspositionTable* tt, ThreadInfo* threadLocal)
{
	assert(options && tt && shared && threadLocal);
	new(this)Searcher(pos, options, shared, tt, threadLocal);
}

//============================================================
// Starts search with given depth
//============================================================
void MultiSearcher::startSearch(const Position& pos)
{
	// If we are already in search, the new one won't be launched
	if (inSearch)
		throw std::runtime_error("Another search is already launched");
	// Set thread count and update thread vector accordingly
	setThreadCount(options.threadCount);
	// Indicate beginning of search
	inSearch = true;
	// Copy input position to internal storage (it's just safer
	// not to assume it will remain valid during thread execution)
	this->pos = pos;
	// Safety check (TODO: maybe get rid of depth limiting (but then dynamic memory
	// will be used in some places, with hopefully rarely (de-)allocating)?)
	if (options.depth > SEARCH_DEPTH_MAX)
		options.depth = SEARCH_DEPTH_MAX;
	// Reset misc info
	shared.stopSearch = false;
	shared.externalStop = false;
	shared.timeout = false;
	// Start the main search thread. If the last search was finished
	// internally, the main thread was not joined, so we need to join it here.
	std::thread& mainThreadHandle = threads[0].handle;
	if (mainThreadHandle.joinable()) // !
		mainThreadHandle.join();
	mainThreadHandle = std::thread(&MultiSearcher::search, this);
	if (!mainThreadHandle.joinable())
	{
		inSearch = false;
		throw std::runtime_error("Unable to create valid main search thread");
	}
}

//============================================================
// Ends started search and returns search information
// Returns last search results if no one is performed at the moment
//============================================================
SearchReturn MultiSearcher::endSearch(void)
{
	if (!inSearch)
		// throw std::runtime_error("There's no search in progress.");
		return lastSearchReturn;
	// If it's not called from search(), we should wait for main search thread to finish
	if (std::this_thread::get_id() != threads[0].handle.get_id())
	{
		// Set stop flag and wait for finishing search
		if (!shared.stopSearch)
		{
			shared.stopCause = StopCause::END_SEARCH_CALL;
			shared.externalStop = true;
			shared.stopSearch = true;
		}
		threads[0].handle.join();
	}
	// Select best thread to retrieve info from
	auto bestThreadIt = bestThread();
	// Signalize the end of search
	inSearch = false;
	// Increment transposition table age (for future searches)
	transpositionTable.incrementAge();
	// Return the results
	return lastSearchReturn = { bestThreadIt->results, shared.stats };
}

//============================================================
// Internal search logic (coordinates searching process, launches PVS-threads)
//============================================================
void MultiSearcher::search(void)
{
	// Misc
	if constexpr (TT_HITS_COUNT_ENABLED)
		shared.stats.ttHits = 0;
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		shared.stats.visitedNodes = 0;
	// Setup time management
	if constexpr (TIME_CHECK_ENABLED)
	{
		shared.startTime = std::chrono::high_resolution_clock::now();
		shared.timeCheckCounter = 0;
	}
	// Set appropriate size for array containing count of threads search(-ing/-ed) at given depth from root
	// shared.depthSearchedByCnt.resize(options.depth + 1);
	// Setup helper threads
	for (unsigned int threadID = 1; threadID < options.threadCount; ++threadID)
	{
		ThreadInfo& threadInfo = threads[threadID];
		threadInfo.ID = threadID;
		threadInfo.searcher.initialize(pos, &options, &shared, &transpositionTable, &threadInfo);
		threadInfo.handle = std::thread(&Searcher::idSearch, &threadInfo.searcher, options.depth);
	}
	// Setup main (this) thread
	threads[0].ID = 0;
	threads[0].searcher.initialize(pos, &options, &shared, &transpositionTable, &threads[0]);
	threads[0].searcher.idSearch(options.depth);
	// After finishing main thread, wait for others to finish
	shared.stopSearch = true; // !
	for (unsigned int threadID = 1; threadID < options.threadCount; ++threadID)
		threads[threadID].handle.join();
	// If the thread terminated not due to endSearch call, we should notify the user
	if (!shared.externalStop)
	{
		if (!shared.timeout)
			shared.stopCause = StopCause::DEPTH_REACHED;
		shared.processer(SearchEvent(SearchEventType::FINISHED, endSearch()));
	}
}

//============================================================
// Select (currently) best search thread
//============================================================
ThreadList::iterator MultiSearcher::bestThread(void)
{
	// !! TODO maybe there's a better way? !!
	auto bestThreadIt = std::max_element(threads.begin(), threads.end(),
		[](const ThreadInfo& ti1, const ThreadInfo& ti2) {
		return ti1.results.resDepth < ti2.results.resDepth ||
			(ti1.results.resDepth == ti2.results.resDepth
				&& ti1.results.score < ti2.results.score);
	});
	assert(bestThreadIt != threads.end());
	return bestThreadIt;
}

//============================================================
// Get count of threads currently searching given move on given depth
//============================================================
int BlendXChess::Searcher::threadsSearching(Depth depth, Move move)
{
	int cnt = 0;
	for (const auto& state : shared->rootSearchStates)
		if (state.depth == depth && state.move.load() == move)
			++cnt;
	return cnt;
}

//============================================================
// Performs given move if legal on pos and updates necessary info
// Returns false if the move is illegal, otherwise
// returns true and fills position info for undo
//============================================================
bool Searcher::doMove(Move move, PositionInfo& pi)
{
	assert(pos.isValid());
	assert(searchPly >= 0);
	pos.doMove(move, pi);
	if (pos.isAttacked(pos.pieceSq[opposite(pos.turn)][KING][0], pos.turn))
	{
		pos.undoMove(move, pi);
		return false;
	}
	prevMoves[searchPly++] = move;
	return true;
}

//============================================================
// Undoes given move and updates necessary position info
//============================================================
void Searcher::undoMove(Move move, const PositionInfo& pi)
{
	assert(pos.isValid());
	pos.undoMove(move, pi);
	--searchPly;
	assert(searchPly >= 0);
}

//============================================================
// Update killer moves
//============================================================
void Searcher::updateKillers(int searchPly, Move bestMove)
{
	// Check whether this potential killer is a new one
	for (auto killer : killers[searchPly])
		if (killer == bestMove)
			return;
	// If it's new, add it to killers (in front)
	for (int i = 0; i < MAX_KILLERS_CNT - 1; ++i)
		killers[searchPly][i + 1] = killers[searchPly][i];
	killers[searchPly][0] = bestMove;
}

//============================================================
// Scores each move from moveList
//============================================================
void Searcher::scoreMoves(MoveList& moveList) const
{
	for (int i = 0; i < moveList.count(); ++i)
	{
		MLNode& moveNode = moveList[i];
		const Move& move = moveNode.move;
		moveNode.score = history[move.from()][move.to()];
		if (pos.isCaptureMove(move))
			moveNode.score += MS_CAPTURE_BONUS_VICTIM[getPieceType(pos.board[move.to()])]
			+ MS_CAPTURE_BONUS_ATTACKER[getPieceType(pos.board[move.from()])];
		//moveNode.score += MS_SEE_MULT * SEECapture(move.from(), move.to(), turn);
		else
		{
			assert(searchPly == 0 || prevMoves[searchPly - 1] != MOVE_NONE);
			if (searchPly > 0 && move == countermoves[prevMoves[searchPly - 1].from()]
				[prevMoves[searchPly - 1].to()])
				moveNode.score += MS_COUNTERMOVE_BONUS;
			for (auto killerMove : killers[searchPly])
				if (move == killerMove)
				{
					moveNode.score += MS_KILLER_BONUS;
					break;
				}
		}
	}
}

//============================================================
// Internal AI logic
// Static evaluation
//============================================================
Score Searcher::evaluate(void)
{
	Score score = pos.psqScore;
	return pos.turn == WHITE ? score : -score;
}

//============================================================
// Static exchange evaluation
//============================================================
Score Searcher::SEE(Square sq, Side by)
{
	const Square from = pos.leastAttacker(sq, by);
	if (from == Sq::NONE)
		return SCORE_ZERO;
	const Piece capt = pos.board[sq];
	assert(capt != PIECE_NULL);
	pos.removePiece(sq);
	pos.movePiece(from, sq);
	const Score value = std::max(0, ptWeight[getPieceType(capt)] - SEE(sq, opposite(by)));
	pos.movePiece(sq, from);
	pos.putPiece(sq, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Static exchange evaluation of specified capture move
//============================================================
Score Searcher::SEECapture(Square from, Square to, Side by)
{
	const Piece capt = pos.board[to];
	assert(capt != PIECE_NULL);
	pos.removePiece(to);
	pos.movePiece(from, to);
	const Score value = ptWeight[getPieceType(capt)] - SEE(to, opposite(by));
	pos.movePiece(to, from);
	pos.putPiece(to, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Top-level search function that implements iterative
// deepening with aspiration windows
//============================================================
void Searcher::idSearch(Depth depth)
{
	assert(options && transpositionTable && shared && threadLocal);
	assert(this == &threadLocal->searcher);
	// Setup search
	Move bestMove = MOVE_NONE, move;
	int bestScore(SCORE_ZERO); // int to avoid overflow
	Score score;
	PositionInfo prevState;
	searchPly = 0;
	threadLocal->results.resDepth = 0;
	// For different positions different aspiration windows can do better
	constexpr int aspirationDeltas[3] = { 25, 10, 40 };
	const int aspirationDelta = aspirationDeltas[threadLocal->ID % 3];
	// Iterative deepening
	SharedInfo::RootSearchState& searchState = shared->rootSearchStates[threadLocal->ID];
	for (Depth curDepth = 1; curDepth <= depth; ++curDepth)
	{
		// Best move and score of current iteration, assigned originally to previous' info
		int curBestScore(bestScore);
		Move curBestMove(bestMove);
		searchState.depth = curDepth;
		searchState.move = MOVE_NONE;
		// Aspiration windows
		int delta = aspirationDelta, alpha = curBestScore - delta, beta = curBestScore + delta;
		while (true)
		{
			// Initialize move picking manager
			MoveManager<true> moveManager(*this, curBestMove);
			curBestScore = alpha;
			// Test every move and choose the best one
			bool pvSearch = true, firstMove = true;
			while ((move = moveManager.next()) != MOVE_NONE)
			{
				// Defer this move if it's not deferred and enough threads are searching it on this depth
				if (!firstMove && !moveManager.lastMoveDeferred()
					&& threadsSearching(curDepth, move) > 0)
				{
					moveManager.defer(move);
					continue;
				}
				// Do move (should be legal due to generation of legal ones)
				searchState.move = move;
				doMove(move, prevState);
				// Principal variation search
				if (pvSearch)
					score = -pvs(curDepth - 1, -beta, -curBestScore);
				else
				{
					score = -pvs(curDepth - 1, -curBestScore - 1, -curBestScore);
					if (!shared->stopSearch && beta > score && score > curBestScore)
						score = -pvs(curDepth - 1, -beta, -score);
				}
				// Undo move
				undoMove(move, prevState);
				searchState.move = MOVE_NONE;
				firstMove = false;
				// Timeout check
				if (shared->stopSearch)
					break;
				// If this move was better than current best one, update best move and score
				if (score > curBestScore)
				{
					pvSearch = false;
					curBestScore = score;
					curBestMove = move;
					// If beta-cutoff occurs, stop search
					if (curBestScore >= beta)
					{
						// Don't update history and killers while in aspiration window
						// history[move.from()][move.to()] += curDepth * curDepth;
						break;
					}
				}
			}
			// Timeout check
			if (shared->stopSearch)
				break;
			// If bestScore is inside the window, it is final score
			if (alpha < curBestScore && curBestScore < beta)
				break;
			// Update delta and aspiration window otherwise
			delta <<= 1;
			alpha = std::max<int>(curBestScore - delta, SCORE_LOSE);
			beta = std::min<int>(curBestScore + delta, SCORE_WIN);
		}
		// Timeout check
		if (shared->stopSearch)
			break;
		// Only if there was no forced search stop we should accept this
		// iteration's best move and score as new best overall
		bestMove = curBestMove;
		bestScore = curBestScore;
		// Update thread's storage
		threadLocal->results.resDepth = curDepth;
		threadLocal->results.bestMove = bestMove;
		threadLocal->results.score = bestScore;
		// Update history and killers
		if (!pos.isCaptureMove(bestMove))
		{
			updateKillers(searchPly, bestMove);
			history[bestMove.from()][bestMove.to()] += curDepth * curDepth;
		}
		// Send info to external event processer
		if (isMainThread())
			shared->processer(SearchEvent(SearchEventType::INFO,
				{ threadLocal->results, shared->stats }));
	}
}

//============================================================
// Internal AI logic
// Quiescent search
//============================================================
Score Searcher::quiescentSearch(Score alpha, Score beta)
{
	static constexpr Score DELTA_MARGIN = 400;
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++shared->stats.visitedNodes;
	// Get stand-pat score
	const Score standPat = evaluate();
	// We assume there's always a move that will increase score, so if
	// stand-pat exceeds beta, we cut this node immediately
	if (standPat >= beta)
		return beta;
	// For the same reason (as beta) we update alpha by stand-pat
	if (alpha < standPat)
		alpha = standPat;
	// Generate capture (but, if we are in check, all evasions) list
	MoveList moveList;
	pos.generatePseudolegalMoves<MG_CAPTURES>(moveList);
	// If we are in check and no evasion was found, we are lost
	if (moveList.empty())
		return pos.isInCheck() ? SCORE_LOSE + searchPly : standPat;
	sortMoves(moveList);
	// Test every capture and choose the best one
	Move move;
	Score score;
	PositionInfo prevState;
	bool anyLegalMove = false, prune;
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		prune = false;
		// Delta pruning
		if (standPat + ptWeight[getPieceType(pos.board[move.to()])] + DELTA_MARGIN < alpha)
			prune = true;
		// Test capture with SEE and if it's score is < 0, than prune
		else if (pos.board[move.to()] != PIECE_NULL && SEECapture(
			move.from(), move.to(), pos.turn) < SCORE_ZERO)
			prune = true;
		// Do move with legality check
		if (!doMove(move, prevState))
			continue;
		anyLegalMove = true;
		// Here we know that this move is legal and anyLegalMove is updated accordingly, so we can prune
		if (prune)
		{
			undoMove(move, prevState);
			continue;
		}
		// Quiescent search
		score = -quiescentSearch(-beta, -alpha);
		// Undo move
		undoMove(move, prevState);
		// Update alpha
		if (score > alpha)
		{
			alpha = score;
			// If beta-cutoff occurs, stop search
			if (alpha >= beta)
				break;
		}
	}
	// Return alpha
	return anyLegalMove ? alpha : pos.isInCheck() ? SCORE_LOSE + searchPly : standPat;
}

//============================================================
// Internal AI logic (Principal Variation Search)
// Get position score by searching with given depth
//============================================================
Score Searcher::pvs(Depth depth, Score alpha, Score beta)
{
	// Time check (if it's enabled)
	if constexpr (TIME_CHECK_ENABLED)
		if ((++shared->timeCheckCounter) == TIME_CHECK_INTERVAL)
		{
			shared->timeCheckCounter = 0;
			auto endTime = std::chrono::high_resolution_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(endTime
				- shared->startTime).count() > options->timeLimit)
			{
				shared->stopSearch = true;
				shared->timeout = true;
				shared->stopCause = StopCause::TIMEOUT;
				return SCORE_ZERO;
			}
		}
	// If depth is zero, we should stop and begin quiescent search
	if (depth == DEPTH_ZERO)
		return quiescentSearch(alpha, beta);
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++shared->stats.visitedNodes;
	// Check for 50-rule draw
	if (pos.info.rule50 >= 100)
		return SCORE_ZERO;
	// Transposition table lookup
	const Score oldAlpha = alpha;
	const TTEntry* ttEntry = transpositionTable->probe(pos.info.keyZobrist);
	Move move, bestMove, ttMove = MOVE_NONE;
	if (ttEntry != nullptr)
	{
		if (ttEntry->depth >= depth)
		{
			const Score ttScore = scoreFromTT(ttEntry->score);
			if ((ttEntry->bound & BOUND_LOWER) && ttScore > alpha)
				alpha = ttScore;
			if ((ttEntry->bound & BOUND_UPPER) && ttScore < beta)
				beta = ttScore;
			if (alpha >= beta)
				return alpha;
		}
		ttMove = ttEntry->move;
		if constexpr (TT_HITS_COUNT_ENABLED)
			++shared->stats.ttHits;
	}
	PositionInfo prevState;
	MoveManager moveManager(*this, ttMove);
	Score bestScore = SCORE_LOSE, score;
	bool anyLegalMove = false, pvSearch = true;
	while ((move = moveManager.next()) != MOVE_NONE)
	{
		// Do move with legality check
		if (!doMove(move, prevState))
			continue;
		anyLegalMove = true;
		// Principal variation search
		if (pvSearch)
			score = -pvs(depth - 1, -beta, -alpha);
		else
		{
			score = -pvs(depth - 1, -alpha - 1, -alpha);
			if (!shared->stopSearch && beta > score && score > alpha)
				score = -pvs(depth - 1, -beta, -score);
		}
		// Undo move
		undoMove(move, prevState);
		// Timeout check
		if (shared->stopSearch)
			return SCORE_ZERO;
		// Update bestScore
		if (score > bestScore)
		{
			pvSearch = false;
			bestScore = score;
			bestMove = move;
			// Update alpha
			if (score > alpha)
			{
				alpha = score;
				// Beta-cutoff
				if (alpha >= beta)
				{
					// Update killers, countermoves and history
					if (!pos.isCaptureMove(move))
					{
						updateKillers(searchPly, move);
						history[move.from()][move.to()] += depth * depth;
						countermoves[prevMoves[searchPly - 1].from()][
							prevMoves[searchPly - 1].to()] = move;
					}
					// Cutoff
					break;
				}
			}
		}
	}
	// Save collected info to the transposition table
	if (anyLegalMove)
		transpositionTable->store(pos.info.keyZobrist, depth, alpha == oldAlpha ? BOUND_UPPER :
			alpha < beta ? BOUND_EXACT : BOUND_LOWER, scoreToTT(bestScore),
			bestMove); // ! NOT searchPly !
	// Return alpha
	return anyLegalMove ? alpha : pos.isInCheck() ? SCORE_LOSE + searchPly : SCORE_ZERO;
}
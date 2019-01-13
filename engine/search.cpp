//============================================================
// search.cpp
// BlendXChess
//============================================================

#include <chrono>
#include <algorithm>
#include "search.h"
#include "move_manager.h"

//============================================================
// Constructor
//============================================================
MultiSearcher::MultiSearcher(void)
	: inSearch(false)
{}

//============================================================
// Constructor
//============================================================
Searcher::Searcher(const Position& pos, SearchOptions* shared,
	TranspositionTable* tt, ThreadInfo* threadLocal)
	: pos(pos), shared(shared), transpositionTable(tt), threadLocal(threadLocal)
{}

//============================================================
// Initializer
//============================================================
void Searcher::initialize(const Position& pos, SearchOptions* shared,
	TranspositionTable* tt, ThreadInfo* threadLocal)
{
	assert(tt && shared && threadLocal);
	new(this)Searcher(pos, shared, tt, threadLocal);
}

//============================================================
// Starts search with given depth (without waiting for the end
//============================================================
void MultiSearcher::startSearch(Depth depth)
{
	// If we are already in search, the new one won't be launched
	if (inSearch)
		throw std::runtime_error("Another search is already launched");
	// Set the inSearch and start the main search thread
	shared.stopSearch = false;
	inSearch = true;
	threads[0].handle = std::thread(&MultiSearcher::search, this, depth);
	if (!threads[0].handle.joinable())
		throw std::runtime_error("Unable to create valid main search thread");
}

//============================================================
// Ends started search (throws if there was no one) and returns search information
//============================================================
Score MultiSearcher::endSearch(Move& bestMove, Depth& resDepth, int& nodes, int& hits)
{
	// Set stop flag and wait for finishing search
	shared.stopSearch = true;
	threads[0].handle.join();
	// Select best thread to retrieve info from
	auto bestThreadIt = std::max_element(threads.begin(), threads.end(),
		[](const ThreadInfo& ti1, const ThreadInfo& ti2) {
		return ti1.resDepth < ti2.resDepth ||
			(ti1.resDepth == ti2.resDepth && ti1.score < ti2.score);
	});
	assert(bestThreadIt != threads.end());
	bestMove = bestThreadIt->bestMove;
	resDepth = bestThreadIt->resDepth;
	nodes = shared.visitedNodes;
	hits = shared.ttHits;
	inSearch = false;
	return bestThreadIt->score;
}

//============================================================
// Internal AI logic (coordinates searching process, launches PVS-threads)
//============================================================
void MultiSearcher::search(const Position& pos, Depth depth)
{
	// Misc
	if constexpr (TT_HITS_COUNT_ENABLED)
		shared.ttHits = 0;
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		shared.visitedNodes = 0;
	// Setup time management
	if constexpr (TIME_CHECK_ENABLED)
	{
		shared.startTime = std::chrono::high_resolution_clock::now();
		shared.timeCheckCounter = 0;
	}
	// Setup helper threads
	for (int threadID = 1; threadID < shared.threadCount; ++threadID)
	{
		ThreadInfo& threadInfo = threads[threadID];
		threadInfo.searcher.initialize(pos, &shared, &transpositionTable, &threadInfo);
		threadInfo.handle = std::thread(&Searcher::idSearch, &threadInfo.searcher, depth);
	}
	// Setup main (this) thread
	threads[0].searcher.initialize(pos, &shared, &transpositionTable, &threads[0]);
	threads[0].searcher.idSearch(depth);
	// After finishing main thread, wait for others to finish
	// shared.stopSearch = true;
	for (int threadID = 1; threadID < shared.threadCount; ++threadID)
		threads[threadID].handle.join();
}

//============================================================
// Performs given move if legal on pos and updates necessary info
// Returns false if the move is illegal, otherwise
// returns true and fills position info for undo
//============================================================
bool Searcher::doMove(Move move, PositionInfo& pi)
{
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
	// If it's new, add it to killers
	killers[searchPly].push_front(bestMove);
	if (killers[searchPly].size() > MAX_KILLERS_CNT)
		killers[searchPly].pop_back();
}

//============================================================
// Scores each move from moveList
//============================================================
void Searcher::scoreMoves(MoveList& moveList)
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
				[prevMoves[searchPly - 1].to()]) // searchPly is NOT 1-biased where it is called
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
	assert(threadLocal && *this == threadLocal->searcher);
	// Setup search
	Move bestMove = MOVE_NONE, move;
	int bestScore(SCORE_ZERO); // int to avoid overflow
	Score score;
	PositionInfo prevState;
	for (int i = 0; i <= depth; ++i)
		killers[i].clear();
	searchPly = 0;
	ttAge = pos.gamePly;
	threadLocal->resDepth = 0;
	// Iterative deepening
	for (Depth searchDepth = 1; searchDepth <= depth; ++searchDepth)
	{
		// Best move and score of current iteration
		int curBestScore(bestScore);
		Move curBestMove(bestMove);
		// Aspiration windows
		int delta = 25, alpha = curBestScore - delta, beta = curBestScore + delta;
		while (true)
		{
			// Initialize move picking manager
			MoveManager<true> moveManager(pos, curBestMove);
			curBestScore = alpha;
			// Test every move and choose the best one
			bool pvSearch = true;
			while ((move = moveManager.next()) != MOVE_NONE)
			{
				// Do move
				doMove(move, prevState);
				// Principal variation search
				if (pvSearch)
					score = -pvs(searchDepth - 1, -beta, -curBestScore);
				else
				{
					score = -pvs(searchDepth - 1, -curBestScore - 1, -curBestScore);
					if (!shared->stopSearch && beta > score && score > curBestScore)
						score = -pvs(searchDepth - 1, -beta, -score);
				}
				// Undo move
				undoMove(move, prevState);
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
						// history[move.from()][move.to()] += searchDepth * searchDepth;
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
		threadLocal->resDepth = searchDepth;
		// Update history and killers
		if (!pos.isCaptureMove(bestMove))
		{
			updateKillers(searchPly, bestMove);
			history[bestMove.from()][bestMove.to()] += searchDepth * searchDepth;
		}
	}
	// Save rest of info to thread specific storage
	threadLocal->bestMove = bestMove;
	threadLocal->score = bestScore;
}

//============================================================
// Internal AI logic
// Quiescent search
//============================================================
Score Searcher::quiescentSearch(Score alpha, Score beta)
{
	static constexpr Score DELTA_MARGIN = 330;
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++shared->visitedNodes;
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
		// Do move
		doMove(move, prevState);
		// Legality check
		if (pos.isAttacked(pos.pieceSq[opposite(pos.turn)][KING][0], pos.turn))
		{
			undoMove(move, prevState);
			continue;
		}
		anyLegalMove = true;
		// Here we know that this move is legal and anyLegalMove is updated accordingly, so we can prune
		if (prune)
		{
			pos.undoMove(move, prevState);
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
				- shared->startTime).count() > shared->timeLimit)
			{
				shared->stopSearch = true;
				return SCORE_ZERO;
			}
		}
	// If depth is zero, we should stop and begin quiescent search
	if (depth == DEPTH_ZERO)
		return quiescentSearch(alpha, beta);
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++shared->visitedNodes;
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
			++shared->ttHits;
	}
	PositionInfo prevState;
	MoveManager moveManager(pos, ttMove);
	Score bestScore = SCORE_LOSE, score;
	bool anyLegalMove = false, pvSearch = true;
	while ((move = moveManager.next()) != MOVE_NONE)
	{
		// Do move
		doMove(move, prevState);
		// Legality check
		if (pos.isAttacked(pos.pieceSq[opposite(pos.turn)][KING][0], pos.turn))
		{
			pos.undoMove(move, prevState);
			continue;
		}
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
			bestMove, ttAge); // ! NOT searchPly !
	// Return alpha
	return anyLegalMove ? alpha : pos.isInCheck() ? SCORE_LOSE + searchPly : SCORE_ZERO;
}
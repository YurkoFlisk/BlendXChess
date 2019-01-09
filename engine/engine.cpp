//============================================================
// engine.cpp
// ChessEngine
//============================================================

#include "engine.h"
#include "move_manager.h"
#include <cctype>
#include <sstream>
#include <algorithm>

//============================================================
// Constructor
//============================================================
Engine::Engine(void)
	: gameState(GS_DRAW)
{
	reset();
}

//============================================================
// Initialization
//============================================================
void Engine::initialize(void)
{
	timeLimit = TIME_LIMIT_DEFAULT;
	initPSQ();
	initBB();
	initZobrist();
}

//============================================================
// Clear position and search info (everything except TT)
//============================================================
void Engine::clear(void)
{
	stablePos.clear();
	lastSearchNodes = 0;
	gameState = GS_UNDEFINED;
	// Killers
	for (auto& killer : killers)
		killer.clear();
	// History heuristic and countermoves table
	for (Square from = Sq::A1; from <= Sq::H8; ++from)
		for (Square to = Sq::A1; to <= Sq::H8; ++to)
		{
			history[from][to] = SCORE_ZERO;
			countermoves[from][to] = MOVE_NONE;
		}
	// Game and position history
	gameHistory.clear();
	positionRepeats.clear();
	// Transposition table clear (TODO)
	transpositionTable.clear();
}

//============================================================
// Reset game (stops search if there's any)
//============================================================
void Engine::reset(void)
{
	// Update maximum available threads info
	maxThreadCount = std::thread::hardware_concurrency();
	// If we are in search, stop it
	if (inSearch)
	{
		assert(mainSearchThread.joinable());
		stopSearch = true;
		mainSearchThread.join();// searchStoppedNotifier.wait; !! TODO !!
	}
	clear();
	stablePos.reset(); // Position::clear will also be called from here but it's not crucial
	gameState = GS_ACTIVE;
	// Killers
	for (auto& killer : killers)
		killer.clear();
	// History heuristic and countermoves table
	for (Square from = Sq::A1; from <= Sq::H8; ++from)
		for (Square to = Sq::A1; to <= Sq::H8; ++to)
		{
			history[from][to] = SCORE_ZERO;
			countermoves[from][to] = MOVE_NONE;
		}
	// Game and position history
	gameHistory.clear();
	positionRepeats.clear();
}

//============================================================
// Whether position is draw by insufficient material
//============================================================
bool Engine::drawByMaterial(void) const
{
	if (stablePos.pieceCount[WHITE][PT_ALL] > 2 || stablePos.pieceCount[BLACK][PT_ALL] > 2)
		return false;
	if (stablePos.pieceCount[WHITE][PT_ALL] == 1 && stablePos.pieceCount[BLACK][PT_ALL] == 1)
		return true;
	for (Side side : {WHITE, BLACK})
		if (stablePos.pieceCount[opposite(side)][PT_ALL] == 1 &&
			(stablePos.pieceCount[side][BISHOP] == 1 || stablePos.pieceCount[side][KNIGHT] == 1))
			return true;
	if (stablePos.pieceCount[WHITE][BISHOP] == 1 && stablePos.pieceCount[BLACK][BISHOP] == 1 &&
		stablePos.pieceSq[WHITE][BISHOP][0].color() == stablePos.pieceSq[BLACK][BISHOP][0].color())
		return true;
	return false;
}

//============================================================
// Whether position is threefold repeated, which results in draw
//============================================================
bool Engine::threefoldRepetitionDraw(void) const
{
	std::stringstream positionFEN;
	stablePos.writePosition(positionFEN, true); // Positions are considered equal iff reduced FENs are
	const auto iter = positionRepeats.find(positionFEN.str());
	return iter != positionRepeats.end() && iter->second >= 3;
}

//============================================================
// Update game state
//============================================================
void Engine::updateGameState(void)
{
	if (inSearch)
		return;
	MoveList moveList;
	stablePos.generateLegalMoves(moveList);
	if (moveList.empty())
		gameState = stablePos.isInCheck() ? (stablePos.turn == WHITE ?
			GS_BLACK_WIN : GS_WHITE_WIN) : GS_DRAW;
	else if (stablePos.info.rule50 >= 100)
		gameState = GS_DRAW, drawCause = DC_RULE_50;
	else if (drawByMaterial())
		gameState = GS_DRAW, drawCause = DC_MATERIAL;
	else if (threefoldRepetitionDraw())
		gameState = GS_DRAW, drawCause = DC_THREEFOLD_REPETITION;
	else
		gameState = GS_ACTIVE;
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
//============================================================
bool Engine::DoMove(Move move)
{
	// If we are in search, don't do any moves
	if (inSearch)
		return false;
	// Generate legal moves
	MoveList legalMoves;
	stablePos.generateLegalMovesEx(legalMoves);
	bool found = false;
	// Check whether given move is among legal ones
	for (int i = 0; i < legalMoves.count(); ++i)
		if (legalMoves[i].move == move)
		{
			found = true;
			break;
		}
	// If it's not, we can't perform it, otherwise we deliver it to doMove function
	// and convert it to SAN notation (using this in updating game information)
	if (!found)
		return false;
	std::string moveSAN = stablePos.moveToSAN(move);
	stablePos.doMove(move);
	// Update game history and position counter
	std::stringstream positionFEN;
	stablePos.writePosition(positionFEN, true);
	++positionRepeats[positionFEN.str()];
	gameHistory.push_back({ move, moveSAN });
	// Update game state
	updateGameState();
	return true;
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
// Input string in algebraic-like format
//============================================================
bool Engine::DoMove(const std::string& moveStr)
{
	// If we are in search, don't do any moves
	if (inSearch)
		return false;
	// Convert the move from string representation to Move type. Legality there is
	// checked only partially (to the stage when move is convertible to Move representation),
	// and MOVE_NONE is returned in case of error
	const Move move = stablePos.moveFromAN(moveStr);
	// Use Move type representation of move for next actions
	return DoMove(move);
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
// Input string in SAN format
//============================================================
bool Engine::DoMoveSAN(const std::string& moveSAN)
{
	// If we are in search, don't do any moves
	if (inSearch)
		return false;
	Move move;
	// Try to reveal the move from SAN format and return false in case of illegal one
	try
	{
		move = stablePos.moveFromSAN(moveSAN);
	}
	catch (const std::runtime_error&)
	{
		return false;
	}
	// If convertion was successfull, we already know that move is legal
	stablePos.doMove(move);
	// Update game state
	updateGameState();
	// Update game history
	gameHistory.push_back({ move, moveSAN });
	return true;
}

//============================================================
// Undo last move. Return false if it is start state now, true otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use undoMove instead
//============================================================
bool Engine::UndoMove(void)
{
	if (gameHistory.empty())
		return false;
	stablePos.undoMove(gameHistory.back().move);
	gameHistory.pop_back();
	updateGameState();
	return true;
}

//============================================================
// Performance test (if MG_LEGAL is true, all moves are tested for legality
// during generation and promotions to bishops and rooks are included)
//============================================================
template<bool MG_LEGAL>
int Engine::perft(Depth depth)
{
	if (depth == 0)
		return 1;
	int nodes(0);
	Move move;
	MoveList moveList;
	if constexpr (MG_LEGAL)
		generateLegalMovesEx(moveList);
	else
		generatePseudolegalMoves(moveList);
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		doMove(move);
		if constexpr (!MG_LEGAL)
			if (isAttacked(pieceSq[opposite(turn)][KING][0], turn))
			{
				undoMove(move);
				continue;
			}
		nodes += perft<MG_LEGAL>(depth - 1);
		undoMove(move);
	}
	return nodes;
}

//============================================================
// Load game from the given stream in SAN notation
//============================================================
void Engine::loadGame(std::istream& istr)
{
	// Don't load game if we are in search
	if (inSearch)
		return;
	// Reset position information
	reset();
	// Read moves until mate/draw or end of file
	static constexpr char delim = '.';
	while (true)
	{
		const int expectedMN = stablePos.gamePly / 2 + 1;
		// If it's white's turn, move number (equal to expectedMN) should be present before it
		if (stablePos.turn == WHITE)
		{
			int moveNumber;
			istr >> moveNumber;
			if (istr.eof())
				break;
			if (!istr || moveNumber != expectedMN)
				throw std::runtime_error("Missing/wrong move number "
					+ std::to_string(expectedMN));
			if (istr.get() != delim)
				throw std::runtime_error("Missing/wrong move number "
					+ std::to_string(expectedMN) + " delimiter (should be '.')");
		}
		// Read a move and perform it if legal
		std::string moveSAN;
		istr >> moveSAN;
		if (istr.eof())
			break;
		if (!istr || !DoMoveSAN(moveSAN))
			throw std::runtime_error((stablePos.turn == WHITE ? "White " : "Black ")
				+ std::string("move at position ") + std::to_string(expectedMN) + " is illegal");
		if (gameState != GS_ACTIVE)
			break;
	}
}

//============================================================
// Write game to the given stream in SAN notation
//============================================================
void Engine::writeGame(std::ostream& ostr)
{
	// Write saved SAN representations of moves along with move number indicators
	for (int ply = 0; ply < gameHistory.size(); ++ply)
	{
		if ((ply & 1) == 0)
			ostr << ply / 2 + 1 << '.';
		ostr << ' ' << gameHistory[ply].moveSAN;
		if (ply & 1)
			ostr << '\n';
	}
}

//============================================================
// Internal AI logic
// Static evaluation
//============================================================
Score Engine::evaluate(SearchState& ss)
{
	Score score = ss.pos.psqScore;
	return ss.pos.turn == WHITE ? score : -score;
}

//============================================================
// Static exchange evaluation
//============================================================
Score Engine::SEE(Square sq, Side by, Position& pos)
{
	const Square from = pos.leastAttacker(sq, by);
	if (from == Sq::NONE)
		return SCORE_ZERO;
	const Piece capt = pos.board[sq];
	assert(capt != PIECE_NULL);
	pos.removePiece(sq);
	pos.movePiece(from, sq);
	const Score value = std::max(0, ptWeight[getPieceType(capt)] - SEE(sq, opposite(by), pos));
	pos.movePiece(sq, from);
	pos.putPiece(sq, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Static exchange evaluation of specified capture move
//============================================================
Score Engine::SEECapture(Square from, Square to, Side by, Position& pos)
{
	const Piece capt = pos.board[to];
	assert(capt != PIECE_NULL);
	pos.removePiece(to);
	pos.movePiece(from, to);
	const Score value = ptWeight[getPieceType(capt)] - SEE(to, opposite(by), pos);
	pos.movePiece(to, from);
	pos.putPiece(to, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Scores each move from moveList
//============================================================
void Engine::scoreMoves(MoveList& moveList, SearchState& ss)
{
	Position& pos = ss.pos;
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
			assert(ss.searchPly == 0 || ss.prevMoves[ss.searchPly - 1] != MOVE_NONE);
			if (ss.searchPly > 0 && move == countermoves[ss.prevMoves[ss.searchPly - 1].from()]
				[ss.prevMoves[ss.searchPly - 1].to()]) // searchPly is NOT 1-biased where it is called
				moveNode.score += MS_COUNTERMOVE_BONUS;
			for (auto killerMove : killers[ss.searchPly])
				if (move == killerMove)
				{
					moveNode.score += MS_KILLER_BONUS;
					break;
				}
		}
	}
}

//============================================================
// Update killer moves
//============================================================
void Engine::updateKillers(int searchPly, Move bestMove)
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
// Starts search with given depth (without waiting for the end
//============================================================
void Engine::startSearch(Depth depth)
{
	// If we are already in search, the new one won't be launched
	if (inSearch)
		throw std::runtime_error("Another search is already launched");
	// Set the inSearch and start the main search thread
	inSearch = true;
	mainSearchThread = std::thread(&Engine::search, this, depth);
	if (!mainSearchThread.joinable())
		throw std::runtime_error("Unable to create valid main search thread");
}

//============================================================
// Ends started search (throws if there was no one) and returns search information
//============================================================
Score Engine::endSearch(Move& bestMove, Depth& resDepth, int& nodes, int& hits)
{

	inSearch = false;
}

//============================================================
// Internal AI logic (coordinates searching process, launches PVS-threads)
//============================================================
void Engine::search(Depth depth)
{
	// Misc
	if constexpr (TT_HITS_COUNT_ENABLED)
		ttHits = 0;
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		lastSearchNodes = 0;
	// 
	if (threadCount == 1)
	{
	}
}

//============================================================
// Internal AI logic
// Quiescent search
//============================================================
Score Engine::quiescentSearch(Score alpha, Score beta, SearchState& ss)
{
	static constexpr Score DELTA_MARGIN = 330;
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++lastSearchNodes;
	Position& pos = ss.pos;
	// Get stand-pat score
	const Score standPat = evaluate(ss);
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
		return pos.isInCheck() ? SCORE_LOSE + ss.searchPly : standPat;
	sortMoves(moveList, ss);
	// Test every capture and choose the best one
	Move move;
	Score score;
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
			move.from(), move.to(), pos.turn, pos) < SCORE_ZERO)
			prune = true;
		// Do move
		doMove(move, ss);
		// Legality check
		if (pos.isAttacked(pos.pieceSq[opposite(pos.turn)][KING][0], pos.turn))
		{
			undoMove(move, ss);
			continue;
		}
		anyLegalMove = true;
		// Here we know that this move is legal and anyLegalMove is updated accordingly, so we can prune
		if (prune)
		{
			pos.undoMove(move);
			continue;
		}
		// Quiescent search
		score = -quiescentSearch(-beta, -alpha, ss);
		// Undo move
		undoMove(move, ss);
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
	return anyLegalMove ? alpha : pos.isInCheck() ? SCORE_LOSE + ss.searchPly : standPat;
}

//============================================================
// Main AI function
// Returns position score and outputs the best move to the reference parameter
// Returns optionally resultant search depth, traversed nodes (including from
// quiscent search), transposition table hits, through reference parameters
//============================================================
Score Engine::AIMove(Move& bestMove, Depth depth, Depth& resDepth, int& nodes, int& hits)
{
	// If we are already in search, the new one won't be launched
	if (inSearch)
		throw std::runtime_error("Another search is already launched");
	nodes = 0;
	// If game is not active, no search is possible
	if (gameState != GS_ACTIVE)
	{
		lastSearchNodes = nodes = 1;
		bestMove = MOVE_NONE;
		if (gameState == GS_DRAW)
			return SCORE_ZERO;
		else
			return (gameState == GS_WHITE_WIN) == (stablePos.turn == WHITE) ? SCORE_WIN : SCORE_LOSE;
	}
	// Misc
	if constexpr (TT_HITS_COUNT_ENABLED)
		ttHits = 0;
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		lastSearchNodes = 0;
	inSearch = true;
	bestMove = MOVE_NONE;
	int bestScore(SCORE_ZERO);
	Move move;
	Score score;
	for (int i = 0; i <= depth; ++i)
		killers[i].clear();
	// Setup time management
	if constexpr (TIME_CHECK_ENABLED)
	{
		startTime = std::chrono::high_resolution_clock::now();
		timeCheckCounter = 0;
	}
	stopSearch = false;
	// Setup search state
	SearchState ss;
	ss.searchPly = 0;
	Position& pos = (ss.pos = stablePos);
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
				doMove(move, ss);
				// Principal variation search
				if (pvSearch)
					score = -pvs(searchDepth - 1, -beta, -curBestScore, ss);
				else
				{
					score = -pvs(searchDepth - 1, -curBestScore - 1, -curBestScore, ss);
					if (!stopSearch && beta > score && score > curBestScore)
						score = -pvs(searchDepth - 1, -beta, -score, ss);
				}
				// Undo move
				undoMove(move, ss);
				// Timeout check
				if (stopSearch)
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
			if (stopSearch)
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
		if (stopSearch)
			break;
		// Only if there was no forced search stop we should accept this
		// iteration's best move and score as new best overall
		bestMove = curBestMove;
		bestScore = curBestScore;
		resDepth = searchDepth;
		// Update history and killers
		if (!pos.isCaptureMove(bestMove))
		{
			updateKillers(ss.searchPly, bestMove);
			history[bestMove.from()][bestMove.to()] += searchDepth * searchDepth;
		}
	}
	// Set nodes count and transposition table hits and return position score
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		nodes = lastSearchNodes;
	if constexpr (TT_HITS_COUNT_ENABLED)
		hits = ttHits;
	inSearch = false;
	return bestScore;
}

//============================================================
// Internal AI logic (Principal Variation Search)
// Get position score by searching with given depth
//============================================================
Score Engine::pvs(Depth depth, Score alpha, Score beta, SearchState& ss)
{
	// Time check (if it's enabled)
	if constexpr (TIME_CHECK_ENABLED)
		if ((++timeCheckCounter) == TIME_CHECK_INTERVAL)
		{
			timeCheckCounter = 0;
			auto endTime = std::chrono::high_resolution_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(endTime
				- startTime).count() > timeLimit)
			{
				stopSearch = true;
				return SCORE_ZERO;
			}
		}
	// If depth is zero, we should stop and begin quiescent search
	if (depth == DEPTH_ZERO)
		return quiescentSearch(alpha, beta, ss);
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++lastSearchNodes;
	Position& pos = ss.pos;
	// Check for 50-rule draw
	if (pos.info.rule50 >= 100)
		return SCORE_ZERO;
	// Transposition table lookup
	const Score oldAlpha = alpha;
	const TTEntry* ttEntry = transpositionTable.probe(pos.info.keyZobrist);
	Move move, bestMove, ttMove = MOVE_NONE;
	if (ttEntry != nullptr)
	{
		if (ttEntry->depth >= depth)
		{
			const Score ttScore = scoreFromTT(ttEntry->score, ss);
			if ((ttEntry->bound & BOUND_LOWER) && ttScore > alpha)
				alpha = ttScore;
			if ((ttEntry->bound & BOUND_UPPER) && ttScore < beta)
				beta = ttScore;
			if (alpha >= beta)
				return alpha;
		}
		ttMove = ttEntry->move;
		if constexpr (TT_HITS_COUNT_ENABLED)
			++ttHits;
	}
	MoveManager moveManager(pos, ttMove);
	Score bestScore = SCORE_LOSE, score;
	bool anyLegalMove = false, pvSearch = true;
	while ((move = moveManager.next()) != MOVE_NONE)
	{
		// Do move
		doMove(move, ss);
		// Legality check
		if (pos.isAttacked(pos.pieceSq[opposite(pos.turn)][KING][0], pos.turn))
		{
			pos.undoMove(move);
			continue;
		}
		anyLegalMove = true;
		// Principal variation search
		if (pvSearch)
			score = -pvs(depth - 1, -beta, -alpha, ss);
		else
		{
			score = -pvs(depth - 1, -alpha - 1, -alpha, ss);
			if (!stopSearch && beta > score && score > alpha)
				score = -pvs(depth - 1, -beta, -score, ss);
		}
		// Undo move
		undoMove(move, ss);
		// Timeout check
		if (stopSearch)
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
						updateKillers(ss.searchPly, move);
						history[move.from()][move.to()] += depth * depth;
						countermoves[ss.prevMoves[ss.searchPly - 1].from()][
							ss.prevMoves[ss.searchPly - 1].to()] = move;
					}
					// Cutoff
					break;
				}
			}
		}
	}
	// Save collected info to the transposition table
	if (anyLegalMove)
		transpositionTable.store(pos.info.keyZobrist, depth, alpha == oldAlpha ? BOUND_UPPER :
			alpha < beta ? BOUND_EXACT : BOUND_LOWER, scoreToTT(bestScore, ss),
			bestMove, pos.gamePly - ss.searchPly); // ! NOT searchPly !
	// Return alpha
	return anyLegalMove ? alpha : pos.isInCheck() ? SCORE_LOSE + ss.searchPly : SCORE_ZERO;
}

//============================================================
// Explicit template instantiations
//============================================================
template int Engine::perft<false>(Depth);
template int Engine::perft<true>(Depth);
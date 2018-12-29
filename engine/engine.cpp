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
	: gameState(GS_DRAW), timeLimit(TIME_LIMIT_DEFAULT)
{
	reset();
}

//============================================================
// Initialization
//============================================================
void Engine::initialize(void)
{
	initPSQ();
	initBB();
	initZobrist();
}

//============================================================
// Clear position and search info (everything except TT)
//============================================================
void Engine::clear(void)
{
	Position::clear();
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
// Reset game
//============================================================
void Engine::reset(void)
{
	clear();
	Position::reset(); // Position::clear will also be called from here but it's not crucial
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
	if (pieceCount[WHITE][PT_ALL] > 2 || pieceCount[BLACK][PT_ALL] > 2)
		return false;
	if (pieceCount[WHITE][PT_ALL] == 1 && pieceCount[BLACK][PT_ALL] == 1)
		return true;
	for (Side side : {WHITE, BLACK})
		if (pieceCount[opposite(side)][PT_ALL] == 1 &&
			(pieceCount[side][BISHOP] == 1 || pieceCount[side][KNIGHT] == 1))
			return true;
	if (pieceCount[WHITE][BISHOP] == 1 && pieceCount[BLACK][BISHOP] == 1 &&
		pieceSq[WHITE][BISHOP][0].color() == pieceSq[BLACK][BISHOP][0].color())
		return true;
	return false;
}

//============================================================
// Whether position is threefold repeated, which results in draw
//============================================================
bool Engine::threefoldRepetitionDraw(void) const
{
	std::stringstream positionFEN;
	writePosition(positionFEN, true); // Positions are considered equal iff reduced FENs are
	const auto iter = positionRepeats.find(positionFEN.str());
	return iter != positionRepeats.end() && iter->second >= 3;
}

//============================================================
// Update game state
//============================================================
void Engine::updateGameState(void)
{
	MoveList moveList;
	generateLegalMoves(moveList);
	if (moveList.empty())
		gameState = isInCheck() ? (turn == WHITE ? GS_BLACK_WIN : GS_WHITE_WIN) : GS_DRAW;
	else if (info.rule50 >= 100)
		gameState = GS_DRAW, drawCause = DC_RULE_50;
	else if (drawByMaterial())
		gameState = GS_DRAW, drawCause = DC_MATERIAL;
	else if (threefoldRepetitionDraw())
		gameState = GS_DRAW, drawCause = DC_THREEFOLD_REPETITION;
	else
		gameState = GS_ACTIVE;
}

//============================================================
// Do move. Return true if move is legal, false otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
//============================================================
bool Engine::DoMove(Move move)
{
	// Generate legal moves
	MoveList legalMoves;
	generateLegalMovesEx(legalMoves);
	bool found = false;
	// Check whether given move is among legal ones
	for (int i = 0; i < legalMoves.count(); ++i)
		if (legalMoves[i].move == move)
		{
			found = true;
			break;
		}
	// If it's not, we can't perform it, otherwise we convert it to
	// SAN notation and deliver it to doMove function
	if (!found)
		return false;
	std::string moveSAN = moveToSAN(move);
	doMove(move);
	// Update game history and position counter
	std::stringstream positionFEN;
	writePosition(positionFEN, true);
	++positionRepeats[positionFEN.str()];
	gameHistory.push_back({ move, moveSAN });
	// Update game state
	updateGameState();
	return true;
}

//============================================================
// Do move. Return true if move is legal, false otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
// Input string in algebraic-like format
//============================================================
bool Engine::DoMove(const std::string& strMove)
{
	Move move;
	// Convert the move from string representation to Move type. Legality here is
	// checked only partially (to the stage when move is convertible to Move representation)
	if (strMove == "O-O")
		move = Move(turn, OO);
	else if (strMove == "O-O-O")
		move = Move(turn, OOO);
	else if (strMove.size() < 5 || strMove.size() > 6 || strMove[2] != '-')
		return false;
	else
	{
		const int8_t rfrom = strMove[1] - '1', ffrom = strMove[0] - 'a',
			rto = strMove[4] - '1', fto = strMove[3] - 'a';
		if (!validRank(rfrom) || !validFile(ffrom)
			|| !validRank(rto) || !validFile(fto))
			return false;
		const Square from = Square(rfrom, ffrom), to = Square(rto, fto);
		if (strMove.size() == 6)
			switch (strMove[5])
			{
			case 'N': move = Move(from, to, MT_PROMOTION, KNIGHT); break;
			case 'B': move = Move(from, to, MT_PROMOTION, BISHOP); break;
			case 'R': move = Move(from, to, MT_PROMOTION, ROOK); break;
			case 'Q': move = Move(from, to, MT_PROMOTION, QUEEN); break;
			default: return false;
			}
		else if (getPieceType(board[from]) == PAWN && distance(from, to) == 2
			&& board[to] == PIECE_NULL && rto == relRank(2, turn))
			move = Move(from, to, MT_EN_PASSANT);
		else
			move = Move(from, to);
	}
	// Use Move type representation of move for next actions
	return DoMove(move);
}

//============================================================
// Do move. Return true if move is legal, false otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
// Input string in SAN format
//============================================================
bool Engine::DoMoveSAN(const std::string& moveSAN)
{
	Move move;
	// Try to reveal the move from SAN format and return false in case of illegal one
	try
	{
		move = moveFromSAN(moveSAN);
	}
	catch (const std::runtime_error&)
	{
		return false;
	}
	// If convertion was successfull, we already know that move is legal
	doMove(move);
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
	undoMove(gameHistory.back().move);
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
// Convert a move from SAN notation to Move. It should be valid in current position
//============================================================
Move Engine::moveFromSAN(const std::string& moveSAN)
{
	// Set values to move information variables indicating them as unknown
	int8_t fromFile = -1, fromRank = -1;
	Square to = Sq::NONE;
	PieceType pieceType = PT_NULL, promotionPT = PT_NULL;
	Move move = MOVE_NONE;
	// If move string is too small, don't parse it
	if (moveSAN.size() < 2)
		throw std::runtime_error("Move string is too short");
	// Parse moveSAN to reveal given move information
	// Parse castlings first
	if (validCastlingSideAN(moveSAN))
		move = Move(turn, castlingSideFromAN(moveSAN));
	// Parse pawn moves
	else if (validFileAN(moveSAN[0]))
	{
		pieceType = PAWN;
		if (moveSAN[1] == 'x')
		{
			if (moveSAN.size() < 4 || 5 < moveSAN.size() || !validSquareAN(moveSAN.substr(2, 2)))
				throw std::runtime_error("Invalid pawn capture destination square");
			to = Square::fromAN(moveSAN.substr(2, 2));
			fromFile = fileFromAN(moveSAN[0]);
			if (moveSAN.size() == 5)
			{
				if (!validPieceTypeAN(moveSAN[4]))
					throw std::runtime_error("Invalid promotion piece type");
				promotionPT = pieceTypeFromAN(moveSAN[4]);
			}
			else if (to.rank() == RANK_CNT - 1)
				throw std::runtime_error("Missing promotion piece type");
		}
		else
		{
			if (!validRankAN(moveSAN[1]) || moveSAN.size() > 3)
				throw std::runtime_error("Invalid pawn move destination square");
			to = Square::fromAN(moveSAN.substr(0, 2));
			fromFile = to.file();
			if (moveSAN.size() == 3)
			{
				if (!validPieceTypeAN(moveSAN[2]))
					throw std::runtime_error("Invalid promotion piece type");
				promotionPT = pieceTypeFromAN(moveSAN[2]);
			}
			else if (to.rank() == RANK_CNT - 1)
				throw std::runtime_error("Missing promotion piece type");
		}
	}
	// Parse other piece moves
	else
	{
		if (moveSAN.size() < 3 || 5 < moveSAN.size())
			throw std::runtime_error("Invalid move string size");
		pieceType = pieceTypeFromAN(moveSAN[0]);
		if (moveSAN.size() == 5)
		{
			if (!validFileAN(moveSAN[1]) || !validRankAN(moveSAN[2]))
				throw std::runtime_error("Invalid move source square");
			fromFile = fileFromAN(moveSAN[1]), fromRank = rankFromAN(moveSAN[2]);
			to = Square::fromAN(moveSAN.substr(3, 2));
		}
		else if (moveSAN.size() == 4)
		{
			if (!validFileAN(moveSAN[1]) && !validRankAN(moveSAN[1]))
				throw std::runtime_error("Invalid move source square file or rank");
			if (validFileAN(moveSAN[1]))
				fromFile = fileFromAN(moveSAN[1]);
			else
				fromRank = rankFromAN(moveSAN[1]);
			to = Square::fromAN(moveSAN.substr(2, 2));
		}
		else
			to = Square::fromAN(moveSAN.substr(1, 2));
	}
	// Generate all legal moves
	MoveList legalMoves;
	generateLegalMovesEx(legalMoves);
	// Now find a legal move corresponding to the parsed information
	Move legalMove;
	bool found = false;
	for (int i = 0; i < legalMoves.count(); ++i)
	{
		legalMove = legalMoves[i].move;
		if ((move == MOVE_NONE || move == legalMove) &&
			(fromFile == -1 || fromFile == legalMove.from().file()) &&
			(fromRank == -1 || fromRank == legalMove.from().rank()) &&
			(to == Sq::NONE || to == legalMove.to()) &&
			(pieceType == PT_NULL || pieceType == getPieceType(board[legalMove.from()])) &&
			(legalMove.type() != MT_PROMOTION || promotionPT == legalMove.promotion()))
			if (found)
				throw std::runtime_error("Given move information is ambiguous");
			else
				found = true, move = legalMove;
	}
	if (!found)
		throw std::runtime_error("Move is illegal");
	return move;
}

//============================================================
// Convert a move to SAN notation. It should be legal in current position
//============================================================
std::string Engine::moveToSAN(Move move)
{
	Move legalMove;
	MoveList moveList;
	generateLegalMovesEx(moveList);
	const Square from = move.from(), to = move.to();
	const MoveType moveType = move.type();
	const PieceType pieceType = getPieceType(board[from]);
	bool found = false, fileUncertainty = false, rankUncertainty = false;
	for (int i = 0; i < moveList.count(); ++i)
	{
		legalMove = moveList[i].move;
		const Square lmFrom = legalMove.from(), lmTo = legalMove.to();
		if (move == legalMove)
			found = true;
		else if (to == lmTo && pieceType == getPieceType(board[lmFrom]))
		{
			if (from.file() == lmFrom.file())
				fileUncertainty = true;
			if (from.rank() == lmFrom.rank())
				rankUncertainty = true;
		}
	}
	if (!found)
		throw std::runtime_error("Given move is illegal");
	if (moveType == MT_CASTLING)
		switch (move.castlingSide())
		{
		case OO:	return "O-O";
		case OOO:	return "O-O-O";
		}
	std::stringstream SAN;
	if (pieceType == PAWN)
	{
		if (from.file() != to.file()) // works for en passant as well
			SAN << from.fileAN() << 'x';
		SAN << to.toAN();
		if (moveType == MT_PROMOTION)
			SAN << pieceTypeToAN(move.promotion());
	}
	else
	{
		SAN << pieceTypeToAN(pieceType);
		if (rankUncertainty)
			SAN << from.fileAN();
		if (fileUncertainty)
			SAN << from.rankAN();
		SAN << to.toAN();
	}
	return SAN.str();
}

//============================================================
// Load game from the given stream in SAN notation
//============================================================
void Engine::loadGame(std::istream& istr)
{
	// Reset position information
	reset();
	// Read moves until mate/draw or end of file
	static constexpr char delim = '.';
	while (true)
	{
		const int expectedMN = gamePly / 2 + 1;
		// If it's white's turn, move number (equal to expectedMN) should be present before it
		if (turn == WHITE)
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
			throw std::runtime_error((turn == WHITE ? "White " : "Black ")
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
Score Engine::evaluate(void)
{
	score = psqScore;
	return turn == WHITE ? score : -score;
}

//============================================================
// Static exchange evaluation
//============================================================
Score Engine::SEE(Square sq, Side by)
{
	const Square from = leastAttacker(sq, by);
	if (from == Sq::NONE)
		return SCORE_ZERO;
	const Piece capt = board[sq];
	assert(capt != PIECE_NULL);
	removePiece(sq);
	movePiece(from, sq);
	const Score value = std::max(0, ptWeight[getPieceType(capt)] - SEE(sq, opposite(by)));
	movePiece(sq, from);
	putPiece(sq, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Static exchange evaluation of specified capture move
//============================================================
Score Engine::SEECapture(Square from, Square to, Side by)
{
	const Piece capt = board[to];
	assert(capt != PIECE_NULL);
	removePiece(to);
	movePiece(from, to);
	const Score value = ptWeight[getPieceType(capt)] - SEE(to, opposite(by));
	movePiece(to, from);
	putPiece(to, getPieceSide(capt), getPieceType(capt));
	return value;
}

//============================================================
// Scores each move from moveList
//============================================================
void Engine::scoreMoves(MoveList& moveList, Move ttMove)
{
	for (int i = 0; i < moveList.count(); ++i)
	{
		MLNode& moveNode = moveList[i];
		const Move& move = moveNode.move;
		moveNode.score = history[move.from()][move.to()];
		if (move == ttMove)
		{
			moveNode.score = MS_TT_BONUS;
			continue;
		}
		if (isCaptureMove(move))
			moveNode.score += MS_CAPTURE_BONUS_VICTIM[getPieceType(board[move.to()])]
				+ MS_CAPTURE_BONUS_ATTACKER[getPieceType(board[move.from()])];
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
// Internal AI logic
// Quiescent search
//============================================================
Score Engine::quiescentSearch(Score alpha, Score beta)
{
	static constexpr Score DELTA_MARGIN = 330;
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++lastSearchNodes;
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
	generatePseudolegalMoves<MG_CAPTURES>(moveList);
	// If we are in check and no evasion was found, we are lost
	if (moveList.empty())
		return isInCheck() ? SCORE_LOSE + searchPly : standPat;
	sortMoves(moveList);
	// Test every capture and choose the best one
	Move move;
	bool anyLegalMove = false, prune;
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		prune = false;
		// Delta pruning
		if (standPat + ptWeight[getPieceType(board[move.to()])] + DELTA_MARGIN < alpha)
			prune = true;
		// Test capture with SEE and if it's score is < 0, than prune
		else if (board[move.to()] != PIECE_NULL && SEECapture(
			move.from(), move.to(), turn) < SCORE_ZERO)
			prune = true;
		// Do move
		doMove(move);
		// Legality check
		if (isAttacked(pieceSq[opposite(turn)][KING][0], turn))
		{
			undoMove(move);
			continue;
		}
		anyLegalMove = true;
		// Here we know that this move is legal and anyLegalMove is updated accordingly, so we can prune
		if (prune)
		{
			undoMove(move);
			continue;
		}
		// Quiescent search
		score = -quiescentSearch(-beta, -alpha);
		// Undo move
		undoMove(move);
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
	return anyLegalMove ? alpha : isInCheck() ? SCORE_LOSE + searchPly : standPat;
}

//============================================================
// Main AI function
// Returns position score and outputs the best move to the reference parameter
// Returns optionally resultant search depth, traversed nodes (including from
// quiscent search), transposition table hits, through reference parameters
//============================================================
Score Engine::AIMove(Move& bestMove, Depth depth, Depth& resDepth, int& nodes, int& hits)
{
	nodes = 0;
	// If game is not active, no search is possible
	if (gameState != GS_ACTIVE)
	{
		lastSearchNodes = nodes = 1;
		bestMove = MOVE_NONE;
		if (gameState == GS_DRAW)
			return SCORE_ZERO;
		else
			return (gameState == GS_WHITE_WIN) == (turn == WHITE) ? SCORE_WIN : SCORE_LOSE;
	}
	// Misc
	if constexpr (TT_HITS_COUNT_ENABLED)
		ttHits = 0;
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		lastSearchNodes = 0;
	searchPly = 0;
	bestMove = MOVE_NONE;
	int bestScore(SCORE_ZERO);
	Move move;
	for (int i = 0; i <= depth; ++i)
		killers[i].clear();
	// Setup time management
	if constexpr (TIME_CHECK_ENABLED)
	{
		startTime = std::chrono::high_resolution_clock::now();
		timeCheckCounter = 0;
		timeout = false;
	}
	// Iterative deepening
	for (Depth searchDepth = 1; searchDepth <= depth; ++searchDepth)
	{
		// Best move and score of current iteration
		int curBestScore(SCORE_ZERO);
		Move curBestMove = bestMove;
		MoveList moveList;
		generateLegalMoves(moveList);
		sortMoves(moveList, curBestMove);
		// Aspiration windows
		int delta = 25, alpha = curBestScore - delta, beta = curBestScore + delta;
		while (true)
		{
			// Initialize move picking manager
			// MoveManager<true> moveManager(*this, curBestMove);
			moveList.reset();
			curBestScore = alpha;
			// Test every move and choose the best one
			bool pvSearch = true;
			while ((move = moveList.getNextBest()) != MOVE_NONE)
			{
				// Do move
				doMove(move);
				// Principal variation search
				if (pvSearch)
					score = -pvs(searchDepth - 1, -beta, -curBestScore);
				else
				{
					score = -pvs(searchDepth - 1, -curBestScore - 1, -curBestScore);
					if (!timeout && beta > score && score > curBestScore)
						score = -pvs(searchDepth - 1, -beta, -score);
				}
				// Undo move
				undoMove(move);
				// Timeout check
				if (timeout)
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
			if (timeout)
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
		if (timeout)
			break;
		// Only if there was no timeout we should accept this
		// iteration's best move and score as new best overall
		bestMove = curBestMove;
		bestScore = curBestScore;
		resDepth = searchDepth;
		// Update history and killers
		if (!isCaptureMove(bestMove))
		{
			updateKillers(searchPly, bestMove);
			history[bestMove.from()][bestMove.to()] += searchDepth * searchDepth;
		}
	}
	// Set nodes count and transposition table hits and return position score
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		nodes = lastSearchNodes;
	if constexpr (TT_HITS_COUNT_ENABLED)
		hits = ttHits;
	return bestScore;
}

//============================================================
// Internal AI logic (Principal Variation Search)
// Get position score by searching with given depth
//============================================================
Score Engine::pvs(Depth depth, Score alpha, Score beta)
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
				timeout = true;
				return SCORE_ZERO;
			}
		}
	// If depth is zero, we should stop and begin quiescent search
	if (depth == DEPTH_ZERO)
		return quiescentSearch(alpha, beta);
	// Increment search nodes count
	if constexpr (SEARCH_NODES_COUNT_ENABLED)
		++lastSearchNodes;
	// Check for 50-rule draw
	if (info.rule50 >= 100)
		return SCORE_ZERO;
	// Transposition table lookup
	const Score oldAlpha = alpha;
	const TTEntry* ttEntry = transpositionTable.probe(info.keyZobrist);
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
			++ttHits;
	}
	MoveManager moveManager(*this, ttMove);
	Score bestScore = SCORE_LOSE;
	bool anyLegalMove = false, pvSearch = true;
	while ((move = moveManager.next()) != MOVE_NONE)
	{
		// Do move
		doMove(move);
		// Legality check
		if (isAttacked(pieceSq[opposite(turn)][KING][0], turn))
		{
			undoMove(move);
			continue;
		}
		anyLegalMove = true;
		// Principal variation search
		if (pvSearch)
			score = -pvs(depth - 1, -beta, -alpha);
		else
		{
			score = -pvs(depth - 1, -alpha - 1, -alpha);
			if (!timeout && beta > score && score > alpha)
				score = -pvs(depth - 1, -beta, -score);
		}
		// Undo move
		undoMove(move);
		// Timeout check
		if (timeout)
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
					if (!isCaptureMove(move))
					{
						updateKillers(searchPly, move);
						history[move.from()][move.to()] += depth * depth;
						countermoves[prevMoves[searchPly - 1].from()][prevMoves[searchPly - 1].to()] = move;
					}
					// Cutoff
					break;
				}
			}
		}
	}
	// Save collected info to the transposition table
	if (anyLegalMove)
		transpositionTable.store(info.keyZobrist, depth, alpha == oldAlpha ? BOUND_UPPER :
			alpha < beta ? BOUND_EXACT : BOUND_LOWER, scoreToTT(alpha), bestMove, gamePly - searchPly); // ! NOT searchPly !
	// Return alpha
	return anyLegalMove ? alpha : isInCheck() ? SCORE_LOSE + searchPly : SCORE_ZERO;
}

//============================================================
// Explicit template instantiations
//============================================================
template int Engine::perft<false>(Depth);
template int Engine::perft<true>(Depth);
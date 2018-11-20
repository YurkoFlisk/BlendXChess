//============================================================
// engine.cpp
// ChessEngine
//============================================================

#include "engine.h"
#include <cctype>
#include <sstream>
#include <algorithm>

//============================================================
// Constructor
//============================================================
Engine::Engine(void)
	: gameState(GS_DRAW), timeLimit(TIME_LIMIT_DEFAULT)
{}

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
// Reset game
//============================================================
void Engine::reset(void)
{
	Position::reset();
	gameState = GS_ACTIVE;
	// Killers
	for (auto& killer : killers)
		killer.clear();
	// History table
	for (Square from = Sq::A1; from <= Sq::H8; ++from)
		for (Square to = Sq::A1; to <= Sq::H8; ++to)
			history[from][to] = SCORE_ZERO;
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
		gameState = GS_DRAW;
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
	// Update game state
	updateGameState();
	// Update game history
	gameHistory.push_back({ move, moveSAN });
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
			&& board[to] == PIECE_NULL && rfrom == relRank(2, turn))
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
// Performance test
//============================================================
int Engine::perft(Depth depth)
{
	if (depth == 0)
		return 1;
	int nodes(0);
	Move move;
	MoveList moveList;
	generatePseudolegalMoves(moveList);
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		doMove(move);
		if (isAttacked(pieceSq[opposite(turn)][KING][0], turn))
		{
			undoMove(move);
			continue;
		}
		nodes += perft(depth - 1);
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
			else if (to.getRank() == RANK_CNT - 1)
				throw std::runtime_error("Missing promotion piece type");
		}
		else
		{
			if (!validRankAN(moveSAN[1]) || moveSAN.size() > 3)
				throw std::runtime_error("Invalid pawn move destination square");
			to = Square::fromAN(moveSAN.substr(0, 2));
			fromFile = to.getFile();
			if (moveSAN.size() == 3)
			{
				if (!validPieceTypeAN(moveSAN[2]))
					throw std::runtime_error("Invalid promotion piece type");
				promotionPT = pieceTypeFromAN(moveSAN[2]);
			}
			else if (to.getRank() == RANK_CNT - 1)
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
			(fromFile == -1 || fromFile == legalMove.getFrom().getFile()) &&
			(fromRank == -1 || fromRank == legalMove.getFrom().getRank()) &&
			(to == Sq::NONE || to == legalMove.getTo()) &&
			(pieceType == PT_NULL || pieceType == getPieceType(board[legalMove.getFrom()])) &&
			(legalMove.getType() != MT_PROMOTION || promotionPT == legalMove.getPromotion()))
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
	const Square from = move.getFrom(), to = move.getTo();
	const MoveType moveType = move.getType();
	const PieceType pieceType = getPieceType(board[from]);
	bool found = false, fileUncertainty = false, rankUncertainty = false;
	for (int i = 0; i < moveList.count(); ++i)
	{
		legalMove = moveList[i].move;
		const Square lmFrom = legalMove.getFrom(), lmTo = legalMove.getTo();
		if (move == legalMove)
			found = true;
		else if (to == lmTo && pieceType == getPieceType(board[lmFrom]))
		{
			if (from.getFile() == lmFrom.getFile())
				fileUncertainty = true;
			if (from.getRank() == lmFrom.getRank())
				rankUncertainty = true;
		}
	}
	if (!found)
		throw std::runtime_error("Given move is illegal");
	if (moveType == MT_CASTLING)
		switch (move.getCastlingSide())
		{
		case OO:	return "O-O";
		case OOO:	return "O-O-O";
		}
	std::stringstream SAN;
	if (pieceType == PAWN)
	{
		if (from.getFile() != to.getFile()) // works for en passant as well
			SAN << from.getFileAN() << 'x';
		SAN << to.toAN();
		if (moveType == MT_PROMOTION)
			SAN << pieceTypeToAN(move.getPromotion());
	}
	else
	{
		SAN << pieceTypeToAN(pieceType);
		if (rankUncertainty)
			SAN << from.getFileAN();
		if (fileUncertainty)
			SAN << from.getRankAN();
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
// Internal AI logic
// Quiescent search
//============================================================
Score Engine::quiescentSearch(Score alpha, Score beta)
{
	static constexpr Score DELTA_MARGIN = 180;
	// Increment search nodes count
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
	generateLegalMoves<MG_CAPTURES>(moveList);
	// If we are in check and no evasion was found, we are lost
	if (moveList.empty())
		return isInCheck() ? SCORE_LOSE + searchPly : standPat;
	sortMoves(moveList);
	// Test every capture and choose the best one
	Move move;
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		// Delta pruning
		if (standPat + ptWeight[getPieceType(board[move.getTo()])] + DELTA_MARGIN < alpha)
			continue;
		// Test capture with SEE and if it's score is < 0, than prune
		if (board[move.getTo()] != PIECE_NULL && SEECapture(
			move.getFrom(), move.getTo(), turn) < SCORE_ZERO)
			continue;
		// Do move
		doMove(move);
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
	return alpha;
}

//============================================================
// Scores each move from moveList. Second parameter - move from TT
//============================================================
void Engine::scoreMoves(MoveList& moveList, Move ttMove)
{
	for (int i = 0; i < moveList.count(); ++i)
	{
		MLNode& moveNode = moveList[i];
		if (moveNode.move == ttMove)
		{
			moveNode.score = MS_TT_BONUS;
			continue;
		}
		moveNode.score = history[moveNode.move.getFrom()][moveNode.move.getTo()];
		/*if (isCaptureMove(moveNode.move))
			moveNode.score += MS_CAPTURE_BONUS;*/
	}
	moveList.reset();
}

//============================================================
// Main AI function
// Returns position score and outputs the best move to the reference parameter
//============================================================
Score Engine::AIMove(int& nodes, Move& bestMove, Depth& resDepth, Depth depth)
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
	ht = 0;
	lastSearchNodes = 0;
	searchPly = 0;
	bestMove = MOVE_NONE;
	int bestScore(SCORE_ZERO);
	Move move;
	// Setup time management
	startTime = std::chrono::high_resolution_clock::now();
	timeCheckCounter = 0;
	timeout = false;
	// Generate possible moves
	MoveList moveList;
	generateLegalMoves(moveList);
	// Iterative deepening
	for (Depth searchDepth = 1; searchDepth <= depth; ++searchDepth)
	{
		sortMoves(moveList, bestMove);
		// Best move and score of current iteration
		int curBestScore(SCORE_ZERO);
		Move curBestMove;
		// Aspiration windows
		int delta = 20, alpha = curBestScore - delta, beta = curBestScore + delta;
		while (true)
		{
			curBestScore = alpha;
			// Test every move and choose the best one
			++searchPly;
			bool pvSearch = true;
			for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
			{
				move = moveList[moveIdx].move;
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
					// Update history
					history[move.getFrom()][move.getTo()] += searchDepth * searchDepth;
					// If beta-cutoff occurs, stop search
					if (curBestScore >= beta)
						break;
				}
			}
			--searchPly;
			// Timeout check
			if (timeout)
				break;
			// Reset move list (so we can traverse it again)
			moveList.reset();
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
	}
	// Set nodes count and return position score
	nodes = lastSearchNodes;
	return bestScore;
}
int ht = 0;
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
		++ht;
	}
	// Generate possible moves
	MoveList moveList;
	generatePseudolegalMoves(moveList);
	// If there are no moves, it's either mate or stalemate
	if (moveList.empty())
		return isInCheck() ? SCORE_LOSE + searchPly : SCORE_ZERO;
	// Sort move list according to their move ordering scores
	sortMoves(moveList, ttMove);
	// Test every move and choose the best one
	Score bestScore = SCORE_LOSE;
	bool anyLegalMove = false, pvSearch = true;
	++searchPly;
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
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
					// Update history table
					history[move.getFrom()][move.getTo()] += depth * depth;
					// Update killers
					if (true)
					{
						killers[searchPly].push_front(move);
						if (killers[searchPly].size() > MAX_KILLERS_CNT)
							killers[searchPly].pop_back();
					}
					// Cutoff
					break;
				}
			}
		}
	}
	--searchPly;
	// Save collected info to the transposition table
	if (anyLegalMove)
		transpositionTable.store(info.keyZobrist, depth, alpha == oldAlpha ? BOUND_UPPER :
			alpha < beta ? BOUND_EXACT : BOUND_LOWER, scoreToTT(alpha), bestMove, gamePly - searchPly); // ! NOT searchPly !
	// Return alpha
	return anyLegalMove ? alpha : isInCheck() ? SCORE_LOSE + searchPly : SCORE_ZERO;
}
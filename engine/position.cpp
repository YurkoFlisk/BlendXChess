//============================================================
// position.cpp
// ChessEngine
//============================================================

#include "position.h"
#include <algorithm>
#include <string>
#include <sstream>

using namespace BlendXChess;

//============================================================
// Constructor
//============================================================
Position::Position(void)
{
	reset();
}

//============================================================
// Whether the position is valid (useful for debug)
//============================================================
bool BlendXChess::Position::isValid(void) const
{
	if (!validPieceType(info.justCaptured))
		return false;
	for (Square sq = Sq::A1; sq <= Sq::H8; ++sq)
		if (!validPiece(board[sq]))
			return false;
	return true;
}

//============================================================
// Clear game state
//============================================================
void Position::clear(void)
{
	turn = NULL_COLOR;
	gamePly = 0;
	psqScore = 0;
	info.keyZobrist = 0;
	info.rule50 = 0;
	info.justCaptured = PT_NULL;
	info.epSquare = Sq::NONE;
	info.castlingRight = CR_NULL;
	// Clear bitboards
	for (Side c = 0; c < COLOR_CNT; ++c)
		colorBB[c] = 0;
	for (PieceType pt = PT_ALL; pt <= KING; ++pt)
		pieceTypeBB[pt] = 0;
	// Clear piece lists
	for (Side c = 0; c < COLOR_CNT; ++c)
		for (PieceType pt = PT_ALL; pt <= KING; ++pt)
			pieceCount[c][pt] = 0;
	// Clear board
	for (Square sq = Sq::A1; sq <= Sq::H8; ++sq)
		board[sq] = PIECE_NULL;
}

//============================================================
// Reset game
//============================================================
void Position::reset(void)
{
	// Clear position info
	clear();
	// Reset starting position
	// Pawns
	for (Square sq = Sq::A2; sq < Sq::A3; ++sq)
		putPiece(sq, WHITE, PAWN);
	for (Square sq = Sq::A7; sq < Sq::A8; ++sq)
		putPiece(sq, BLACK, PAWN);
	// Rooks
	putPiece(Sq::A1, WHITE, ROOK);
	putPiece(Sq::H1, WHITE, ROOK);
	putPiece(Sq::A8, BLACK, ROOK);
	putPiece(Sq::H8, BLACK, ROOK);
	// Knights
	putPiece(Sq::B1, WHITE, KNIGHT);
	putPiece(Sq::G1, WHITE, KNIGHT);
	putPiece(Sq::B8, BLACK, KNIGHT);
	putPiece(Sq::G8, BLACK, KNIGHT);
	// Bishops
	putPiece(Sq::C1, WHITE, BISHOP);
	putPiece(Sq::F1, WHITE, BISHOP);
	putPiece(Sq::C8, BLACK, BISHOP);
	putPiece(Sq::F8, BLACK, BISHOP);
	// Queens
	putPiece(Sq::D1, WHITE, QUEEN);
	putPiece(Sq::D8, BLACK, QUEEN);
	// King
	putPiece(Sq::E1, WHITE, KING);
	putPiece(Sq::E8, BLACK, KING);
	// Misc
	turn = WHITE;
	info.castlingRight = CR_ALL;
	info.keyZobrist ^=
		ZobristCR[CR_WHITE_OO] ^ ZobristCR[CR_WHITE_OOO] ^
		ZobristCR[CR_BLACK_OO] ^ ZobristCR[CR_BLACK_OOO];
}

//============================================================
// Performance test (if MG_LEGAL is true, all moves are tested for legality
// during generation and promotions to bishops and rooks are included)
//============================================================
template<bool MG_LEGAL>
int Position::perft(Depth depth)
{
	if (depth == 0)
		return 1;
	int nodes(0);
	Move move;
	MoveList moveList;
	PositionInfo prevState;
	if constexpr (MG_LEGAL)
		generateLegalMovesEx(moveList);
	else
		generatePseudolegalMoves(moveList);
	for (int moveIdx = 0; moveIdx < moveList.count(); ++moveIdx)
	{
		move = moveList[moveIdx].move;
		doMove(move, prevState);
		if constexpr (!MG_LEGAL)
			if (isAttacked(pieceSq[opposite(turn)][KING][0], turn))
			{
				undoMove(move, prevState);
				continue;
			}
		nodes += perft<MG_LEGAL>(depth - 1);
		undoMove(move, prevState);
	}
	return nodes;
}

//============================================================
// Function for doing a move and updating all board state information
// Performs given move if legal and updates necessary info
// Returns false if the move is illegal, otherwise
// returns true and fills position info for undo
//============================================================
void Position::doMove(Move move, PositionInfo& outPrevInfo)
{
	const Square from = move.from(), to = move.to();
	const MoveType type = move.type();
	const PieceType from_pt = getPieceType(board[from]);
	outPrevInfo = info;
	if (info.epSquare != Sq::NONE)
	{
		info.keyZobrist ^= ZobristEP[info.epSquare.file()];
		info.epSquare = Sq::NONE; // If it is present, it will be set later
	}
	info.justCaptured = (type == MT_EN_PASSANT ? PAWN : getPieceType(board[to]));
	if (info.justCaptured != PT_NULL)
	{
		if (type == MT_EN_PASSANT)
			removePiece(to + (turn == WHITE ? Sq::D_DOWN : Sq::D_UP));
		else
		{
			if (info.justCaptured == ROOK) // Check for rook here is redundant, but good for performance
			{
				if (to == relSquare(Sq::A1, opposite(turn)))
					removeCastlingRight(makeCastling(opposite(turn), OOO));
				else if (to == relSquare(Sq::H1, opposite(turn)))
					removeCastlingRight(makeCastling(opposite(turn), OO));
			}
			removePiece(to);
		}
		info.rule50 = 0;
	}
	else if (from_pt == PAWN)
	{
		if (abs(to - from) == 16)
		{
			info.epSquare = (from + to) >> 1;
			info.keyZobrist ^= ZobristEP[info.epSquare.file()];
		}
		info.rule50 = 0;
	}
	else
		++info.rule50;
	if (from_pt == KING)
	{
		removeCastlingRight(makeCastling(turn, OO));
		removeCastlingRight(makeCastling(turn, OOO));
	}
	else if (from_pt == ROOK)
		if (from == relSquare(Sq::A1, turn))
			removeCastlingRight(makeCastling(turn, OOO));
		else if (from == relSquare(Sq::H1, turn))
			removeCastlingRight(makeCastling(turn, OO));
	if (type == MT_PROMOTION)
	{
		putPiece(to, turn, move.promotion());
		removePiece(from);
	}
	else
		movePiece(from, to);
	if (type == MT_CASTLING)
	{
		const bool kingSide = (to > from);
		movePiece(Square(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('h') : fileFromAN('a')),
			Square(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('f') : fileFromAN('d')));
	}
	info.keyZobrist ^= ZobristBlackSide;
	turn = opposite(turn);
	++gamePly;
}

//============================================================
// Function for undoing a move and restoring to a previous state
//============================================================
void Position::undoMove(Move move, const PositionInfo& prevInfo)
{
	const Square from = move.from(), to = move.to();
	const MoveType type = move.type();
	--gamePly;
	turn = opposite(turn);
	if (type == MT_PROMOTION)
	{
		putPiece(from, turn, PAWN);
		removePiece(to);
	}
	else
		movePiece(to, from);
	if (info.justCaptured != PT_NULL)
		if (type == MT_EN_PASSANT)
			putPiece(to + (turn == WHITE ? Sq::D_DOWN : Sq::D_UP), opposite(turn), info.justCaptured);
		else
			putPiece(to, opposite(turn), info.justCaptured);
	if (type == MT_CASTLING)
	{
		const bool kingSide = (to > from);
		movePiece(Square(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('f') : fileFromAN('d')),
			Square(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('h') : fileFromAN('a')));
	}
	info = prevInfo;
}

//============================================================
// Internal test for pseudo-legality (still assumes some conditions
// which TT-move satisfies, eg castlings have appropriate from-to squares)
//============================================================
bool Position::isPseudoLegal(Move move) const
{
	const Square from = move.from(), to = move.to();
	if (getPieceSide(board[from]) != turn || getPieceSide(board[to]) == turn
		|| occupiedBB() & bbBetween[from][to])
		return false;
	// We don't check eg whether the king is on E1/E8 square, since tested move
	// is assumed to be previously generated for some valid position
	// Check on 'from' is needed if 'move' could have been generated for opposite side to 'turn'
	if (move.type() == MT_CASTLING)
		if (move.castlingSide() == OO)
			return info.castlingRight & makeCastling(turn, OO) && from == relSquare(Sq::E1, turn)
				&& (occupiedBB() & bbCastlingInner[turn][OO]) == 0
				&& !isAttacked(from, opposite(turn))
				&& !isAttacked(relSquare(Sq::F1, turn), opposite(turn))
				&& !isAttacked(relSquare(Sq::G1, turn), opposite(turn));
		else
			return info.castlingRight & makeCastling(turn, OOO) && from == relSquare(Sq::E1, turn)
				&& (occupiedBB() & bbCastlingInner[turn][OOO]) == 0
				&& !isAttacked(from, opposite(turn))
				&& !isAttacked(relSquare(Sq::D1, turn), opposite(turn))
				&& !isAttacked(relSquare(Sq::C1, turn), opposite(turn));
	if (const PieceType pt = getPieceType(board[from]); pt == PAWN)
	{
		if (move.type() == MT_EN_PASSANT)
			return to == info.epSquare && bbPawnAttack[turn][from] & bbSquare[to];
		else
			return bbSquare[to] & (board[to] == PIECE_NULL ? bbPawnQuiet : bbPawnAttack)[turn][from];
	}
	else
		return bbAttackEB[pt][from] & bbSquare[to];
}

//============================================================
// Reveal PAWN moves in given direction from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Side TURN, bool LEGAL>
void Position::revealPawnMoves(Bitboard destBB, Square direction, MoveList& moves) const
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr PieceType promPieceType[2] = { KNIGHT, QUEEN };
	static constexpr Piece TURN_PAWN = (TURN == WHITE ? W_PAWN : B_PAWN);
	static constexpr Square LEFT_CAPT = (TURN == WHITE ? Sq::D_LU : Sq::D_LD);
	static constexpr Square RIGHT_CAPT = (TURN == WHITE ? Sq::D_RU : Sq::D_RD);
	static constexpr Square FORWARD = (TURN == WHITE ? Sq::D_UP : Sq::D_DOWN);
	assert(direction == LEFT_CAPT || direction == RIGHT_CAPT || direction == FORWARD); // Maybe make direction a template parameter?
	while (destBB)
	{
		const Square to = popLSB(destBB), from = to - direction;
		assert(board[from] == TURN_PAWN);
		assert(getPieceSide(board[to]) == (direction == FORWARD ? NULL_COLOR : opposite(TURN)));
		if (TURN == WHITE ? to > Sq::H7 : to < Sq::A2)
			for (int promIdx = 0; promIdx < 2; ++promIdx)
				addMoveIfSuitable<TURN, LEGAL>(Move(from, to, MT_PROMOTION, promPieceType[promIdx]), moves);
		else
			addMoveIfSuitable<TURN, LEGAL>(Move(from, to), moves);
	}
}

//============================================================
// Reveal NON-PAWN moves from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Side TURN, bool LEGAL>
void Position::revealMoves(Square from, Bitboard destBB, MoveList& moves) const
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	while (destBB)
	{
		const Square to = popLSB(destBB);
		assert(getPieceSide(board[from]) == TURN);
		assert(getPieceSide(board[to]) != TURN);
		addMoveIfSuitable<TURN, LEGAL>(Move(from, to), moves);
	}
}

//============================================================
// Generate pawn moves. Only to distBB squares if MG_TYPE == MG_EVASIONS
//============================================================
template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
void Position::generatePawnMoves(MoveList& moves, Bitboard destBB) const
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr Piece TURN_PAWN =        (TURN == WHITE ? W_PAWN : B_PAWN);
	static constexpr Square LEFT_CAPT =       (TURN == WHITE ? Sq::D_LU : Sq::D_LD);
	static constexpr Square RIGHT_CAPT =      (TURN == WHITE ? Sq::D_RU : Sq::D_RD);
	static constexpr Square FORWARD =         (TURN == WHITE ? Sq::D_UP : Sq::D_DOWN);
	static constexpr Bitboard BB_REL_RANK_3 = (TURN == WHITE ? BB_RANK_3 : BB_RANK_6);
	if constexpr (MG_TYPE != MG_NON_CAPTURES)
	{
		// Left and right pawn capture moves (including promotions)
		if constexpr (MG_TYPE == MG_EVASIONS) // destBB is valid in this case
		{
			revealPawnMoves<TURN, LEGAL>(bbShiftD<LEFT_CAPT>(
				pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)] & destBB, LEFT_CAPT, moves);
			revealPawnMoves<TURN, LEGAL>(bbShiftD<RIGHT_CAPT>(
				pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)] & destBB, RIGHT_CAPT, moves);
		}
		else
		{
			revealPawnMoves<TURN, LEGAL>(bbShiftD<LEFT_CAPT>(
				pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)], LEFT_CAPT, moves);
			revealPawnMoves<TURN, LEGAL>(bbShiftD<RIGHT_CAPT>(
				pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)], RIGHT_CAPT, moves);
		}
		// En passant. If MG_TYPE == MG_EVASIONS, check was either not double-pawn push (so epSquare is Sq::NONE)
		// or there is epSquare and we want to consider EP evasion (because we consider this
		// here, it's also not mandatory to include EP square in destBB in generateMoves)
		if (info.epSquare != Sq::NONE) // && destBB & bbSquare[info.epSquare] not needed
		{
			assert(board[info.epSquare] == PIECE_NULL && board[info.epSquare + FORWARD] == PIECE_NULL);
			assert(board[info.epSquare - FORWARD] == makePiece(opposite(TURN), PAWN));
			Square from;
			if (info.epSquare.file() != 7 && board[from = info.epSquare - LEFT_CAPT] == TURN_PAWN)
				addMoveIfSuitable<TURN, LEGAL>(Move(from, info.epSquare, MT_EN_PASSANT), moves);
			if (info.epSquare.file() != 0 && board[from = info.epSquare - RIGHT_CAPT] == TURN_PAWN)
				addMoveIfSuitable<TURN, LEGAL>(Move(from, info.epSquare, MT_EN_PASSANT), moves);
		}
	}
	if constexpr (MG_TYPE != MG_CAPTURES)
	{
		if constexpr (MG_TYPE == MG_EVASIONS) // destBB is valid in this case
		{
			// One-step pawn forward moves (including promotions)
			const Bitboard pawnDestBB = bbShiftD<FORWARD>(pieceBB(TURN, PAWN)) & emptyBB();
			revealPawnMoves<TURN, LEGAL>(destBB & pawnDestBB, FORWARD, moves);
			// Two-step pawn forward moves (here we can't promote, so don't use revealPawnMoves)
			destBB &= bbShiftD<FORWARD>(pawnDestBB & BB_REL_RANK_3) & emptyBB();
		}
		else // here we can do without new pawnDestBB variable
		{
			// One-step pawn forward moves (including promotions)
			destBB = bbShiftD<FORWARD>(pieceBB(TURN, PAWN)) & emptyBB();
			revealPawnMoves<TURN, LEGAL>(destBB, FORWARD, moves);
			// Two-step pawn forward moves (here we can't promote, so don't use revealPawnMoves)
			destBB = bbShiftD<FORWARD>(destBB & BB_REL_RANK_3) & emptyBB();
		}
		// Manually reveal moves from destination bitboard
		while (destBB)
		{
			const Square to = popLSB(destBB);
			assert(getPieceSide(board[to]) == NULL_COLOR);
			addMoveIfSuitable<TURN, LEGAL>(Move(to - (FORWARD + FORWARD), to), moves);
		}
	}
}

//============================================================
// Generate non-pawn and non-king moves. Only to destBB squares irrespectively of MG_TYPE
//============================================================
template<Side TURN, bool LEGAL>
void Position::generateFigureMoves(MoveList& moves, Bitboard destBB) const
{
	Square from;
	// Knight moves
	for (int i = 0; i < pieceCount[TURN][KNIGHT]; ++i)
	{
		from = pieceSq[turn][KNIGHT][i];
		revealMoves<TURN, LEGAL>(from, bbKnightAttack[from] & destBB, moves);
	}
	// Rook and partially queen moves
	for (int i = 0; i < pieceCount[TURN][ROOK]; ++i)
	{
		from = pieceSq[turn][ROOK][i];
		revealMoves<TURN, LEGAL>(from, magicRookAttacks(from, occupiedBB()) & destBB, moves);
	}
	for (int i = 0; i < pieceCount[TURN][QUEEN]; ++i)
	{
		from = pieceSq[turn][QUEEN][i];
		revealMoves<TURN, LEGAL>(from, magicRookAttacks(from, occupiedBB()) & destBB, moves);
	}
	// Bishop and partially queen moves
	for (int i = 0; i < pieceCount[TURN][BISHOP]; ++i)
	{
		from = pieceSq[turn][BISHOP][i];
		revealMoves<TURN, LEGAL>(from, magicBishopAttacks(from, occupiedBB()) & destBB, moves);
	}
	for (int i = 0; i < pieceCount[TURN][QUEEN]; ++i)
	{
		from = pieceSq[turn][QUEEN][i];
		revealMoves<TURN, LEGAL>(from, magicBishopAttacks(from, occupiedBB()) & destBB, moves);
	}
}

//============================================================
// Generate all moves (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
void Position::generateMoves(MoveList& moves) const
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr Side OPPONENT = opposite(TURN);
	static constexpr Square REL_SQ_E1 = relSquare(Sq::E1, TURN),
		REL_SQ_C1 = relSquare(Sq::C1, TURN), REL_SQ_D1 = relSquare(Sq::D1, TURN),
		REL_SQ_G1 = relSquare(Sq::G1, TURN), REL_SQ_F1 = relSquare(Sq::F1, TURN);
	// TURN is a template parameter and is used only for optimization purposes, so it should be equal to turn
	assert(TURN == turn);
	// Position should be valid here
	assert(isValid());
	// For evasions we use more efficient approach
	if constexpr (MG_TYPE == MG_EVASIONS)
	{
		// Since we are generating evasions, we should be in check
		assert(isInCheck());

		const Square kingSq = pieceSq[TURN][KING][0];
		const Bitboard checkers = allAttackers(kingSq, opposite(TURN));
		// There must be at least one checker and in standard chess there are no tripple ot higher order checks
		assert(checkers && countSet(checkers) <= 2);
		// Generate king moves to unattacked squares
		const Bitboard rescueBB = ~colorBB[TURN] & bbKingAttack[kingSq];
		revealMoves<TURN, LEGAL>(kingSq, rescueBB, moves);
		// If check is not double, we can obstruct checking path or capture the checker
		if (zeroOrSingular(checkers)) // we know it's not zero
		{
			const Square checker = getLSB(checkers);
			const Bitboard destBB = bbBetween[checker][kingSq] | bbSquare[checker];
			// Generate pawn and usual piece (without king) moves to appropriate
			// destinations, omit castlings (they can't be legal during checks)
			generatePawnMoves<TURN, MG_TYPE, LEGAL>(moves, destBB);
			generateFigureMoves<TURN, LEGAL>(moves, destBB);
		}
	}
	else
	{
		// Pawn moves
		generatePawnMoves<TURN, MG_TYPE, LEGAL>(moves);
		// Castlings. Their legality (king's path should not be under attack) is checked right here
		if constexpr (MG_TYPE != MG_CAPTURES)
		{
			if ((info.castlingRight & makeCastling(TURN, OO)) &&
				(occupiedBB() & bbCastlingInner[TURN][OO]) == 0 &&
				!(isAttacked(REL_SQ_G1, OPPONENT) || isAttacked(REL_SQ_F1, OPPONENT) ||
					isAttacked(REL_SQ_E1, OPPONENT)))
				moves.add(Move(REL_SQ_E1, REL_SQ_G1, MT_CASTLING));
			if ((info.castlingRight & makeCastling(TURN, OOO)) &&
				(occupiedBB() & bbCastlingInner[TURN][OOO]) == 0 &&
				!(isAttacked(REL_SQ_C1, OPPONENT) || isAttacked(REL_SQ_D1, OPPONENT) ||
					isAttacked(REL_SQ_E1, OPPONENT)))
				moves.add(Move(REL_SQ_E1, REL_SQ_C1, MT_CASTLING));
		}
		Bitboard destBB;
		// Usual piece moves
		if constexpr (MG_TYPE == MG_CAPTURES)
			destBB = colorBB[opposite(TURN)];
		else if constexpr (MG_TYPE == MG_NON_CAPTURES)
			destBB = emptyBB();
		else // if constexpr (MG_TYPE == MG_ALL), omitted because MG_EVASIONS is handled before
			destBB = ~colorBB[TURN];
		generateFigureMoves<TURN, LEGAL>(moves, destBB);
		// King moves
		const Square kingSq = pieceSq[turn][KING][0];
		revealMoves<TURN, LEGAL>(kingSq, bbKingAttack[kingSq] & destBB, moves);
	}
	// For debugging purposes this is sometimes needed to make move
	// ordering independent of current order of pieces in piece lists
	// In these cases it may also be useful to disable history and countermove heuristics
#ifdef ENGINE_DEBUG
	if constexpr (SORT_GENMOVES_ON_DEBUG)
	{
		std::sort(moves.begin(), moves.end(),
			[](const MLNode& ml1, const MLNode& ml2) {
			return ml1.move.raw() < ml2.move.raw();
		});
	}
#endif
}

//============================================================
// Convert a move from AN notation to Move. It should be valid in current position
//============================================================
Move Position::moveFromAN(const std::string& strMove)
{
	if (strMove == "O-O")
		return Move(turn, OO);
	else if (strMove == "O-O-O")
		return Move(turn, OOO);
	else if (strMove.size() < 5 || strMove.size() > 6 || strMove[2] != '-')
		return MOVE_NONE;
	else
	{
		const int8_t rfrom = strMove[1] - '1', ffrom = strMove[0] - 'a',
			rto = strMove[4] - '1', fto = strMove[3] - 'a';
		if (!validRank(rfrom) || !validFile(ffrom)
			|| !validRank(rto) || !validFile(fto))
			return MOVE_NONE;
		const Square from = Square(rfrom, ffrom), to = Square(rto, fto);
		if (strMove.size() == 6)
			switch (toupper(strMove[5]))
			{
			case 'N': return Move(from, to, MT_PROMOTION, KNIGHT);
			case 'B': return Move(from, to, MT_PROMOTION, BISHOP);
			case 'R': return Move(from, to, MT_PROMOTION, ROOK);
			case 'Q': return Move(from, to, MT_PROMOTION, QUEEN);
			default: return MOVE_NONE;
			}
		else if (getPieceType(board[from]) == PAWN && distance(from, to) == 2
			&& board[to] == PIECE_NULL && rto == relRank(2, turn))
			return Move(from, to, MT_EN_PASSANT);
		else
			return Move(from, to);
	}
}

//============================================================
// Convert a move from SAN notation to Move. It should be valid in current position
//============================================================
Move Position::moveFromSAN(const std::string& moveSAN)
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
std::string Position::moveToSAN(Move move) const
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
		if (from.file() != to.file()) // works for endTime passant as well
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
// Convert a move from UCI notation. It should be legal in current position
//============================================================
Move Position::moveFromUCI(const std::string& strMove)
{
	Move move;
	if (strMove == "e1g1")
		move = Move(WHITE, OO);
	else if (strMove == "e1c1")
		move = Move(WHITE, OOO);
	else if (strMove == "e8g8")
		move = Move(BLACK, OO);
	else if (strMove == "e8c8")
		move = Move(BLACK, OOO);
	else
	{
		std::string anStrMove = strMove;
		anStrMove.insert(2, 1, '-');
		move = moveFromAN(anStrMove);
	}
	return move;
}

//============================================================
// Convert move from given string format
//============================================================
Move Position::moveFromStr(const std::string& moveStr, MoveFormat moveFormat)
{
	switch (moveFormat)
	{
	case FMT_AN: return moveFromAN(moveStr);
	case FMT_SAN: return moveFromSAN(moveStr);
	case FMT_UCI: return moveFromUCI(moveStr);
	default: assert(false); return MOVE_NONE; // Should't get here
	}
}

//============================================================
// Convert move to given string format
//============================================================
std::string Position::moveToStr(Move move, MoveFormat moveFormat) const
{
	switch (moveFormat)
	{
	case FMT_AN: return move.toAN();
	case FMT_SAN: return moveToSAN(move);
	case FMT_UCI: return move.toUCI();
	default: assert(false); return ""; // Should't get here
	}
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
//============================================================
bool Position::DoMove(Move move, PositionInfo* prevInfo)
{
	// Generate all legal moves
	MoveList legalMoves;
	generateLegalMovesEx(legalMoves);
	// Check whether given move is among legal ones. If it's not, we can't perform it.
	if (std::find_if(legalMoves.begin(), legalMoves.end(), [move](const MLNode& elem) {
		return elem.move == move;}) == legalMoves.end())
		return false;
	// Save previous state info in case it's requested
	if (prevInfo)
		doMove(move, *prevInfo);
	else
	{
		PositionInfo temp;
		doMove(move, temp);
	}
	return true;
}

//============================================================
// Do move given as string of given format.
// Return true if succeded, false otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
//============================================================
bool Position::DoMove(const std::string& moveStr, MoveFormat moveFormat,
	Move* outMove, PositionInfo* prevState)
{
	// Convert the move from string representation to Move type. Legality there may be
	// checked only partially (to the stage when move is convertible to Move representation),
	// and MOVE_NONE is returned in case of error
	const Move move = moveFromStr(moveStr, moveFormat);
	// Use Move type representation of move for next actions
	if (DoMove(move, prevState))
	{
		if (outMove)
			*outMove = move;
		return true;
	}
	else
		return false;
}

//============================================================
// Undo last move. Return true if succeeded, false otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use undoMove instead
//============================================================
bool Position::UndoMove(Move move, const PositionInfo& prevInfo)
{
	// To check validity of undo we try to perform undo and
	// see whether the move is among legal ones for resultant position
	const Position backup = *this;
	MoveList legalMoves;
	try
	{
		undoMove(move, prevInfo);
		generateLegalMovesEx(legalMoves);
		if (std::find_if(legalMoves.begin(), legalMoves.end(), [move](const MLNode& elem) {
			return elem.move == move; }) == legalMoves.end())
			throw std::exception(); // Just to avoid duplicating code of catch block
	}
	// There may have been an exception due to uncheked
	// actions (during undo/generation) in invalid position state
	catch (...)
	{
		*this = backup; // Restore position from potentially invalid state
		return false;
	}
	return true;
}

//============================================================
// Load position from a given stream in FEN notation
// (bool parameter says whether to omit move counters)
//============================================================
void Position::loadFEN(std::istream& istr, bool omitCounters)
{
	// Clear current game state
	clear();
	// Piece placement information
	char delim, piece;
	for (int rank = 7; rank >= 0; --rank)
	{
		for (int file = 0; file < 8; ++file)
		{
			istr >> piece;
			if (isdigit(piece))
			{
				const int filePass = piece - '0';
				if (filePass == 0 || file + filePass > 8)
					throw std::runtime_error("Invalid file pass number "
						+ std::to_string(filePass));
				file += filePass - 1; // - 1 because of the following ++file
				continue;
			}
			const PieceType pieceType = pieceTypeFromFEN(toupper(piece));
			putPiece(Square(rank, file), isupper(piece) ? WHITE : BLACK, pieceType);
		}
		if (rank)
		{
			istr >> delim;
			if (delim != '/')
				throw std::runtime_error("Missing/invalid rank delimiter "
					+ std::string({ delim }));
		}
	}
	// Side to move information
	char side;
	istr >> side;
	if (side == 'w')
		turn = WHITE;
	else if (side == 'b')
		turn = BLACK, info.keyZobrist ^= ZobristBlackSide;
	else
		throw std::runtime_error("Invalid side to move identifier "
			+ std::string({ side }));
	// Castling ability information
	char castlingRight, castlingSide;
	istr >> castlingRight;
	if (castlingRight != '-')
		do
		{
			castlingSide = tolower(castlingRight);
			if (castlingSide != 'k' && castlingSide != 'q')
				throw std::runtime_error("Invalid castling right token "
					+ std::string({ castlingRight }));
			const CastlingRight crMask = makeCastling(isupper(castlingRight) ? WHITE : BLACK,
				castlingSide == 'k' ? OO : OOO);
			info.castlingRight |= crMask;
			info.keyZobrist ^= ZobristCR[crMask];
		} while ((castlingRight = istr.get()) != ' ');
	// En passant information
	char epFile;
	istr >> epFile;
	if (epFile != '-')
	{
		int epRank;
		istr >> epRank;
		if (!validRank(epRank) || !validFile(fileFromAN(epFile))
			|| (epRank != 2 && epRank != 5))
			throw std::runtime_error("Invalid en-passant square "
				+ std::string({ epFile, rankToAN(epRank) }));
		info.epSquare = Square(fileFromAN(epFile), epRank);
		info.keyZobrist ^= ZobristEP[fileFromAN(epFile)];
	}
	if (!omitCounters)
	{
		// Halfmove counter (for 50 move draw rule) information
		istr >> info.rule50;
		if (info.rule50 < 0 || 50 < info.rule50)
			throw std::runtime_error("Rule-50 halfmove counter"
				+ std::to_string(info.rule50) + " is invalid ");
		// Counter of full moves (starting at 1) information
		int fullMoves;
		istr >> fullMoves;
		if (fullMoves <= 0)
			throw std::runtime_error("Invalid full move counter "
				+ std::to_string(fullMoves));
		gamePly = (fullMoves - 1) * 2 + (side == 'b' ? 1 : 0);
	}
}

//============================================================
// Load position from a given string in FEN notation
// (bool parameter says whether to omit move counters)
//============================================================
void Position::loadFEN(const std::string& str, bool omitCounters)
{
	std::istringstream iss(str);
	loadFEN(iss, omitCounters);
}

//============================================================
// Write position to a given stream in FEN notation
//============================================================
void Position::writeFEN(std::ostream& ostr, bool omitCounters) const
{
	// Piece placement information
	for (int rank = 7, consecutiveEmpty = 0; rank >= 0; --rank)
	{
		for (int file = 0; file < 8; ++file)
		{
			const Piece curPiece = board[Square(rank, file)];
			if (curPiece == PIECE_NULL)
			{
				++consecutiveEmpty;
				continue;
			}
			if (consecutiveEmpty)
			{
				ostr << consecutiveEmpty;
				consecutiveEmpty = 0;
			}
			char cur_ch = pieceTypeToFEN(getPieceType(curPiece));
			if (getPieceSide(curPiece) == BLACK)
				cur_ch = tolower(cur_ch);
		}
		if (consecutiveEmpty)
			ostr << consecutiveEmpty;
		ostr << (rank == 0 ? ' ' : '/');
	}
	// Side to move information
	ostr << (turn == WHITE ? "w " : "b ");
	// Castling ability information
	if (info.castlingRight == CR_NULL)
		ostr << "- ";
	else
	{
		if (info.castlingRight & CR_WHITE_OO)
			ostr << 'K';
		if (info.castlingRight & CR_WHITE_OOO)
			ostr << 'Q';
		if (info.castlingRight & CR_BLACK_OO)
			ostr << 'k';
		if (info.castlingRight & CR_BLACK_OOO)
			ostr << 'q';
		ostr << ' ';
	}
	// En passant information
	if (info.epSquare == Sq::NONE)
		ostr << "- ";
	else
		ostr << info.epSquare.file() + 'a' << info.epSquare.rank() << ' ';
	if (!omitCounters)
	{
		// Halfmove counter (for 50 move draw rule) information
		ostr << info.rule50 << ' ';
		// Counter of full moves (starting at 1) information
		ostr << gamePly / 2;
	}
}

//============================================================
// Explicit template instantiations
//============================================================
template void Position::revealPawnMoves<WHITE, true>(Bitboard, Square, MoveList&) const;
template void Position::revealPawnMoves<WHITE, false>(Bitboard, Square, MoveList&) const;
template void Position::revealPawnMoves<BLACK, true>(Bitboard, Square, MoveList&) const;
template void Position::revealPawnMoves<BLACK, false>(Bitboard, Square, MoveList&) const;
template void Position::revealMoves<WHITE, true>(Square, Bitboard, MoveList&) const;
template void Position::revealMoves<WHITE, false>(Square, Bitboard, MoveList&) const;
template void Position::revealMoves<BLACK, true>(Square, Bitboard, MoveList&) const;
template void Position::revealMoves<BLACK, false>(Square, Bitboard, MoveList&) const;
template void Position::generatePawnMoves<WHITE, MG_NON_CAPTURES, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<WHITE, MG_NON_CAPTURES, false>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<WHITE, MG_CAPTURES, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<WHITE, MG_CAPTURES, false>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<WHITE, MG_ALL, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<WHITE, MG_ALL, false>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_NON_CAPTURES, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_NON_CAPTURES, false>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_CAPTURES, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_CAPTURES, false>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_ALL, true>(MoveList&, Bitboard) const;
template void Position::generatePawnMoves<BLACK, MG_ALL, false>(MoveList&, Bitboard) const;
template void Position::generateFigureMoves<WHITE, true>(MoveList&, Bitboard) const;
template void Position::generateFigureMoves<WHITE, false>(MoveList&, Bitboard) const;
template void Position::generateFigureMoves<BLACK, true>(MoveList&, Bitboard) const;
template void Position::generateFigureMoves<BLACK, false>(MoveList&, Bitboard) const;
template void Position::generateMoves<WHITE, MG_EVASIONS, true>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_EVASIONS, false>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_NON_CAPTURES, true>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_NON_CAPTURES, false>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_CAPTURES, true>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_CAPTURES, false>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_ALL, true>(MoveList&) const;
template void Position::generateMoves<WHITE, MG_ALL, false>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_EVASIONS, true>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_EVASIONS, false>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_NON_CAPTURES, true>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_NON_CAPTURES, false>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_CAPTURES, true>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_CAPTURES, false>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_ALL, true>(MoveList&) const;
template void Position::generateMoves<BLACK, MG_ALL, false>(MoveList&) const;
template int Position::perft<false>(Depth);
template int Position::perft<true>(Depth);
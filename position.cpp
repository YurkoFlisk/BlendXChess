//============================================================
// position.cpp
// ChessEngine
//============================================================

#include "position.h"

//============================================================
// Reset game
//============================================================
void Position::reset(void)
{
	// Reset position info
	turn = WHITE;
	gamePly = 0;
	psqScore = 0;
	info.keyZobrist = 0;
	info.rule50 = 0;
	info.justCaptured = NO_PIECE_TYPE;
	info.epSquare = SQ_NONE;
	info.castlingRight = CR_ALL;
	// Reset bitboards
	for (Color c = 0; c < COLOR_CNT; ++c)
		colorBB[c] = 0;
	for (PieceType pt = PT_ALL; pt <= KING; ++pt)
		pieceTypeBB[pt] = 0;
	// Reset piece lists
	for (Color c = 0; c < COLOR_CNT; ++c)
		for (PieceType pt = PT_ALL; pt <= KING; ++pt)
			pieceCount[c][pt] = 0;
	// Reset board
	for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
		board[sq] = NO_PIECE;
	// Pawns
	for (Square sq = SQ_A2; sq < SQ_A3; ++sq)
		putPiece(sq, WHITE, PAWN);
	for (Square sq = SQ_A7; sq < SQ_A8; ++sq)
		putPiece(sq, BLACK, PAWN);
	// Rooks
	putPiece(SQ_A1, WHITE, ROOK);
	putPiece(SQ_H1, WHITE, ROOK);
	putPiece(SQ_A8, BLACK, ROOK);
	putPiece(SQ_H8, BLACK, ROOK);
	// Knights
	putPiece(SQ_B1, WHITE, KNIGHT);
	putPiece(SQ_G1, WHITE, KNIGHT);
	putPiece(SQ_B8, BLACK, KNIGHT);
	putPiece(SQ_G8, BLACK, KNIGHT);
	// Bishops
	putPiece(SQ_C1, WHITE, BISHOP);
	putPiece(SQ_F1, WHITE, BISHOP);
	putPiece(SQ_C8, BLACK, BISHOP);
	putPiece(SQ_F8, BLACK, BISHOP);
	// Queens
	putPiece(SQ_D1, WHITE, QUEEN);
	putPiece(SQ_D8, BLACK, QUEEN);
	// King
	putPiece(SQ_E1, WHITE, KING);
	putPiece(SQ_E8, BLACK, KING);
}

//============================================================
// Whether a square is attacked by given side
//============================================================
bool Position::isAttacked(Square sq, Color by) const
{
	assert(by == WHITE || by == BLACK);
	assert(validSquare(sq));
	return	(bbPawnAttack[opposite(by)][sq] &			pieceBB(by, PAWN)) ||
			(bbKnightAttack[sq] &						pieceBB(by, KNIGHT)) ||
			(bbKingAttack[sq] &							pieceBB(by, KING)) ||
			(magicRookAttacks(sq, occupiedBB()) & (		pieceBB(by, ROOK) | pieceBB(by, QUEEN))) ||
			(magicBishopAttacks(sq, occupiedBB()) & (	pieceBB(by, BISHOP) | pieceBB(by, QUEEN)));
}

//============================================================
// Least valuable attacker on given square by given side(king is considered most valuable here)
//============================================================
Square Position::leastAttacker(Square sq, Color by) const
{
	assert(by == WHITE || by == BLACK);
	assert(validSquare(sq));
	Bitboard from;
	if (from = (bbPawnAttack[opposite(by)][sq] & pieceBB(by, PAWN)))
		return getLSB(from);
	if (from = (bbKnightAttack[sq] & pieceBB(by, KNIGHT)))
		return getLSB(from);
	Bitboard mBA, mRA;
	if (from = ((mBA = magicBishopAttacks(sq, occupiedBB())) & pieceBB(by, BISHOP)))
		return getLSB(from);
	if (from = ((mRA = magicRookAttacks(sq, occupiedBB())) & pieceBB(by, ROOK)))
		return getLSB(from);
	if (from = ((mBA | mRA) & pieceBB(by, QUEEN)))
		return getLSB(from);
	if (from = (bbKingAttack[sq] & pieceBB(by, KING)))
		return getLSB(from);
	return SQ_NONE;
}

//============================================================
// Function for doing a move and updating all board state information
//============================================================
void Position::doMove(Move move)
{
	const Square from = getFrom(move), to = getTo(move);
	const MoveType type = getMoveType(move);
	const PieceType from_pt = getPieceType(board[from]);
	prevStates[gamePly] = info;
	if (info.epSquare != SQ_NONE)
	{
		info.keyZobrist ^= ZobristEP[getFile(info.epSquare)];
		info.epSquare = SQ_NONE; // If it is present, it will be set later
	}
	info.justCaptured = (type == MT_EN_PASSANT ? PAWN : getPieceType(board[to]));
	if (info.justCaptured != NO_PIECE_TYPE)
	{
		if (type == MT_EN_PASSANT)
			removePiece(to + (turn == WHITE ? D_DOWN : D_UP));
		else
			removePiece(to);
		info.rule50 = 0;
	}
	else if (from_pt == PAWN)
	{
		if (abs(to - from) == 16)
		{
			info.epSquare = (from + to) >> 1;
			info.keyZobrist ^= ZobristEP[getFile(info.epSquare)];
		}
		info.rule50 = 0;
	}
	else
		++info.rule50;
	if (from_pt == KING)
		if (turn == WHITE)
		{
			if (info.castlingRight & WHITE_OO)
				info.castlingRight &= ~WHITE_OO, info.keyZobrist ^= ZobristCR[WHITE_OO];
			if (info.castlingRight & WHITE_OOO)
				info.castlingRight &= ~WHITE_OOO, info.keyZobrist ^= ZobristCR[WHITE_OOO];
		}
		else
		{
			if (info.castlingRight & BLACK_OO)
				info.castlingRight &= ~BLACK_OO, info.keyZobrist ^= ZobristCR[BLACK_OO];
			if (info.castlingRight & BLACK_OOO)
				info.castlingRight &= ~BLACK_OOO, info.keyZobrist ^= ZobristCR[BLACK_OOO];
		}
	else if (from_pt == ROOK)
		if (turn == WHITE)
		{
			if (from == SQ_A1 && (info.castlingRight & WHITE_OOO))
				info.castlingRight &= ~WHITE_OOO, info.keyZobrist ^= ZobristCR[WHITE_OOO];
			else if (from == SQ_H1 && (info.castlingRight & WHITE_OO))
				info.castlingRight &= ~WHITE_OO, info.keyZobrist ^= ZobristCR[WHITE_OO];
		}
		else if (from == SQ_A8 && (info.castlingRight & BLACK_OOO))
			info.castlingRight &= ~BLACK_OOO, info.keyZobrist ^= ZobristCR[BLACK_OOO];
		else if (from == SQ_H8 && (info.castlingRight & BLACK_OO))
			info.castlingRight &= ~BLACK_OO, info.keyZobrist ^= ZobristCR[BLACK_OO];
	if (type == MT_PROMOTION)
	{
		putPiece(to, turn, getPromotion(move));
		removePiece(from);
	}
	else
		movePiece(from, to);
	if (type == MT_CASTLING)
	{
		const bool kingSide = (to > from);
		movePiece(makeSquare(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('h') : fileFromAN('a')),
			makeSquare(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('f') : fileFromAN('d')));
	}
	info.keyZobrist ^= ZobristSide;
	turn = opposite(turn);
	++gamePly;
}

//============================================================
// Function for undoing a move and restoring to a previous state
//============================================================
void Position::undoMove(Move move)
{
	const Square from = getFrom(move), to = getTo(move);
	const MoveType type = getMoveType(move);
	--gamePly;
	turn = opposite(turn);
	if (type == MT_PROMOTION)
	{
		putPiece(from, turn, PAWN);
		removePiece(to);
	}
	else
		movePiece(to, from);
	if (info.justCaptured != NO_PIECE_TYPE)
		if (type == MT_EN_PASSANT)
			putPiece(to + (turn == WHITE ? D_DOWN : D_UP), opposite(turn), info.justCaptured);
		else
			putPiece(to, opposite(turn), info.justCaptured);
	if (type == MT_CASTLING)
	{
		const bool kingSide = (to > from);
		movePiece(makeSquare(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('f') : fileFromAN('d')),
			makeSquare(turn == WHITE ? 0 : 7, kingSide ? fileFromAN('h') : fileFromAN('a')));
	}
	info = prevStates[gamePly];
}

//============================================================
// Reveal PAWN moves in given direction from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Color TURN, bool LEGAL>
void Position::revealPawnMoves(Bitboard destBB, Square direction, MoveList& moves)
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr PieceType promPieceType[2] = { KNIGHT, QUEEN };
	static constexpr Piece TURN_PAWN = (TURN == WHITE ? W_PAWN : B_PAWN);
	static constexpr Square LEFT_CAPT = (TURN == WHITE ? D_LU : D_LD);
	static constexpr Square RIGHT_CAPT = (TURN == WHITE ? D_RU : D_RD);
	static constexpr Square FORWARD = (TURN == WHITE ? D_UP : D_DOWN);
	assert(direction == LEFT_CAPT || direction == RIGHT_CAPT || direction == FORWARD); // Maybe make direction a template parameter?
	while (destBB)
	{
		const Square to = popLSB(destBB), from = to - direction;
		assert(board[from] == TURN_PAWN);
		assert(getPieceColor(board[to]) == (direction == FORWARD ? NO_COLOR : opposite(TURN)));
		if (TURN == WHITE ? to > SQ_H7 : to < SQ_A2)
			for (int promIdx = 0; promIdx < 2; ++promIdx)
				addMoveIfSuitable<TURN, LEGAL>(makeMove(from, to, MT_PROMOTION, promPieceType[promIdx]), moves);
		else
			addMoveIfSuitable<TURN, LEGAL>(makeMove(from, to), moves);
	}
}

//============================================================
// Reveal NON-PAWN moves from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Color TURN, bool LEGAL>
void Position::revealMoves(Square from, Bitboard attackBB, MoveList& moves)
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	while (attackBB)
	{
		const Square to = popLSB(attackBB);
		assert(getPieceColor(board[from]) == TURN);
		assert(getPieceColor(board[to]) != TURN);
		addMoveIfSuitable<TURN, LEGAL>(makeMove(from, to), moves);
	}
}

//============================================================
// Generate all pawn moves for given TURN(should match position's turn value)
//============================================================
template<Color TURN, MoveGen MG_TYPE, bool LEGAL>
void Position::generatePawnMoves(MoveList& moves)
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr Piece TURN_PAWN =			(TURN == WHITE ? W_PAWN : B_PAWN);
	static constexpr Square LEFT_CAPT =			(TURN == WHITE ? D_LU : D_LD);
	static constexpr Square RIGHT_CAPT =		(TURN == WHITE ? D_RU : D_RD);
	static constexpr Square FORWARD =			(TURN == WHITE ? D_UP : D_DOWN);
	static constexpr Bitboard BB_REL_RANK_3 =	(TURN == WHITE ? BB_RANK_3 : BB_RANK_6);
	if (MG_TYPE & MG_CAPTURES)
	{
		// Left and right pawn capture moves(including promotions)
		revealPawnMoves<TURN, LEGAL>(shiftD<LEFT_CAPT>(pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)], LEFT_CAPT, moves);
		revealPawnMoves<TURN, LEGAL>(shiftD<RIGHT_CAPT>(pieceBB(TURN, PAWN)) & colorBB[opposite(TURN)], RIGHT_CAPT, moves);
		// En passant
		if ((MG_TYPE & MG_CAPTURES) && info.epSquare != SQ_NONE)
		{
			assert(board[info.epSquare - FORWARD] == makePiece(opposite(TURN), PAWN));
			Square from;
			if (getFile(info.epSquare) != 7 && board[from = info.epSquare - LEFT_CAPT] == TURN_PAWN)
				addMoveIfSuitable<TURN, LEGAL>(makeMove(from, info.epSquare, MT_EN_PASSANT), moves);
			if (getFile(info.epSquare) != 0 && board[from = info.epSquare - RIGHT_CAPT] == TURN_PAWN)
				addMoveIfSuitable<TURN, LEGAL>(makeMove(from, info.epSquare, MT_EN_PASSANT), moves);
		}
	}
	if (MG_TYPE & MG_NON_CAPTURES)
	{
		// One-step pawn forward moves(including promotions)
		Bitboard destBB = shiftD<FORWARD>(pieceBB(TURN, PAWN)) & emptyBB();
		revealPawnMoves<TURN, LEGAL>(destBB, FORWARD, moves);
		// Two-step pawn forward moves(here we can't promote)
		destBB = shiftD<FORWARD>(destBB & BB_REL_RANK_3) & emptyBB();
		while (destBB)
		{
			const Square to = popLSB(destBB);
			assert(getPieceColor(board[to]) == NO_COLOR);
			addMoveIfSuitable<TURN, LEGAL>(makeMove(to - (FORWARD + FORWARD), to), moves);
		}
	}
}

//============================================================
// Generate all check evasions (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Color TURN, bool LEGAL>
void Position::generateEvasions(MoveList& moves)
{
	generateMoves<TURN, MG_ALL, LEGAL>(moves); // !! TEMPORARILY !!
}

//============================================================
// Generate all moves (legal if LEGAL == true and pseudolegal otherwise)
//============================================================
template<Color TURN, MoveGen MG_TYPE, bool LEGAL>
void Position::generateMoves(MoveList& moves)
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	static constexpr Color OPPONENT = opposite(TURN);
	static constexpr Square REL_SQ_E1 = relSquare(SQ_E1, TURN),
		REL_SQ_C1 = relSquare(SQ_C1, TURN), REL_SQ_D1 = relSquare(SQ_D1, TURN),
		REL_SQ_G1 = relSquare(SQ_G1, TURN), REL_SQ_F1 = relSquare(SQ_F1, TURN);
	// TURN is a template parameter and is used only for optimization purposes, so it should be equal to turn
	assert(TURN == turn);
	// Pawn moves
	generatePawnMoves<TURN, MG_TYPE, LEGAL>(moves);
	// Castlings. Their legality (king's path should not be under attack) is checked right here
	if (MG_TYPE & MG_NON_CAPTURES)
	{
		if ((info.castlingRight & makeCastling(TURN, OO)) && (occupiedBB() & bbCastlingInner[TURN][OO]) == 0 &&
			!(isAttacked(REL_SQ_G1, OPPONENT) || isAttacked(REL_SQ_F1, OPPONENT) ||
				isAttacked(REL_SQ_E1, OPPONENT)))
			moves.add(makeMove(REL_SQ_E1, REL_SQ_G1, MT_CASTLING));
		if ((info.castlingRight & makeCastling(TURN, OOO)) && (occupiedBB() & bbCastlingInner[TURN][OOO]) == 0 &&
			!(isAttacked(REL_SQ_C1, OPPONENT) || isAttacked(REL_SQ_D1, OPPONENT) ||
				isAttacked(REL_SQ_E1, OPPONENT)))
			moves.add(makeMove(REL_SQ_E1, REL_SQ_C1, MT_CASTLING));
	}
	// Target for usual piece moves
	const Bitboard target = (MG_TYPE == MG_CAPTURES ? colorBB[opposite(TURN)] :
		MG_TYPE == MG_NON_CAPTURES ? emptyBB() : ~colorBB[TURN]);
	Square from;
	// Knight moves
	for (int i = 0; i < pieceCount[TURN][KNIGHT]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][KNIGHT][i], bbKnightAttack[from] & target, moves);
	// King moves
	for (int i = 0; i < pieceCount[TURN][KING]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][KING][i], bbKingAttack[from] & target, moves);
	// Rook and partially queen moves
	for (int i = 0; i < pieceCount[TURN][ROOK]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][ROOK][i], magicRookAttacks(from, occupiedBB()) & target, moves);
	for (int i = 0; i < pieceCount[TURN][QUEEN]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][QUEEN][i], magicRookAttacks(from, occupiedBB()) & target, moves);
	// Bishop and partially queen moves
	for (int i = 0; i < pieceCount[TURN][BISHOP]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][BISHOP][i], magicBishopAttacks(from, occupiedBB()) & target, moves);
	for (int i = 0; i < pieceCount[TURN][QUEEN]; ++i)
		revealMoves<TURN, LEGAL>(from = pieceSq[turn][QUEEN][i], magicBishopAttacks(from, occupiedBB()) & target, moves);
}

//============================================================
// Load position from a given stream in FEN notation
//============================================================
void Position::loadPosition(std::istream& istr)
{
	// Reset current game state
	reset();
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
			board[makeSquare(rank, file)] = makePiece(
				isupper(piece) ? WHITE : BLACK, pieceType);
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
		turn = BLACK;
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
			info.castlingRight |= makeCastling(isupper(castlingRight) ? WHITE : BLACK,
				castlingSide == 'k' ? OO : OOO);
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
		info.epSquare = makeSquare(fileFromAN(epFile), epRank);
	}
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

//============================================================
// Write position to a given stream in FEN notation
//============================================================
void Position::writePosition(std::ostream& ostr)
{
	// Piece placement information
	for (int rank = 7, consecutiveEmpty = 0; rank >= 0; --rank)
	{
		for (int file = 0; file < 8; ++file)
		{
			const Piece curPiece = board[makeSquare(rank, file)];
			if (curPiece == NO_PIECE)
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
			if (getPieceColor(curPiece) == BLACK)
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
		if (info.castlingRight & WHITE_OO)
			ostr << 'K';
		if (info.castlingRight & WHITE_OOO)
			ostr << 'Q';
		if (info.castlingRight & BLACK_OO)
			ostr << 'k';
		if (info.castlingRight & BLACK_OOO)
			ostr << 'q';
		ostr << ' ';
	}
	// En passant information
	if (info.epSquare == SQ_NONE)
		ostr << "- ";
	else
		ostr << getFile(info.epSquare) + 'a' << getRank(info.epSquare) << ' ';
	// Halfmove counter (for 50 move draw rule) information
	ostr << info.rule50 << ' ';
	// Counter of full moves (starting at 1) information
	ostr << gamePly / 2;
}

//============================================================
// Explicit template instantiations
//============================================================
template void Position::revealPawnMoves<WHITE, true>(Bitboard, Square, MoveList&);
template void Position::revealPawnMoves<WHITE, false>(Bitboard, Square, MoveList&);
template void Position::revealPawnMoves<BLACK, true>(Bitboard, Square, MoveList&);
template void Position::revealPawnMoves<BLACK, false>(Bitboard, Square, MoveList&);
template void Position::revealMoves<WHITE, true>(Square, Bitboard, MoveList&);
template void Position::revealMoves<WHITE, false>(Square, Bitboard, MoveList&);
template void Position::revealMoves<BLACK, true>(Square, Bitboard, MoveList&);
template void Position::revealMoves<BLACK, false>(Square, Bitboard, MoveList&);
template void Position::generateEvasions<WHITE, true>(MoveList&);
template void Position::generateEvasions<WHITE, false>(MoveList&);
template void Position::generateEvasions<BLACK, true>(MoveList&);
template void Position::generateEvasions<BLACK, false>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_NON_CAPTURES, true>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_NON_CAPTURES, false>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_CAPTURES, true>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_CAPTURES, false>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_ALL, true>(MoveList&);
template void Position::generatePawnMoves<WHITE, MG_ALL, false>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_NON_CAPTURES, true>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_NON_CAPTURES, false>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_CAPTURES, true>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_CAPTURES, false>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_ALL, true>(MoveList&);
template void Position::generatePawnMoves<BLACK, MG_ALL, false>(MoveList&);
template void Position::generateMoves<WHITE, MG_NON_CAPTURES, true>(MoveList&);
template void Position::generateMoves<WHITE, MG_NON_CAPTURES, false>(MoveList&);
template void Position::generateMoves<WHITE, MG_CAPTURES, true>(MoveList&);
template void Position::generateMoves<WHITE, MG_CAPTURES, false>(MoveList&);
template void Position::generateMoves<WHITE, MG_ALL, true>(MoveList&);
template void Position::generateMoves<WHITE, MG_ALL, false>(MoveList&);
template void Position::generateMoves<BLACK, MG_NON_CAPTURES, true>(MoveList&);
template void Position::generateMoves<BLACK, MG_NON_CAPTURES, false>(MoveList&);
template void Position::generateMoves<BLACK, MG_CAPTURES, true>(MoveList&);
template void Position::generateMoves<BLACK, MG_CAPTURES, false>(MoveList&);
template void Position::generateMoves<BLACK, MG_ALL, true>(MoveList&);
template void Position::generateMoves<BLACK, MG_ALL, false>(MoveList&);
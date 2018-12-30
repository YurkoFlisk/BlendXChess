//============================================================
// position.h
// ChessEngine
//============================================================

#pragma once
#ifndef _POSITION_H
#define _POSITION_H
#include <utility>
#include <cassert>
#include "bitboard.h"
#include "evaluate.h"
#include "movelist.h"

#ifdef ENGINE_DEBUG
constexpr bool SORT_GENMOVES_ON_DEBUG = false; // rarely needed and only in debug
#endif

//============================================================
// Struct for storing some information about position
// Includes data that is changed during move doing-undoing and thus needs to be preserved
//============================================================

struct PositionInfo
{
	PieceType justCaptured; // If last move was a capture, we store it's type here
	uint8_t rule50; // Counter for 50-move draw rule
	Square epSquare; // Square to which en passant is possible (if the last move was double pushed pawn)
	int8_t castlingRight; // Mask representing valid castlings
	Key keyZobrist; // Zobrist key of the position
};

constexpr inline bool operator!=(PositionInfo p1, PositionInfo p2)
{
	return p1.justCaptured != p2.justCaptured || p1.epSquare != p2.epSquare
		|| p1.rule50 != p2.rule50 || p1.castlingRight != p2.castlingRight;
}

//============================================================
// Class representing current position
// Includes board, piece lists, various positional information
//============================================================

class Position
{
public:
	// Constructor
	Position(void) = default;
	// Destructor
	~Position(void) = default;
	// Getters
	inline int getGamePly(void) const noexcept;
	inline int getTurn(void) const noexcept;
	inline Key getZobristKey(void) const noexcept;
	// Bitboard helpers
	inline Bitboard pieceBB(Side, PieceType) const;
	inline Bitboard occupiedBB(void) const;
	inline Bitboard emptyBB(void) const;
	// Clear position
	void clear(void);
	// Reset position
	void reset(void);
	// Whether a square is attacked by given side
	bool isAttacked(Square, Side) const;
	// Least valuable attacker on given square by given side (king is considered most valuable here)
	Square leastAttacker(Square, Side) const;
	// All attackers on given square by given side
	Bitboard allAttackers(Square, Side) const;
	// Whether current side is in check
	inline bool isInCheck(void) const;
	// Load position from a given stream in FEN notation (bool parameter says whether to omit move counters)
	void loadPosition(std::istream&, bool = false);
	// Write position to a given stream in FEN notation, possibly omitting half- and full-move counters
	void writePosition(std::ostream&, bool = false) const;
protected:
	// Putting, moving and removing pieces
	inline void putPiece(Square, Side, PieceType);
	inline void movePiece(Square, Square);
	inline void removePiece(Square);
	// Internal helper of doMove for safely updating castling rights (ie taking care of Zobrists)
	// Assumes only singular castling right arguments
	inline void removeCastlingRight(CastlingRight);
	// Internal doing and undoing moves
	void doMove(Move);
	void undoMove(Move);
	// Internal test for pseudo-legality (still assumes some conditions which TT-move must satisfy)
	bool isPseudoLegal(Move);
	// Internal test for legality (assumes pseudo-legality of argument)
	inline bool isLegal(Move);
	// Helper for move generating functions
	// It adds pseudo-legal move to vector if LEGAL == false or, otherwise, if move is legal
	template<Side TURN, bool LEGAL>
	inline void addMoveIfSuitable(Move, MoveList&);
	// Reveal PAWN moves in given direction from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
	template<Side TURN, bool LEGAL>
	void revealPawnMoves(Bitboard, Square, MoveList&);
	// Reveal NON-PAWN moves from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
	template<Side TURN, bool LEGAL>
	void revealMoves(Square, Bitboard, MoveList&);
	// Generate pawn moves. If MG_TYPE == MG_EVASIONS, only to distBB squares
	template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
	void generatePawnMoves(MoveList&, Bitboard = Bitboard());
	// Generate non-pawn and non-king moves. Only to destBB squares irrespectively of MG_TYPE
	template<Side TURN, bool LEGAL>
	void generateFigureMoves(MoveList&, Bitboard);
	// Generate moves (legal if LEGAL == true and pseudolegal otherwise)
	template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
	void generateMoves(MoveList&);
	// Generate moves helpers
	// Note that when we are in check, all evasions are generated regardless of what MG_TYPE parameter is passed
	// Useless promotions to rook and bishop are omitted even in case of MG_TYPE == MG_ALL
	template<MoveGen MG_TYPE = MG_ALL>
	inline void generateLegalMoves(MoveList&);
	template<MoveGen MG_TYPE = MG_ALL>
	inline void generatePseudolegalMoves(MoveList&);
	// Same as generateLegalMoves, but promotions to rook and bishop are included
	// Thought to be useful only when checking moves for legality
	template<MoveGen MG_TYPE = MG_ALL>
	inline void generateLegalMovesEx(MoveList&);
	// Board
	Piece board[SQUARE_CNT];
	// Piece list and supporting information
	Square pieceSq[COLOR_CNT][PIECETYPE_CNT][MAX_PIECES_OF_ONE_TYPE];
	int pieceCount[COLOR_CNT][PIECETYPE_CNT];
	int index[SQUARE_CNT]; // Index of a square in pieceSq[c][pt] array, where c and pt are color and type of piece at this square
	// Bitboards
	Bitboard colorBB[COLOR_CNT];
	Bitboard pieceTypeBB[PIECETYPE_CNT];
	// Information about position (various parameters that should be reversible during move doing-undoing)
	PositionInfo info;
	// Stack of previous states
	PositionInfo prevStates[MAX_GAME_PLY];
	// Piece-square score. It is updated incrementally
	Score psqScore;
	// Other
	int gamePly;
	Side turn;
};

//============================================================
// Implementation of inline functions
//============================================================

inline int Position::getGamePly(void) const noexcept
{
	return gamePly;
}

inline int Position::getTurn(void) const noexcept
{
	return turn;
}

inline Key Position::getZobristKey(void) const noexcept
{
	return info.keyZobrist;
}

inline Bitboard Position::pieceBB(Side c, PieceType pt) const
{
	return colorBB[c] & pieceTypeBB[pt];
}

inline Bitboard Position::occupiedBB(void) const
{
	return pieceTypeBB[PT_ALL];
}

inline Bitboard Position::emptyBB(void) const
{
	return ~occupiedBB();
}

inline bool Position::isInCheck(void) const
{
	return isAttacked(pieceSq[turn][KING][0], opposite(turn));
}

inline void Position::putPiece(Square sq, Side c, PieceType pt)
{
	assert(board[sq] == PIECE_NULL);
	colorBB[c] |= bbSquare[sq];
	pieceTypeBB[pt] |= bbSquare[sq];
	pieceTypeBB[PT_ALL] |= bbSquare[sq];
	pieceSq[c][pt][index[sq] = pieceCount[c][pt]++] = sq;
	++pieceCount[c][PT_ALL];
	board[sq] = makePiece(c, pt);
	psqScore += psqTable[c][pt][sq];
	info.keyZobrist ^= ZobristPSQ[c][pt][sq];
}

inline void Position::movePiece(Square from, Square to)
{
	const Side c = getPieceSide(board[from]);
	const PieceType pt = getPieceType(board[from]);
	const Bitboard fromToBB = bbSquare[from] ^ bbSquare[to];
	assert(board[from] != PIECE_NULL);
	assert(board[to] == PIECE_NULL);
	colorBB[c] ^= fromToBB;
	pieceTypeBB[pt] ^= fromToBB;
	pieceTypeBB[PT_ALL] ^= fromToBB;
	pieceSq[c][pt][index[to] = index[from]] = to;
	board[to] = board[from];
	board[from] = PIECE_NULL;
	psqScore += psqTable[c][pt][to] - psqTable[c][pt][from];
	info.keyZobrist ^= ZobristPSQ[c][pt][from] ^ ZobristPSQ[c][pt][to];
}

inline void Position::removePiece(Square sq)
{
	const Side c = getPieceSide(board[sq]);
	const PieceType pt = getPieceType(board[sq]);
	assert(pt != PT_NULL);
	colorBB[c] ^= bbSquare[sq];
	pieceTypeBB[pt] ^= bbSquare[sq];
	pieceTypeBB[PT_ALL] ^= bbSquare[sq];
	std::swap(pieceSq[c][pt][--pieceCount[c][pt]], pieceSq[c][pt][index[sq]]);
	--pieceCount[c][PT_ALL];
	index[pieceSq[c][pt][index[sq]]] = index[sq];
	board[sq] = PIECE_NULL;
	psqScore -= psqTable[c][pt][sq];
	info.keyZobrist ^= ZobristPSQ[c][pt][sq];
}

inline void Position::removeCastlingRight(CastlingRight cr)
{
	assert(isSingularCR(cr));
	if (info.castlingRight & cr)
		info.castlingRight &= ~cr, info.keyZobrist ^= ZobristCR[cr];
}

inline bool Position::isLegal(Move move)
{
	// MAYBE TEMPORARY (relatively slow and easy approach is used)
	doMove(move);
	const bool legal = !isAttacked(pieceSq[opposite(turn)][KING][0], turn);
	undoMove(move);
	return legal;
}

template<MoveGen MG_TYPE>
inline void Position::generateLegalMoves(MoveList& moves)
{
	if (turn == WHITE)
		if (isAttacked(pieceSq[WHITE][KING][0], BLACK))
			generateMoves<WHITE, MG_EVASIONS, true>(moves);
		else
			generateMoves<WHITE, MG_TYPE, true>(moves);
	else
		if (isAttacked(pieceSq[BLACK][KING][0], WHITE))
			generateMoves<BLACK, MG_EVASIONS, true>(moves);
		else
			generateMoves<BLACK, MG_TYPE, true>(moves);
}

template<MoveGen MG_TYPE>
inline void Position::generatePseudolegalMoves(MoveList& moves)
{
	if (turn == WHITE)
		if (isAttacked(pieceSq[WHITE][KING][0], BLACK))
			generateMoves<WHITE, MG_EVASIONS, false>(moves);
		else
			generateMoves<WHITE, MG_TYPE, false>(moves);
	else
		if (isAttacked(pieceSq[BLACK][KING][0], WHITE))
			generateMoves<BLACK, MG_EVASIONS, false>(moves);
		else
			generateMoves<BLACK, MG_TYPE, false>(moves);
}

template<MoveGen MG_TYPE>
inline void Position::generateLegalMovesEx(MoveList& moves)
{
	generateLegalMoves<MG_TYPE>(moves);
	Move move;
	for (int moveIdx = 0; moveIdx < moves.count(); ++moveIdx)
	{
		move = moves[moveIdx].move;
		if (move.type() == MT_PROMOTION
			&& move.promotion() == QUEEN) // This condition is true only once for each promotion square
		{ // Score is irrelevant, thus omitted
			moves.add(Move(move.from(), move.to(), MT_PROMOTION, BISHOP));
			moves.add(Move(move.from(), move.to(), MT_PROMOTION, ROOK));
		}
	}
	moves.reset();
}

template<Side TURN, bool LEGAL>
inline void Position::addMoveIfSuitable(Move move, MoveList& moves)
{
	static_assert(TURN == WHITE || TURN == BLACK,
		"TURN template parameter should be either WHITE or BLACK in this function");
	// If we are looking for legal moves, do a legality check
	if constexpr (LEGAL)
	{
		//if (isLegal(move))
		//	moves.add(move);
		// A pseudolegal move is legal iff the king is not under attack when it is performed
		doMove(move);
		// After doMove, turn has changed until undoMove, so it is critical that in the next two lines we use TURN
		// Score is set in move scoring function of Engine class, thus omitted
		if (!isAttacked(pieceSq[TURN][KING][0], opposite(TURN)))
			moves.add(move);
		// Restore to a previous state
		undoMove(move);
	}
	// If we are looking for pseudolegal moves, don't check anything, as we already know that move is pseudolegal
	else
		moves.add(move);
}

#endif
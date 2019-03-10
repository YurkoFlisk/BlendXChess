//============================================================
// position.h
// ChessEngine
//============================================================

#pragma once
#ifndef _POSITION_H
#define _POSITION_H
#include <utility>
#include <cassert>
#include <sstream>
#include "bitboard.h"
#include "evaluate.h"
#include "movelist.h"

namespace BlendXChess
{

#ifdef ENGINE_DEBUG
	constexpr bool SORT_GENMOVES_ON_DEBUG = false; // rarely needed and only in debug
#endif

	enum MoveFormat
	{
		FMT_AN, FMT_SAN, FMT_UCI
	};

	constexpr int MOVE_FORMAT_CNT = 3;

	//============================================================
	// Struct for storing some information about position
	// Includes data that is changed during move doing-undoing and thus needs to be preserved
	//============================================================

	struct PositionInfo
	{
		PieceType justCaptured; // If last move was a capture, we store it's type here
		uint8_t rule50; // Counter for 50-move draw rule
		Square epSquare; // Square to which endTime passant is possible (if the last move was double pushed pawn)
		CastlingRight castlingRight; // Mask representing valid castlings
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

	template<bool> class MoveManager;

	class Position
	{
		friend class Game;
		friend class Searcher;
		friend class MoveManager<true>;
		friend class MoveManager<false>;
	public:
		// Default constructor
		Position(void);
		// Copy constructor
		// Position(const Position&);
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
		// Whether the position is valid (useful for debug)
		bool isValid(void) const;
		// Clear position
		void clear(void);
		// Reset position
		void reset(void);
		// Performs a perft for current position, returns count of visited nodes
		// If MG_LEGAL == true, includes all moves (with promotions to rooks and bishops)
		template<bool MG_LEGAL = false>
		int perft(Depth);
		// Whether a square is attacked by given side
		inline bool isAttacked(Square, Side) const;
		// Least valuable attacker on given square by given side (king is considered most valuable here)
		inline Square leastAttacker(Square, Side) const;
		// All attackers on given square by given side
		inline Bitboard allAttackers(Square, Side) const;
		// Whether current side is in check
		inline bool isInCheck(void) const;
		// Convert a move from AN notation to Move. It should be valid in current position
		Move moveFromAN(const std::string&);
		// Convert a move from SAN notation to Move. It should be valid in current position
		Move moveFromSAN(const std::string&);
		// Convert a move to SAN notation. It should be valid in current position
		std::string moveToSAN(Move) const;
		// Convert move from given string format
		Move moveFromStr(const std::string&, MoveFormat);
		// Convert move to given string format
		std::string moveToStr(Move, MoveFormat) const;
		// Convert a move from UCI notation to Move. It should be valid in current position 
		Move moveFromUCI(const std::string&);
		// Doing and undoing a move
		// These are not well optimized and are being interface for external calls
		// Engine internals (eg in search) use doMove and undoMove instead
		bool DoMove(Move, PositionInfo* = nullptr);
		bool DoMove(const std::string&, MoveFormat, Move* = nullptr, PositionInfo* = nullptr);
		bool UndoMove(Move, const PositionInfo&);
		// Load position from a given stream in FEN notation (bool parameter says whether to omit move counters)
		void loadFEN(std::istream&, bool = false);
		// Load position from a given string in FEN notation (bool parameter says whether to omit move counters)
		void loadFEN(const std::string&, bool = false);
		// Write position to a given stream in FEN notation, possibly omitting half- and full-move counters
		void writeFEN(std::ostream&, bool = false) const;
		// Get positon FEN
		inline std::string getFEN(bool = false) const;
	protected:
		// Putting, moving and removing pieces
		inline void putPiece(Square, Side, PieceType);
		inline void movePiece(Square, Square);
		inline void removePiece(Square);
		// Internal helper of doMove for safely updating castling rights (ie taking care of Zobrists)
		// Assumes only singular castling right arguments
		inline void removeCastlingRight(CastlingRight);
		// Internal doing and undoing moves
		void doMove(Move, PositionInfo&);
		void undoMove(Move, const PositionInfo&);
		// Whether the move is a capture
		inline bool isCaptureMove(Move) const;
		// Internal test for pseudo-legality (still assumes some conditions which TT-move must satisfy)
		bool isPseudoLegal(Move) const;
		// Internal test for legality (assumes pseudo-legality of argument)
		inline bool isLegal(Move) const;
		// Helper for move generating functions
		// It adds pseudo-legal move to vector if LEGAL == false or, otherwise, if move is legal
		template<Side TURN, bool LEGAL>
		inline void addMoveIfSuitable(Move, MoveList&) const;
		// Reveal PAWN moves in given direction from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
		template<Side TURN, bool LEGAL>
		void revealPawnMoves(Bitboard, Square, MoveList&) const;
		// Reveal NON-PAWN moves from attack bitboard (legal if LEGAL == true and pseudolegal otherwise)
		template<Side TURN, bool LEGAL>
		void revealMoves(Square, Bitboard, MoveList&) const;
		// Generate pawn moves. If MG_TYPE == MG_EVASIONS, only to distBB squares
		template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
		void generatePawnMoves(MoveList&, Bitboard = Bitboard()) const;
		// Generate non-pawn and non-king moves. Only to destBB squares irrespectively of MG_TYPE
		template<Side TURN, bool LEGAL>
		void generateFigureMoves(MoveList&, Bitboard) const;
		// Generate moves (legal if LEGAL == true and pseudolegal otherwise)
		template<Side TURN, MoveGen MG_TYPE, bool LEGAL>
		void generateMoves(MoveList&) const;
		// Generate moves helpers
		// Note that when we are in check, all evasions are generated regardless of what MG_TYPE parameter is passed
		// (Almost) useless promotions to rook and bishop are omitted even in case of MG_TYPE == MG_ALL
		template<MoveGen MG_TYPE = MG_ALL>
		inline void generateLegalMoves(MoveList&) const;
		template<MoveGen MG_TYPE = MG_ALL>
		inline void generatePseudolegalMoves(MoveList&) const;
		// Same as generateLegalMoves, but promotions to rook and bishop are included
		// Thought to be useful only when checking moves for legality
		template<MoveGen MG_TYPE = MG_ALL>
		inline void generateLegalMovesEx(MoveList&) const;
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
		// PositionInfo prevStates[MAX_GAME_PLY];
		// Piece-square score. It is updated incrementally
		Score psqScore;
		// Other
		int gamePly; // !! NOT search ply !!
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

	inline bool Position::isCaptureMove(Move move) const
	{
		return board[move.to()] != PIECE_NULL;
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

	inline std::string Position::getFEN(bool omitCounters) const
	{
		std::stringstream fenSS;
		writeFEN(fenSS, omitCounters);
		return fenSS.str();
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

	//============================================================
	// Whether a square is attacked by given side
	//============================================================
	inline bool Position::isAttacked(Square sq, Side by) const
	{
		assert(by == WHITE || by == BLACK);
		assert(sq.isValid());
		return (bbPawnAttack[opposite(by)][sq] & pieceBB(by, PAWN)) ||
			(bbKnightAttack[sq] & pieceBB(by, KNIGHT)) ||
			(bbKingAttack[sq] & pieceBB(by, KING)) ||
			(magicRookAttacks(sq, occupiedBB()) & (pieceBB(by, ROOK) | pieceBB(by, QUEEN))) ||
			(magicBishopAttacks(sq, occupiedBB()) & (pieceBB(by, BISHOP) | pieceBB(by, QUEEN)));
	}

	//============================================================
	// Least valuable attacker on given square by given
	// side (king is considered most valuable here)
	//============================================================
	inline Square Position::leastAttacker(Square sq, Side by) const
	{
		assert(by == WHITE || by == BLACK);
		assert(sq.isValid());
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
		return Sq::NONE;
	}

	//============================================================
	// All attackers on given square by given side
	//============================================================
	inline Bitboard Position::allAttackers(Square sq, Side by) const
	{
		Bitboard mBA, mRA;
		return (bbPawnAttack[opposite(by)][sq] & pieceBB(by, PAWN))
			| (bbKnightAttack[sq] & pieceBB(by, KNIGHT))
			| (bbKingAttack[sq] & pieceBB(by, KING))
			| ((mBA = magicBishopAttacks(sq, occupiedBB())) & pieceBB(by, BISHOP))
			| ((mRA = magicRookAttacks(sq, occupiedBB())) & pieceBB(by, ROOK))
			| ((mBA | mRA) & pieceBB(by, QUEEN));
	}

	inline void Position::removeCastlingRight(CastlingRight cr)
	{
		assert(isSingularCR(cr));
		if (info.castlingRight & cr)
			info.castlingRight &= ~cr, info.keyZobrist ^= ZobristCR[cr];
	}

	inline bool Position::isLegal(Move move) const
	{
		// MAYBE TEMPORARY (relatively slow and easy approach is used)
		PositionInfo prevState;
		// Here const_cast-approach is acceptable since the principal structure of position
		// is not changed, even though e.g. order of elements in piece lists may be changed
		auto* const thisObj = const_cast<Position*>(this);
		thisObj->doMove(move, prevState);
		const bool legal = !isAttacked(pieceSq[opposite(turn)][KING][0], turn);
		thisObj->undoMove(move, prevState);
		return legal;
	}

	template<MoveGen MG_TYPE>
	inline void Position::generateLegalMoves(MoveList& moves) const
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
	inline void Position::generatePseudolegalMoves(MoveList& moves) const
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
	inline void Position::generateLegalMovesEx(MoveList& moves) const
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
	inline void Position::addMoveIfSuitable(Move move, MoveList& moves) const
	{
		static_assert(TURN == WHITE || TURN == BLACK,
			"TURN template parameter should be either WHITE or BLACK in this function");
		// If we are looking for legal moves, do a legality check
		if constexpr (LEGAL)
		{
			PositionInfo prevState;
			// Here const_cast-approach is acceptable since the principal structure of position
			// is not changed, even though e.g. order of elements in piece lists may be changed
			auto* const thisObj = const_cast<Position*>(this);
			// A pseudolegal move is legal iff the king is not under attack when it is performed
			thisObj->doMove(move, prevState);
			// After doMove, turn has changed until undoMove, so it is critical that in the next two lines we use TURN
			// Score is set in move scoring function of Engine class, thus omitted
			if (!isAttacked(pieceSq[TURN][KING][0], opposite(TURN)))
				moves.add(move);
			// Restore to a previous state
			thisObj->undoMove(move, prevState);
		}
		// If we are looking for pseudolegal moves, don't check anything, as we already know that move is pseudolegal
		else
			moves.add(move);
	}
};

#endif
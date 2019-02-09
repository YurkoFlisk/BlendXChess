//============================================================
// ChessEngine
// bitboard.h
//============================================================

#pragma once
#ifndef _BITBOARD_H
#define _BITBOARD_H
#include <string>
#include "basic_types.h"

namespace BlendXChess
{

	typedef uint64_t Bitboard;

	//============================================================
	// Struct representing magic's info
	//============================================================
	struct Magic
	{
		Bitboard* attack;
		Bitboard relOcc;
		Bitboard mul;
		int shifts;
	};

	//============================================================
	// Constants and global variables
	//============================================================

	constexpr Bitboard BB_RANK_1 = 0xff;
	constexpr Bitboard BB_RANK_2 = BB_RANK_1 << (1 * 8);
	constexpr Bitboard BB_RANK_3 = BB_RANK_1 << (2 * 8);
	constexpr Bitboard BB_RANK_4 = BB_RANK_1 << (3 * 8);
	constexpr Bitboard BB_RANK_5 = BB_RANK_1 << (4 * 8);
	constexpr Bitboard BB_RANK_6 = BB_RANK_1 << (5 * 8);
	constexpr Bitboard BB_RANK_7 = BB_RANK_1 << (6 * 8);
	constexpr Bitboard BB_RANK_8 = BB_RANK_1 << (7 * 8);
	constexpr Bitboard BB_FILE_A = 0x0101010101010101ULL;
	constexpr Bitboard BB_FILE_B = BB_FILE_A << 1;
	constexpr Bitboard BB_FILE_C = BB_FILE_A << 2;
	constexpr Bitboard BB_FILE_D = BB_FILE_A << 3;
	constexpr Bitboard BB_FILE_E = BB_FILE_A << 4;
	constexpr Bitboard BB_FILE_F = BB_FILE_A << 5;
	constexpr Bitboard BB_FILE_G = BB_FILE_A << 6;
	constexpr Bitboard BB_FILE_H = BB_FILE_A << 7;
	extern Bitboard bbRank[RANK_CNT];
	extern Bitboard bbFile[FILE_CNT];
	extern Bitboard bbSquare[SQUARE_CNT];
	extern Bitboard bbDiagonal[DIAG_CNT];
	extern Bitboard bbAntidiagonal[DIAG_CNT];
	extern Bitboard bbPawnQuiet[COLOR_CNT][SQUARE_CNT];
	extern Bitboard bbPawnAttack[COLOR_CNT][SQUARE_CNT];
	extern Bitboard bbAttackEB[PIECETYPE_CNT][SQUARE_CNT]; // attack bitboard for pieces (except pawns) on empty board
	extern Bitboard(&bbKnightAttack)[SQUARE_CNT];
	extern Bitboard(&bbKingAttack)[SQUARE_CNT];
	extern Bitboard bbCastlingInner[COLOR_CNT][CASTLING_SIDE_CNT];
	extern Bitboard bbBetween[SQUARE_CNT][SQUARE_CNT];
	extern Magic mRookMagics[SQUARE_CNT];
	extern Magic mBishopMagics[SQUARE_CNT];
	extern Key ZobristPSQ[COLOR_CNT][PIECETYPE_CNT][SQUARE_CNT];
	extern Key ZobristCR[CR_BLACK_OOO + 1]; // valid only for 'singular' castling rights
	extern Key ZobristEP[FILE_CNT];
	extern Key ZobristBlackSide;

	//============================================================
	// Functions
	//============================================================

	// Initialize Zobrist keys
	void initZobrist(void);
	// Converts given bitboard to string
	std::string bbToStr(Bitboard bb);
	// Count set bits in given bitboard
	int countSet(Bitboard);
	// Get least significant bit of a bitboard
	Square getLSB(Bitboard);
	// Get least significant bit of a bitboard and clear it
	Square popLSB(Bitboard&);
	// Computes bitboard of attacks from given square on 4 given directions with given relative occupancy
	Bitboard lineAttacks(Square, Bitboard, Square[4]);
	// Initialization of global bitboards
	void initBB(void);
	// Initialization of magics bitboards for rooks and bishops(piece is determined by parameters)
	void initMagics(const Square[4], const Bitboard[], const Bitboard[],
		int8_t(Square::*)() const, int8_t(Square::*)() const, Magic[SQUARE_CNT]);

	//============================================================
	// Inline functions
	//============================================================

	// Whether bitboard has at most or more than one bit set
	inline bool zeroOrSingular(Bitboard bb)
	{
		return !(bb & (bb - 1));
	}
	// Gets magic rook moves
	inline Bitboard magicRookAttacks(Square from, Bitboard occupancy)
	{
		return mRookMagics[from].attack[((mRookMagics[from].relOcc & occupancy)
			* mRookMagics[from].mul) >> mRookMagics[from].shifts];
	}

	// Gets magic bishop moves
	inline Bitboard magicBishopAttacks(Square from, Bitboard occupancy)
	{
		return mBishopMagics[from].attack[((mBishopMagics[from].relOcc & occupancy)
			* mBishopMagics[from].mul) >> mBishopMagics[from].shifts];
	}

	// Shifts a bitboard in a direction specified by one of the D_ constants
	template<SquareRaw DELTA>
	constexpr inline Bitboard bbShiftD(Bitboard bb) noexcept
	{
		using namespace Sq;
		static_assert(DELTA == D_LEFT || DELTA == D_RIGHT || DELTA == D_UP || DELTA == D_DOWN ||
			DELTA == D_LU || DELTA == D_RU || DELTA == D_LD || DELTA == D_RD,
			"DELTA template parameter should be one of D_ variables");
		return
			DELTA == D_LEFT ? bb >> 1 : DELTA == D_RIGHT ? bb << 1 :
			DELTA == D_UP ? bb << 8 : DELTA == D_DOWN ? bb >> 8 :
			DELTA == D_LU ? (bb << 7) & ~BB_FILE_H : DELTA == D_RU ? (bb << 9) & ~BB_FILE_A :
			DELTA == D_LD ? (bb >> 9) & ~BB_FILE_H : DELTA == D_RD ? (bb >> 7) & ~BB_FILE_A :
			0; // Should not occur
	}

};

#endif
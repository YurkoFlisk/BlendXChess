//============================================================
// ChessEngine
// bitboard.cpp
//============================================================

#include "bitboard.h"
#include <cassert>
#include <random>

//============================================================
// Global variables
//============================================================

Bitboard bbRank[RANK_CNT];
Bitboard bbFile[FILE_CNT];
Bitboard bbSquare[SQUARE_CNT];
Bitboard bbDiagonal[DIAG_CNT] = {};
Bitboard bbAntidiagonal[DIAG_CNT] = {};
Bitboard bbPawnAttack[COLOR_CNT][SQUARE_CNT] = {};
Bitboard bbKnightAttack[SQUARE_CNT] = {};
Bitboard bbKingAttack[SQUARE_CNT] = {};
Bitboard bbCastlingInner[COLOR_CNT][CASTLING_SIDE_CNT];
Magic mRookMagics[SQUARE_CNT];
Magic mBishopMagics[SQUARE_CNT];
Key ZobristPSQ[COLOR_CNT][PIECETYPE_CNT][SQUARE_CNT];
Key ZobristCR[CR_BLACK_OOO + 1];
Key ZobristEP[FILE_CNT];
Key ZobristSide;

//============================================================
// Local namespace
//============================================================
namespace
{
	// Here magic moves are stored('attack' member of Magic object links somewhere inside this array)
	Bitboard bbAttackTable[1 << 19];
	// Random number generator
	class PRNGen
	{
	public:
		PRNGen(Bitboard seed, Bitboard m, Bitboard a, Bitboard md)
			: cur(seed), mul(m), add(a), mod(md)
		{}
		inline Bitboard getNext(void) volatile // volatile because of VS compiler optimizer bug with -Og parameter
		{
			return ((cur *= mul) += add) % mod;
		}
	private:
		Bitboard cur, mul, add, mod;
	};
}

//============================================================
// Initialize Zobrist keys
//============================================================
void initZobrist(void)
{
	static constexpr Bitboard PRNG_MUL = 6364136223846930515, PRNG_ADD = 14426950408963407454,
		PRNG_MOD = 4586769527459239595;
	PRNGen lcg(1, PRNG_MUL, PRNG_ADD, PRNG_MOD);
	ZobristSide = lcg.getNext();
	ZobristCR[CR_WHITE_OO] = lcg.getNext();
	ZobristCR[CR_WHITE_OOO] = lcg.getNext();
	ZobristCR[CR_BLACK_OO] = lcg.getNext();
	ZobristCR[CR_BLACK_OOO] = lcg.getNext();
	for (int8_t f = fileFromAN('a'); f <= fileFromAN('h'); ++f)
		ZobristEP[f] = lcg.getNext();
	for (Color c = WHITE; c <= BLACK; ++c)
		for (PieceType pt = PAWN; pt <= KING; ++pt)
			for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
				ZobristPSQ[c][pt][sq] = lcg.getNext();

}

//============================================================
// Converts given bitboard to string
//============================================================
std::string bbToStr(Bitboard bb)
{
	std::string ret;
	for (int i = 0; i < 64; ++i, bb >>= 1)
	{
		if (i != 0 && !(i & 7))
			ret.push_back('\n');
		if (bb & 1)
			ret.push_back('1');
		else
			ret.push_back('0');
	}
	return ret;
}

//============================================================
// Count set bits in given bitboard
//============================================================
int countSet(Bitboard bb)
{
	int cnt(0);
	while (bb)
		bb &= bb - 1, ++cnt;
	return cnt;
}

//============================================================
// Get least significant bit of a bitboard
//============================================================
Square getLSB(Bitboard bb)
{
	Square lsb = SQ_A1;
	// Binary search
	if ((bb & 0xffffffff) == 0)
		lsb += Square(32), bb >>= 32;
	if ((bb & 0xffff) == 0)
		lsb += Square(16), bb >>= 16;
	if ((bb & 0xff) == 0)
		lsb += Square(8), bb >>= 8;
	if ((bb & 0xf) == 0)
		lsb += Square(4), bb >>= 4;
	if ((bb & 0x3) == 0)
		lsb += Square(2), bb >>= 2;
	if ((bb & 0x1) == 0)
		++lsb;
	return lsb;
}

//============================================================
// Get least significant bit of a bitboard and clear it
//============================================================
Square popLSB(Bitboard& bb)
{
	Bitboard b = bb;
	Square lsb = SQ_A1;
	// Binary search
	if ((b & 0xffffffff) == 0)
		lsb += Square(32), b >>= 32;
	if ((b & 0xffff) == 0)
		lsb += Square(16), b >>= 16;
	if ((b & 0xff) == 0)
		lsb += Square(8), b >>= 8;
	if ((b & 0xf) == 0)
		lsb += Square(4), b >>= 4;
	if ((b & 0x3) == 0)
		lsb += Square(2), b >>= 2;
	if ((b & 0x1) == 0)
		++lsb;
	bb &= bb - 1;
	return lsb;
}

//============================================================
// Computes bitboard of attacks from given square on 4 given
// directions (lines) with given relative occupancy
//============================================================
Bitboard lineAttacks(Square from, Bitboard relOcc, const Square dir[4])
{
	Bitboard attacks = 0;
	for (int8_t di = 0; di < 4; ++di)
		for (Square to = from + dir[di]; to.isValid() &&
			distance(to, to - dir[di]) <= 2; to += dir[di])
		{
			attacks |= bbSquare[to];
			if (relOcc & bbSquare[to])
				break;
		}
	return attacks;
}

//============================================================
// Initialization of global bitboards
//============================================================
void initBB(void)
{
	// Initialize bbRank, bbFile and bbSquare
	bbRank[0] = BB_RANK_1;
	for (int8_t i = 1; i < RANK_CNT; ++i)
		bbRank[i] = (bbRank[i - 1] << 8);
	bbFile[0] = BB_FILE_A;
	for (int8_t i = 1; i < FILE_CNT; ++i)
		bbFile[i] = (bbFile[i - 1] << 1);
	for (Square sq = SQ_A1; sq < SQUARE_CNT; ++sq)
		bbSquare[sq] = (1ull << sq);
	// Initialize bbDiagonal and bbAntidiagonal
	for (int8_t i = 0; i < DIAG_CNT; ++i)
	{
		for (int8_t r = (i > 7 ? i - 7 : 0), c = (i > 7 ? 0 : 7 - i); r < 8 && c < 8; ++r, ++c)
			bbDiagonal[i] |= bbSquare[Square(r, c)];
		for (int8_t r = (i > 7 ? 7 : i), c = (i > 7 ? i - 7 : 0); r >= 0 && c < 8; --r, ++c)
			bbAntidiagonal[i] |= bbSquare[Square(r, c)];
	}
	// Initialize bbPawnAttack
	for (Color c = WHITE; c <= BLACK; ++c)
		for (Square sq = SQ_A1, to; sq < SQ_H8; ++sq)
		{
			if (sq.getFile() != fileFromAN('a') && (to = sq + D_LU - (c << 4)).isValid())
				bbPawnAttack[c][sq] |= bbSquare[to];
			if (sq.getFile() != fileFromAN('h') && (to = sq + D_RU - (c << 4)).isValid())
				bbPawnAttack[c][sq] |= bbSquare[to];
		}
	// Initialize bbKnightAttack and bbKingAttack
	static constexpr Square
		knightStep[8] = { -17, -15, -10, -6, 6, 10, 15, 17 },
		kingStep[8] = { D_LD, D_DOWN, D_RD, D_LEFT, D_RIGHT, D_LU, D_UP, D_RU};
	for (Square sq = SQ_A1, to; sq < SQUARE_CNT; ++sq)
		for (int8_t d = 0; d < 8; ++d)
		{
			if ((to = sq + knightStep[d]).isValid() && distance(sq, to) == 3)
				bbKnightAttack[sq] |= bbSquare[to];
			if ((to = sq + kingStep[d]).isValid() && distance(sq, to) <= 2)
				bbKingAttack[sq] |= bbSquare[to];
		}
	// Initialize bitboards for castling's inners
	for (Color c = WHITE; c <= BLACK; ++c)
	{
		bbCastlingInner[c][OO] = bbSquare[relSquare(SQ_G1, c)] | bbSquare[relSquare(SQ_F1, c)];
		bbCastlingInner[c][OOO] = bbSquare[relSquare(SQ_B1, c)] | bbSquare[relSquare(SQ_C1, c)]
			| bbSquare[relSquare(SQ_D1, c)];
	}
	// Initialize magics
	static constexpr Square
		ROOK_DIR[4] = { D_UP, D_DOWN, D_LEFT, D_RIGHT },
		BISHOP_DIR[4] = { D_LD, D_RD, D_RU, D_LU };
	initMagics(ROOK_DIR, bbRank, bbFile, &Square::getRank, &Square::getFile,
		mRookMagics);
	initMagics(BISHOP_DIR, bbDiagonal, bbAntidiagonal, &Square::getDiagonal, &Square::getAntidiagonal,
		mBishopMagics);
}

//============================================================
// Initialization of magics bitboards for rooks and bishops (piece is determined by parameters)
//============================================================
void initMagics(const Square dir[4], const Bitboard bbLine1[], const Bitboard bbLine2[],
	int8_t (Square::*getLine1)(void) const, int8_t (Square::*getLine2)(void) const,
	Magic mPieceMagics[SQUARE_CNT])
{
	static constexpr Bitboard PRNG_MUL = 6364136223846930515, PRNG_ADD = 14426950408963407454,
		PRNG_MOD = 4586769527459239595;
	static Bitboard* attackTablePos = bbAttackTable;
	Bitboard curOcc, bbBorder, magicCandidate, attacks[1 << 12];
	int idx, siz, magicIdx;
	for (Square sq = SQ_A1; sq < SQUARE_CNT; ++sq)
	{
		// Border (we should carefully handle situations where rook is itself on border)
		// It remains the same for bishops because 'and' changes don't affect them
		bbBorder = ((BB_RANK_1 | BB_RANK_8) & ~bbRank[sq.getRank()]) | ((BB_FILE_A | BB_FILE_H) & ~bbFile[sq.getFile()]);
		// Relative occupancy mask
		mPieceMagics[sq].relOcc = (bbLine1[(sq.*getLine1)()] | bbLine2[(sq.*getLine2)()])
			& ~bbSquare[sq] & ~bbBorder;
		// Shifts (which is 64 minus number of bits in relOcc)
		mPieceMagics[sq].shifts = 64 - countSet(mPieceMagics[sq].relOcc);
		// Compute all possible attacks for this position and store them in 'attacks' array
		curOcc = mPieceMagics[sq].relOcc, idx = 0;
		do
		{
			attacks[idx] = lineAttacks(sq, curOcc, dir);
			curOcc = (curOcc - 1) & mPieceMagics[sq].relOcc, ++idx;
		} while (curOcc != mPieceMagics[sq].relOcc);
		assert(idx == (1 << (64 - mPieceMagics[sq].shifts)));
		// Set this position's attacks pointer to appropriate position in a common table
		mPieceMagics[sq].attack = attackTablePos, attackTablePos += idx;
		// Test pseudo-random bit-sparse numbers until we find an appropriate one for being magic multiplier
		siz = idx;
		PRNGen prng(curOcc, PRNG_MUL, PRNG_ADD + sq, PRNG_MOD);
		do
		{
			curOcc = mPieceMagics[sq].relOcc, magicCandidate = prng.getNext()
				& prng.getNext() & prng.getNext(), idx = 0;
			bool magicIdxUsed[1 << 12] = {};
			do
			{
				magicIdx = static_cast<int>(((curOcc & mPieceMagics[sq].relOcc)
					* magicCandidate) >> mPieceMagics[sq].shifts);
				// If this magic index was already set and it's attacks bitboard is not the same as
				// for curOcc relative occupancy, we have a collision, so current magic candidate is not valid
				if (magicIdxUsed[magicIdx] && mPieceMagics[sq].attack[magicIdx] != attacks[idx])
					break;
				magicIdxUsed[magicIdx] = true, mPieceMagics[sq].attack[magicIdx] = attacks[idx];
				curOcc = (curOcc - 1) & mPieceMagics[sq].relOcc, ++idx;
			} while (curOcc != mPieceMagics[sq].relOcc);
		} while (idx != siz);
		mPieceMagics[sq].mul = magicCandidate;
	}
}
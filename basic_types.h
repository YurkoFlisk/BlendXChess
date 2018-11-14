//============================================================
// basic_types.h
// ChessEngine
//============================================================

#pragma once
#ifndef _BASIC_TYPES
#define _BASIC_TYPES
#include <cstdint>
#include <cmath>
#include <string>

//============================================================
// Basic typedef types
//============================================================

typedef int8_t SquareRaw, Color, Depth, PieceType, Piece;
typedef int16_t Score;
typedef uint64_t Key;
typedef uint16_t MoveRaw;

//============================================================
// Basic constants
//============================================================

constexpr int8_t FILE_CNT = 8, FILE_MIN = 0, FILE_MAX = 7, RANK_CNT = 8, RANK_MIN = 0,
	RANK_MAX = 7, DIAG_CNT = 15, SQUARE_CNT = FILE_CNT * RANK_CNT,
	COLOR_CNT = 2, PIECETYPE_CNT = 7, MAX_PIECES_OF_ONE_TYPE = 9, CASTLING_SIDE_CNT = 2;
constexpr int16_t MAX_GAME_PLY = 1024, MAX_SEARCH_PLY = 50, MAX_KILLERS_CNT = 3;

//============================================================
// Namespace containing constants defining structure of Move presentation
// Move uses 16 bits: 6 for destination, 6 for start squares and 4 for other info
// In info there are 2 bits for move type and 2 bits for promotion piece type (encoded as decreased by 2)
// En passant and castling are coded as moves of corresponding pieces
//============================================================

namespace MoveDesc
{
	constexpr uint16_t FROM_FB = 0, TO_FB = 6, TYPE_FB = 12, PROMOTION_FB = 14,
		FROM_MASK = 63 << FROM_FB, TO_MASK = 63 << TO_FB,
		TYPE_MASK = 3 << TYPE_FB, PROMOTION_MASK = 3 << PROMOTION_FB;
}

//============================================================
// Basic enum types and unnamed enum constants
//============================================================

// Piece types
enum : PieceType {
	NO_PIECE_TYPE,
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
	PT_ALL = 0 // For using as index in bitboard arrays
};
// Pieces
enum : Piece {
	NO_PIECE,
	W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
	B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING
};
// Castling sides
enum CastlingSide : int8_t {
	OO, OOO
};
// Castling rights
enum CastlingRight : int8_t {
	CR_NULL,
	CR_WHITE_OO = 1,
	CR_WHITE_OOO = CR_WHITE_OO << 1,
	CR_BLACK_OO = CR_WHITE_OO << 2,
	CR_BLACK_OOO = CR_WHITE_OO << 3,
	CR_ALL_WHITE = CR_WHITE_OO | CR_WHITE_OOO,
	CR_ALL_BLACK = CR_BLACK_OO | CR_BLACK_OOO,
	CR_ALL = CR_ALL_WHITE | CR_ALL_BLACK
};
// Bounds in transposition table
enum Bound : int8_t {
	BOUND_LOWER = 1, BOUND_UPPER, BOUND_EXACT
};
// Move types
enum MoveType : int8_t {
	MT_NORMAL, MT_CASTLING, MT_PROMOTION, MT_EN_PASSANT
};
// Move generation types in move generator
// Note that when we are in check, all evasions are generated regardless of what MoveGen parameter is passed
enum MoveGen : int8_t {
	MG_NON_CAPTURES = 1, MG_CAPTURES, MG_ALL
};
// Game states
enum GameState : int8_t {
	GS_ACTIVE,
	GS_DRAW,
	GS_WHITE_WIN,
	GS_BLACK_WIN
};
// Colors
enum : int8_t {
	WHITE, BLACK, NO_COLOR,
};
// Depths
enum : Depth {
	DEPTH_ZERO = 0, DEPTH_MAX = 10
};
// Scores
enum : Score {
	SCORE_ZERO = 0, SCORE_LOSE = -30000, SCORE_WIN = 30000,
	SCORE_LOSE_MAX = SCORE_LOSE + MAX_GAME_PLY, SCORE_WIN_MIN = SCORE_WIN - MAX_GAME_PLY
};
// Squares
namespace
{
	enum : SquareRaw {
		SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
		SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
		SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
		SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
		SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
		SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
		SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
		SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
		SQ_NONE,
		D_LEFT = -1, D_RIGHT = 1, D_UP = FILE_CNT, D_DOWN = -FILE_CNT,
		D_LU = D_LEFT + D_UP, D_RU = D_RIGHT + D_UP, D_LD = D_LEFT + D_DOWN, D_RD = D_RIGHT + D_DOWN
	};
}
// Moves
enum : MoveRaw {
	// There are no such valid moves ('from' and 'to' are the same), so we can use these values as special ones
	MOVE_NONE = 0, // 'from' == 'to' == SQ_A1
	MOVE_NULL = 0xfff // 'from' == 'to' == SQ_H8
};

//============================================================
// Inline functions
//============================================================

constexpr inline Color opposite(Color c) noexcept
{
	return c == WHITE ? BLACK : WHITE;
}

//constexpr inline int8_t getFile(Square sq) noexcept
//{
//	return sq & 7;
//}
//
//constexpr inline int8_t getRank(Square sq) noexcept
//{
//	return sq >> 3;
//}

//constexpr inline int8_t getDiagonal(Square sq) noexcept
//{
//	return getRank(sq) - getFile(sq) + 7;
//}
//
//constexpr inline int8_t getAntidiagonal(Square sq) noexcept
//{
//	return getRank(sq) + getFile(sq);
//}
//// Manhattan distance between squares
//inline int8_t distance(Square sq1, Square sq2) noexcept
//{
//	return abs(getRank(sq1) - getRank(sq2)) + abs(getFile(sq1) - getFile(sq2));
//}
// Rank relative to a given side
constexpr inline int8_t relRank(int8_t r, Color c) noexcept
{
	return c == WHITE ? r : RANK_CNT - 1 - r;
}
//// Square relative to a given side
//constexpr inline int8_t relSquare(Square sq, Color c) noexcept
//{
//	return c == WHITE ? sq : sq + FILE_CNT*(RANK_CNT - 1 - (getRank(sq) << 1));
//}

constexpr inline bool validRank(int8_t rank) noexcept
{
	return 0 <= rank && rank < RANK_CNT;
}

constexpr inline bool validFile(int8_t column) noexcept
{
	return 0 <= column && column < FILE_CNT;
}

//constexpr inline bool validSquare(Square sq) noexcept
//{
//	return 0 <= sq && sq < SQUARE_CNT;
//}

constexpr inline bool validRankAN(char rankAN) noexcept
{
	return validRank(rankAN - '1');
}

constexpr inline bool validFileAN(char fileAN) noexcept
{
	return validFile(fileAN - 'a');
}

inline bool validSquareAN(std::string sqAN) noexcept
{
	return sqAN.size() == 2 && validRankAN(sqAN[1]) && validFileAN(sqAN[0]);
}

inline bool validCastlingSideAN(std::string csAN) noexcept
{
	return csAN == "O-O" || csAN == "O-O-O";
}

// Whether uppercase piece type identifier in algebraic notations is valid
constexpr bool validPieceTypeAN(char ptAN) noexcept
{
	switch (ptAN)
	{
	case 'N':
	case 'B':
	case 'R':
	case 'Q':
	case 'K':	return true;
	default:	return false;
	}
}

// Whether uppercase piece type identifier in FEN notation is valid
constexpr bool validPieceTypeFEN(char ptFEN) noexcept
{
	return ptFEN == 'P' || validPieceTypeAN(ptFEN);
}

//constexpr inline bool cornerSquare(Square sq) noexcept
//{
//	return sq == SQ_A1 || sq == SQ_A8 || sq == SQ_H1 || sq == SQ_H8;
//}
//
//constexpr inline bool borderSquare(Square sq) noexcept
//{
//	const int8_t r = getRank(sq), f = getFile(sq);
//	return r == 0 || r == 7 || f == 0 || f == 7;
//}

constexpr inline int8_t fileFromAN(char fileAN) noexcept
{
	return (int8_t)(fileAN - 'a');
}

constexpr inline char fileToAN(int8_t file) noexcept
{
	return (char)('a' + file);
}

constexpr inline int8_t rankFromAN(char rankAN) noexcept
{
	return (int8_t)(rankAN - 'a');
}

constexpr inline char rankToAN(int8_t rank) noexcept
{
	return (char)('1' + rank);
}

//constexpr inline char getFileAN(Square sq) noexcept
//{
//	return fileToAN(getFile(sq));
//}
//
//constexpr inline char getRankAN(Square sq) noexcept
//{
//	return rankToAN(getRank(sq));
//}

//constexpr inline Square makeSquare(int8_t rank, int8_t file) noexcept
//{
//	return Square((rank << 3) | file);
//}
//
//inline std::string squareToAN(Square sq)
//{
//	return { (char)(getFile(sq) + 'a'), (char)(getRank(sq) + '1') };
//}
//
//inline Square squareFromAN(const std::string& sqAN)
//{
//	return makeSquare(rankFromAN(sqAN[1]), fileFromAN(sqAN[0]));
//}
//
//constexpr inline Square getFrom(Move m) noexcept
//{
//	return Square((m & MoveDesc::FROM_MASK) >> MoveDesc::FROM_FB);
//}
//
//constexpr inline Square getTo(Move m) noexcept
//{
//	return Square((m & MoveDesc::TO_MASK) >> MoveDesc::TO_FB);
//}

inline CastlingSide castlingSideFromAN(std::string csAN)
{
	return csAN == "O-O" ? OO : OOO;
}

inline std::string castlingSideToAN(CastlingSide cs)
{
	return cs == OO ? "O-O" : "O-O-O";
}

//constexpr inline CastlingSide getCastlingSide(Move m) noexcept
//{
//	return getFile(getTo(m)) == 2 ? OOO : OO;
//}
//
//inline std::string getCastlingSideAN(Move m) noexcept
//{
//	return castlingSideToAN(getCastlingSide(m));
//}

constexpr inline CastlingRight makeCastling(Color c, CastlingSide cs) noexcept
{
	return CastlingRight(CR_WHITE_OO << ((c << 1) | cs));
}

// Piece type from uppercase identifier in algebraic notations
constexpr inline PieceType pieceTypeFromAN(char ptAN) noexcept
{
	switch (ptAN)
	{
	case 'N':	return KNIGHT;
	case 'B':	return BISHOP;
	case 'R':	return ROOK;
	case 'Q':	return QUEEN;
	case 'K':	return KING;
	default:	return NO_PIECE_TYPE;
	}
}

// Piece type from uppercase identifier in FEN notation
constexpr inline PieceType pieceTypeFromFEN(char ptFEN) noexcept
{
	return ptFEN == 'P' ? PAWN : pieceTypeFromAN(ptFEN);
}

// Uppercase piece type identifier in algebraic notations
constexpr inline char pieceTypeToAN(PieceType pt) noexcept
{
	switch (pt)
	{
	case KNIGHT:	return 'N';
	case BISHOP:	return 'B';
	case ROOK:		return 'R';
	case QUEEN:		return 'Q';
	case KING:		return 'K';
	default:		return '\0';
	}
}

// Uppercase piece type identifier in FEN notation
constexpr inline char pieceTypeToFEN(PieceType pt) noexcept
{
	return pt == PAWN ? 'P' : pieceTypeToAN(pt);
}

constexpr inline PieceType getPieceType(Piece pc) noexcept
{
	return PieceType(pc & 7);
}

constexpr inline Color getPieceColor(Piece pc) noexcept
{
	return pc == NO_PIECE ? NO_COLOR : Color(pc >> 3);
}

constexpr inline Piece makePiece(Color c, PieceType pt) noexcept
{
	return Piece((c << 3) | pt);
}

//constexpr inline Move makeMove(int8_t from, int8_t to,
//	MoveType mt = MT_NORMAL, PieceType promotion = KNIGHT) noexcept
//{
//	return (from << MoveDesc::FROM_FB) | (to << MoveDesc::TO_FB) |
//		(mt << MoveDesc::TYPE_FB) | ((promotion - 2) << MoveDesc::PROMOTION_FB);
//}
//
//constexpr inline Move makeCastlingMove(Color c, CastlingSide cs) noexcept
//{
//	return cs == OO ? makeMove(relSquare(SQ_E1, c), relSquare(SQ_G1, c), MT_CASTLING)
//		: makeMove(relSquare(SQ_E1, c), relSquare(SQ_C1, c), MT_CASTLING);
//}
//
//constexpr inline MoveType getMoveType(Move m) noexcept
//{
//	return MoveType((m & MoveDesc::TYPE_MASK) >> MoveDesc::TYPE_FB);
//}
//
//constexpr inline PieceType getPromotion(Move m) noexcept
//{
//	return ((m & MoveDesc::PROMOTION_MASK) >> MoveDesc::PROMOTION_FB) + 2;
//}

//==============================================
// Class approach
//==============================================

namespace
{
	template<typename T>
	struct Envelope
	{
		constexpr T& _this(void) noexcept
		{
			return static_cast<T&>(*this);
		}
		constexpr const T& _this(void) const noexcept
		{
			return static_cast<T&>(*this);
		}
	};

	template<typename T>
	struct OperatorEnvelope : Envelope<T>
	{
		/*constexpr friend bool operator==(T lhs, T rhs) noexcept
		{
			return lhs.raw() == rhs.raw();
		}
		constexpr friend bool operator!=(T lhs, T rhs) noexcept
		{
			return lhs.raw() != rhs.raw();
		}*/
		/*constexpr friend T operator+(T lhs, T rhs) noexcept
		{
			return T(lhs.raw() + rhs.raw());
		}
		constexpr friend T operator-(T lhs, T rhs) noexcept
		{
			return T(lhs.raw() - rhs.raw());
		}*/
		/*constexpr friend T operator*(T lhs, T rhs) noexcept
		{
			return T(lhs.raw() * rhs.raw());
		}
		constexpr friend T operator/(T lhs, T rhs)
		{
			return T(lhs.raw() / rhs.raw());
		}*/
		constexpr T& operator++() noexcept
		{
			++_this().raw();
			return _this();
		}
		constexpr T& operator++(int) noexcept
		{
			++_this().raw();
			return _this();
		}
		constexpr T& operator--() noexcept
		{
			--_this().raw();
			return _this();
		}
		constexpr T& operator--(int) noexcept
		{
			--_this().raw();
			return _this();
		}
		constexpr T& operator+=(T rhs) noexcept
		{
			_this().raw() += rhs.raw();
			return _this();
		}
		constexpr T& operator-=(T rhs) noexcept
		{
			_this().raw() -= rhs.raw();
			return _this();
		}
		constexpr T& operator*=(T rhs) noexcept
		{
			_this().raw() *= rhs.raw();
			return _this();
		}
		constexpr T& operator/=(T rhs)
		{
			_this().raw() /= rhs.raw();
			return _this();
		}
	};
}

class Square : public OperatorEnvelope<Square>
{
	friend class Move;
	friend struct OperatorEnvelope<Square>;
public:
	static Square fromAN(const std::string& sqAN)
	{
		return Square(rankFromAN(sqAN[1]), fileFromAN(sqAN[0]));
	}
	Square(void) = default;
	constexpr Square(SquareRaw sq) noexcept
		: sq(sq)
	{}
	constexpr Square(int8_t rank, int8_t file) noexcept
		: sq((rank << 3) | file)
	{}
	constexpr int8_t getFile(void) const noexcept
	{
		return sq & 7;
	}
	constexpr int8_t getRank(void) const noexcept
	{
		return sq >> 3;
	}
	constexpr int8_t getDiagonal(void) const noexcept
	{
		return getRank() - getFile() + 7;
	}
	constexpr int8_t getAntidiagonal(void) const noexcept
	{
		return getRank() + getFile();
	}
	constexpr bool isBorder(void) const noexcept
	{
		const int8_t r = getRank(), f = getFile();
		return r == RANK_MIN || r == RANK_MAX || f == FILE_MIN || f == FILE_MAX;
	}
	constexpr bool isCorner(void) const noexcept
	{
		return sq == SQ_A1 || sq == SQ_A8 || sq == SQ_H1 || sq == SQ_H8;
	}
	constexpr bool isValid(void) const noexcept
	{
		return SQ_A1 <= sq && sq < SQUARE_CNT;
	}
	// Square relative to a given side (flips row if c is BLACK)
	constexpr inline Square relativeTo(Color c) const noexcept
	{
		return c == WHITE ? *this : sq + FILE_CNT * (RANK_CNT - 1 - (getRank() << 1));
	}
	constexpr inline char getFileAN(void) const noexcept
	{
		return fileToAN(getFile());
	}
	constexpr inline char getRankAN(void) const noexcept
	{
		return rankToAN(getRank());
	}
	inline std::string toAN(void) const
	{
		return { (char)(getFile() + 'a'), (char)(getRank() + '1') };
	}
	constexpr operator SquareRaw() const noexcept
	{
		return sq;
	}
	constexpr SquareRaw& raw(void) noexcept
	{
		return sq;
	}
	constexpr const SquareRaw& raw(void) const noexcept
	{
		return sq;
	}
private:
	SquareRaw sq;
};

// Manhattan distance between squares
inline int8_t distance(Square sq1, Square sq2) noexcept
{
	return abs(sq1.getRank() - sq2.getRank()) + abs(sq1.getFile() - sq2.getFile());
}
constexpr inline Square relSquare(Square sq, Color c) noexcept
{
	return sq.relativeTo(c);
}

class Move
{
public:
	Move(void) = default;
	constexpr Move(MoveRaw move) noexcept : move(move)
	{}
	constexpr Move(Square from, Square to,
		MoveType mt = MT_NORMAL, PieceType promotion = KNIGHT) noexcept : move(
			makeRaw(from, to, mt, promotion))
	{}
	constexpr Move(Color c, CastlingSide cs) noexcept : move(cs == OO
		? makeRaw(relSquare(SQ_E1, c), relSquare(SQ_G1, c), MT_CASTLING)
		: makeRaw(relSquare(SQ_E1, c), relSquare(SQ_C1, c), MT_CASTLING))
	{}
	constexpr bool operator==(Move rhs) const noexcept
	{
		return move == rhs.move;
	}
	constexpr bool operator!=(Move rhs) const noexcept
	{
		return move != rhs.move;
	}
	constexpr Square getFrom(void) const noexcept
	{
		return Square((move & MoveDesc::FROM_MASK) >> MoveDesc::FROM_FB);
	}
	constexpr Square getTo(void) const noexcept
	{
		return Square((move & MoveDesc::TO_MASK) >> MoveDesc::TO_FB);
	}
	constexpr MoveType getType(void) const noexcept
	{
		return MoveType((move & MoveDesc::TYPE_MASK) >> MoveDesc::TYPE_FB);
	}
	constexpr PieceType getPromotion(void) const noexcept
	{
		return ((move & MoveDesc::PROMOTION_MASK) >> MoveDesc::PROMOTION_FB) + 2;
	}
	constexpr CastlingSide getCastlingSide(void) const noexcept
	{
		return getTo().getFile() == 2 ? OOO : OO;
	}
	std::string getCastlingSideAN(void) const
	{
		return castlingSideToAN(getCastlingSide());
	}
	constexpr MoveRaw& raw(void) noexcept
	{
		return move;
	}
	constexpr const MoveRaw& raw(void) const noexcept
	{
		return move;
	}
private:
	static constexpr uint16_t makeRaw(Square from, Square to,
		MoveType mt = MT_NORMAL, PieceType promotion = KNIGHT) noexcept
	{
		return (from.sq << MoveDesc::FROM_FB) | (to.sq << MoveDesc::TO_FB) |
			(mt << MoveDesc::TYPE_FB) | ((promotion - 2) << MoveDesc::PROMOTION_FB);
	}
	MoveRaw move;
};

#endif
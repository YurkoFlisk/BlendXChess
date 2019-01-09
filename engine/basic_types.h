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

#if defined(_DEBUG) | defined(DEBUG)
#define ENGINE_DEBUG
#endif

//============================================================
// Basic typedef types
//============================================================

typedef int8_t SquareRaw, Side, Color, Depth, PieceType, Piece;
typedef int16_t Score;
typedef int32_t MoveScore;
typedef uint64_t Key;
typedef uint16_t MoveRaw;
typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

//============================================================
// Basic constants
//============================================================

constexpr int8_t FILE_CNT = 8, FILE_MIN = 0, FILE_MAX = 7, RANK_CNT = 8, RANK_MIN = 0,
	RANK_MAX = 7, DIAG_CNT = 15, SQUARE_CNT = FILE_CNT * RANK_CNT,
	COLOR_CNT = 2, PIECETYPE_CNT = 7, MAX_PIECES_OF_ONE_TYPE = 9, CASTLING_SIDE_CNT = 2;
constexpr int16_t MAX_GAME_PLY = 1024, MAX_SEARCH_PLY = 100;

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
	PT_NULL,
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
	PT_ALL = 0 // For using as index in bitboard arrays
};
// Pieces
enum : Piece {
	PIECE_NULL,
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
	MG_EVASIONS, MG_NON_CAPTURES, MG_CAPTURES, MG_ALL
};
// Game states
enum GameState : int8_t {
	GS_ACTIVE,
	GS_DRAW,
	GS_WHITE_WIN,
	GS_BLACK_WIN,
	GS_UNDEFINED
};
// Draw causes
enum DrawCause : int8_t {
	DC_RULE_50,
	DC_MATERIAL,
	DC_THREEFOLD_REPETITION
};
// Sides (Colors)
enum : Side {
	WHITE, BLACK, NULL_COLOR,
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
namespace Sq
{
	enum : SquareRaw {
		A1, B1, C1, D1, E1, F1, G1, H1,
		A2, B2, C2, D2, E2, F2, G2, H2,
		A3, B3, C3, D3, E3, F3, G3, H3,
		A4, B4, C4, D4, E4, F4, G4, H4,
		A5, B5, C5, D5, E5, F5, G5, H5,
		A6, B6, C6, D6, E6, F6, G6, H6,
		A7, B7, C7, D7, E7, F7, G7, H7,
		A8, B8, C8, D8, E8, F8, G8, H8,
		NONE,
		D_LEFT = -1, D_RIGHT = 1, D_UP = FILE_CNT, D_DOWN = -FILE_CNT,
		D_LU = D_LEFT + D_UP, D_RU = D_RIGHT + D_UP, D_LD = D_LEFT + D_DOWN, D_RD = D_RIGHT + D_DOWN
	};
}
// Moves
enum : MoveRaw {
	// There are no such valid moves ('from' and 'to' are the same), so we can use these values as special ones
	MOVE_NONE = 0, // 'from' == 'to' == Sq::A1
	MOVE_NULL = 0xfff // 'from' == 'to' == Sq::H8
};

//============================================================
// Inline functions
//============================================================

constexpr inline Side opposite(Side c) noexcept
{
	return c == WHITE ? BLACK : WHITE;
}

// Rank relative to a given side
constexpr inline int8_t relRank(int8_t r, Side c) noexcept
{
	return c == WHITE ? r : RANK_CNT - 1 - r;
}

constexpr inline bool validRank(int8_t rank) noexcept
{
	return 0 <= rank && rank < RANK_CNT;
}

constexpr inline bool validFile(int8_t column) noexcept
{
	return 0 <= column && column < FILE_CNT;
}

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

inline CastlingSide castlingSideFromAN(std::string csAN)
{
	return csAN == "O-O" ? OO : OOO;
}

inline std::string castlingSideToAN(CastlingSide cs)
{
	return cs == OO ? "O-O" : "O-O-O";
}

constexpr inline CastlingRight makeCastling(Side c, CastlingSide cs) noexcept
{
	return CastlingRight(CR_WHITE_OO << ((c << 1) | cs));
}

constexpr inline bool isSingularCR(CastlingRight cr) noexcept
{
	return cr == CR_WHITE_OO || cr == CR_WHITE_OOO || cr == CR_BLACK_OO || cr == CR_BLACK_OOO;
	// return cr && (!(cr & (cr - 1))); // because singular CRs are powers of 2
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
	default:	return PT_NULL;
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

constexpr inline Side getPieceSide(Piece pc) noexcept
{
	return pc == PIECE_NULL ? NULL_COLOR : Side(pc >> 3);
}

constexpr inline Piece makePiece(Side c, PieceType pt) noexcept
{
	return Piece((c << 3) | pt);
}

//==============================================
// Class approach
//==============================================

namespace
{
	template<typename T>
	struct Envelope
	{
		constexpr inline T& _this(void) noexcept
		{
			return static_cast<T&>(*this);
		}
		constexpr inline const T& _this(void) const noexcept
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
		constexpr inline T& operator++() noexcept
		{
			++_this().raw();
			return _this();
		}
		constexpr inline T& operator++(int) noexcept
		{
			++_this().raw();
			return _this();
		}
		constexpr inline T& operator--() noexcept
		{
			--_this().raw();
			return _this();
		}
		constexpr inline T& operator--(int) noexcept
		{
			--_this().raw();
			return _this();
		}
		constexpr inline T& operator+=(T rhs) noexcept
		{
			_this().raw() += rhs.raw();
			return _this();
		}
		constexpr inline T& operator-=(T rhs) noexcept
		{
			_this().raw() -= rhs.raw();
			return _this();
		}
		constexpr inline T& operator*=(T rhs) noexcept
		{
			_this().raw() *= rhs.raw();
			return _this();
		}
		constexpr inline T& operator/=(T rhs)
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
	static inline Square fromAN(const std::string& sqAN)
	{
		return Square(rankFromAN(sqAN[1]), fileFromAN(sqAN[0]));
	}
	inline Square(void) = default;
	constexpr inline Square(SquareRaw sq) noexcept
		: sq(sq)
	{}
	constexpr inline Square(int8_t rank, int8_t file) noexcept
		: sq((rank << 3) | file)
	{}
	constexpr inline int8_t file(void) const noexcept
	{
		return sq & 7;
	}
	constexpr inline int8_t rank(void) const noexcept
	{
		return sq >> 3;
	}
	constexpr inline int8_t diagonal(void) const noexcept
	{
		return rank() - file() + 7;
	}
	constexpr inline int8_t antidiagonal(void) const noexcept
	{
		return rank() + file();
	}
	constexpr inline Color color(void) const noexcept // color of square
	{
		return (sq & 1) ^ 1; // Converse nonimplication, btw
	}
	constexpr inline bool isBorder(void) const noexcept
	{
		const int8_t r = rank(), f = file();
		return r == RANK_MIN || r == RANK_MAX || f == FILE_MIN || f == FILE_MAX;
	}
	constexpr inline bool isCorner(void) const noexcept
	{
		return sq == Sq::A1 || sq == Sq::A8 || sq == Sq::H1 || sq == Sq::H8;
	}
	constexpr inline bool isValid(void) const noexcept
	{
		return Sq::A1 <= sq && sq < SQUARE_CNT;
	}
	// Square relative to a given side (flips row if c is BLACK)
	constexpr inline Square relativeTo(Side c) const noexcept
	{
		return c == WHITE ? *this : sq + FILE_CNT * (RANK_CNT - 1 - (rank() << 1));
	}
	constexpr inline char fileAN(void) const noexcept
	{
		return fileToAN(file());
	}
	constexpr inline char rankAN(void) const noexcept
	{
		return rankToAN(rank());
	}
	// Shifts square to a given direction given number of times (assumes one of Sq::D_'s)
	// Recognizes getting outside of the board and sets square to NONE in these cases
	inline std::string toAN(void) const
	{
		return { (char)(file() + 'a'), (char)(rank() + '1') };
	}
	constexpr inline operator SquareRaw(void) const noexcept
	{
		return sq;
	}
	constexpr inline SquareRaw& raw(void) noexcept
	{
		return sq;
	}
	constexpr inline const SquareRaw& raw(void) const noexcept
	{
		return sq;
	}
private:
	SquareRaw sq;
};

// Manhattan distance between squares
inline int8_t distance(Square sq1, Square sq2) noexcept
{
	return abs(sq1.rank() - sq2.rank()) + abs(sq1.file() - sq2.file());
}
// Square relative to a given side (flips row if c is BLACK)
constexpr inline Square relSquare(Square sq, Side c) noexcept
{
	return sq.relativeTo(c);
}
// Shifts square to a given direction given number of times (assumes one of Sq::D_'s)
// Recognizes getting outside of the board and sets square to NONE in these cases
constexpr inline Square shiftD(Square sq, Square d, int cnt = 1) noexcept
{
	while (cnt-- > 0 && sq.isValid())
	{
		sq += d;
		if (!(sq.isValid() && distance(sq, sq - d) <= 2))
			sq = Sq::NONE;
	}
	return sq;
}

class Move
{
public:
	static Move fromUCI(const std::string& str);
	inline Move(void) = default;
	constexpr inline Move(MoveRaw move) noexcept : move(move)
	{}
	constexpr inline Move(Square from, Square to,
		MoveType mt = MT_NORMAL, PieceType promotion = KNIGHT) noexcept : move(
			makeRaw(from, to, mt, promotion))
	{}
	constexpr inline Move(Side c, CastlingSide cs) noexcept : move(cs == OO
		? makeRaw(relSquare(Sq::E1, c), relSquare(Sq::G1, c), MT_CASTLING)
		: makeRaw(relSquare(Sq::E1, c), relSquare(Sq::C1, c), MT_CASTLING))
	{}
	constexpr inline bool operator==(Move rhs) const noexcept
	{
		return move == rhs.move;
	}
	constexpr inline bool operator!=(Move rhs) const noexcept
	{
		return move != rhs.move;
	}
	constexpr inline Square from(void) const noexcept
	{
		return Square((move & MoveDesc::FROM_MASK) >> MoveDesc::FROM_FB);
	}
	constexpr inline Square to(void) const noexcept
	{
		return Square((move & MoveDesc::TO_MASK) >> MoveDesc::TO_FB);
	}
	constexpr inline MoveType type(void) const noexcept
	{
		return MoveType((move & MoveDesc::TYPE_MASK) >> MoveDesc::TYPE_FB);
	}
	constexpr inline PieceType promotion(void) const noexcept
	{
		return ((move & MoveDesc::PROMOTION_MASK) >> MoveDesc::PROMOTION_FB) + 2;
	}
	constexpr inline CastlingSide castlingSide(void) const noexcept
	{
		return to().file() == fileFromAN('c') ? OOO : OO;
	}
	inline std::string castlingSideAN(void) const
	{
		return castlingSideToAN(castlingSide());
	}
	std::string toAN(void) const
	{
		if (type() == MT_CASTLING)
			return (to().file() == fileFromAN('g') ? "O-O" : "O-O-O");
		else
		{
			std::string AN = from().toAN() + "-" + to().toAN();
			if (type() == MT_PROMOTION)
				switch (promotion())
				{
				case KNIGHT:	AN.push_back('N'); break;
				case BISHOP:	AN.push_back('B'); break;
				case ROOK:		AN.push_back('R'); break;
				case QUEEN:		AN.push_back('Q'); break;
				}
			return AN;
		}
	}
	constexpr inline MoveRaw& raw(void) noexcept
	{
		return move;
	}
	constexpr inline const MoveRaw& raw(void) const noexcept
	{
		return move;
	}
private:
	static constexpr inline uint16_t makeRaw(Square from, Square to,
		MoveType mt = MT_NORMAL, PieceType promotion = KNIGHT) noexcept
	{
		return (from.sq << MoveDesc::FROM_FB) | (to.sq << MoveDesc::TO_FB) |
			(mt << MoveDesc::TYPE_FB) | ((promotion - 2) << MoveDesc::PROMOTION_FB);
	}
	MoveRaw move;
};

Move Move::fromUCI(const std::string& str)
{
	return Move();
}

#endif
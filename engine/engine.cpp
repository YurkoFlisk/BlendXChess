//============================================================
// engine.cpp
// ChessEngine
//============================================================

#include "engine.h"
#include "move_manager.h"
#include <cctype>
#include <sstream>
#include <algorithm>

using namespace BlendXChess;

//============================================================
// Constructor
//============================================================
Game::Game(void)
	: gameState(GameState::UNDEFINED)
{
	if (initialized)
		reset();
}

//============================================================
// Initialization
//============================================================
void Game::initialize(void)
{
	initPSQ();
	initBB();
	initZobrist();
	initialized = true;
}

//============================================================
// Clear position and search info (everything except TT)
//============================================================
void Game::clear(void)
{
	pos.clear();
	gameState = GameState::UNDEFINED;
	// Game and position history
	gameHistory.clear();
	positionRepeats.clear();
}

//============================================================
// Reset game (stops search if there's any)
//============================================================
void Game::reset(void)
{
	if (!initialized)
		return;
	// If we are in search, stop it
	if (searcher.isInSearch())
		searcher.endSearch();
	clear();
	pos.reset(); // Position::clear will also be called from here but it's not crucial
	gameState = GameState::ACTIVE;
}

//============================================================
// Whether position is draw by insufficient material
//============================================================
bool Game::drawByMaterial(void) const
{
	if (pos.pieceCount[WHITE][PT_ALL] > 2 || pos.pieceCount[BLACK][PT_ALL] > 2)
		return false;
	if (pos.pieceCount[WHITE][PT_ALL] == 1 && pos.pieceCount[BLACK][PT_ALL] == 1)
		return true;
	for (Side side : {WHITE, BLACK})
		if (pos.pieceCount[opposite(side)][PT_ALL] == 1 &&
			(pos.pieceCount[side][BISHOP] == 1 || pos.pieceCount[side][KNIGHT] == 1))
			return true;
	if (pos.pieceCount[WHITE][BISHOP] == 1 && pos.pieceCount[BLACK][BISHOP] == 1 &&
		pos.pieceSq[WHITE][BISHOP][0].color() == pos.pieceSq[BLACK][BISHOP][0].color())
		return true;
	return false;
}

//============================================================
// Whether position is threefold repeated, which results in draw
//============================================================
bool Game::threefoldRepetitionDraw(void) const
{
	std::stringstream positionFEN;
	pos.writeFEN(positionFEN, true); // Positions are considered equal iff reduced FENs are
	const auto iter = positionRepeats.find(positionFEN.str());
	return iter != positionRepeats.end() && iter->second >= 3;
}

//============================================================
// Update game state
//============================================================
void Game::updateGameState(void)
{
	if (searcher.isInSearch())
		return;
	MoveList moveList;
	pos.generateLegalMoves(moveList);
	if (moveList.empty())
		gameState = pos.isInCheck() ? (pos.turn == WHITE ?
			GameState::BLACK_WIN : GameState::WHITE_WIN) : GameState::DRAW;
	else if (pos.info.rule50 >= 100)
		gameState = GameState::DRAW, drawCause = DrawCause::RULE_50;
	else if (drawByMaterial())
		gameState = GameState::DRAW, drawCause = DrawCause::MATERIAL;
	else if (threefoldRepetitionDraw())
		gameState = GameState::DRAW, drawCause = DrawCause::THREEFOLD_REPETITION;
	else
		gameState = GameState::ACTIVE;
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
//============================================================
bool Game::DoMove(Move move)
{
	// If we are in search, don't do any moves
	if (searcher.isInSearch())
		return false;
	// Try to get string representation of and perform given move on current position
	PositionInfo prevState;
	std::array<std::string, MOVE_FORMAT_CNT> moveStr;
	try
	{
		for (auto fmt : {FMT_AN, FMT_SAN, FMT_UCI})
			moveStr[fmt] = pos.moveToStr(move, fmt);
		if (!pos.DoMove(move, &prevState))
			return false;
	}
	catch (...)
	{
		return false;
	}
	// If it's legal, update game info and state
	std::stringstream positionFEN;
	pos.writeFEN(positionFEN, true);
	++positionRepeats[positionFEN.str()];
	gameHistory.push_back(GHRecord{ move, prevState, moveStr });
	updateGameState();
	return true;
}

//============================================================
// Do move. Return true if succeded, false otherwise
// (false may be due to illegal move or inappropriate engine state)
// It is not well optimized and is an interface for external calls
// Engine internals use doMove instead
// Input string in algebraic-like format
//============================================================
bool Game::DoMove(const std::string& moveStr, MoveFormat moveFormat)
{
	// If we are in search, don't do any moves
	if (searcher.isInSearch())
		return false;
	// Try to convert to Move type representation of move and use it for next actions
	Move move;
	try
	{
		move = pos.moveFromStr(moveStr, moveFormat);
	}
	catch (const std::runtime_error&)
	{
		return false;
	}
	return DoMove(move);
}

//============================================================
// Undo last move. Return false if it is start state now, true otherwise
// It is not well optimized and is an interface for external calls
// Engine internals use undoMove instead
//============================================================
bool Game::UndoMove(void)
{
	// If we are in search, don't undo any moves
	if (searcher.isInSearch())
		return false;
	// If there wew no moves since start/set position, there's nothing to undo
	if (gameHistory.empty())
		return false;
	// Try to undo (there should be no errors there, but it's good to check if possible)
	if (pos.UndoMove(gameHistory.back().move, gameHistory.back().prevState))
		return false;
	// If succeded, update game info and state
	gameHistory.pop_back();
	updateGameState();
	return true;
}

//============================================================
// Change value of specified option to given (it's parsed here)
// Throws if no such option found
//============================================================
void Game::setOption(std::string name, const std::string& value)
{
	// options.set(name, value);
	std::transform(name.begin(), name.end(), name.begin(), std::tolower);
	if (name == "timelimit")
		searcher.setTimeLimit(convertTo<unsigned int>(value));
	else if (name == "threadcount")
		searcher.setThreadCount(convertTo<unsigned int>(value));
	else if (name == "searchdepth" || name == "depth")
		searcher.setDepth(convertTo<Depth>(value));
	else
		throw std::runtime_error("Unrecognized option '" + name + "'");
}

//============================================================
// Begins search on current position and using current options
//============================================================
void Game::startSearch(void)
{
	searcher.startSearch(pos);
}

//============================================================
// Stops the searcher and returns results
//============================================================
SearchReturn Game::endSearch(void)
{
	return searcher.endSearch();
}

//============================================================
// Load game from the given stream assuming given move format
//============================================================
void Game::loadGame(std::istream& istr, MoveFormat fmt)
{
	// Don't load game if we are in search
	if (searcher.isInSearch())
		throw std::runtime_error("Search is currently launched");
	// Reset position information
	reset();
	// Read moves until mate/draw or end of file
	static constexpr char delim = '.';
	while (true)
	{
		const int expectedMN = pos.gamePly / 2 + 1;
		// If it's white's turn, move number (equal to expectedMN) should be present before it
		if (pos.turn == WHITE)
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
		if (!istr || !DoMove(moveSAN, fmt))
			throw std::runtime_error((pos.turn == WHITE ? "White " : "Black ")
				+ std::string("move at position ") + std::to_string(expectedMN) + " is illegal");
		if (gameState != GameState::ACTIVE)
			break;
	}
}

//============================================================
// Write game to the given stream in SAN notation
//============================================================
void Game::writeGame(std::ostream& ostr, MoveFormat fmt)
{
	// Write saved SAN representations of moves along with move number indicators
	for (int ply = 0; ply < gameHistory.size(); ++ply)
	{
		if ((ply & 1) == 0)
			ostr << ply / 2 + 1 << '.';
		ostr << ' ' << gameHistory[ply].moveStr[fmt];
		if (ply & 1)
			ostr << '\n';
	}
}

//============================================================
// Load position from a given stream in FEN notation
// (bool parameter says whether to omit move counters)
//============================================================
void Game::loadFEN(std::istream& istr, bool omitCounters)
{
	if (isInSearch())
		throw std::runtime_error("Can't load position during search. Call endSearch before");
	clear();
	pos.loadFEN(istr, omitCounters);
	std::stringstream positionFEN;
	pos.writeFEN(positionFEN, true);
	++positionRepeats[positionFEN.str()];
}

//============================================================
// Load position from a given string in FEN notation
// (bool parameter says whether to omit move counters)
//============================================================
void Game::loadFEN(const std::string& str, bool omitCounters)
{
	if (isInSearch())
		throw std::runtime_error("Can't load position during search. Call endSearch before");
	clear();
	pos.loadFEN(str, omitCounters);
	std::stringstream positionFEN;
	pos.writeFEN(positionFEN, true);
	++positionRepeats[positionFEN.str()];
}

//============================================================
// Write position to a given stream in FEN notation
//============================================================
void Game::writeFEN(std::ostream& ostr, bool omitCounters) const
{
	pos.writeFEN(ostr, omitCounters);
}
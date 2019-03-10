//============================================================
// Standalone console interface to the engine
// BlendXChess
//============================================================

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include "../engine/engine.h"
#include "event.h"

using namespace std;
using namespace BlendXChess;

Game game;
EventLoop el;
bool inGame = false;
Side userSide;
MoveFormat moveFmt = FMT_AN;
TimePoint startTime, endTime;

//============================================================
// Struct for holding various info for search options
//============================================================

struct OptionInfo
{
	OptionInfo(void) = default;
	OptionInfo(int minValue, int maxValue, int defaultValue, std::string desc)
		: minValue(minValue), maxValue(maxValue),
		defaultValue(defaultValue), description(std::move(desc))
	{}
	int minValue;
	int maxValue;
	int defaultValue;
	std::string description;
};

// Map of available options
std::unordered_map<string, OptionInfo> optionInfos = {
	{"depth", OptionInfo(1, 20, 10,
	"Orientive depth of search until quiescence search is applied")},
	{"threadCount", OptionInfo(1, Game::getMaxThreadCount(), Game::getMaxThreadCount(),
	"Count of threads for search")},
	{"timeLimit", OptionInfo(1, 100000, 5000,
	"Time limit of search")}
};

//============================================================
// Tokenizes input string by default (space) delimiter
//============================================================
vector<string> tokenize(string input)
{
	string token;
	vector<string> tokens;
	istringstream iss(input);
	while (iss >> token)
		tokens.push_back(token);
	return tokens;
}

//============================================================
// Show greeting at startup
//============================================================
void showGreeting(void)
{
#ifndef ENGINE_DEBUG
	for (int i = 1; i <= 5; ++i)
	{
		auto st = chrono::high_resolution_clock::now();
		cout << game.perft(i); // perft<true>
		auto en = chrono::high_resolution_clock::now();
		cout << " Perft(" << i << ") on initial position is " << chrono::duration_cast<
			chrono::milliseconds>(en - st).count() << "ms\n";
	}
#endif
	cout << "Welcome to the native console interface of BlendX chess engine!" << endl;
	cout << "Type 'help' to see supported commands and options" << endl;
}

//============================================================
// Show help info
//============================================================
void showHelp(void)
{
	cout << "You can set search options with 'set' command. There are following: " << endl;
	for (const auto&[name, opt] : optionInfos)
		cout << name << " (min " << opt.minValue << ", max " << opt.maxValue
		     << ", def " << opt.defaultValue << "): " << endl << "  " << opt.description << endl;
}

//============================================================
// Show current search options
//============================================================
void showOptions(void)
{
	const SearchOptions& opts = game.getSearchOptions();
	cout << "Depth: " << (int)opts.depth << endl;
	cout << "Thread count: " << opts.threadCount << endl;
	cout << "Time limit: " << opts.timeLimit << "ms" << endl;
}

//============================================================
// Start search
//============================================================
void startSearch(void)
{
	cout << "Starting search on current position..." << endl;
	try
	{
		startTime = chrono::high_resolution_clock::now();
		game.startSearch();
	}
	catch (const std::runtime_error& err)
	{
		cout << "Error: " << err.what() << endl;
	}
}

//============================================================
// End search, perform best move and display results
//============================================================
void endSearch(void)
{
	try
	{
		// Returns results of just stopped search (lastSearchReturn)
		auto[results, stats] = game.endSearch();
		endTime = chrono::high_resolution_clock::now();
		if (!game.DoMove(results.bestMove))
		{
			cout << "Search returned with illegal move" << endl;
			return;
		}
		cout << "Search finished with move ";
		cout << results.bestMove.toAN();
		cout << ". " << stats.visitedNodes << " nodes searched in "
			<< chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count()
			<< " ms to depth " << (int)results.resDepth << ". The score is " << results.score << ". "
			<< ttFreeEntries << " free slots in TT. " << stats.ttHits << " hits made." << endl;
	}
	catch (const std::runtime_error& err)
	{
		cout << "Error: " << err.what() << endl;
	}
}

//============================================================
// Update game state and signalize in case of a finish
//============================================================
void updateGameState(void)
{
	if (game.getGameState() != GameState::ACTIVE)
	{
		if (game.getGameState() == GameState::DRAW)
		{
			const DrawCause drawCause = game.getDrawCause();
			cout << "Draw! Cause: " << (int)game.getDrawCause() << endl;
			switch (drawCause)
			{
			case DrawCause::RULE_50: cout << "Rule 50"; break;
			case DrawCause::MATERIAL: cout << "Insufficient material"; break;
			case DrawCause::THREEFOLD_REPETITION: cout << "Threefold repetition"; break;
			default: cout << "Unknown"; break;
			}
		}
		else if (game.getGameState() == GameState::WHITE_WIN)
			cout << "Checkmate! White win" << endl;
		else if (game.getGameState() == GameState::BLACK_WIN)
			cout << "Checkmate! Black win" << endl;
		std::ofstream gameLog("lastGame.txt");
		game.writeGame(gameLog);
		gameLog.close();
		inGame = false;
	}
}

//============================================================
// Load position from given filepath (resets if error)
//============================================================
void loadPosition(const std::string& filepath)
{
	try
	{
		game.loadFEN(filepath, true);
	}
	catch (const runtime_error& err)
	{
		game.reset();
		cout << "Error: " << err.what() << endl;
	}
}

//============================================================
// Load game from given filepath (resets if error)
//============================================================
void loadGame(const std::string& filepath)
{
	std::string strMove;
	ifstream in(filepath);
	while (getline(in, strMove))
		if (!game.DoMove(strMove, moveFmt))
		{
			cout << "Error: Wrong move " << strMove << " in input file" << endl;
			game.reset();
			break;
		}
	in.close();
}

//============================================================
// Processing events from console and engine
// Returns true if the event wasn't quit, false if it was
//============================================================
bool processEvent(const Event& e)
{
	if (e.source == EventSource::CONSOLE)
	{
		std::string input = get<string>(e.eventInfo);
		vector<string> tokens = tokenize(input);
		if (tokens.empty())
			return true;
		if (tokens[0] == "quit")
			return false;
		else if (tokens[0] == "help")
			showHelp();
		else if (tokens[0] == "showOptions")
			showOptions();
		else if (inGame)
		{
			if (game.isInSearch())
			{
				if (tokens[0] == "stop")
				{
					endSearch();
					updateGameState();
				}
				else
					cout << "Error: Unrecognized command " << tokens[0] << ", please try again." << endl;
			}
			else if (tokens[0] == "abort")
			{
				game.reset();
				inGame = false;
				cout << "Game aborted successfully" << endl;
			}
			else if (tokens[0] == "staticEvaluate")
				cout << "Temporary unavailable..." << endl;
			else if (tokens[0] == "move")
			{
				if (tokens.size() == 1)
					cout << "Error: what move?" << endl;
				else if (!game.DoMove(tokens[1], moveFmt))
					cout << "Error: illegal move" << endl;
				else
				{
					cout << "Move successfully performed!" << endl;
					updateGameState();
					if (inGame)
						startSearch();
				}
			}
			else if (game.DoMove(tokens[0], moveFmt)) // Assume this is just a move
			{
				cout << "Move successfully performed!" << endl;
				updateGameState();
				if (inGame)
					startSearch();
			}
			else
				cout << "Error: Unrecognized command or move " << tokens[0]
				     << ", please try again." << endl;
		}
		else
		{
			if (tokens[0] == "set")
			{
				if (tokens.size() == 1)
					cout << "Error: set what?" << endl;
				else if (tokens.size() == 2)
					cout << "Error: set " << tokens[1] << " to what value?" << endl;
				else try
				{
					game.setOption(tokens[1], tokens[2]);
					cout << "Option " << tokens[1] << " successfully set to " << tokens[2] << endl;
				}
				catch (const std::runtime_error& err)
				{
					cout << err.what() << endl;
				}
			}
			else if (tokens[0] == "position")
			{
				if (tokens.size() == 1)
					cout << "Error: missing position filepath" << endl;
				else
					loadPosition(tokens[1]);
			}
			else if (tokens[0] == "game")
			{
				if (tokens.size() == 1)
					cout << "Error: missing game filepath" << endl;
				else
					loadGame(tokens[1]);
			}
			else if (tokens[0] == "start")
			{
				if (tokens.size() == 1)
				{
					cout << "Warning: missing user side, assuming white" << endl;
					userSide = WHITE;
				}
				else if (tokens[1] == "w" || tokens[1] == "white")
				{
					cout << "Starting game with user as white..." << endl;
					userSide = WHITE;
				}
				else if (tokens[1] == "b" || tokens[1] == "black")
				{
					cout << "Starting game with user as black..." << endl;
					userSide = BLACK;
				}
				else
				{
					cout << "Error: can't understand user side" << endl;
					return true;
				}
				inGame = true;
				if (userSide == BLACK)
					startSearch();
			}
			else
				cout << "Error: Unrecognized command " << tokens[0] << ", please try again." << endl;
		}
	}
	else if (e.source == EventSource::ENGINE)
	{
		SearchEvent se = get<SearchEvent>(e.eventInfo);
		SearchResults& res = se.results.first;
		if (se.type == SearchEventType::FINISHED)
			endSearch(); // Search is already stopped, so this will use 'lastSearchReturn'
		else if (se.type == SearchEventType::INFO)
			cout << "Depth " << (int)res.resDepth << ": " << res.bestMove.toAN()
			     << ", the score is " << res.score << "." << endl;
		else
			cout << "Error: Unrecognized search event type " << (int)(se.type) << endl;
	}
	else
		cout << "Error: Unrecognized event source" << (int)(e.source) << endl;
	return true;
}

//============================================================
// Main function
//============================================================
int main(void)
{
	Game::initialize();
	game.reset(); // Because during construction engine was not initialized
	game.setSearchProcesser(el.getEngineProcesser());
	showGreeting();
	Event e;
	do
		e = el.next();
	while (processEvent(e));
	return 0;
}
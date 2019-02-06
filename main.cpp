//============================================================
// Standalone console interface to the engine
// BlendXChess
//============================================================

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include "engine/engine.h"
#include "event.h"

using namespace std;

Game game;
EventLoop el;
bool inGame = false;
Side userSide;
MoveFormat moveFmt = FMT_AN;
TimePoint startTime, endTime;

//============================================================
// Struct for holding various info for search options
//============================================================

struct Option
{
	Option(int minValue, int maxValue, int defaultValue)
		: minValue(minValue), maxValue(maxValue),
		defaultValue(defaultValue), value(defaultValue)
	{}
	int minValue;
	int maxValue;
	int defaultValue;
	int value;
};

// Map of available options
std::unordered_map<string, Option> options = {
	{"depth", Option(1, 20, 10)},
	{"threadCount", Option(1, 16, 4)},
	{"timeLimit", Option(1, 100000, 5000)}
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
// Show help info
//============================================================
void showHelp(void)
{
	
}

//============================================================
// Show current search options
//============================================================
void showOptions(void)
{
	cout << "Depth: " << options["depth"].value << endl;
	cout << "Thread count: " << options["threadCount"].value << endl;
	cout << "Time limit: " << options["timeLimit"].value << "ms" << endl;
}

//============================================================
// Set search option
//============================================================
void setOption(const string& opt, const string& val)
{
	auto optIt = options.find(opt);
	if (optIt == options.end())
	{
		cout << "Error: Could not find option " << opt << endl;
		return;
	}
	int value;
	try
	{
		value = std::stoi(val);
	}
	catch (const invalid_argument& ex)
	{
		cout << "Error: value for " << opt << " is not an integer";
		return;
	}
	catch (const out_of_range& ex)
	{
		cout << "Error: value for " << opt << " is out of integer range";
		return;
	}
	Option& option = optIt->second;
	if (value < option.minValue)
	{
		option.value = option.minValue;
		cout << "Warning: value for" << opt << " is too small, so it's set to minimum value "
			<< option.minValue << endl;
	}
	else if (value > option.maxValue)
	{
		option.value = option.maxValue;
		cout << "Warning: value for" << opt << " is too big, so it's set to maximum value "
			<< option.maxValue << endl;
	}
	else
	{
		option.value = value;
		cout << "Option " << opt << " successfully set to " << value << endl;
	}
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
			<< " ms to depth " << results.resDepth << ". The score is " << results.score << ". "
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
	if (game.getGameState() != GS_ACTIVE)
	{
		if (game.getGameState() == GS_DRAW)
		{
			const DrawCause drawCause = game.getDrawCause();
			cout << "Draw! Cause: " << game.getDrawCause() << endl;
			switch (drawCause)
			{
			case DC_RULE_50: cout << "Rule 50"; break;
			case DC_MATERIAL: cout << "Insufficient material"; break;
			case DC_THREEFOLD_REPETITION: cout << "Threefold repetition"; break;
			default: cout << "Unknown"; break;
			}
		}
		else if (game.getGameState() == GS_WHITE_WIN)
			cout << "Checkmate! White win" << endl;
		else if (game.getGameState() == GS_BLACK_WIN)
			cout << "Checkmate! Black win" << endl;
		std::ofstream gameLog("lastGame.txt");
		game.writeGame(gameLog);
		gameLog.close();
		inGame = false;
	}
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
		if (inGame)
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
				game.reset(); // Maybe unnecessary (but at least this ends search)?
				inGame = false;
			}
			else if (tokens[0] == "staticEvaluate")
				;
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
			else
				cout << "Error: Unrecognized command " << tokens[0] << ", please try again." << endl;
		}
		else
		{
			if (tokens[0] == "help")
				showHelp();
			else if (tokens[0] == "showOptions")
				showOptions();
			else if (tokens[0] == "set")
			{
				if (tokens.size() == 1)
					cout << "Error: set what?" << endl;
				else if (tokens.size() == 2)
					cout << "Error: set " << tokens[1] << " to what value?" << endl;
				else
					setOption(tokens[1], tokens[2]);
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
					cout << "Starting game with user as white...";
					userSide = WHITE;
				}
				else if (tokens[1] == "b" || tokens[1] == "black")
				{
					cout << "Starting game with user as black...";
					userSide = BLACK;
				}
				else
				{
					cout << "Error: can't understand user side" << endl;
					return true;
				}
				inGame = true;
				game.reset();
				if (userSide == BLACK)
					startSearch();
			}
			else if (tokens[0] == "quit")
				return false;
			else
				cout << "Error: Unrecognized command " << tokens[0] << ", please try again." << endl;
		}
	}
	else if (e.source == EventSource::ENGINE)
	{
		SearchEvent se = get<SearchEvent>(e.eventInfo);
		if (se.type == SearchEventType::FINISHED) // still need to call endSearch
			endSearch();
		else if (se.type == SearchEventType::INFO)
		{
			cout << "Depth " << se.results.resDepth << ": " << se.results.bestMove.toAN()
				<< ", the score is " << se.results.score << "." << endl;
		}
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
int main(/*int argc, char **argv*/)
{
	Game::initialize();
	game.setSearchProcesser(el.getEngineProcesser());
	searchOptions = game.getSearchOptions();
	Event e;
	char userWhite, startMode;
	Side userTurn;
	Depth resDepth, depth;
	int nodes, timeLimit, inDepth, ttHits, perftDepth;
	Move cpuMove;
	SearchOptions searchOptions;
	string strMove, path, perft;
	game.reset();
#ifndef ENGINE_DEBUG
	for (int i = 1; i <= 5; ++i)
	{
		auto st = chrono::high_resolution_clock::now();
		cout << game.perft(i); // perft<true>
		auto en = chrono::high_resolution_clock::now();
		cout << " Perft("<< i << ") on initial position is " << chrono::duration_cast<
			chrono::milliseconds>(en - st).count() << "ms\n";
	}
#endif
	do
		e = el.next();
	while (!processEvent(e));
		cout << "Do you want to quit ('Q'/'q'), load game ('G'/'g') or "
			"position ('P'/'p') or start from initial position (everything else)?: ";
		cin >> startMode;
		startMode = tolower(startMode);
		if (startMode == 'q')
			break;
		else if (tolower(startMode) == 'p')
		{
			cout << "Enter position filepath: ";
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
			getline(cin, path);
			ifstream in(path);
			try
			{
				game.loadFEN(in, true);
			}
			catch (const runtime_error& err)
			{
				cerr << err.what() << endl;
				return 0;
			}
			in.close();
			cout << "Perft (ignoring timelimit)? (number for depth, other for no): ";
			getline(cin, perft);
			bool doPerft = true;
			try
			{
				perftDepth = std::stoi(perft);
			}
			catch (const invalid_argument&)
			{
				doPerft = false;
			}
			if (doPerft)
			{
				for (int i = 1; i <= perftDepth; ++i)
				{
					auto st = chrono::high_resolution_clock::now();
					cout << game.perft<true>(i);
					auto en = chrono::high_resolution_clock::now();
					cout << " Perft(" << i << ") on given is " << chrono::duration_cast<
						chrono::milliseconds>(en - st).count() << "ms\n";
				}
				return 0;
			}
		}
		else if (tolower(startMode) == 'g')
		{
			cout << "Enter game filepath: ";
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
			getline(cin, path);
			ifstream in(path);
			while (getline(in, strMove))
				if (!game.DoMove(strMove, FMT_AN))
				{
					cout << "Wrong move " << strMove << " in input.txt" << endl;
					return 0;
				}
			in.close();
		}
		searchOptions.threadCount = game.getMaxThreadCount();
		cout << "Set timelimit please (in ms): ";
		cin >> searchOptions.timeLimit;
		cout << "Set search depth please: ";
		cin >> searchOptions.depth;
		game.setSearchOptions(searchOptions);
		cout << "User is white? (Y/y - yes, otherwise - no): ";
		cin >> userWhite;
		if (tolower(userWhite) == 'y')
			userTurn = WHITE;
		else
			userTurn = BLACK;
		while (true)
		{
			if (game.getPosition().getTurn() == userTurn)
			{
				do
				{
					cout << "Enter your move please (Re - restart game): ";
					cin >> strMove;
				} while (!(strMove == "Re" || game.DoMove(strMove, FMT_AN)));
				if (strMove == "Re")
				{
					cout << "Restarting..." << endl;
					game.reset();
				}
			}
			else
			{
				auto st = chrono::high_resolution_clock::now();
				const Score score = game.AIMove(cpuMove, depth, resDepth, nodes, ttHits);
				auto en = chrono::high_resolution_clock::now();
				game.DoMove(cpuMove);
				cout << cpuMove.toAN();
				cout << ' ' << nodes << " nodes searched in "
					<< chrono::duration_cast<chrono::milliseconds>(en - st).count()
					<< " ms to depth " << (int)resDepth << ". The score is " << score << ". "
					<< ttFreeEntries << " free slots in TT. " << ttHits << " hits made." << endl;
			}
			if (game.getGameState() != GS_ACTIVE)
			{
				if (game.getGameState() == GS_DRAW)
				{
					const DrawCause drawCause = game.getDrawCause();
					cout << "Draw! Cause: " << game.getDrawCause() << endl;
					switch (drawCause)
					{
					case DC_RULE_50: cout << "Rule 50"; break;
					case DC_MATERIAL: cout << "Insufficient material"; break;
					case DC_THREEFOLD_REPETITION: cout << "Threefold repetition"; break;
					default: cout << "Unknown"; break;
					}
				}
				else if (game.getGameState() == GS_WHITE_WIN)
					cout << "Checkmate! White win" << endl;
				else if (game.getGameState() == GS_BLACK_WIN)
					cout << "Checkmate! Black win" << endl;
				std::ofstream gameLog("lastGame.txt");
				game.writeGame(gameLog);
				gameLog.close();
				break;
			}
		}
	return 0;
}
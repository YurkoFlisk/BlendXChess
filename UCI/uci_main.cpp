//============================================================
// UCI console interface for the engine
// BlendXChess
//============================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include "../engine/engine.h"

using namespace std;
using namespace BlendXChess;

//============================================================
// General engine info
//============================================================

constexpr char LOG_FILE[] = "BlendXErrors.log";
constexpr char ENGINE_NAME[] = "BlendX";
constexpr char AUTHOR[] = "Yurko Prokopets";
constexpr char VERSION[] = "0.1";

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
// Logs errors and unrecognized input
//============================================================
void errorLog(const string& what)
{
	ofstream logFS(LOG_FILE, ios::app);
	logFS << what << endl;
}

// Searcher is here because of huge transposition table that it contains
MultiSearcher searcher;

//============================================================
// Main function
//============================================================
int main(int argc, char **argv)
{
	// Initialization of basic engine-related stuff
	Game::initialize();
	Position pos;
	SearchReturn results;
	SearchOptions options = DEFAULT_SEARCH_OPTIONS;
	// Default options
	// Main UCI loop
	string input, command, fen;
	bool loop = true;
	while (loop && getline(cin, input))
	{
		try
		{
			// Input should be case-insensitive
			transform(input.begin(), input.end(), input.begin(), tolower);
			vector<string> tokens = tokenize(input);
			command = tokens[0];
			if (command == "uci")
			{
				cout << "id name " << ENGINE_NAME << ' ' << VERSION << endl;
				cout << "id author " << AUTHOR << endl;
				cout << "option name TimeLimit type spin default " << TIME_LIMIT_DEFAULT
				     << " min " << TIME_LIMIT_MIN << " max " << TIME_LIMIT_MAX << endl;
				cout << "option name ThreadCount type spin default " << searcher.getMaxThreadCount()
				     << " min 1 max " << searcher.getMaxThreadCount();
				cout << "option name SearchDepth type spin default " << SEARCH_DEPTH_DEFAULT
				     << " min " << SEARCH_DEPTH_MIN << " max " << SEARCH_DEPTH_MAX;
				cout << "uciok" << endl;
			}
			else if (command == "isready")
				cout << "readyok" << endl;
			else if (command == "setoption")
			{
				// TEMPORARY ASSUME option name doesn't contain
				// spaces (so current token separation is correct)
				if (tokens.size() <= 2 || tokens[1] != "name") // <=
					errorLog("Warning: missing option name for 'setoption'");
				else if (tokens.size() == 3)
					tokens[2];
				else if (tokens.size() < 5 || tokens[3] != "value")
				{
					tokens[2] tokens[4];
				}
			}
			else if (command == "ucinewgame")
			{
				pos.reset();
			}
			else if (command == "position")
			{
				if (tokens.size() < 2)
					errorLog("Warning: 'position' command without argument");
				else if (tokens[1] == "startpos")
					pos.reset();
				else if (tokens[1] == "fen")
				{
					const bool omitCounters = (tokens.size() < 7 || tokens.at(6) == "moves");
					const auto fenTokenEnd = tokens.begin() + (omitCounters ? 6 : 8);
					fen = accumulate(tokens.begin() + 3, fenTokenEnd, tokens[2],
						[](const string& s1, const string& s2) {return s1 + " " + s2; });
					pos.loadFEN(fen, omitCounters);
					if (fenTokenEnd != tokens.end() && *fenTokenEnd == "moves")
						for (auto moveStrIt = next(fenTokenEnd); moveStrIt != tokens.end(); ++moveStrIt)
							pos.DoMove(*moveStrIt, FMT_UCI);
				}
				else
					errorLog("Warning: wrong UCI position, ignored. 'input' = '" + input + "'");
			}
			else if (command == "go")
			{
				searcher.startSearch(pos, options);
			}
			else if (command == "stop")
			{
				auto[results, stats] = searcher.endSearch();
				cout << "bestmove " << pos.moveToStr(results.bestMove, FMT_UCI) << endl;
			}
			else if (command == "ponderhit")
			{
				// EMPTY NOW
			}
			else if (command == "quit")
			{
				loop = false;
			}
			else // By UCI specification, any unrecognized input should be ignored
				errorLog("Warning: unrecognized UCI input, ignored. 'input' = '" + input + "'");
		}
		catch (const exception& err)
		{
			errorLog(string("Error: ") + err.what());
		}
	}
	return 0;
}
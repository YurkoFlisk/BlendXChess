//============================================================
// UCI console interface for the engine
// BlendXChess
//============================================================

#include "../engine/engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>

using namespace std;

constexpr char LOG_FILE[] = "BlendXErrors.log";
constexpr char ENGINE_NAME[] = "BlendX";
constexpr char AUTHOR[] = "Yurko Prokopets";
constexpr int VERSION = 0.1;

Engine eng;

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
	logFS << what;
}

//============================================================
// Main function
//============================================================
int main(int argc, char **argv)
{
	eng.initialize();
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

				cout << "uciok" << endl;
			}
			else if (command == "isready")
				cout << "readyok" << endl;
			else if (command == "setoption")
			{

			}
			else if (command == "ucinewgame")
			{
				eng.reset();
			}
			else if (command == "position")
			{
				if (tokens[1] == "startpos")
					eng.reset();
				else if (tokens[1] == "fen")
				{
					const bool omitCounters = (tokens.size() < 7 || tokens.at(6) == "moves");
					const auto fenTokenEnd = tokens.begin() + (omitCounters ? 6 : 8);
					fen = accumulate(tokens.begin() + 3, fenTokenEnd, tokens[2],
						[](const string& s1, const string& s2) {return s1 + " " + s2; });
					eng.loadPosition(fen, omitCounters);
					if (fenTokenEnd != tokens.end() && *fenTokenEnd == "moves")
						for (auto moveStrIt = next(fenTokenEnd); moveStrIt != tokens.end(); ++moveStrIt)
							eng.DoMove(eng.moveFromUCI(*moveStrIt));
				}
				else
					errorLog("Warning: wrong UCI position, ignored. 'input' = '" + input + "'");
			}
			else if (command == "go")
			{

			}
			else if (command == "stop")
			{

			}
			else if (command == "ponderhit")
			{

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
			errorLog(string("Error: ") + err.what);
		}
	}
	return 0;
}
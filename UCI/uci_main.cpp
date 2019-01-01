//============================================================
// UCI console interface for the engine
// BlendXChess
//============================================================

#include "../engine/engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

constexpr char ENGINE_NAME[] = "BlendX";
constexpr char AUTHOR[] = "Yuriy Prokopets";
constexpr int VERSION = 0.1;

Engine eng;

vector<string> tokenize(string input)
{
	string token;
	vector<string> tokens;
	istringstream iss(input);
	while (iss >> token)
		tokens.push_back(token);
	return tokens;
}

int main(int argc, char **argv)
{
	eng.initialize();
	string input, command;
	while (getline(cin, input))
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

		}
		else
			; // By UCI specification, this should be ignored
	}
	return 0;
}
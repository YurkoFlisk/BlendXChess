//============================================================
// UCI console interface for the engine
// BlendXChess
//============================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <variant>
#include "../engine/engine.h"

using namespace std;
using namespace BlendXChess;

//============================================================
// General engine info
//============================================================

constexpr char DUMP_FILE[] = "BlendXDump.log";
constexpr char LOG_FILE[] = "BlendXErrors.log";
constexpr char ENGINE_NAME[] = "BlendX";
constexpr char AUTHOR[] = "Yurko Prokopets";
constexpr char VERSION[] = "0.1";
constexpr bool UCI_IO_DUMP = true;

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
	static bool firstCall = true;
	ofstream logFS(LOG_FILE, ios::app);
	if (firstCall) // Separate output of different launches
	{
		logFS << "------------------------------------------\n";
		firstCall = false;
	}
	logFS << what << endl;
}

//============================================================
// Stream structs for communication with UCI
// Support dump of I/O operations
//============================================================

class Dumper
{
public:
	Dumper(void)
	{
		if constexpr (UCI_IO_DUMP)
		{
			dumpFile.open(DUMP_FILE, ios::app);
			dumpFile << "----------------------------------------\n";
		}
	}
	~Dumper(void)
	{
		if constexpr (UCI_IO_DUMP)
			dumpFile << "\n----------------------------------------\n";
	}
	template<typename T>
	void dumpIn(T&& arg)
	{
		if constexpr (!UCI_IO_DUMP)
			return;
		beginInput();
		dumpFile << forward<T>(arg) << endl; // ! input is expected as whole-line !
	}
	template<typename T>
	void dumpOut(T&& arg)
	{
		if constexpr (!UCI_IO_DUMP)
			return;
		beginOutput();
		dumpFile << forward<T>(arg); // No space-separation needed
	}
	friend struct UCIOStream& uci_endl(struct UCIOStream&);
private:
	void beginInput(void)
	{
		if (dumpingState != DumpingState::IN)
		{
			dumpFile << endl << "Input:\n";
			dumpingState = DumpingState::IN;
		}
	}
	void beginOutput(void)
	{
		if (dumpingState != DumpingState::OUT)
		{
			dumpFile << endl << "Output:\n";
			dumpingState = DumpingState::OUT;
		}
	}
	enum class DumpingState
	{
		NONE, IN, OUT
	};
	DumpingState dumpingState;
	std::ofstream dumpFile;
};

// Shared (input/output) dumper object
Dumper dumper;

struct UCIOStream
{
	template<typename T>
	UCIOStream& operator<<(T&& arg)
	{
		cout << arg;
		if constexpr (UCI_IO_DUMP)
			dumper.dumpOut(arg);
		return *this;
	}
	UCIOStream& operator<<(UCIOStream&(*arg)(UCIOStream&))
	{
		return arg(*this);
	}
};

// End line manipulator
UCIOStream& uci_endl(UCIOStream& ostr)
{
	cout << endl;
	if constexpr (UCI_IO_DUMP)
	{
		dumper.beginOutput();
		dumper.dumpFile << endl;
	}
	return ostr;
}

// Stream for UCI output
UCIOStream uci_out;
// Access to the I/O streams should be thread-safe (in a sense of exclusive access
// for one LOGICAL command, thus it's not used in UCI(I/O)Stream)
mutex ioMutex;

//============================================================
// Informs UCI about engine events (is set as processer to searcher)
//============================================================
void UCIInformer(const SearchEvent& event)
{
	lock_guard lock(ioMutex);
	const auto& [res, stats] = event.results;
	switch (event.type)
	{
	case SearchEventType::FINISHED:
		uci_out << "bestmove " << res.bestMove.toUCI() << uci_endl;
		break;
	case SearchEventType::INFO:
		uci_out << "info depth " << (int)res.resDepth << " score cp " << res.score
		     << " pv " << res.bestMove.toUCI() << " nodes " << stats.visitedNodes << uci_endl;
		break;
	default:
		errorLog("Unrecognized event type " + to_string((int)event.type));
	}
}

//============================================================
// Main function
//============================================================
int main(int argc, char **argv)
{
	// Initialization of basic engine-related stuff
	Game::initialize();
	Game game;
	game.setSearchProcesser(UCIInformer);
	SearchReturn results;
	string input, command, fen;
	bool loop = true, firstCommand = true;
	// Main UCI loop
	while (loop && getline(cin, input))
	{
		//if (!(firstCommand && !input.empty()) && !getline(cin, input))
		//	break;
		dumper.dumpIn(input);
		try
		{
			vector<string> tokens = tokenize(input);
			command = tokens[0];
			lock_guard lock(ioMutex);
			if (command == "uci")
			{
				uci_out << "id name " << ENGINE_NAME << ' ' << VERSION << uci_endl;
				uci_out << "id author " << AUTHOR << uci_endl;
				uci_out << "option name TimeLimit type spin default " << TIME_LIMIT_DEFAULT
				     << " min " << TIME_LIMIT_MIN << " max " << TIME_LIMIT_MAX << uci_endl;
				uci_out << "option name ThreadCount type spin default " << game.getMaxThreadCount()
				     << " min 1 max " << game.getMaxThreadCount() << uci_endl;
				uci_out << "option name SearchDepth type spin default " << (int)SEARCH_DEPTH_DEFAULT
				     << " min " << (int)SEARCH_DEPTH_MIN << " max " << (int)SEARCH_DEPTH_MAX << uci_endl;
				uci_out << "uciok" << uci_endl;
			}
			else if (command == "isready")
				uci_out << "readyok" << uci_endl;
			else if (command == "setoption")
			{
				// TEMPORARY ASSUME option name and value don't contain
				// spaces (so current token separation is correct)
				if (tokens.size() <= 2 || tokens[1] != "name") // <=
					errorLog("Warning: missing option name for 'setoption'");
				else if (tokens.size() == 3)
					game.setOption(tokens[2]);
				else if (tokens.size() <= 5 || tokens[3] != "value")
					game.setOption(tokens[2], tokens[4]);
				else
					errorLog("Warning: unrecognized setOption format");
			}
			else if (command == "ucinewgame")
			{
				game.reset();
			}
			else if (command == "position")
			{
				if (tokens.size() < 2)
					errorLog("Warning: 'position' command without argument");
				else if (tokens[1] == "startpos")
				{
					game.reset();
					if (tokens.size() > 2 && tokens[2] == "moves")
						for (int i = 3; i < tokens.size(); ++i)
							if (!game.DoMove(tokens[i], FMT_UCI))
							{
								errorLog("Error: wrong input move '" + tokens[i]
									+ "', ignored with all next ones");
								break;
							}
				}
				else if (tokens[1] == "fen")
				{
					const bool omitCounters = (tokens.size() < 7 || tokens.at(6) == "moves");
					const auto fenTokenEnd = tokens.begin() + (omitCounters ? 6 : 8);
					fen = accumulate(tokens.begin() + 3, fenTokenEnd, tokens[2],
						[](const string& s1, const string& s2) {return s1 + " " + s2; });
					game.loadFEN(fen, omitCounters);
					if (fenTokenEnd != tokens.end() && *fenTokenEnd == "moves")
						for (auto moveStrIt = next(fenTokenEnd); moveStrIt != tokens.end(); ++moveStrIt)
							game.DoMove(*moveStrIt, FMT_UCI);
				}
				else
					errorLog("Warning: wrong UCI position, ignored. 'input' = '" + input + "'");
			}
			else if (command == "go")
			{
				game.startSearch();
			}
			else if (command == "stop")
			{
				if (!game.isInSearch())
				{
					errorLog("Warning: received 'stop' command while not in search, ignored");
					continue;
				}
				auto[results, stats] = game.endSearch();
				uci_out << "bestmove " << results.bestMove.toUCI() << uci_endl;
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
		catch (const runtime_error& err)
		{
			errorLog(string("Error: ") + err.what());
		}
		// firstCommand = false;
	}
	return 0;
}
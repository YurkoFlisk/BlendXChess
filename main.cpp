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

//============================================================
// Main function
//============================================================
int main(/*int argc, char **argv*/)
{
	Game::initialize();
	game.setSearchProcesser(el.getEngineProcesser());
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
	while (true)
	{
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
	}
	return 0;
}
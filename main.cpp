//============================================================
// Standalone console interface to the engine
// BlendXChess
//============================================================

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "engine/engine.h"

using namespace std;
Engine pos;

//============================================================
// Main function
//============================================================
int main(/*int argc, char **argv*/)
{
	Engine::initialize();
	char userWhite, startMode;
	Side userTurn;
	Depth resDepth, depth;
	int nodes, timeLimit, inDepth, ttHits, perftDepth;
	Move cpuMove;
	string strMove, path, perft;
	pos.reset();
#ifndef ENGINE_DEBUG
	for (int i = 1; i <= 5; ++i)
	{
		auto st = chrono::high_resolution_clock::now();
		cout << pos.perft(i); // perft<true>
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
				pos.loadPosition(in, true);
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
					cout << pos.perft<true>(i);
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
				if (!pos.DoMove(strMove))
				{
					cout << "Wrong move " << strMove << " in input.txt" << endl;
					return 0;
				}
			in.close();
		}
		cout << "Set timelimit please (in ms): ";
		cin >> timeLimit;
		cout << "Set search depth please: ";
		cin >> inDepth;
		cout << "User is white? (Y/y - yes, otherwise - no): ";
		cin >> userWhite;
		if (tolower(userWhite) == 'y')
			userTurn = WHITE;
		else
			userTurn = BLACK;
		depth = inDepth;
		pos.setTimeLimit(timeLimit);
		while (true)
		{
			if (pos.getTurn() == userTurn)
			{
				do
				{
					cout << "Enter your move please (Re - restart game): ";
					cin >> strMove;
				} while (!(strMove == "Re" || pos.DoMove(strMove)));
				if (strMove == "Re")
				{
					cout << "Restarting..." << endl;
					pos.reset();
				}
			}
			else
			{
				auto st = chrono::high_resolution_clock::now();
				const Score score = pos.AIMove(cpuMove, depth, resDepth, nodes, ttHits);
				auto en = chrono::high_resolution_clock::now();
				pos.DoMove(cpuMove);
				cout << cpuMove.toAN();
				cout << ' ' << nodes << " nodes searched in "
					<< chrono::duration_cast<chrono::milliseconds>(en - st).count()
					<< " ms to depth " << (int)resDepth << ". The score is " << score << ". "
					<< ttFreeEntries << " free slots in TT. " << ttHits << " hits made." << endl;
			}
			if (pos.getGameState() != GS_ACTIVE)
			{
				if (pos.getGameState() == GS_DRAW)
				{
					const DrawCause drawCause = pos.getDrawCause();
					cout << "Draw! Cause: " << pos.getDrawCause() << endl;
					switch (drawCause)
					{
					case DC_RULE_50: cout << "Rule 50"; break;
					case DC_MATERIAL: cout << "Insufficient material"; break;
					case DC_THREEFOLD_REPETITION: cout << "Threefold repetition"; break;
					default: cout << "Unknown"; break;
					}
				}
				else if (pos.getGameState() == GS_WHITE_WIN)
					cout << "Checkmate! White win" << endl;
				else if (pos.getGameState() == GS_BLACK_WIN)
					cout << "Checkmate! Black win" << endl;
				std::ofstream gameLog("lastGame.txt");
				pos.writeGame(gameLog);
				gameLog.close();
				break;
			}
		}
	}
	return 0;
}
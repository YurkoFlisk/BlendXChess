#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "engine/engine.h"

using namespace std;
Engine eng;

int main(void)
{
	Engine::initialize();
	/* for (int i = 0; i < 8; ++i)
		cout << ZobristEP[i] << endl;
	cout << endl;
	for (Color c = WHITE; c <= BLACK; ++c)
		for (PieceType pt = PAWN; pt <= KING; ++pt)
			for (Square sq = Sq::A1; sq <= Sq::H8; ++sq)
				cout << ZobristPSQ[c][pt][sq] << endl; */
	char userWhite, startMode;
	Side userTurn;
	Depth resDepth, depth;
	int nodes, timeLimit, inDepth, ttHits, perftDepth;
	Move cpuMove;
	string strMove, path, perft;
	eng.reset();
#ifndef _DEBUG
	for (int i = 1; i <= 5; ++i)
	{
		auto st = chrono::high_resolution_clock::now();
		cout << eng.perft(i);
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
				eng.loadPosition(in, true);
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
					cout << eng.perft<true>(i);
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
				if (!eng.DoMove(strMove))
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
		eng.setTimeLimit(timeLimit);
		while (true)
		{
			if (eng.getTurn() == userTurn)
				do
				{
					cout << "Enter your move please: ";
					cin >> strMove;
				} while (!eng.DoMove(strMove));
			else
			{
				auto st = chrono::high_resolution_clock::now();
				const Score score = eng.AIMove(cpuMove, depth, resDepth, nodes, ttHits);
				auto en = chrono::high_resolution_clock::now();
				eng.DoMove(cpuMove);
				cout << cpuMove.toAN();
				cout << ' ' << nodes << " nodes searched in "
					<< chrono::duration_cast<chrono::milliseconds>(en - st).count()
					<< " ms to depth " << (int)resDepth << ". The score is " << score << ". "
					<< fr << " free slots in TT. " << ttHits << " hits made." << endl;
			}
			if (eng.getGameState() != GS_ACTIVE)
			{
				if (eng.getGameState() == GS_DRAW)
				{
					const DrawCause drawCause = eng.getDrawCause();
					cout << "Draw! Cause: " << eng.getDrawCause() << endl;
					switch (drawCause)
					{
					case DC_RULE_50: cout << "Rule 50"; break;
					case DC_MATERIAL: cout << "Insufficient material"; break;
					case DC_THREEFOLD_REPETITION: cout << "Threefold repetition"; break;
					default: cout << "Unknown"; break;
					}
				}
				else if (eng.getGameState() == GS_WHITE_WIN)
					cout << "Checkmate! White win" << endl;
				else if (eng.getGameState() == GS_BLACK_WIN)
					cout << "Checkmate! Black win" << endl;
				std::ofstream gameLog("lastGame.txt");
				eng.writeGame(gameLog);
				gameLog.close();
				break;
			}
		}
	}
	return 0;
}
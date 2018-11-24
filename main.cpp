#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "engine.h"

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
	char userWhite;
	Side userTurn;
	Depth resDepth, depth;
	int nodes, timeLimit, inDepth, ttHits;
	Move cpuMove;
	string strMove;
	eng.reset();
	for (int i = 5; i <= 5; ++i)
	{
		auto st = chrono::high_resolution_clock::now();
		cout << eng.perft(i);
		auto en = chrono::high_resolution_clock::now();
		cout << " Perft("<< i << ") is " << chrono::duration_cast<
			chrono::milliseconds>(en - st).count() << "ms\n";
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
	ifstream in("input.txt");
	while (getline(in, strMove))
		if (!eng.DoMove(strMove))
		{
			cout << "Wrong move " << strMove << " in input.txt" << endl;
			return 0;
		}
	in.close();
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
			if (cpuMove.type() == MT_CASTLING)
				cout << (cpuMove.to().file() == fileFromAN('g') ? "O-O" : "O-O-O");
			else
			{
				cout << cpuMove.from().toAN() << '-' << cpuMove.to().toAN();
				if (cpuMove.type() == MT_PROMOTION)
					switch (cpuMove.promotion())
					{
					case KNIGHT: cout << 'N'; break;
					case QUEEN: cout << 'Q'; break;
					}
			}
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
	return 0;
}
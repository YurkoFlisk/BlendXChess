//============================================================
// ChessEngine
// evaluate.h
//============================================================

#pragma once
#ifndef _EVALUATE_H
#define _EVALUATE_H
#include "basic_types.h"

namespace BlendXChess
{

	//============================================================
	// Global variables
	//============================================================

	// Piece weights
	extern Score ptWeight[PIECETYPE_CNT];
	// Piece-square table of bonuses for each piece(white) on corresponding square
	extern Score psqBonus[PIECETYPE_CNT][SQUARE_CNT];
	// Piece-square table for 2 sides with weights of corresponding pieces added
	extern Score psqTable[COLOR_CNT][PIECETYPE_CNT][SQUARE_CNT];

	//============================================================
	// Functions
	//============================================================

	// Initialization of piece-square table
	void initPSQ(void);

};

#endif
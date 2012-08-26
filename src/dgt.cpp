/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2012 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2012 Jean-Francois Romang

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>

#include "evaluate.h"
#include "notation.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"
#include "dgtnix.h"
#include "movegen.h"

using namespace std;

namespace DGT
{

Search::LimitsType limits, resetLimits;
enum Side { WHITE, BLACK, ANALYSIS } computerPlays;
vector<Move> game;
const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; // FEN string of the initial position, normal chess


/// Give the current board setup as FEN string
/// char  :  tomove = 'w' or 'b' : the side to move (white is default)
string getDgtFEN(char tomove = 'w')
{
	const char *board = dgtnixGetBoard();
	char  FEN[90];
	int   pos = 0;
	int   empty = 0;

	for (int sq = 0; sq < 64; sq++)
	{
		if (board[sq] != 32) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			FEN[pos] = char(board[sq]);
			pos++;
		}
		else empty++;
		if ((sq+1) % 8 == 0) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			if (sq < 63) {
				FEN[pos] = '/';
				pos++;
			}
			empty = 0;
		}
	}

	// FEN data fields
	FEN[pos++] = ' '; FEN[pos++] = tomove; // side to move
	FEN[pos++] = ' ';
	// possible castelings
	FEN[pos++] = 'K'; FEN[pos++] = 'Q';
	FEN[pos++] = 'k'; FEN[pos++] = 'q';
	FEN[pos++] = ' '; FEN[pos++] = '-';
	FEN[pos++] = ' '; FEN[pos++] = '0';
	FEN[pos++] = ' '; FEN[pos++] = '1';

	// Mark the end of the string
	FEN[pos] = char(0);

    return string(FEN);
}

/// Change UCI parameters with special positions on the board
void configure(const string& fen)
{
	//if(fen=="8/8/8/8/8/8/8/PP6 w KQkq - 0 1")  { dgtnixPrintMessageOnClock("config", 1); }

	//set skill level
	if(fen=="rnbqkbnr/pppppppp/q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 00", 1); Options[string("Skill Level")] = string("0"); }
	if(fen=="rnbqkbnr/pppppppp/1q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 01", 1); Options[string("Skill Level")] = string("1"); }
	if(fen=="rnbqkbnr/pppppppp/2q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 02", 1); Options[string("Skill Level")] = string("2"); }
	if(fen=="rnbqkbnr/pppppppp/3q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 03", 1); Options[string("Skill Level")] = string("3"); }
	if(fen=="rnbqkbnr/pppppppp/4q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 04", 1); Options[string("Skill Level")] = string("4"); }
	if(fen=="rnbqkbnr/pppppppp/5q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 05", 1); Options[string("Skill Level")] = string("5"); }
	if(fen=="rnbqkbnr/pppppppp/6q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 06", 1); Options[string("Skill Level")] = string("6"); }
	if(fen=="rnbqkbnr/pppppppp/7q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 07", 1); Options[string("Skill Level")] = string("7"); }
	if(fen=="rnbqkbnr/pppppppp/8/q7/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 08", 1); Options[string("Skill Level")] = string("8"); }
	if(fen=="rnbqkbnr/pppppppp/8/1q6/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 09", 1); Options[string("Skill Level")] = string("9"); }
	if(fen=="rnbqkbnr/pppppppp/8/2q5/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 10", 1); Options[string("Skill Level")] = string("10"); }
	if(fen=="rnbqkbnr/pppppppp/8/3q4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 11", 1); Options[string("Skill Level")] = string("11"); }
	if(fen=="rnbqkbnr/pppppppp/8/4q3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 12", 1); Options[string("Skill Level")] = string("12"); }
	if(fen=="rnbqkbnr/pppppppp/8/5q2/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 13", 1); Options[string("Skill Level")] = string("13"); }
	if(fen=="rnbqkbnr/pppppppp/8/6q1/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 14", 1); Options[string("Skill Level")] = string("14"); }
	if(fen=="rnbqkbnr/pppppppp/8/7q/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 15", 1); Options[string("Skill Level")] = string("15"); }
	if(fen=="rnbqkbnr/pppppppp/8/8/q7/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 16", 1); Options[string("Skill Level")] = string("16"); }
	if(fen=="rnbqkbnr/pppppppp/8/8/1q6/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 17", 1); Options[string("Skill Level")] = string("17"); }
	if(fen=="rnbqkbnr/pppppppp/8/8/2q5/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 18", 1); Options[string("Skill Level")] = string("18"); }
	if(fen=="rnbqkbnr/pppppppp/8/8/3q4/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 19", 1); Options[string("Skill Level")] = string("19"); }
	if(fen=="rnbqkbnr/pppppppp/8/8/4q3/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 20", 1); Options[string("Skill Level")] = string("20"); }

	//set time control
	if(fen=="rnbqkbnr/pppppppp/Q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  1", 1); limits=resetLimits; limits.movetime=1000; }
	if(fen=="rnbqkbnr/pppppppp/1Q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  5", 1); limits=resetLimits; limits.movetime=5000; }
	if(fen=="rnbqkbnr/pppppppp/2Q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 10", 1); limits=resetLimits; limits.movetime=10000; }
	if(fen=="rnbqkbnr/pppppppp/3Q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 20", 1); limits=resetLimits; limits.movetime=20000; }
	if(fen=="rnbqkbnr/pppppppp/4Q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 40", 1); limits=resetLimits; limits.movetime=40000; }
	if(fen=="rnbqkbnr/pppppppp/5Q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 1m", 1); limits=resetLimits; limits.movetime=60000; }
	if(fen=="rnbqkbnr/pppppppp/6Q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 3m", 1); limits=resetLimits; limits.movetime=180000; }
	if(fen=="rnbqkbnr/pppppppp/7Q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 5m", 1); limits=resetLimits; limits.movetime=300000; }

	//board orientation
	if(fen=="RNBKQBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbkqbnr w KQkq - 0 1") { cout << "right" << endl; dgtnixSetOption(DGTNIX_BOARD_ORIENTATION, DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT); }

	//set side to play (simply remove the king of the side you are playing and put it back on the board)
	if(fen=="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq - 0 1") { cout << "You play white"<< endl; computerPlays=BLACK; }
	if(fen=="rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { cout << "You play black"<< endl; computerPlays=WHITE; }

	//new game
	if(fen=="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
	{
		//stop the current search
		Search::Signals.stop = true;
		Threads.wait_for_search_finished();
		//reset the game
		game.clear();
		TT.clear();
	}
}

/// Test if the given fen is playable in the current game.
/// If true, return the move leading to this fen, else return MOVE_NONE
Move isPlayable(const string& _fen)
{
	Position pos(StartFEN, false, Threads.main_thread()); // The root position
	stack<StateInfo> states;
	string fen=_fen.substr(0, _fen.find(' '));

	//First, we do all the game moves
	for (vector<Move>::iterator it = game.begin(); it!=game.end(); ++it) {
		states.push(StateInfo());
	    pos.do_move(*it, states.top());
	}

	//Check is the fen is playable in current game position
	for (MoveList<LEGAL> ml(pos); !ml.end(); ++ml) {
					StateInfo state;
					pos.do_move(ml.move(), state);
					string positionFEN = pos.to_fen();
					positionFEN = positionFEN.substr(0, positionFEN.find(' '));
					//cout << "Compare " << positionFEN << " with "<<fen <<  endl;
					if(!positionFEN.compare(fen)) return ml.move();
					pos.undo_move(ml.move());
	}

	/*
	//Next we check from the end of the game to the beginning if we reached a playable position
	vector<Move>::reverse_iterator rit;
	  for ( rit=game.rbegin() ; rit < game.rend(); ++rit )
	  {
		  pos.undo_move(*rit);
	      cout << pos.to_fen() << endl;
	  }*/

	return MOVE_NONE;
}

void loop(const string& args) {
	// Initialization
	Position pos(StartFEN, false, Threads.main_thread()); // The root position
	computerPlays=BLACK;
	bool searching = false;
	limits.movetime = 5000; //search defaults to 5 seconds per move
	Move playerMove=MOVE_NONE;
	StateInfo state;

	// DGT Board Initialization
	int BoardDescriptor;
	char port[256];
	dgtnixSetOption(DGTNIX_DEBUG, DGTNIX_DEBUG_WITH_TIME); //all debug informations are printed
	strncpy(port, args.c_str(), 256);
	BoardDescriptor = dgtnixInit(port);
	int err = dgtnix_errno;
	if (BoardDescriptor < 0) {
		cout << "Unable to connect to DGT board on port " << port << " : ";
		switch (BoardDescriptor) {
		case -1:
			cout << strerror(err) << endl;
			break;
		case -2:
			cout << "Not responding to the DGT_SEND_BRD message" << endl;
			break;
		default:
			cout << "Unrecognized response to the DGT_SEND_BRD message :"
					<< BoardDescriptor << endl;
		}
		exit(-1);
	}
	cout << "The board was found" << BoardDescriptor << endl;
	dgtnixPrintMessageOnClock(" hello", 1);
	dgtnixUpdate();

	// Get the first board state
	string currentFEN = getDgtFEN();
	configure(currentFEN); //useful for orientation

	// Main DGT event loop
	while (true) {
		string s = getDgtFEN();
		if (currentFEN != s) { //There is some change on the DGT board
			currentFEN = s;

			cout << currentFEN << endl;
			configure(currentFEN); //on board configuration

			//Test if we reach a playable position in the current game
			Move move=isPlayable(currentFEN);
			cout<< "-------------------------Move:" << move <<  endl;
			if( move!=MOVE_NONE || (!currentFEN.compare(StartFEN) && computerPlays==WHITE) )
			{
				//stop the current search
				Search::Signals.stop = true;
				Threads.wait_for_search_finished();

				playerMove=move;
				pos.from_fen(StartFEN, false, Threads.main_thread()); // The root position

				//Do all the game moves
				for (vector<Move>::iterator it = game.begin(); it!=game.end(); ++it)
					pos.do_move(*it, state);
				if(move!=MOVE_NONE) pos.do_move(playerMove,state); //Do the board move

				//Launch the search
				dgtnixPrintMessageOnClock("search", 0);
				Threads.start_searching(pos, limits, vector<Move>());
				searching = true;
			}
		}

		//Check for finished search
		if (Search::Signals.stop == true && searching) {
			searching = false;
			cout << "stopped with move " << move_to_uci(Search::RootMoves[0].pv[0], false) << endl;

			//print the move on the clock
			string dgtMove = move_to_uci(Search::RootMoves[0].pv[0], false);
			dgtMove.insert(2, 1, ' ');
			if (dgtMove.length() < 6)
				dgtMove.append(" ");
			cout << '[' << dgtMove << ']' << endl;
			dgtnixPrintMessageOnClock(dgtMove.c_str(), 1);

			//do the moves in the game
			if(playerMove!=MOVE_NONE) game.push_back(playerMove);
			game.push_back(Search::RootMoves[0].pv[0]);
		}

		//sleep
		struct timespec tim, tim2;
		tim.tv_sec = 0;
		tim.tv_nsec = 30000000L;
		nanosleep(&tim, &tim2);
	}

	dgtnixClose();
}

}

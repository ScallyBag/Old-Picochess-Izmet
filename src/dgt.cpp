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
#include <iomanip>
#include <unistd.h> 

#include "evaluate.h"
#include "notation.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"
#include "dgtnix.h"
#include "movegen.h"
#include "book.h"
#include "platform.h"

using namespace std;

namespace DGT
{

Search::LimitsType limits, resetLimits;
Color computerPlays;
vector<Move> game;
//const char* StartFEN ="r1r4k/4Np1p/3R1PpP/1P2p3/p3P1K1/P6P/8/3R4 w - - 0 1";
const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; // FEN string of the initial position, normal chess
bool boardReversed=false;

/*
class Clock
{
    //time in seconds, inc in seconds
    Clock(int64_t _time=600, int _inc=0)
    { 
        wtime=btime=_time*1000;
        inc=_inc;
    }
    
    void display()
    {
        
    }
    
    int64_t wtime, btime; //Remaining time in msec
    int inc; //Increment time in seconds
};*/


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
void configure(string& fen)
{
	//set skill level
    static string skillFENs[]={
    "rnbqkbnr/pppppppp/q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/1q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/2q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/3q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/4q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/5q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/6q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/7q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/q7/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/1q6/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/2q5/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/3q4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/4q3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/5q2/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/6q1/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/7q/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/8/q7/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/8/1q6/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/8/2q5/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/8/3q4/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"rnbqkbnr/pppppppp/8/8/4q3/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };
    static vector<string> skill(skillFENs,skillFENs+21);
    
    unsigned int idx = find(skill.begin(), skill.end(), fen) - skill.begin();
    if (idx < skill.size())
    {
        stringstream ss_uci, ss_dgt;
        ss_uci << "setoption name Skill Level value " << idx;
        ss_dgt << "lvl" << setw(3) << idx;
        UCI::loop(ss_uci.str());
        dgtnixPrintMessageOnClock(ss_dgt.str().c_str(), 1);
    }

    /*
    Rank 6: 'Fixed time' or 'Medium Time' 
    1, 3, 5, 10, 15, 30, 60, 120 seconds 
    
    Rank 5: 'Tournament levels' 
    40/4 (40 moves in 4 minutes), 60/15, 60/30, 30/30, 30/60, , 40/40, 40/120, 40/150 
    
    Rank 4: 'Blitz' 
    1, 3, 5, 10, 15, 30, 60, 90  minutes
    
    Rank 3 : 'Blitz Fischer' + 'Special Lvl' 
    3+2, 3+5, 4+5, 5+1, 15+5, 20+10, opponents average, 1/2 opponents average 
    --------------------------------------------------------------------------
    I am thinking for a way to put the time control easily.
    One option is the following. If you put on the board only the white queen (i think it is the auxiliriary queen that comes with the dgt), the time control is the same for both sides. But if you put also the black queen it indicates handicap level.
    I think that the handicap level has no sense with Fixed time, that is, 6th rank.
    In the rest of levels if you put white queen on rank 3,4 or 5, so you can use the 6th rank to put the black queen indicating the time control for black.
    So the rules are 
    1) the rank of white queen indicates the type of control, the same for both if you only put the white queen.
    2) If you put the black queen (always on the 6th rank), you can put different time control for each color.
    3) the FixedTime levels has no option to specify different times for each color because this levels has no strict control.
    (Javier's idea)
    +control opponent's average rate (from 30% to 100%) with black queen on 6th rank
    */

	//set time control
	if(fen=="rnbqkbnr/pppppppp/Q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  1", 1); limits=resetLimits; limits.movetime=1000; }
	if(fen=="rnbqkbnr/pppppppp/1Q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  3", 1); limits=resetLimits; limits.movetime=3000; }
	if(fen=="rnbqkbnr/pppppppp/2Q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  5", 1); limits=resetLimits; limits.movetime=5000; }
	if(fen=="rnbqkbnr/pppppppp/3Q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 10", 1); limits=resetLimits; limits.movetime=10000; }
	if(fen=="rnbqkbnr/pppppppp/4Q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 15", 1); limits=resetLimits; limits.movetime=15000; }
	if(fen=="rnbqkbnr/pppppppp/5Q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 30", 1); limits=resetLimits; limits.movetime=30000; }
	if(fen=="rnbqkbnr/pppppppp/6Q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 60", 1); limits=resetLimits; limits.movetime=60000; }
	if(fen=="rnbqkbnr/pppppppp/7Q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov120", 1); limits=resetLimits; limits.movetime=120000; }

    //choose opening book
    typedef map<string, string> BookMap; 
    static const BookMap::value_type rawData[] = {
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/q7/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "nobook"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/1q6/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "fun"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/2q5/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "anand"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/3q4/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "korchnoi"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/4q3/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "larsen"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/5q2/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "pro"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/6q1/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "gm2001"),
       BookMap::value_type("rnbqkbnr/pppppppp/8/8/8/7q/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "varied")};
    BookMap book(rawData, rawData + 8);
    BookMap::iterator it=book.find(fen);
    if(it!=book.end())
    {
        string s=it->second;
        UCI::loop(string("setoption name Book File value /home/miniand/git/Stockfish/books/")+s+".bin");
        UCI::loop(string("setoption name OwnBook value ")+(s.compare("nobook")?"true":"false"));
        if(s.size()<6) s.insert(s.begin(), 6 - s.size(), ' ');
    	dgtnixPrintMessageOnClock(s.c_str(), 1);      
    }

	//board orientation
	if(fen=="RNBKQBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbkqbnr w KQkq - 0 1") 
    { 
        dgtnixSetOption(DGTNIX_BOARD_ORIENTATION, boardReversed?DGTNIX_BOARD_ORIENTATION_CLOCKLEFT:DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT);
        boardReversed=!boardReversed;
        fen=StartFEN; //trigger new game start
    }

	//set side to play (simply remove the king of the side you are playing and put it back on the board)
	if(fen=="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq - 0 1") { cout << "You play white"<< endl; computerPlays=BLACK; }
	if(fen=="rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") { cout << "You play black"<< endl; computerPlays=WHITE; }

	//new game
	if(fen==StartFEN)
	{
		UCI::loop("stop"); //stop the current search
		game.clear(); //reset the game
		TT.clear();
	}

	//shutdown
	if(fen=="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQQBNR w KQkq - 0 1")
	{
		UCI::loop("stop"); //stop the current search
        if(!system("shutdown -h now"))
		    dgtnixPrintMessageOnClock("pwroff", 1);	
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
		if (pos.to_fen().find(fen) != string::npos)
			return ml.move();
		pos.undo_move(ml.move());
	}

	//Next we check from the end of the game to the beginning if we reached a position already played
	//If this is the case, we takeback the moves and return MOVE_NONE
	for (vector<Move>::reverse_iterator rit=game.rbegin() ; rit < game.rend(); ++rit )
	{
		pos.undo_move(*rit);
		if((pos.to_fen().find(fen) != string::npos) && (pos.side_to_move()!=computerPlays)) //we found a position that was played
		{
			UCI::loop("stop"); //stop the current search
			cout << "Rolling back to position" << pos.to_fen() << endl;
			dgtnixPrintMessageOnClock(" undo ", 1);
			game.erase((rit+1).base(),game.end()); //delete the moves from the game
			return MOVE_NONE;
		}
	}

	return MOVE_NONE;
}

/// Prints a move on the dgt clock
void printMoveOnClock(Move move)
{
	//print the move on the clock
	string dgtMove = move_to_uci(move, false);
	dgtMove.insert(2, 1, ' ');
	if (dgtMove.length() < 6)
		dgtMove.append(" ");
	cout << '[' << dgtMove << ']' << endl;
	dgtnixPrintMessageOnClock(dgtMove.c_str(), 1);
}

void loop(const string& args) {
	// Initialization
	Position pos(StartFEN, false, Threads.main_thread()); // The root position
	computerPlays=BLACK;
	bool searching = false;
	limits.movetime = 5000; //search defaults to 5 seconds per move
	Move playerMove=MOVE_NONE;
	static PolyglotBook book; // Defined static to initialize the PRNG only once


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
	sleep(3);
    	dgtnixPrintMessageOnClock("pic003", 1); //Display version number
	dgtnixUpdate();

    //Engine options
    UCI::loop("setoption name Hash value 512");

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
				UCI::loop("stop"); //stop the current search

				playerMove=move;
				pos.from_fen(StartFEN, false, Threads.main_thread()); // The root position
                MoveList<LEGAL> ml(pos); //the legal move list

				// Keep track of position keys along the setup moves (from start position to the
				// position just before to start searching). Needed by repetition draw detection.
				Search::StateStackPtr SetupStates = Search::StateStackPtr(new std::stack<StateInfo>());;

				//Do all the game moves
				for (vector<Move>::iterator it = game.begin(); it!=game.end(); ++it)
				{
					SetupStates->push(StateInfo());
					pos.do_move(*it, SetupStates->top());
				}
				if(move!=MOVE_NONE)
				{
					SetupStates->push(StateInfo());
					pos.do_move(playerMove,SetupStates->top()); //Do the board move
				}

				//Check if we can find a move in the book
				Move bookMove = book.probe(pos, Options["Book File"], Options["Best Book Move"]);
				if(bookMove && Options["OwnBook"] && !limits.infinite)
				{
					sleep(1); //don't play immediately, wait for 1 second
					printMoveOnClock(bookMove);
					//do the moves in the game
					if(playerMove!=MOVE_NONE) game.push_back(playerMove);
					game.push_back(bookMove);
				}
                //Check if there is a single legal move
                else if(ml.size()==1)
                {
                    sleep(1); //don't play immediately, wait for 1 second
    				printMoveOnClock(ml.move());
					//do the moves in the game
					if(playerMove!=MOVE_NONE) game.push_back(playerMove);
					game.push_back(ml.move());
                }
				else if(ml.size()) //Launch the search if there are legal moves
				{
					dgtnixPrintMessageOnClock("search", 0);
					Threads.start_searching(pos, limits, vector<Move>(),SetupStates);
					searching = true;
				}
			}
		}

		//Check for finished search
		if (Search::Signals.stop == true && searching) {
			searching = false;
			cout << "stopped with move " << move_to_uci(Search::RootMoves[0].pv[0], false) << endl;
			printMoveOnClock(Search::RootMoves[0].pv[0]);
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

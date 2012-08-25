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

//--------------------------------------------------------------------
// Give the current board setup as FEN string
// char  :  tomove = 'w' or 'b' : the side to move
//
// char     : tomove         : the side to move (white is default)
//--------------------------------------------------------------------
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
	//printf("FEN %s\n", FEN);
}

//change parameters with special position on the board
void configure(const string& fen)
{
	if(fen=="8/8/8/8/8/8/8/PP6 w KQkq - 0 1")  { dgtnixPrintMessageOnClock("config", 1); }

	//set skill level
	if(fen=="p7/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 00", 1); Options[string("Skill Level")] = string("0"); }
	if(fen=="1p6/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 01", 1); Options[string("Skill Level")] = string("1"); }
	if(fen=="2p5/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 02", 1); Options[string("Skill Level")] = string("2"); }
	if(fen=="3p4/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 03", 1); Options[string("Skill Level")] = string("3"); }
	if(fen=="4p3/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 04", 1); Options[string("Skill Level")] = string("4"); }
	if(fen=="5p2/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 05", 1); Options[string("Skill Level")] = string("5"); }
	if(fen=="6p1/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 06", 1); Options[string("Skill Level")] = string("6"); }
	if(fen=="7p/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 07", 1); Options[string("Skill Level")] = string("7"); }
	if(fen=="8/p7/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 08", 1); Options[string("Skill Level")] = string("8"); }
	if(fen=="8/1p6/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 09", 1); Options[string("Skill Level")] = string("9"); }
	if(fen=="8/2p5/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 10", 1); Options[string("Skill Level")] = string("10"); }
	if(fen=="8/3p4/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 11", 1); Options[string("Skill Level")] = string("11"); }
	if(fen=="8/4p3/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 12", 1); Options[string("Skill Level")] = string("12"); }
	if(fen=="8/5p2/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 13", 1); Options[string("Skill Level")] = string("13"); }
	if(fen=="8/6p1/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 14", 1); Options[string("Skill Level")] = string("14"); }
	if(fen=="8/7p/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 15", 1); Options[string("Skill Level")] = string("15"); }
	if(fen=="8/8/p7/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 16", 1); Options[string("Skill Level")] = string("16"); }
	if(fen=="8/8/1p6/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 17", 1); Options[string("Skill Level")] = string("17"); }
	if(fen=="8/8/2p5/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 18", 1); Options[string("Skill Level")] = string("18"); }
	if(fen=="8/8/3p4/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 19", 1); Options[string("Skill Level")] = string("19"); }
	if(fen=="8/8/4p3/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 20", 1); Options[string("Skill Level")] = string("20"); }

	//set time control
	if(fen=="n7/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  1", 1); limits=resetLimits; limits.movetime=1000; }
	if(fen=="1n6/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  5", 1); limits=resetLimits; limits.movetime=5000; }
	if(fen=="2n5/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 10", 1); limits=resetLimits; limits.movetime=10000; }
	if(fen=="3n4/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 20", 1); limits=resetLimits; limits.movetime=20000; }
	if(fen=="4n3/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 40", 1); limits=resetLimits; limits.movetime=40000; }
	if(fen=="5n2/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 1m", 1); limits=resetLimits; limits.movetime=60000; }
	if(fen=="6n1/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 3m", 1); limits=resetLimits; limits.movetime=180000; }
	if(fen=="7n/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 5m", 1); limits=resetLimits; limits.movetime=300000; }

	//board orientation
	if(fen=="RNBKQBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbkqbnr w KQkq - 0 1") { cout << "right" << endl; dgtnixSetOption(DGTNIX_BOARD_ORIENTATION, DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT); }
}

void loop(const string& args) {
	// FEN string of the initial position, normal chess
	  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

	/*if(argc!=2)
	      {
	        cout << "usage: ./stockfish dgt port" << endl;
	        cout << "Port is the port to which the board is connected." << endl;
	        cout << "For usb connection, try : /dev/usb/tts/0, /dev/usb/tts/1, /dev/usb/tts/2 ..." << endl;
	        cout << "For serial, try : /dev/ttyS0, /dev/ttyS1, /dev/ttyS2 ..." << endl;
	        //cout << "For the virtual board /tmp/dgtnixBoard is the default but you can change it." << endl;
	        exit(1);
	      }*/

	    Position pos(StartFEN, false, Threads.main_thread()); // The root position

	    //compute the next legal fens
	    StateInfo state;

	    std::map<string,Move> legalFENs;
	    for (MoveList<LEGAL> ml(pos); !ml.end(); ++ml)
	    {
	        //StateInfo state;
	        pos.do_move(ml.move(),state);
	        //cout<<pos.to_fen()<<endl;
	        string str=pos.to_fen();
	        str=str.substr(0, str.find(' '));
	        legalFENs.insert(pair<string,Move>(str,ml.move()));
	        cout << str << endl;
	        pos.undo_move(ml.move());
	    }


	    int BoardDescriptor;
	    //int optionError=0;
	    char port[256];
	    /* all debug informations are printed */
	    dgtnixSetOption(DGTNIX_DEBUG, DGTNIX_DEBUG_WITH_TIME);
	    /* Initialize the driver with port argv[2] */
	    strncpy(port, args.c_str(), 256);
	    BoardDescriptor=dgtnixInit(port);
	    int err = dgtnix_errno;
	    if(BoardDescriptor < 0 )
	      {
	        cout << "Unable to connect to DGT board on port "  << port <<" : " ;
	        switch (BoardDescriptor)
	          {
	          case -1:
	            cout << strerror(err) << endl;
	            break;
	          case -2:
	            cout << "Not responding to the DGT_SEND_BRD message" << endl;
	            break;
	          default:
	            cout << "Unrecognized response to the DGT_SEND_BRD message :" <<  BoardDescriptor <<endl;
	          }
	        exit(-1);
	      }
	    cout << "The board was found" << BoardDescriptor << endl;


	    dgtnixPrintMessageOnClock(" hello", 1);
	    dgtnixUpdate();

	    bool searching=false;


	    limits.movetime=5000;
	    string currentFEN=getDgtFEN();
	    configure(currentFEN); //useful for orientation
	    //pos.from_fen(currentFEN,false, Threads.main_thread());

	    //"8/8/8/8/8/8/8/PP6 w KQkq - 0 1"
	    while(true)
	    {
	        //cout << "DGT Loop" << endl;
	        //dgtnixPrintMessageOnClock("abcdeg", 1);

	        string s=getDgtFEN();
	        if(currentFEN!=s)
	        {
	            currentFEN=s;
	            //pos.from_fen(currentFEN,false, Threads.main_thread());

	            cout << currentFEN << endl;

	            //if(currentFEN=="8/8/8/8/8/8/8/PP6 w KQkq - 0 1")  { dgtnixPrintMessageOnClock("config", 1); }

	            //if(currentFEN.find("/PP6 w")!=string::npos) //config mode
	            //{
	            	configure(currentFEN);
	            //}

	            //launch the search
	            string str=currentFEN.substr(0, currentFEN.find(' '));
	            if(legalFENs.count(str))
	            {

	                Position p(pos);
	                p.do_move(legalFENs.at(str),state);


	                Search::Signals.stop = true;
	                Threads.wait_for_search_finished(); // Cannot quit while threads are running

	                dgtnixPrintMessageOnClock("search", 0);

	                vector<Move> searchMoves;
	                Threads.start_searching(p, limits, searchMoves);
	                searching=true;
	            }



	            /*
	            Search::Signals.stop = true;
	            Threads.wait_for_search_finished(); // Cannot quit while threads are running

	            Search::LimitsType limits;
	            limits.movetime=3000;
	            vector<Move> searchMoves;

	            Threads.start_searching(pos, limits, searchMoves);*/

	        }

	        //check for pos update after search
	        if(Search::Signals.stop == true && searching)
	        {
	            searching=false;
	            cout << "stopped with move " <<  move_to_uci(Search::RootMoves[0].pv[0], false) << endl;

	            //print the move on the clock
	            string dgtMove=move_to_uci(Search::RootMoves[0].pv[0], false);
	            dgtMove.insert(2, 1, ' ');
	            if(dgtMove.length()<6) dgtMove.append(" ");
	            cout << '[' <<  dgtMove << ']' << endl;
	            dgtnixPrintMessageOnClock(dgtMove.c_str(), 1);


	            string str=currentFEN.substr(0, currentFEN.find(' ')); //do the player move
	            pos.do_move(legalFENs.at(str),state);
	            pos.do_move(Search::RootMoves[0].pv[0],state); //do the engine's move
	            legalFENs.clear();
	            for (MoveList<LEGAL> ml(pos); !ml.end(); ++ml)
	            {
	                //StateInfo state;
	                pos.do_move(ml.move(),state);
	                cout<<pos.to_fen()<<endl;
	                string str=pos.to_fen();
	                str=str.substr(0, str.find(' '));
	                legalFENs.insert(pair<string,Move>(str,ml.move()));
	                cout << str << endl;
	                pos.undo_move(ml.move());
	            }
	        }

	        //sleep
	        struct timespec tim, tim2;
	        tim.tv_sec = 0;
	        tim.tv_nsec = 30000000L;
	        nanosleep(&tim , &tim2);
	    }

	    dgtnixClose();
}

}
